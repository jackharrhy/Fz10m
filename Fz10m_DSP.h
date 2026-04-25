#pragma once

#include <array>
#include <cmath>

#include "ADSREnvelope.h"
#include "MidiSynth.h"
#include "Oscillator.h"
#include "SVF.h"
#include "Smoothers.h"

using namespace iplug;

// kWavetableSize is declared in Fz10m.h (shared with the UI layer).

/** Linear-interpolated single-cycle wavetable oscillator.
 *  Non-owning reference to a wavetable buffer held by Fz10mDSP. */
template <typename T>
class WavetableOscillator : public IOscillator<T>
{
public:
  WavetableOscillator() = default;

  void SetTable(const T* pTable, int tableSize)
  {
    mTable = pTable;
    mTableSize = tableSize;
  }

  T Process(double freqHz) override
  {
    IOscillator<T>::SetFreqCPS(freqHz);
    IOscillator<T>::mPhase = IOscillator<T>::mPhase + IOscillator<T>::mPhaseIncr;
    // Wavetable lookup needs phase in [0,1). iPlug2's IOscillator base doesn't
    // enforce this — SinOscillator relies on sin's periodicity instead — so we
    // wrap explicitly here. At audio rates with freqHz < SR/2, phaseIncr < 0.5,
    // so each while loop body runs at most once per call.
    while (IOscillator<T>::mPhase >= 1.0) IOscillator<T>::mPhase -= 1.0;
    while (IOscillator<T>::mPhase < 0.0) IOscillator<T>::mPhase += 1.0;

    if (!mTable || mTableSize <= 0) return T(0);

    const double pos = IOscillator<T>::mPhase * mTableSize;
    const int i0 = static_cast<int>(pos);
    const int i1 = (i0 + 1) % mTableSize;
    const T frac = static_cast<T>(pos - i0);
    return mTable[i0] * (T(1) - frac) + mTable[i1] * frac;
  }

private:
  const T* mTable = nullptr;
  int mTableSize = 0;
};

/** Per-voice lo-fi effect: sample-rate hold + bit-depth quantization.
 *  Simulates lower sample rates by holding values, and lower bit depths
 *  by quantizing amplitude. Amount blends between clean and processed. */
template <typename T>
class LoFiStage
{
public:
  LoFiStage() = default;

  void SetSampleRate(double hostSampleRate) { mHostSampleRate = hostSampleRate; }

  /** Target lo-fi rate in Hz (e.g. 36000, 18000, 9000).
   *  Values >= hostSampleRate effectively bypass the hold. */
  void SetRateHz(double rateHz) { mRateHz = rateHz; }

  /** Bit depth for quantization (e.g. 8, 12, 16).
   *  16 is effectively transparent. */
  void SetBits(int bits) { mBits = bits; }

  /** Wet/dry blend: 0 = fully clean, 1 = fully crushed. */
  void SetAmount(T amount) { mAmount = amount; }

  void Reset()
  {
    mPhase = 0.0;
    mHeld = T(0);
    mInitialized = false;
  }

  T Process(T input)
  {
    // Bypass: rate at or above host rate AND bits >= 16 means no effect
    if (mAmount <= T(0))
      return input;

    // Sample-rate hold via phase accumulator
    if (!mInitialized)
    {
      mHeld = _Quantize(input);
      mInitialized = true;
    }

    if (mRateHz < mHostSampleRate)
    {
      mPhase += mRateHz / mHostSampleRate;
      if (mPhase >= 1.0)
      {
        mPhase -= 1.0;
        mHeld = _Quantize(input);
      }
    }
    else
    {
      // No rate reduction, but still quantize
      mHeld = _Quantize(input);
    }

    // Blend clean input with held/quantized output
    return input + mAmount * (mHeld - input);
  }

private:
  T _Quantize(T input) const
  {
    if (mBits >= 16) return input;
    const T levels = static_cast<T>(1 << mBits);
    // Clamp to [-1, 1], quantize, then scale back
    const T clamped = std::max(T(-1), std::min(T(1), input));
    return std::round(clamped * levels) / levels;
  }

  double mHostSampleRate = 48000.0;
  double mRateHz = 36000.0;
  double mPhase = 0.0;
  int mBits = 8;
  T mAmount = T(1);
  T mHeld = T(0);
  bool mInitialized = false;
};

/** One voice of the synth. Owns oscillator, amp envelope, and filter.
 *  Constructed with a pointer to the shared wavetable buffer. */
template <typename T>
class Fz10mVoice : public SynthVoice
{
public:
  Fz10mVoice(const T* pTable, int tableSize)
  : mAmpEnv("amp", [this]() { mOsc.Reset(); })
  {
    mOsc.SetTable(pTable, tableSize);
    mFilter.SetMode(SVF<T>::kLowPass);
    mFilter.SetFreqCPS(20000.0);
    mFilter.SetQ(0.707);
  }

  bool GetBusy() const override
  {
    return mAmpEnv.GetBusy();
  }

  void Trigger(double level, bool isRetrigger) override
  {
    mOsc.Reset();  // belt-and-braces: ADSR's reset callback also triggers on attack,
                   // but explicit reset here matches IPlugInstrument and ensures
                   // phase alignment on every note-on regardless of env state.
    mLoFi.Reset();
    if (isRetrigger)
      mAmpEnv.Retrigger(level);
    else
      mAmpEnv.Start(level);
  }

  void Release() override
  {
    mAmpEnv.Release();
  }

  void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs,
                                  int startIdx, int nFrames) override
  {
    const double pitch = mInputs[kVoiceControlPitch].endValue;
    const double pitchBend = mInputs[kVoiceControlPitchBend].endValue;
    const double freqHz = 440.0 * std::pow(2.0, pitch + pitchBend);

    // iPlug2's SVF is block-based. We buffer one frame at a time through it
    // using stack arrays for simplicity since block sizes here are small.
    for (int i = startIdx; i < startIdx + nFrames; ++i)
    {
      T s = mOsc.Process(freqHz);

      s = mLoFi.Process(s);

      T* inPtr[1] = { &s };
      T* outPtr[1] = { &s };
      mFilter.ProcessBlock(inPtr, outPtr, 1, 1);

      const T envVal = mAmpEnv.Process(mSustainLevel);
      s *= envVal;
      outputs[0][i] += s;
      outputs[1][i] += s;
    }
  }

  void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
  {
    mOsc.SetSampleRate(sampleRate);
    mAmpEnv.SetSampleRate(sampleRate);
    mFilter.SetSampleRate(sampleRate);
    mFilter.Reset();
    mLoFi.SetSampleRate(sampleRate);
  }

  void SetProgramNumber(int pgm) override {}
  void SetControl(int controlNumber, float value) override {}

  void SetSustainLevel(T level) { mSustainLevel = level; }

public:
  WavetableOscillator<T> mOsc;
  ADSREnvelope<T> mAmpEnv;
  SVF<T> mFilter;
  LoFiStage<T> mLoFi;
  T mSustainLevel = T(0.5);
};

/** Top-level DSP wrapper owned by the plugin. Holds the shared wavetable,
 *  the MidiSynth (with one mono voice), and param smoothers. */
template <typename T>
class Fz10mDSP
{
public:
  Fz10mDSP()
  : mSynth(VoiceAllocator::kPolyModeMono, MidiSynth::kDefaultBlockSize)
  {
    // Seed wavetable with one cycle of a sine
    for (int i = 0; i < kWavetableSize; ++i)
      mWavetable[i] = static_cast<T>(std::sin(2.0 * M_PI * i / kWavetableSize));

    auto* pVoice = new Fz10mVoice<T>(mWavetable.data(), kWavetableSize);
    mSynth.AddVoice(pVoice, 0);
  }

  void Reset(double sampleRate, int blockSize)
  {
    mSynth.SetSampleRateAndBlockSize(sampleRate, blockSize);
    mSynth.Reset();
    mGainSmoother.SetSmoothTime(5.0, sampleRate);
  }

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    mSynth.AddMidiMsgToQueue(msg);
  }

  void ProcessBlock(T** outputs, int nOutputs, int nFrames)
  {
    // Zero outputs (voices accumulate)
    for (int ch = 0; ch < nOutputs; ++ch)
      std::fill_n(outputs[ch], nFrames, T(0));

    mSynth.ProcessBlock(nullptr, outputs, 0, nOutputs, nFrames);

    // Apply master gain sample-by-sample with smoothing
    for (int s = 0; s < nFrames; ++s)
    {
      const T g = mGainSmoother.Process(mGainTarget);
      for (int ch = 0; ch < nOutputs; ++ch)
        outputs[ch][s] *= g;
    }
  }

  void SetParam(int paramIdx, double value)
  {
    using EEnvStage = typename ADSREnvelope<T>::EStage;
    switch (paramIdx)
    {
      case 0 /* kParamGain */:
        mGainTarget = static_cast<T>(value / 100.0);
        break;
      case 1 /* kParamAttack */:
      case 2 /* kParamDecay */:
      case 4 /* kParamRelease */:
      {
        // Attack=1, Decay=2, Release=4 → EStage offsets
        const int stage = (paramIdx == 1) ? EEnvStage::kAttack
                        : (paramIdx == 2) ? EEnvStage::kDecay
                        :                    EEnvStage::kRelease;
        mSynth.ForEachVoice([stage, value](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mAmpEnv.SetStageTime(stage, value);
        });
        break;
      }
      case 3 /* kParamSustain */:
      {
        // ADSREnvelope::Process(T sustainLevel) takes sustain as a per-sample
        // argument. We cache it on the voice and pass it in each frame.
        const T sus = static_cast<T>(value / 100.0);
        mSynth.ForEachVoice([sus](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).SetSustainLevel(sus);
        });
        break;
      }
      case 5 /* kParamCutoff */:
        mSynth.ForEachVoice([value](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mFilter.SetFreqCPS(value);
        });
        break;
      case 6 /* kParamResonance */:
        mSynth.ForEachVoice([value](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mFilter.SetQ(value);
        });
        break;
      default:
        break;
    }
  }

  /** Called via OnMessage. In iPlug2 VST3, OnMessage dispatches on the host's
   *  messaging/transport thread, not necessarily the audio thread. Writes here
   *  are NOT synchronized with audio-thread reads in WavetableOscillator::Process.
   *  Torn reads across the 128-float buffer can produce a brief click when the
   *  table changes. Acceptable for Phase 1 because updates are user-initiated
   *  (dragging the UI) and therefore rare. Revisit if we need atomic swaps. */
  void UpdateWavetable(const float* newValues01, int count)
  {
    const int n = std::min(count, kWavetableSize);
    for (int i = 0; i < n; ++i)
      mWavetable[i] = static_cast<T>(newValues01[i] * 2.0 - 1.0);
  }

private:
  MidiSynth mSynth;
  std::array<T, kWavetableSize> mWavetable {};
  LogParamSmooth<T> mGainSmoother { 5.0 }; // 5ms smoothing
  T mGainTarget = T(1);
};
