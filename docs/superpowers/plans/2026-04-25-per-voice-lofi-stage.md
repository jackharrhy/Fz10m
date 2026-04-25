# Per-Voice Lo-Fi Stage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add per-voice sample-rate reduction and bit-depth quantization to give Fz10m FZ-inspired digital character.

**Architecture:** A small `LoFiStage<T>` class is added to `Fz10m_DSP.h`, owned by each `Fz10mVoice`. Three new parameters (Character, Rate, Bits) are added to `Fz10m.h`, wired through `Fz10m.cpp` for UI and `Fz10mDSP::SetParam()` for DSP. The lo-fi stage sits between the wavetable oscillator and the SVF filter in the per-sample voice loop.

**Tech Stack:** C++17, iPlug2 (MidiSynth, SynthVoice, SVF, ADSREnvelope, IVKnobControl)

**Spec:** `docs/superpowers/specs/2026-04-24-per-voice-lofi-design.md`

---

### Task 1: Add three new parameters to the enum and plugin constructor

**Files:**
- Modify: `Fz10m.h:8-18` (EParams enum)
- Modify: `Fz10m.cpp:7-20` (parameter init in constructor)

- [ ] **Step 1: Add parameter enum entries**

In `Fz10m.h`, add three entries before `kNumParams`:

```cpp
enum EParams
{
  kParamGain = 0,
  kParamAttack,
  kParamDecay,
  kParamSustain,
  kParamRelease,
  kParamCutoff,
  kParamResonance,
  kParamLoFiCharacter,
  kParamLoFiRate,
  kParamLoFiBits,
  kNumParams
};
```

- [ ] **Step 2: Initialize the new parameters in the constructor**

In `Fz10m.cpp`, after the `kParamResonance` init (line 20), add:

```cpp
  GetParam(kParamLoFiCharacter)->InitDouble("Character", 100., 0., 100., 0.1, "%",
                                            IParam::kFlagsNone, "LoFi");
  GetParam(kParamLoFiRate)->InitDouble("Rate", 36000., 9000., 48000., 1., "Hz",
                                       IParam::kFlagsNone, "LoFi",
                                       IParam::ShapePowCurve(2.));
  GetParam(kParamLoFiBits)->InitDouble("Bits", 8., 4., 16., 1., "bits",
                                       IParam::kFlagsNone, "LoFi");
```

Default Character is 100% (full lo-fi). Default Rate is 36 kHz. Default Bits is 8. This gives an FZ-ish character out of the box. Users turn Character toward 0% to clean it up.

- [ ] **Step 3: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 4: Commit**

```bash
git add Fz10m.h Fz10m.cpp
git commit -m "feat: add LoFi Character/Rate/Bits parameter definitions"
```

---

### Task 2: Implement LoFiStage<T> in Fz10m_DSP.h

**Files:**
- Modify: `Fz10m_DSP.h` (add class before `Fz10mVoice`)

- [ ] **Step 1: Add LoFiStage class after WavetableOscillator, before Fz10mVoice**

Insert after line 53 (closing `};` of `WavetableOscillator`) and before line 55 (comment for `Fz10mVoice`):

```cpp
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
```

- [ ] **Step 2: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 3: Commit**

```bash
git add Fz10m_DSP.h
git commit -m "feat: add LoFiStage<T> sample-hold and bitcrush class"
```

---

### Task 3: Wire LoFiStage into Fz10mVoice

**Files:**
- Modify: `Fz10m_DSP.h` (`Fz10mVoice` class)

- [ ] **Step 1: Add LoFiStage member to Fz10mVoice**

In `Fz10mVoice`, add a member alongside the existing `mOsc`, `mAmpEnv`, `mFilter` (at line 128-132):

```cpp
public:
  WavetableOscillator<T> mOsc;
  ADSREnvelope<T> mAmpEnv;
  SVF<T> mFilter;
  LoFiStage<T> mLoFi;
  T mSustainLevel = T(0.5);
```

- [ ] **Step 2: Reset LoFiStage when voice triggers**

In `Fz10mVoice::Trigger()` (currently line 75-84), add `mLoFi.Reset();` after `mOsc.Reset();`:

```cpp
  void Trigger(double level, bool isRetrigger) override
  {
    mOsc.Reset();
    mLoFi.Reset();
    if (isRetrigger)
      mAmpEnv.Retrigger(level);
    else
      mAmpEnv.Start(level);
  }
```

- [ ] **Step 3: Set LoFiStage sample rate in SetSampleRateAndBlockSize**

In `Fz10mVoice::SetSampleRateAndBlockSize()` (currently line 115-121), add `mLoFi.SetSampleRate(sampleRate);`:

```cpp
  void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
  {
    mOsc.SetSampleRate(sampleRate);
    mAmpEnv.SetSampleRate(sampleRate);
    mFilter.SetSampleRate(sampleRate);
    mFilter.Reset();
    mLoFi.SetSampleRate(sampleRate);
  }
```

- [ ] **Step 4: Insert LoFiStage into the per-sample processing loop**

In `Fz10mVoice::ProcessSamplesAccumulating()` (currently line 91-113), insert `mLoFi.Process(s)` between the oscillator and filter:

```cpp
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
```

- [ ] **Step 5: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 6: Commit**

```bash
git add Fz10m_DSP.h
git commit -m "feat: wire LoFiStage into Fz10mVoice signal chain"
```

---

### Task 4: Route new parameters from Fz10mDSP::SetParam to voices

**Files:**
- Modify: `Fz10m_DSP.h` (`Fz10mDSP::SetParam()`)

- [ ] **Step 1: Add cases for the three new parameters**

In `Fz10mDSP::SetParam()` (currently line 181-225), add three new cases before the `default:` at line 222:

```cpp
      case 7 /* kParamLoFiCharacter */:
      {
        const T amt = static_cast<T>(value / 100.0);
        mSynth.ForEachVoice([amt](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mLoFi.SetAmount(amt);
        });
        break;
      }
      case 8 /* kParamLoFiRate */:
        mSynth.ForEachVoice([value](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mLoFi.SetRateHz(value);
        });
        break;
      case 9 /* kParamLoFiBits */:
      {
        const int bits = static_cast<int>(value);
        mSynth.ForEachVoice([bits](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mLoFi.SetBits(bits);
        });
        break;
      }
```

- [ ] **Step 2: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 3: Commit**

```bash
git add Fz10m_DSP.h
git commit -m "feat: route LoFi params through Fz10mDSP::SetParam to voices"
```

---

### Task 5: Add UI knobs for the three new parameters

**Files:**
- Modify: `Fz10m.cpp` (mLayoutFunc lambda)

- [ ] **Step 1: Expand the knob grid from 1x7 to 1x10**

In `Fz10m.cpp`, the knob row is currently a `1x7` grid (lines 45-51). Change all `1, 7` grid references to `1, 10` and add three new knobs:

```cpp
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 0, 1, 10).GetCentredInside(100), kParamGain, "Gain"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 1, 1, 10).GetCentredInside(100), kParamAttack, "Attack"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 2, 1, 10).GetCentredInside(100), kParamDecay, "Decay"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 3, 1, 10).GetCentredInside(100), kParamSustain, "Sustain"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 4, 1, 10).GetCentredInside(100), kParamRelease, "Release"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 5, 1, 10).GetCentredInside(100), kParamCutoff, "Cutoff"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 6, 1, 10).GetCentredInside(100), kParamResonance, "Res"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 7, 1, 10).GetCentredInside(100), kParamLoFiCharacter, "Char"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 8, 1, 10).GetCentredInside(100), kParamLoFiRate, "Rate"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 9, 1, 10).GetCentredInside(100), kParamLoFiBits, "Bits"));
```

- [ ] **Step 2: Update the comment above the knob row**

Change the comment from `7 knobs in a 1×7 grid` to `10 knobs in a 1×10 grid`:

```cpp
    // Top row: 10 knobs in a 1×10 grid. GetGridCell has two overloads; we use
    // the 4-arg (row, col, nRows, nCols) form explicitly to avoid the 3-arg
    // (cellIndex, nRows, nCols) overload that would collapse to nRows=0.
```

- [ ] **Step 3: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 4: Commit**

```bash
git add Fz10m.cpp
git commit -m "feat: add Character/Rate/Bits knobs to UI"
```

---

### Task 6: Final build and manual verification

**Files:** None (verification only)

- [ ] **Step 1: Clean build**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug clean build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 2: Verify parameter count**

Confirm `kNumParams` is now 10 (was 7). The enum in `Fz10m.h` should list:
```text
kParamGain = 0
kParamAttack = 1
kParamDecay = 2
kParamSustain = 3
kParamRelease = 4
kParamCutoff = 5
kParamResonance = 6
kParamLoFiCharacter = 7
kParamLoFiRate = 8
kParamLoFiBits = 9
kNumParams = 10
```

And the `case` statements in `Fz10mDSP::SetParam()` use matching indices 7, 8, 9.

- [ ] **Step 3: Manual listening checks (in Ableton or standalone)**

```text
- Character 0%: should sound identical to before this change
- Character 100%, Rate 36k, Bits 8: audible lo-fi grit
- Character 100%, Rate 9k, Bits 8: heavy stepped/crunchy character
- Character 100%, Rate 48k, Bits 16: should sound clean (no rate hold, no quantization)
- ADSR, Cutoff, Resonance, Gain: should still work as before
- Wavetable drawing: should still work as before
- QWERTY keyboard: should still trigger notes
```
