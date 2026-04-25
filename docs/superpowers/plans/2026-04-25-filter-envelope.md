# Filter Envelope Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a per-voice ADSR filter envelope that modulates cutoff frequency, with a bipolar Amount control and a two-row knob layout.

**Architecture:** Each `Fz10mVoice` gets a second `ADSREnvelope<T>` for the filter. The envelope advances every sample but only applies to the SVF on step counter ticks. Five new parameters are added. The UI splits into two rows of knob groups.

**Tech Stack:** C++17, iPlug2 (ADSREnvelope, SVF, MidiSynth, IVKnobControl, IVGroupControl)

**Spec:** `docs/superpowers/specs/2026-04-25-filter-envelope-design.md`

---

### Task 1: Add five filter envelope parameters to enum and constructor

**Files:**
- Modify: `Fz10m.h` (EParams enum)
- Modify: `Fz10m.cpp` (parameter init in constructor)

- [ ] **Step 1: Add parameter enum entries**

In `Fz10m.h`, add five entries before `kParamWavePreset`:

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
  kParamFilterStep,
  kParamFEnvAttack,
  kParamFEnvDecay,
  kParamFEnvSustain,
  kParamFEnvRelease,
  kParamFEnvAmount,
  kParamWavePreset,
  kNumParams
};
```

- [ ] **Step 2: Initialize the parameters in the constructor**

In `Fz10m.cpp`, after the `kParamFilterStep` init and before the `kParamWavePreset` init, add:

```cpp
  GetParam(kParamFEnvAttack)->InitDouble("FAtk", 10., 5., 1000., 0.1, "ms",
                                          IParam::kFlagsNone, "FiltEnv",
                                          IParam::ShapePowCurve(3.));
  GetParam(kParamFEnvDecay)->InitDouble("FDec", 200., 20., 1000., 0.1, "ms",
                                         IParam::kFlagsNone, "FiltEnv",
                                         IParam::ShapePowCurve(3.));
  GetParam(kParamFEnvSustain)->InitDouble("FSus", 0., 0., 100., 1., "%",
                                           IParam::kFlagsNone, "FiltEnv");
  GetParam(kParamFEnvRelease)->InitDouble("FRel", 50., 20., 1000., 0.1, "ms",
                                           IParam::kFlagsNone, "FiltEnv",
                                           IParam::ShapePowCurve(3.));
  GetParam(kParamFEnvAmount)->InitDouble("FAmt", 0., -100., 100., 0.1, "%",
                                          IParam::kFlagsNone, "FiltEnv");
```

- [ ] **Step 3: Build**

```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

- [ ] **Step 4: Commit**

```bash
git add Fz10m.h Fz10m.cpp
git commit -m "feat: add filter envelope parameter definitions"
```

---

### Task 2: Add filter envelope to Fz10mVoice and wire into processing

**Files:**
- Modify: `Fz10m_DSP.h` (Fz10mVoice class)

- [ ] **Step 1: Add filter envelope members**

Add to `Fz10mVoice`'s public member section (after `mFilterStepCounter`):

```cpp
  ADSREnvelope<T> mFilterEnv;
  T mFilterEnvAmount = T(0);
  T mFilterEnvSustain = T(0);
  T mFilterEnvVal = T(0);
```

- [ ] **Step 2: Initialize filter envelope in constructor**

In the `Fz10mVoice` constructor, after the filter setup, add:

```cpp
    mFilterEnv.SetSampleRate(48000.0); // will be overridden by SetSampleRateAndBlockSize
```

- [ ] **Step 3: Trigger and release filter envelope in sync with amp envelope**

Update `Trigger`:

```cpp
  void Trigger(double level, bool isRetrigger) override
  {
    mOsc.Reset();
    mLoFi.Reset();
    if (isRetrigger)
    {
      mAmpEnv.Retrigger(level);
      mFilterEnv.Retrigger(level);
    }
    else
    {
      mAmpEnv.Start(level);
      mFilterEnv.Start(level);
    }
  }
```

Update `Release`:

```cpp
  void Release() override
  {
    mAmpEnv.Release();
    mFilterEnv.Release();
  }
```

- [ ] **Step 4: Set filter envelope sample rate**

In `SetSampleRateAndBlockSize`, add `mFilterEnv.SetSampleRate(sampleRate);`:

```cpp
  void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
  {
    mOsc.SetSampleRate(sampleRate);
    mAmpEnv.SetSampleRate(sampleRate);
    mFilterEnv.SetSampleRate(sampleRate);
    mFilter.SetSampleRate(sampleRate);
    mFilter.Reset();
    mLoFi.SetSampleRate(sampleRate);
  }
```

- [ ] **Step 5: Update the processing loop**

In `ProcessSamplesAccumulating`, advance the filter envelope every sample and use its output on step counter ticks. Replace the current step counter block:

```cpp
    for (int i = startIdx; i < startIdx + nFrames; ++i)
    {
      T s = mOsc.Process(freqHz);

      s = mLoFi.Process(s);

      // Advance filter envelope every sample for accurate timing.
      mFilterEnvVal = mFilterEnv.Process(mFilterEnvSustain);

      if (--mFilterStepCounter <= 0)
      {
        // Apply filter envelope modulation to cutoff.
        T effectiveCutoff = mTargetCutoff + mFilterEnvVal * mFilterEnvAmount * T(20000);
        effectiveCutoff = std::max(T(20), std::min(T(20000), effectiveCutoff));
        mFilter.SetFreqCPS(effectiveCutoff);
        mFilter.SetQ(mTargetQ);
        mFilterStepCounter = mFilterStepInterval;
      }

      T* inPtr[1] = { &s };
      T* outPtr[1] = { &s };
      mFilter.ProcessBlock(inPtr, outPtr, 1, 1);

      const T envVal = mAmpEnv.Process(mSustainLevel);
      s *= envVal;
      outputs[0][i] += s;
      outputs[1][i] += s;
    }
```

- [ ] **Step 6: Build**

```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

- [ ] **Step 7: Commit**

```bash
git add Fz10m_DSP.h
git commit -m "feat: add per-voice filter envelope to Fz10mVoice"
```

---

### Task 3: Route filter envelope parameters through SetParam

**Files:**
- Modify: `Fz10m_DSP.h` (Fz10mDSP::SetParam)

- [ ] **Step 1: Add five new cases before `default:`**

```cpp
      case kParamFEnvAttack:
      case kParamFEnvDecay:
      case kParamFEnvRelease:
      {
        using EEnvStage = typename ADSREnvelope<T>::EStage;
        const int stage = (paramIdx == kParamFEnvAttack)  ? EEnvStage::kAttack
                        : (paramIdx == kParamFEnvDecay)   ? EEnvStage::kDecay
                        :                                   EEnvStage::kRelease;
        mSynth.ForEachVoice([stage, value](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mFilterEnv.SetStageTime(stage, value);
        });
        break;
      }
      case kParamFEnvSustain:
      {
        const T sus = static_cast<T>(value / 100.0);
        mSynth.ForEachVoice([sus](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mFilterEnvSustain = sus;
        });
        break;
      }
      case kParamFEnvAmount:
      {
        const T amt = static_cast<T>(value / 100.0);
        mSynth.ForEachVoice([amt](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mFilterEnvAmount = amt;
        });
        break;
      }
```

- [ ] **Step 2: Build**

```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

- [ ] **Step 3: Commit**

```bash
git add Fz10m_DSP.h
git commit -m "feat: route filter envelope params through SetParam"
```

---

### Task 4: Redesign UI to two rows with Filter Env group

**Files:**
- Modify: `Fz10m.cpp` (mLayoutFunc lambda)

- [ ] **Step 1: Replace the knob layout with two rows**

Replace the entire knob section (from `// Top row:` comment through the three `IVGroupControl` lines) with:

```cpp
    // Two rows of knobs, 80px each, with grouped sections.
    const float kGap = 5.f;
    const float kKnobSize = 80.f;
    const IRECT row1 = b.ReduceFromTop(150.f);
    const IRECT row2 = b.ReduceFromTop(150.f);

    // Row 1: Synth (4) + ADSR (4) = 8 knobs in an 8-column grid
    const IRECT synthRect = row1.SubRectHorizontal(8, 0).Union(row1.SubRectHorizontal(8, 3)).GetPadded(-kGap);
    const IRECT adsrRect  = row1.SubRectHorizontal(8, 4).Union(row1.SubRectHorizontal(8, 7)).GetPadded(-kGap);

    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 0, 1, 4).GetCentredInside(kKnobSize), kParamGain, "Gain"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 1, 1, 4).GetCentredInside(kKnobSize), kParamCutoff, "Cutoff"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 2, 1, 4).GetCentredInside(kKnobSize), kParamResonance, "Res"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 3, 1, 4).GetCentredInside(kKnobSize), kParamFilterStep, "Step"), kNoTag, "Synth");

    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 0, 1, 4).GetCentredInside(kKnobSize), kParamAttack, "Attack"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 1, 1, 4).GetCentredInside(kKnobSize), kParamDecay, "Decay"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 2, 1, 4).GetCentredInside(kKnobSize), kParamSustain, "Sustain"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 3, 1, 4).GetCentredInside(kKnobSize), kParamRelease, "Release"), kNoTag, "ADSR");

    // Row 2: FiltEnv (5) + LoFi (3) = 8 knobs in an 8-column grid
    const IRECT fenvRect = row2.SubRectHorizontal(8, 0).Union(row2.SubRectHorizontal(8, 4)).GetPadded(-kGap);
    const IRECT lofiRect = row2.SubRectHorizontal(8, 5).Union(row2.SubRectHorizontal(8, 7)).GetPadded(-kGap);

    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 0, 1, 5).GetCentredInside(kKnobSize), kParamFEnvAttack, "FAtk"), kNoTag, "FiltEnv");
    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 1, 1, 5).GetCentredInside(kKnobSize), kParamFEnvDecay, "FDec"), kNoTag, "FiltEnv");
    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 2, 1, 5).GetCentredInside(kKnobSize), kParamFEnvSustain, "FSus"), kNoTag, "FiltEnv");
    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 3, 1, 5).GetCentredInside(kKnobSize), kParamFEnvRelease, "FRel"), kNoTag, "FiltEnv");
    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 4, 1, 5).GetCentredInside(kKnobSize), kParamFEnvAmount, "FAmt"), kNoTag, "FiltEnv");

    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 0, 1, 3).GetCentredInside(kKnobSize), kParamLoFiCharacter, "Char"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 1, 1, 3).GetCentredInside(kKnobSize), kParamLoFiRate, "Rate"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 2, 1, 3).GetCentredInside(kKnobSize), kParamLoFiBits, "Bits"), kNoTag, "LoFi");

    // Group borders
    pGraphics->AttachControl(new IVGroupControl("Synth", "Synth", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("ADSR", "ADSR", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("Filter Env", "FiltEnv", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("LoFi", "LoFi", 5.f, 20.f, 5.f, 5.f));
```

- [ ] **Step 2: Build**

```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

- [ ] **Step 3: Launch and visual check**

```bash
killall Fz10m 2>/dev/null; sleep 0.3; just app
```

Verify: two rows of knobs, four groups, no overlapping.

- [ ] **Step 4: Commit**

```bash
git add Fz10m.cpp
git commit -m "feat: two-row knob layout with Filter Env group"
```

---

### Task 5: Final build and manual verification

- [ ] **Step 1: Build**

```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

- [ ] **Step 2: Manual listening checks**

```text
- FAmt = 0%: should sound identical to before this change.
- Cutoff = 500 Hz, FAmt = +80%, FAtk short, FDec = 200ms, FSus = 0%:
  play a note -- should hear filter open then close (classic pluck).
- Same but FAmt = -80%, Cutoff = 15000 Hz:
  play a note -- should hear filter close then open back up.
- Play chords: each note has its own independent filter sweep.
- Step = 256 with filter envelope active: stepped/zippered filter sweeps.
- Existing amp ADSR, lo-fi, gain all still work.
- Reset button resets all knobs including new filter env ones.
```
