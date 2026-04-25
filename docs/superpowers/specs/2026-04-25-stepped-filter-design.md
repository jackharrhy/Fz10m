# Stepped Filter Parameter Updates Design

## Goal

Add FZ-inspired "zipper" character to the SVF filter by updating its coefficients at a slower rate than every sample. This completes Phase 2 of the lo-fi character alongside the existing LoFiStage (sample-hold + bitcrush).

## Current Behavior

`Fz10mDSP::SetParam()` calls `mFilter.SetFreqCPS()` and `mFilter.SetQ()` on the voice immediately when the host delivers a parameter change. The SVF recalculates its coefficients on the next `ProcessBlock` call. This produces smooth, artifact-free filter sweeps.

## Approach

Cache target cutoff and Q values on each voice. In the per-sample processing loop, only push them to the SVF every N samples. Between updates, the filter runs with stale coefficients, producing audible stepping on filter sweeps.

A new parameter `kParamFilterStep` controls N. At 1, the filter updates every sample (smooth, same as current behavior). At higher values, the stepping becomes audible.

## Changes to Fz10mVoice

Add four new members:

```cpp
T mTargetCutoff = T(20000);
T mTargetQ = T(0.707);
int mFilterStepInterval = 1;
int mFilterStepCounter = 0;
```

Modify `ProcessSamplesAccumulating` to update the filter on a counter:

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

## Changes to Fz10mDSP::SetParam

The existing `kParamCutoff` and `kParamResonance` cases currently call `mFilter.SetFreqCPS()` and `mFilter.SetQ()` directly on the voice. Change them to write to `mTargetCutoff` and `mTargetQ` instead. The filter will pick up the new values on its next step counter tick.

Add a new case for `kParamFilterStep` that sets `mFilterStepInterval` on each voice.

## New Parameter

```text
kParamFilterStep: "Step" -- range 1 to 512, default 1, step 1, unit "smp", group "Synth"
```

At 48 kHz:
- 1 = smooth (every sample, backwards compatible)
- 64 = ~1.3ms steps, subtle
- 128 = ~2.7ms steps, noticeable
- 256 = ~5.3ms steps, classic FZ character
- 512 = ~10.7ms steps, aggressive

## UI

Add the Step knob to the Synth group, making it 4 knobs: Gain, Cutoff, Res, Step. Update the Synth group rect from a 1x3 to a 1x4 grid. The layout becomes 4 / 4 / 3 across the three groups.

## Signal Chain

Unchanged:

```text
osc -> lofi -> filter (stepped coefficient updates) -> envelope -> output
```

## Testing

Build macOS VST3 Debug and standalone app:

```bash
just build
just app
```

Manual listening checks:

```text
Step = 1: filter sweeps should sound smooth (same as before this change)
Step = 256: sweeping Cutoff should produce audible staircasing / zipper
Step = 512: aggressive stepping, very digital character
Existing params (Gain, ADSR, LoFi) should be unaffected
```

## Non-Goals

- No filter parameter value quantization (rounding cutoff to N steps). The stepping comes from update rate, not resolution.
- No changes to the LoFiStage or other DSP.
- No polyphony changes.
