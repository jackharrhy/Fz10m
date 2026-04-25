# Stepped Filter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add per-voice stepped filter coefficient updates with a user-controllable Step knob, completing Phase 2 of the FZ-inspired lo-fi character.

**Architecture:** Cache target cutoff/Q values on each `Fz10mVoice`. A counter in the per-sample loop controls how often the cached values are pushed to the SVF. A new `kParamFilterStep` parameter controls the interval. The Synth UI group grows from 3 to 4 knobs.

**Tech Stack:** C++17, iPlug2 (SVF, MidiSynth, SynthVoice, IVKnobControl, IVGroupControl)

**Spec:** `docs/superpowers/specs/2026-04-25-stepped-filter-design.md`

---

### Task 1: Add kParamFilterStep to enum and constructor

**Files:**
- Modify: `Fz10m.h:8-21` (EParams enum)
- Modify: `Fz10m.cpp:7-27` (parameter init in constructor)

- [ ] **Step 1: Add parameter enum entry**

In `Fz10m.h`, add `kParamFilterStep` before `kNumParams`:

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
  kNumParams
};
```

- [ ] **Step 2: Initialize the parameter in the constructor**

In `Fz10m.cpp`, after the `kParamLoFiBits` init (line 26-27), add:

```cpp
  GetParam(kParamFilterStep)->InitDouble("Step", 1., 1., 512., 1., "smp",
                                         IParam::kFlagsNone, "Synth");
```

Default 1 (smooth). Range 1-512 samples. Integer steps.

- [ ] **Step 3: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 4: Commit**

```bash
git add Fz10m.h Fz10m.cpp
git commit -m "feat: add kParamFilterStep parameter definition"
```

---

### Task 2: Add stepped filter members and processing to Fz10mVoice

**Files:**
- Modify: `Fz10m_DSP.h` (`Fz10mVoice` class)

- [ ] **Step 1: Add target/counter members to Fz10mVoice**

Add four new members to the public section of `Fz10mVoice` (after `mSustainLevel` at line 217):

```cpp
  T mTargetCutoff = T(20000);
  T mTargetQ = T(0.707);
  int mFilterStepInterval = 1;
  int mFilterStepCounter = 0;
```

- [ ] **Step 2: Insert stepped filter update into ProcessSamplesAccumulating**

Replace the current filter call block (lines 187-189) with a counter-gated update:

The loop body should become:

```cpp
    for (int i = startIdx; i < startIdx + nFrames; ++i)
    {
      T s = mOsc.Process(freqHz);

      s = mLoFi.Process(s);

      if (--mFilterStepCounter <= 0)
      {
        mFilter.SetFreqCPS(mTargetCutoff);
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

- [ ] **Step 3: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 4: Commit**

```bash
git add Fz10m_DSP.h
git commit -m "feat: add stepped filter coefficient updates to Fz10mVoice"
```

---

### Task 3: Route parameters through SetParam

**Files:**
- Modify: `Fz10m_DSP.h` (`Fz10mDSP::SetParam()`)

- [ ] **Step 1: Change kParamCutoff to write to mTargetCutoff**

Replace the current `kParamCutoff` case (lines 294-298):

```cpp
      case kParamCutoff:
        mSynth.ForEachVoice([value](SynthVoice& voice) {
          auto& v = static_cast<Fz10mVoice<T>&>(voice);
          v.mTargetCutoff = static_cast<T>(value);
        });
        break;
```

- [ ] **Step 2: Change kParamResonance to write to mTargetQ**

Replace the current `kParamResonance` case (lines 299-303):

```cpp
      case kParamResonance:
        mSynth.ForEachVoice([value](SynthVoice& voice) {
          auto& v = static_cast<Fz10mVoice<T>&>(voice);
          v.mTargetQ = static_cast<T>(value);
        });
        break;
```

- [ ] **Step 3: Add kParamFilterStep case**

Add a new case before `default:`:

```cpp
      case kParamFilterStep:
      {
        const int interval = static_cast<int>(value);
        mSynth.ForEachVoice([interval](SynthVoice& voice) {
          static_cast<Fz10mVoice<T>&>(voice).mFilterStepInterval = interval;
        });
        break;
      }
```

- [ ] **Step 4: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 5: Commit**

```bash
git add Fz10m_DSP.h
git commit -m "feat: route Cutoff/Res through target cache, add FilterStep param"
```

---

### Task 4: Add Step knob to UI and update Synth group layout

**Files:**
- Modify: `Fz10m.cpp` (mLayoutFunc lambda)

- [ ] **Step 1: Update grid layout for 11 knobs (4 + 4 + 3)**

Change the grid from 10-column to 11-column. Synth takes columns 0-3, ADSR takes 4-7, LoFi takes 8-10. Update all sub-rect computations and knob lines:

```cpp
    // Top row: 11 knobs split into 3 groups (Synth 4, ADSR 4, LoFi 3).
    // Divide into 11 equal slices, then union slices into group rects with
    // 5px inset on each side so group borders don't overlap each other.
    const float kGap = 5.f;
    const IRECT synthRect = knobRow.SubRectHorizontal(11, 0).Union(knobRow.SubRectHorizontal(11, 3)).GetPadded(-kGap);
    const IRECT adsrRect  = knobRow.SubRectHorizontal(11, 4).Union(knobRow.SubRectHorizontal(11, 7)).GetPadded(-kGap);
    const IRECT lofiRect  = knobRow.SubRectHorizontal(11, 8).Union(knobRow.SubRectHorizontal(11, 10)).GetPadded(-kGap);

    // Synth group: Gain, Cutoff, Resonance, Step
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 0, 1, 4).GetCentredInside(90), kParamGain, "Gain"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 1, 1, 4).GetCentredInside(90), kParamCutoff, "Cutoff"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 2, 1, 4).GetCentredInside(90), kParamResonance, "Res"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 3, 1, 4).GetCentredInside(90), kParamFilterStep, "Step"), kNoTag, "Synth");

    // ADSR group: Attack, Decay, Sustain, Release
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 0, 1, 4).GetCentredInside(90), kParamAttack, "Attack"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 1, 1, 4).GetCentredInside(90), kParamDecay, "Decay"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 2, 1, 4).GetCentredInside(90), kParamSustain, "Sustain"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 3, 1, 4).GetCentredInside(90), kParamRelease, "Release"), kNoTag, "ADSR");

    // LoFi group: Character, Rate, Bits
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 0, 1, 3).GetCentredInside(90), kParamLoFiCharacter, "Char"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 1, 1, 3).GetCentredInside(90), kParamLoFiRate, "Rate"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 2, 1, 3).GetCentredInside(90), kParamLoFiBits, "Bits"), kNoTag, "LoFi");

    // Group borders (attached after knobs so they auto-size around them)
    // Padding: left, top, right, bottom. Extra top padding avoids group label
    // overlapping with knob labels inside.
    pGraphics->AttachControl(new IVGroupControl("Synth", "Synth", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("ADSR", "ADSR", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("LoFi", "LoFi", 5.f, 20.f, 5.f, 5.f));
```

- [ ] **Step 2: Build to verify no compile errors**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 3: Build and launch standalone for visual check**

Run:
```bash
killall Fz10m 2>/dev/null; sleep 0.3; just app
```

Verify: Synth group now shows 4 knobs (Gain, Cutoff, Res, Step). Groups don't overlap.

- [ ] **Step 4: Commit**

```bash
git add Fz10m.cpp
git commit -m "feat: add Step knob to Synth group in UI"
```

---

### Task 5: Final build and manual verification

**Files:** None (verification only)

- [ ] **Step 1: Build**

Run:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build 2>&1 | tail -5
```

Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 2: Verify parameter count**

Confirm `kNumParams` is now 11. The enum should list:
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
kParamFilterStep = 10
kNumParams = 11
```

- [ ] **Step 3: Manual listening checks (standalone or Ableton)**

```text
Step = 1: filter sweeps should sound smooth (identical to before this change)
Step = 128: sweeping Cutoff should produce noticeable staircasing
Step = 256: classic FZ zipper character on filter sweeps
Step = 512: aggressive stepping
Resonance changes should also step (not just cutoff)
All other params (Gain, ADSR, LoFi) should be unaffected
Wavetable drawing should still work
QWERTY keyboard should still trigger notes
```
