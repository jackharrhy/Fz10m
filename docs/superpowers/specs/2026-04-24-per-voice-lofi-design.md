# Per-Voice Lo-Fi Stage Design

## Goal

Add FZ-inspired digital character without changing the host sample rate. Ableton/Core Audio continues to run the plugin at the real host rate, while each synth voice simulates lower-rate and lower-bit-depth behavior internally.

## Current Context

Fz10m currently has one mono `Fz10mVoice` owned by `MidiSynth`. The voice generates a wavetable oscillator sample, runs it through an `SVF` low-pass filter, applies an ADSR envelope, and accumulates mono output into both stereo channels.

`MidiSynth::SetSampleRateAndBlockSize()` should continue receiving the host sample rate. That value is needed for correct oscillator phase, envelope timing, filter coefficients, and MIDI scheduling. It should not be used as the creative low-sample-rate control.

## Approach

Add a small per-voice lo-fi stage between the oscillator and filter:

```text
wavetable oscillator
-> per-voice lo-fi stage
   -> sample-rate hold
   -> bit-depth quantization
   -> character amount blend
-> SVF low-pass filter
-> ADSR amp envelope
-> stereo output
```

This is intentionally behavioral rather than hardware-accurate. The plugin still outputs one sample per host frame, but the lo-fi stage can hold values across multiple frames to create lower-rate character.

## Components

Introduce a small `LoFiStage<T>` type, likely in `Fz10m_DSP.h` unless it grows enough to justify its own header.

Public methods:

```cpp
void SetSampleRate(double sampleRate);
void SetRateHz(double rateHz);
void SetBits(int bits);
void SetAmount(T amount);
void Reset();
T Process(T input);
```

Each `Fz10mVoice` owns one `LoFiStage<T>` so future polyphony or unison naturally applies the character per note rather than as a post-synth effect.

## Parameters

Add a small Lo-Fi section:

```text
Character: 0-100%
Rate: Clean / 36k / 18k / 9k
Bits: 16 / 12 / 8
```

`Character` blends between the clean oscillator output and the crushed/held output. `Rate` controls sample-hold frequency. `Bits` controls amplitude quantization resolution.

## Processing Details

The rate reducer should use a phase accumulator rather than integer-only host-rate divisors, because host rates like 44.1 kHz do not divide evenly into 36 kHz, 18 kHz, or 9 kHz.

Conceptually:

```cpp
if (rateHz >= hostSampleRate || amount <= 0) {
  return input;
}

if (!initialized) {
  held = quantize(input, bits);
  initialized = true;
}

phase += rateHz / hostSampleRate;
if (phase >= 1.0) {
  phase -= 1.0;
  held = quantize(input, bits);
}

return input + amount * (held - input);
```

Bit quantization should clamp to the audio range and map the sample onto a fixed number of levels. This does not need dithering for the first version; the quantization noise is part of the intended character.

## iPlug2 Extras

Continue using these existing pieces:

```text
MidiSynth       -> MIDI queueing, mono voice allocation, voice triggering
SynthVoice      -> per-note voice abstraction
ADSREnvelope    -> amplitude envelope
SVF             -> low-pass filter
Smoothers       -> master gain smoothing
```

Do not use `RealtimeResampler` for this first pass. It is designed for running arbitrary DSP at another real sample rate and resampling back, which is more complex and cleaner than the desired sample-hold lo-fi behavior.

## UI

Add three controls to the existing top knob row or split the controls into two rows if needed. Preserve the existing simple iPlug2 UI style.

If adding all three controls makes the current `1x7` knob row cramped, prefer a minimal layout adjustment over a larger redesign.

## Testing

Build the macOS VST3 Debug scheme after implementation:

```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build
```

Manual listening checks:

```text
Character 0% should sound effectively unchanged.
Lower rate modes should sound increasingly stepped/grainy.
Lower bit modes should add quantization grit.
ADSR, filter cutoff, resonance, and gain should continue to work.
```

## Non-Goals

Do not emulate the Casio FZ hardware exactly.
Do not change Ableton's sample rate or the plugin's reported host sample rate.
Do not introduce polyphony as part of this change.
Do not use oversampling or high-quality resampling for the first lo-fi pass.
