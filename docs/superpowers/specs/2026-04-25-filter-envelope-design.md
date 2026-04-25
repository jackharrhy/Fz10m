# Filter Envelope Design

## Goal

Add a per-voice ADSR envelope that modulates the filter cutoff frequency. This is the single biggest musical upgrade -- enables classic synth plucks, pads that open and close, and per-note filter sweeps.

## Current Behavior

Filter cutoff is static, set by the Cutoff knob and delivered to voices via `mTargetCutoff`. The stepped filter update mechanism applies it every N samples. This does not change -- the filter envelope modulates the target cutoff, and the stepped update still controls how often the SVF receives new coefficients.

## Approach

Each `Fz10mVoice` gets a second `ADSREnvelope<T>` called `mFilterEnv`. It is triggered and released in sync with the amp envelope. Each sample, the filter envelope output is scaled by the Amount parameter and added to the base cutoff, then clamped to the valid SVF range (20-20000 Hz).

The stepped filter update counter still gates when coefficients are pushed to the SVF, so the filter envelope interacts naturally with the Step knob.

## New Parameters (5)

```text
kParamFEnvAttack:   "FAtk",  default 10ms,  range 5-1000ms,    group "FiltEnv"
kParamFEnvDecay:    "FDec",  default 200ms, range 20-1000ms,   group "FiltEnv"
kParamFEnvSustain:  "FSus",  default 0%,    range 0-100%,      group "FiltEnv"
kParamFEnvRelease:  "FRel",  default 50ms,  range 20-1000ms,   group "FiltEnv"
kParamFEnvAmount:   "FAmt",  default 0%,    range -100% to +100%, group "FiltEnv"
```

Default amount is 0% so existing behavior is unchanged until the user touches the envelope.

FEnvSustain default is 0% -- this means the envelope sweeps up on attack and falls back to the base cutoff during decay, which is the most common filter envelope shape (pluck/stab).

## Changes to Fz10mVoice

New members:

```cpp
ADSREnvelope<T> mFilterEnv;
T mFilterEnvAmount = T(0);
T mFilterEnvSustain = T(0);
```

Trigger/Release updated to also start/release the filter envelope.

Processing loop updated: after the step counter fires, compute effective cutoff:

```cpp
if (--mFilterStepCounter <= 0)
{
  T envVal = mFilterEnv.Process(mFilterEnvSustain);
  T effectiveCutoff = mTargetCutoff + envVal * mFilterEnvAmount * T(20000);
  effectiveCutoff = std::max(T(20), std::min(T(20000), effectiveCutoff));
  mFilter.SetFreqCPS(effectiveCutoff);
  mFilter.SetQ(mTargetQ);
  mFilterStepCounter = mFilterStepInterval;
}
```

Note: the filter envelope needs to be processed every sample (not just on step counter ticks) to keep its internal timing accurate. But the SVF only receives new coefficients on step counter ticks. So the envelope advances every sample, but its output is only sampled when the counter fires.

Actually, simpler: advance the envelope every sample but only apply its output to the SVF on counter ticks. Store the latest envelope value in a member.

## Changes to Fz10mDSP::SetParam

Add 5 new cases routing to voice members:
- FEnvAttack/Decay/Release -> `mFilterEnv.SetStageTime(...)`
- FEnvSustain -> `mFilterEnvSustain`
- FEnvAmount -> `mFilterEnvAmount` (divided by 100, so -1..+1 range)

## UI Layout Change

Split the knob area into two rows. Knobs at 80px instead of 90px.

```text
Row 1 (170px): [Synth: Gain, Cutoff, Res, Step]  [ADSR: Atk, Dec, Sus, Rel]
Row 2 (170px): [FiltEnv: FAtk, FDec, FSus, FRel, FAmt]  [LoFi: Char, Rate, Bits]
```

Row 1: 8 knobs split into 2 groups of 4.
Row 2: 8 knobs split into groups of 5 and 3.

Total knob area height increases from 170px to 340px, which eats into wavetable space. Accept this for the PoC layout -- Aaron's custom GUI will redesign this.

## State Serialization

No changes needed. The filter envelope state is driven by parameters (which are already serialized) and runtime envelope state (which resets on load). The state version stays at 1.

## Testing

```text
- FEnvAmt = 0%: should sound identical to before this change.
- Cutoff = 500 Hz, FEnvAmt = +80%, short FAtk, medium FDec, FSus = 0%:
  play a note, should hear filter open then close (classic pluck).
- Same but FEnvAmt = -80%, Cutoff = 15000 Hz:
  play a note, should hear filter close then open back up (reverse sweep).
- Play chords: each note should have its own independent filter sweep.
- Step = 256 with filter envelope active: should hear stepped/zippered filter sweeps.
- Existing amp ADSR should work independently.
```

## Non-Goals

- No filter envelope looping or complex envelope shapes.
- No velocity-to-filter-envelope modulation (yet).
- No GUI redesign beyond the two-row layout.
