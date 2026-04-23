# Fz10m Phase 1 — Wavetable Synth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current Fz10m gain plugin with a monophonic wavetable synth — drawable 128-point wavetable, ADSR amp envelope, state-variable low-pass filter with Q, master gain — all driven by MIDI.

**Architecture:** Mirrors `iPlug2/Examples/IPlugInstrument`. One plugin-owned `Fz10mDSP` that holds a `MidiSynth` configured for one voice (`kPolyModeMono`), plus a shared `std::array<float, 128>` wavetable buffer. A custom `WavetableOscillator<T>` (subclass of `IOscillator<T>`) reads from the buffer; each `Fz10mVoice` owns an oscillator, an `ADSREnvelope<T>`, and an `SVF<T>`. The UI uses stock iPlug2 controls: one `IVMultiSliderControl<128>` for the wavetable, seven `IVKnobControl`s for params, and an `IVKeyboardControl` for testing without a DAW. Drawing into the multi-slider ships a 128-float snapshot to the audio thread via `SendArbitraryMsgFromUI`, handled in `OnMessage`.

**Tech Stack:** iPlug2 (C++17 plugin framework), xcodebuild, `just` (build runner), pluginval (host validation).

**Spec:** `docs/superpowers/specs/2026-04-22-fz10m-phase1-wavetable-synth-design.md`

**Key references:**
- `iPlug2/Examples/IPlugInstrument/IPlugInstrument.cpp` — plugin + UI + MIDI routing reference
- `iPlug2/Examples/IPlugInstrument/IPlugInstrument_DSP.h` — Voice / DSP pattern reference
- `iPlug2/Examples/IPlugChunks/IPlugChunks.cpp:23-31` — `IVMultiSliderControl` + `SendArbitraryMsgFromUI` pattern
- `iPlug2/IPlug/Extras/Oscillator.h` — `IOscillator<T>` base to subclass
- `iPlug2/IPlug/Extras/ADSREnvelope.h` — ADSR
- `iPlug2/IPlug/Extras/SVF.h` — state-variable filter
- `iPlug2/IPlug/Extras/Synth/MidiSynth.h` — synth runtime
- `iPlug2/IPlug/Extras/Synth/SynthVoice.h` — `SynthVoice` base + `kVoiceControlPitch` etc.

---

## File structure

Every file lives at the repo root level (`/Users/jack/repos/personal/Fz10m/`):

| File | Responsibility |
|---|---|
| `config.h` (modified) | Compile-time plugin flags: `PLUG_TYPE 1`, `PLUG_DOES_MIDI_IN 1`, `PLUG_CHANNEL_IO "0-2"` |
| `Fz10m.h` (modified) | Plugin class declaration, param enum, control-tag enum, message-tag enum, `IPlug_include_in_plug_hdr.h` |
| `Fz10m.cpp` (rewritten) | Plugin constructor (params + UI), `ProcessBlock`, `ProcessMidiMsg`, `OnReset`, `OnParamChange`, `OnMessage`, `OnIdle` |
| `Fz10m_DSP.h` (new, header-only) | `WavetableOscillator<T>`, `Fz10mVoice`, `Fz10mDSP<T>`. All templates, header-only, no `.cpp` needed. |
| `projects/Fz10m-macOS.xcodeproj/project.pbxproj` (maybe modified) | Add `Fz10m_DSP.h` to the project if Xcode doesn't auto-discover it. |

**Why this shape:** Matches iPlug2's own `IPlugInstrument` convention exactly. DSP header is template-heavy and small enough to stay in one file; any larger and we'd split per-component. The plan.md describes distinct modules (Oscillator, LoFi, Filter, Synth) but Phase 1 only has the Oscillator and Filter bits — shipping them as separate files now would be premature.

---

## Task 1: Bump `config.h` to instrument-with-MIDI

**Files:**
- Modify: `/Users/jack/repos/personal/Fz10m/config.h`

- [ ] **Step 1: Read current values**

Run:
```bash
grep -nE 'PLUG_TYPE|PLUG_DOES_MIDI|PLUG_CHANNEL_IO' /Users/jack/repos/personal/Fz10m/config.h
```
Expected:
```
18:#define PLUG_CHANNEL_IO "2-2"
21:#define PLUG_TYPE 0
22:#define PLUG_DOES_MIDI_IN 0
23:#define PLUG_DOES_MIDI_OUT 0
```

- [ ] **Step 2: Edit three lines**

Change `PLUG_CHANNEL_IO "2-2"` → `PLUG_CHANNEL_IO "0-2"` (no audio inputs; stereo output).
Change `PLUG_TYPE 0` → `PLUG_TYPE 1` (instrument, not effect).
Change `PLUG_DOES_MIDI_IN 0` → `PLUG_DOES_MIDI_IN 1`.
Leave `PLUG_DOES_MIDI_OUT 0` alone.

After edits:
```
#define PLUG_CHANNEL_IO "0-2"
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
```

- [ ] **Step 3: Verify the edits**

Run:
```bash
grep -nE 'PLUG_TYPE|PLUG_DOES_MIDI|PLUG_CHANNEL_IO' /Users/jack/repos/personal/Fz10m/config.h
```
Expect the four lines above.

- [ ] **Step 4: Commit**

```bash
cd /Users/jack/repos/personal/Fz10m
git add config.h
git commit -m "config: flip to instrument with MIDI input

PLUG_TYPE 1 (synth), PLUG_DOES_MIDI_IN 1, PLUG_CHANNEL_IO \"0-2\"
for stereo output with no audio inputs."
```

---

## Task 2: Replace `Fz10m.h` with synth declarations

**Files:**
- Modify: `/Users/jack/repos/personal/Fz10m/Fz10m.h`

This replaces the current three-param (gain only) enum with the seven Phase 1 params, adds a control-tag enum for the keyboard + wavetable controls, and a message-tag enum for UI→DSP communication. Also forward-declares the DSP wrapper from `Fz10m_DSP.h`.

- [ ] **Step 1: Read current contents for context**

Run:
```bash
cat /Users/jack/repos/personal/Fz10m/Fz10m.h
```
Confirm it's the current gain-plugin declaration (≈20-30 lines, has `kParamGain`, `kCtrlTagSlider`, `kCtrlTagTitle`, `kCtrlTagVersionNumber`).

- [ ] **Step 2: Overwrite with the new contents**

Full new contents of `/Users/jack/repos/personal/Fz10m/Fz10m.h`:

```cpp
#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"

const int kNumPresets = 1;

enum EParams
{
  kParamGain = 0,
  kParamAttack,
  kParamDecay,
  kParamSustain,
  kParamRelease,
  kParamCutoff,
  kParamResonance,
  kNumParams
};

enum EControlTags
{
  kCtrlTagWavetable = 0,
  kCtrlTagKeyboard,
  kNumCtrlTags
};

enum EMessageTags
{
  kMsgTagWavetableChanged = 0,
};

#if IPLUG_DSP
#include "Fz10m_DSP.h"
#endif

using namespace iplug;
using namespace igraphics;

class Fz10m final : public Plugin
{
public:
  Fz10m(const InstanceInfo& info);

#if IPLUG_DSP
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  Fz10mDSP<sample> mDSP;
#endif
};
```

- [ ] **Step 3: Verify the write landed**

Run:
```bash
grep -E 'kParamCutoff|kCtrlTagWavetable|kMsgTagWavetableChanged|Fz10mDSP' /Users/jack/repos/personal/Fz10m/Fz10m.h
```
Expected: 4 matching lines, one per pattern.

No commit yet — `Fz10m_DSP.h` is referenced but doesn't exist, and `Fz10m.cpp` will break. Tasks 3 and 4 fix that.

---

## Task 3: Create `Fz10m_DSP.h` — oscillator, voice, and DSP wrapper

**Files:**
- Create: `/Users/jack/repos/personal/Fz10m/Fz10m_DSP.h`

This is the most complex task. Breaking it into three sub-components (oscillator → voice → DSP wrapper), each appended to the same file.

- [ ] **Step 1: Create the file with the opening boilerplate and `WavetableOscillator`**

Create `/Users/jack/repos/personal/Fz10m/Fz10m_DSP.h` with this content:

```cpp
#pragma once

#include <array>
#include <cmath>

#include "ADSREnvelope.h"
#include "MidiSynth.h"
#include "Oscillator.h"
#include "SVF.h"
#include "Smoothers.h"

using namespace iplug;

static constexpr int kWavetableSize = 128;

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
    // mPhase is kept in [0,1) by iPlug2 convention, so wrap manually if needed
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
```

- [ ] **Step 2: Append the `Fz10mVoice` class to `Fz10m_DSP.h`**

Append this to the end of the file:

```cpp

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
  }

  void SetProgramNumber(int pgm) override {}
  void SetControl(int controlNumber, float value) override {}

  void SetSustainLevel(T level) { mSustainLevel = level; }

public:
  WavetableOscillator<T> mOsc;
  ADSREnvelope<T> mAmpEnv;
  SVF<T> mFilter;
  T mSustainLevel = T(0.5);
};
```

- [ ] **Step 3: Append the `Fz10mDSP` wrapper class to `Fz10m_DSP.h`**

Append this:

```cpp

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
    mGainSmoother.SetSampleRate(sampleRate);
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
          dynamic_cast<Fz10mVoice<T>&>(voice).mAmpEnv.SetStageTime(stage, value);
        });
        break;
      }
      case 3 /* kParamSustain */:
      {
        // ADSREnvelope::Process(T sustainLevel) takes sustain as a per-sample
        // argument. We cache it on the voice and pass it in each frame.
        const T sus = static_cast<T>(value / 100.0);
        mSynth.ForEachVoice([sus](SynthVoice& voice) {
          dynamic_cast<Fz10mVoice<T>&>(voice).SetSustainLevel(sus);
        });
        break;
      }
      case 5 /* kParamCutoff */:
        mSynth.ForEachVoice([value](SynthVoice& voice) {
          dynamic_cast<Fz10mVoice<T>&>(voice).mFilter.SetFreqCPS(value);
        });
        break;
      case 6 /* kParamResonance */:
        mSynth.ForEachVoice([value](SynthVoice& voice) {
          dynamic_cast<Fz10mVoice<T>&>(voice).mFilter.SetQ(value);
        });
        break;
      default:
        break;
    }
  }

  /** Called from the audio thread via OnMessage, given a fresh 128-float snapshot. */
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
```

- [ ] **Step 4: Verify the whole file builds cleanly**

Sanity-check the file's shape:
```bash
grep -nE '^class|^template' /Users/jack/repos/personal/Fz10m/Fz10m_DSP.h
```
Expected: three `template <typename T>` lines and three `class ...` lines (`WavetableOscillator`, `Fz10mVoice`, `Fz10mDSP`).

Also make sure the file closes cleanly (no trailing open brace):
```bash
tail -3 /Users/jack/repos/personal/Fz10m/Fz10m_DSP.h
```
Last non-blank line should be `};`.

- [ ] **Step 5: First build attempt — expect some compile errors, they tell us what to fix**

Run:
```bash
cd /Users/jack/repos/personal/Fz10m
just build 2>&1 | tee /tmp/fz10m-build.log | tail -30
```

If `** BUILD SUCCEEDED **`, skip to Step 7.

If it fails, the most likely issues (with fixes):

**Issue A**: `MidiSynth::AddMidiMsgToQueue` signature mismatch. Verify:
```bash
grep -n 'AddMidiMsgToQueue' /Users/jack/repos/personal/Fz10m/iPlug2/IPlug/Extras/Synth/MidiSynth.h
```
If the name differs, use the exact name from the header.

**Issue B**: `LogParamSmooth` constructor mismatch. Verify:
```bash
grep -n 'LogParamSmooth\|class LogParamSmooth' /Users/jack/repos/personal/Fz10m/iPlug2/IPlug/Extras/Smoothers.h | head
```
Adjust constructor args to match.

Fix the first error, rerun the build, repeat until `** BUILD SUCCEEDED **`. If stuck after 3 iterations, report BLOCKED with the error log.

- [ ] **Step 6: Still Task 3 — no commit yet.**

Build succeeds on its own only if `Fz10m.cpp` has already been updated to match. Since we're doing `.h` + DSP now and `.cpp` in Task 4, it's OK if the build fails purely on missing `Fz10m.cpp` symbols; we verify the DSP header itself compiles in isolation by building in Task 4.

- [ ] **Step 7: Verify file size is sane**

```bash
wc -l /Users/jack/repos/personal/Fz10m/Fz10m_DSP.h
```
Expected: between 150 and 220 lines. If it's dramatically different, you probably appended wrong.

No commit — Task 4 will finalize.

---

## Task 4: Rewrite `Fz10m.cpp`

**Files:**
- Modify: `/Users/jack/repos/personal/Fz10m/Fz10m.cpp` (rewrite entire file)

- [ ] **Step 1: Overwrite with the new implementation**

Full new contents of `/Users/jack/repos/personal/Fz10m/Fz10m.cpp`:

```cpp
#include "Fz10m.h"
#include "IPlug_include_in_plug_src.h"

Fz10m::Fz10m(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100., 0.01, "%");
  GetParam(kParamAttack)->InitDouble("Attack", 10., 1., 1000., 0.1, "ms",
                                     IParam::kFlagsNone, "ADSR",
                                     IParam::ShapePowCurve(3.));
  GetParam(kParamDecay)->InitDouble("Decay", 10., 1., 1000., 0.1, "ms",
                                    IParam::kFlagsNone, "ADSR",
                                    IParam::ShapePowCurve(3.));
  GetParam(kParamSustain)->InitDouble("Sustain", 50., 0., 100., 1., "%",
                                      IParam::kFlagsNone, "ADSR");
  GetParam(kParamRelease)->InitDouble("Release", 10., 2., 1000., 0.1, "ms",
                                      IParam::kFlagsNone, "ADSR",
                                      IParam::ShapePowCurve(3.));
  GetParam(kParamCutoff)->InitFrequency("Cutoff", 20000., 20., 20000.);
  GetParam(kParamResonance)->InitDouble("Resonance", 0.707, 0.5, 10., 0.01);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IRECT b = pGraphics->GetBounds().GetPadded(-20.f);

    // Top: wavetable drawing surface (128 sliders)
    const IRECT wtBounds = b.GetFromTop(220.f);

    // Initialize wavetable slider values to a sine (displayed range 0..1)
    auto* pWT = new IVMultiSliderControl<kWavetableSize>(wtBounds, "Wavetable",
                                                         DEFAULT_STYLE);
    pGraphics->AttachControl(pWT, kCtrlTagWavetable);
    for (int i = 0; i < kWavetableSize; ++i)
    {
      const double v = 0.5 + 0.5 * std::sin(2.0 * M_PI * i / kWavetableSize);
      pWT->SetValue(v, i);
    }
    pWT->SetActionFunction([pGraphics](IControl* pCaller) {
      float vals[kWavetableSize];
      for (int i = 0; i < kWavetableSize; ++i)
        vals[i] = static_cast<float>(pCaller->GetValue(i));
      pGraphics->GetDelegate()->SendArbitraryMsgFromUI(kMsgTagWavetableChanged,
                                                      kCtrlTagWavetable,
                                                      sizeof(vals), vals);
    });

    // Middle row: knobs
    const IRECT knobRow = b.ReduceFromTop(240.f).GetFromTop(140.f);
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 1, 7).GetCentredInside(100), kParamGain, "Gain"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 1, 7).GetCentredInside(100), kParamAttack, "Attack"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 2, 7).GetCentredInside(100), kParamDecay, "Decay"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 3, 7).GetCentredInside(100), kParamSustain, "Sustain"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 4, 7).GetCentredInside(100), kParamRelease, "Release"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 5, 7).GetCentredInside(100), kParamCutoff, "Cutoff"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 6, 7).GetCentredInside(100), kParamResonance, "Res"));

    // Bottom: on-screen keyboard
    const IRECT kbBounds = b.GetFromBottom(180.f);
    pGraphics->AttachControl(new IVKeyboardControl(kbBounds), kCtrlTagKeyboard);

    pGraphics->SetQwertyMidiKeyHandlerFunc([pGraphics](const IMidiMsg& msg) {
      pGraphics->GetControlWithTag(kCtrlTagKeyboard)
        ->As<IVKeyboardControl>()
        ->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
    });
  };
#endif
}

#if IPLUG_DSP
void Fz10m::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  mDSP.ProcessBlock(outputs, 2, nFrames);
}

void Fz10m::ProcessMidiMsg(const IMidiMsg& msg)
{
  const int status = msg.StatusMsg();
  switch (status)
  {
    case IMidiMsg::kNoteOn:
    case IMidiMsg::kNoteOff:
    case IMidiMsg::kPolyAftertouch:
    case IMidiMsg::kControlChange:
    case IMidiMsg::kProgramChange:
    case IMidiMsg::kChannelAftertouch:
    case IMidiMsg::kPitchWheel:
      mDSP.ProcessMidiMsg(msg);
      SendMidiMsg(msg);
      break;
    default:
      break;
  }
}

void Fz10m::OnReset()
{
  mDSP.Reset(GetSampleRate(), GetBlockSize());
}

void Fz10m::OnParamChange(int paramIdx)
{
  mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

bool Fz10m::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if (msgTag == kMsgTagWavetableChanged && ctrlTag == kCtrlTagWavetable)
  {
    const int count = dataSize / static_cast<int>(sizeof(float));
    mDSP.UpdateWavetable(static_cast<const float*>(pData), count);
    return true;
  }
  return false;
}
#endif
```

- [ ] **Step 2: Verify file contents**

```bash
grep -nE 'ProcessBlock|ProcessMidiMsg|OnReset|OnParamChange|OnMessage|UpdateWavetable' /Users/jack/repos/personal/Fz10m/Fz10m.cpp
```
Expected: 6 matching lines (one per named method call/definition) — actually might match more (multiple occurrences), but should include all six symbols at least once each.

- [ ] **Step 3: Build**

```bash
cd /Users/jack/repos/personal/Fz10m
just clean
just build 2>&1 | tee /tmp/fz10m-build.log | tail -10
```

**Expected:** `** BUILD SUCCEEDED **`.

**If it fails:** the error log tells you which API name is wrong in `Fz10m_DSP.h` or `Fz10m.cpp`. The three most likely failure classes (from Task 3 Step 5 notes):

1. `LogParamSmooth` constructor arity — check the header and fix.
2. `MidiSynth::AddMidiMsgToQueue` name — present per `IPlugInstrument_DSP.h:166`, should work.
3. Xcode project not finding `Fz10m_DSP.h` — see Step 3 addendum below.

If Xcode complains the `Fz10m_DSP.h` file is not found even though it's sitting next to `Fz10m.h`, add it to the Xcode project: open `Fz10m.xcworkspace`, drag `Fz10m_DSP.h` into the file tree under `Project Files` (matches where `Fz10m.h` is), close without saving other changes. Alternatively, ignore it — since it's only `#include`d from `Fz10m.h`, Xcode finds it transitively by header search path.

- [ ] **Step 4: Verify the plugin is installed**

```bash
ls -la ~/Library/Audio/Plug-Ins/VST3/Fz10m.vst3/Contents/MacOS/Fz10m
codesign -dvvv ~/Library/Audio/Plug-Ins/VST3/Fz10m.vst3 2>&1 | grep -E 'Signature|Sealed'
```
Expected:
- Mach-O binary present
- `Signature=adhoc`
- `Sealed Resources version=2`

- [ ] **Step 5: pluginval smoke test**

```bash
cd /Users/jack/repos/personal/Fz10m
just validate 2>&1 | tail -10
```
Expected: final line `SUCCESS`.

If it fails with a message complaining about MIDI or plugin-type flags, re-check Task 1 config edits.

- [ ] **Step 6: Commit**

```bash
cd /Users/jack/repos/personal/Fz10m
git add config.h Fz10m.h Fz10m.cpp Fz10m_DSP.h
git commit -m "feat: Phase 1 wavetable synth — osc, ADSR, SVF filter

Replace the gain plugin with a monophonic MIDI synth.

- Fz10m_DSP.h: WavetableOscillator<T>, Fz10mVoice (SynthVoice subclass),
  Fz10mDSP<T> wrapper with MidiSynth in mono mode.
- Fz10m.cpp: new param list (Gain/ADSR/Cutoff/Res), UI with
  IVMultiSliderControl<128> wavetable, keyboard, knobs. Wavetable
  changes travel UI→DSP via SendArbitraryMsgFromUI.
- config.h: PLUG_TYPE=1, PLUG_DOES_MIDI_IN=1, PLUG_CHANNEL_IO=\"0-2\"."
```

If the Xcode project needed a manual file-add, include `projects/Fz10m-macOS.xcodeproj/project.pbxproj` in the staged files.

---

## Task 5: Audible verification in Ableton

**Files:** (none — live host test)

- [ ] **Step 1: Open a Live test session**

Launch Ableton Live. Create a new Live set (or open an existing test one). Create a MIDI track. Set its Input to "No Input", Arm ON, Monitor In.

- [ ] **Step 2: Load Fz10m**

In the Browser, Plug-Ins → VST3 → Grovepark → Fz10m. Drag onto the MIDI track.

**If Fz10m doesn't appear:** See the codesign / DB-reset dance from earlier in the project. Quick fix:
```bash
# Quit Live fully first
pgrep -lf "Live|Ableton" && echo "Live still running — quit it first" || echo "ok"
sqlite3 ~/Library/Application\ Support/Ableton/Live\ Database/Live-plugins-1.db \
  "DELETE FROM plugins WHERE module_id IN (SELECT module_id FROM plugin_modules WHERE path LIKE '%Fz10m%');"
sqlite3 ~/Library/Application\ Support/Ableton/Live\ Database/Live-plugins-1.db \
  "DELETE FROM plugin_modules WHERE path LIKE '%Fz10m%';"
rm -f ~/Library/Application\ Support/Ableton/Live\ Database/Live-plugins-1.db-wal \
      ~/Library/Application\ Support/Ableton/Live\ Database/Live-plugins-1.db-shm
```
Relaunch Live. Try again.

- [ ] **Step 3: Run through each acceptance check from the spec**

Document result in a scratch note — either "pass" or describe the failure.

| Check | Action | Expected |
|---|---|---|
| Loads | Drag plugin to track | Plugin UI opens, no Live warning dialog |
| Sine plays | Play middle C (C4, MIDI 60) | Steady pure tone, ~261 Hz |
| Drawing works | Drag the multi-slider into a sawtooth | Tone becomes bright / harmonic |
| ADSR | Set Attack to ~500 ms, play a note | Note fades in smoothly |
| Sustain | Hold a note with Sustain at 50% | Note level drops to ~50% after decay and holds |
| Release | Set Release to ~1 s, release a note | Note fades out over ~1 s |
| Cutoff | Sweep Cutoff from 20 kHz down while holding a note | Tone darkens |
| Resonance | Set Res to ~5, sweep Cutoff | Audible resonance peak / whistle |
| No clicks | Hit notes rapidly | No clicks/zipper |
| Stereo | Pan check | Output on both L and R channels |

- [ ] **Step 4: Report pass/fail for each**

If all pass, proceed to Task 6. If any fail, root-cause the issue (likely a bug in Task 3 or 4). The most likely bugs given the shape of this code:

- **No sound at all:** MidiSynth pitch control isn't propagating. Check that `kVoiceControlPitch` in `mInputs` is being read correctly and that `AddMidiMsgToQueue` is being called.
- **Sound but no pitch variation:** Frequency math in `ProcessSamplesAccumulating`. Log or printf the `freqHz` value for one note and verify.
- **Clicks at note-on:** ADSR `mResetFunc` isn't firing. Verify the lambda captures `this` correctly and that `mOsc.Reset()` compiles.
- **Drawing changes don't affect sound:** `OnMessage` dispatch is broken. Add a `DBGMSG("got %d bytes\n", dataSize);` temporarily to confirm it fires.

- [ ] **Step 5: No commit (test only, no code changes) unless fixes were needed**

If fixes were needed, amend the Task 4 commit:
```bash
cd /Users/jack/repos/personal/Fz10m
git add -A
git commit --amend --no-edit
```
(Safe to amend: the commit hasn't been pushed and was made in this session by us.)

---

## Task 6: Remove `tmp/` reference clones (cleanup)

**Files:**
- Delete: `/Users/jack/repos/personal/Fz10m/tmp/` (contents only; `.gitignore` already excludes it)

- [ ] **Step 1: Confirm nothing in tmp/ is referenced from the source**

```bash
cd /Users/jack/repos/personal/Fz10m
grep -rE 'tmp/(Wavetable|wavetable-synth|Soundpipe|Downsamplr)' \
  --include='*.cpp' --include='*.h' --include='*.py' \
  --exclude-dir=tmp --exclude-dir=iPlug2 . 2>&1 | head
```
Expected: no output.

- [ ] **Step 2: Remove the directory**

```bash
cd /Users/jack/repos/personal/Fz10m
rm -rf tmp/
```

- [ ] **Step 3: Verify `.gitignore` still ignores it**

```bash
cat /Users/jack/repos/personal/Fz10m/.gitignore | grep tmp
```
Expected: `tmp/` line present.

- [ ] **Step 4: Nothing to commit**

`tmp/` was gitignored and never tracked, so deleting it leaves git state unchanged. Verify:
```bash
cd /Users/jack/repos/personal/Fz10m
git status --short
```
Expected: empty.

---

## Self-review notes

**Spec coverage:**

| Spec requirement | Implemented in |
|---|---|
| Monophonic via `MidiSynth` with `kPolyModeMono` | Task 3 Step 3 (Fz10mDSP ctor) |
| Drawable 128-point wavetable | Task 4 Step 1 (`IVMultiSliderControl<kWavetableSize>`) |
| Wavetable seeded to sine | Task 3 Step 3 (`Fz10mDSP` ctor) and Task 4 Step 1 (UI init loop) |
| `WavetableOscillator` subclassing `IOscillator` with linear interp | Task 3 Step 1 |
| `Fz10mVoice` with osc + ADSR + SVF | Task 3 Step 2 |
| ADSR `mResetFunc` resets osc on retrigger | Task 3 Step 2 (ctor lambda) |
| UI→DSP via `SendArbitraryMsgFromUI` / `OnMessage` | Task 4 Step 1 |
| Stock iPlug2 controls only | Task 4 Step 1 (IVKnob, IVMultiSlider, IVKeyboard) |
| `PLUG_TYPE 1`, `PLUG_DOES_MIDI_IN 1`, `PLUG_CHANNEL_IO "0-2"` | Task 1 |
| Params: Gain, A, D, S, R, Cutoff, Q | Task 2 + Task 4 Step 1 |
| Param ranges / defaults from spec | Task 4 Step 1 |
| Master gain smoother | Task 3 Step 3 |
| No lofi, no stepped-filter — they're Phase 2 | Confirmed absent in all tasks |
| Build passes, pluginval passes, signature sealed | Task 4 Steps 3–5 |
| Audible checks in Ableton | Task 5 |

**Placeholder scan:** clean. The "If this fails, here's what to try" sections in Task 3 Step 5 and Task 4 Step 3 are concrete, enumerated failure modes with specific fixes — not placeholders.

**Type consistency:**
- `WavetableOscillator<T>`, `Fz10mVoice<T>`, `Fz10mDSP<T>` all templated on the same `T`.
- `sample` (iPlug2's default audio type) used at the plugin boundary: `Fz10mDSP<sample> mDSP;` in `Fz10m.h`.
- `kWavetableSize = 128` used consistently across DSP header and UI layout.
- Param enum values used via named constants in Task 4; numeric literals (0–6) in `SetParam` switch are commented with the enum names and match.

**Identified risk already in spec:** Xcode project auto-discovery of `Fz10m_DSP.h`. Task 4 Step 3 addresses it inline.

---

## Out of scope reminders

Phase 2 will add:
- Bit-depth crushing (continuous knob)
- Sample-rate reduction (continuous knob)
- Stepped filter parameter updates (update every N samples)

Phase 3+:
- Preset / additive / noise wavetable sources
- Polyphony (flip `kPolyModeMono` → `kPolyModePoly`, set `nVoices > 1`)
- Filter envelope
- UI polish (your pass)
