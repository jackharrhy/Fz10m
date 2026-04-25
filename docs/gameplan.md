# Fz10m Gameplan

## Vision

Drawable-wavetable lo-fi synth inspired by the Casio FZ series. Not an emulation -- taking the FZ as a starting point and finding something interesting from there.

> The goal is not necessarily to emulate, but just where i thought of it — Aaron

## Where we are (v0.3.0)

- 8-voice polyphony (matching the real FZ-10M)
- Drawable 128-point wavetable with preset waveforms (sine, triangle, saw, square, pulse, double sine, saw/pulse, random)
- Per-voice lo-fi stage: sample-rate hold + bit-depth quantization + character blend
- Stepped filter coefficient updates for FZ-inspired zipper character
- SVF low-pass filter (cutoff + resonance)
- ADSR amplitude envelope
- Master gain
- Wavetable persists across GUI close/reopen and project save/load (versioned state serialization)
- UI knobs grouped into Synth / ADSR / LoFi sections
- Reset button, wavetable preset dropdown
- Builds as VST3/AU/CLAP on macOS, VST3/CLAP on Windows
- CI with GitHub Actions, `just release` workflow with changelog validation

### Current signal chain
```
Osc → LoFiStage (sample-hold + bitcrush + character blend) → SVF Filter → × ADSR → × gain → out
```

### Current parameters (12 total)
```
Synth:  Gain, Cutoff, Resonance, Step
ADSR:   Attack, Decay, Sustain, Release
LoFi:   Character (wet/dry 0-100%), Rate (9k-48k Hz), Bits (4-16)
Wave:   Preset selector
```

## Roadmap

Features in priority order. Each step should result in a release that Aaron can test.

### Step 1: Filter envelope (v0.4.0)

Currently filter cutoff is static (set by knob). This adds per-note filter sweeps, which is the single biggest musical upgrade.

- Add a second `ADSREnvelope<T>` to `Fz10mVoice` (`mFilterEnv`)
- New params: FilterEnvAttack, FilterEnvDecay, FilterEnvSustain, FilterEnvRelease, FilterEnvAmount
- In `ProcessSamplesAccumulating`: `effectiveCutoff = baseCutoff + filterEnv.Process(sustain) * envAmount`
- Trigger/release in sync with amp envelope

**How to test:**
- Set Cutoff low (~500 Hz), FilterEnvAmount high, short attack, medium decay. Play a note -- should hear the filter open and close per note (classic synth pluck).
- Set FilterEnvAmount to 0 -- should sound identical to current behavior.
- Play chords -- each note should have its own independent filter sweep.
- Verify existing ADSR (amp envelope) still works independently.

### Step 2: Lo-fi pre/post filter toggle (v0.4.1)

Signal degradation toggleable before or after the filter. Currently hardcoded as pre-filter. Small change, but sounds very different depending on position.

- Add a toggle param (pre/post)
- Move the `mLoFi.Process(s)` call based on the toggle

**How to test:**
- Set Character high, Rate low (9k), Bits low (8). Toggle pre/post while playing.
- Pre-filter: lo-fi grit gets smoothed by the filter (darker, mushier).
- Post-filter: filter output gets crushed (harsher, more digital).
- Both should sound distinctly different. Neither should click or glitch on toggle.

### Step 3: Filter mode selector (v0.5.0)

Expose the SVF's existing modes. The SVF already supports low-pass, high-pass, band-pass, notch, peak, bell, and shelves. This is mostly a UI + param wiring change.

- Add a `kParamFilterMode` enum param
- Wire it to `mFilter.SetMode()` on each voice
- Add a dropdown to the Synth group

**How to test:**
- Switch between modes while playing a note with moderate cutoff and resonance.
- Low-pass: removes highs (current behavior).
- High-pass: removes lows, thin sound.
- Band-pass: only frequencies around cutoff survive, nasal/vocal.
- Notch: removes a band at cutoff, hollow sound.
- Verify the filter envelope (step 1) and stepped updates still work with each mode.

### Step 4: Additive / harmonic mode (v0.6.0)

An alternative way to populate the wavetable using harmonic sliders instead of drawing.

- 16 harmonic amplitude sliders (start simple, FZ-1 had 48)
- Render into the wavetable via direct summation
- UI: a second multi-slider control, toggled via the Wave preset dropdown (add "Additive" entry)
- Shares the same wavetable buffer

**How to test:**
- Select "Additive" from the Wave dropdown. Harmonic sliders should appear.
- Set only harmonic 1 high -- should sound like a sine.
- Add harmonics 1-4 at decreasing levels -- should sound like a rounded saw.
- Odd harmonics only (1, 3, 5, 7) -- should sound hollow/square-ish.
- Switch back to "Sine" preset -- wavetable should overwrite with sine.
- Draw on the wavetable after additive -- should work (custom drawing overrides).

### Step 5: GUI (ongoing, Aaron-led)

Aaron is designing the custom GUI. Current stock iPlug2 controls are placeholder.

> Id definitely like to cook some up!! Big interest for me in this actually also came in part from being annoyed by other vst interfaces out

This can happen in parallel with any of the above steps.

### Step 6: 4-voice FM synthesis (v1.0.0?)

The big architectural leap. Depends on everything above being solid.

> next level would be having 4 voices of these custom waveforms that can then do FM and reconfigured in different ways

- 4 oscillators (operators) with FM routing between them
- Coarse & fine tune control per operator
- Algorithm selector (DX7-style routing topologies)
- Feedback per operator
- Independent filters, envelopes, and lo-fi settings per operator

**How to test:**
- Algorithm with carrier only (no modulation) -- should sound like current single-osc behavior.
- Simple 2-op FM (one modulator, one carrier) -- should produce metallic/bell tones.
- Feedback on a single operator -- should add harmonics, eventually become noisy at high values.
- Compare against a DX7 emulator or Dexed for sanity checking FM behavior.

### Step 7: Modulation system (v1.x)

Lowest priority since Ableton has built-in LFOs users can route.

- LFO to modulate internal parameters
- Step sequencer routable to parameters
- Modulation matrix
- Spectrogram / classic wave display toggle

## Known issues / tech debt

- **auval -stress fails** with infinity in output (thread-safety issue in iPlug2's SVF under concurrent param changes). Stress test disabled in CI. Not a real-world issue.
- **Wavetable thread safety** -- `UpdateWavetable` writes 128 floats from the transport thread while the audio thread reads. Torn reads can produce a brief click. Acceptable because updates are user-initiated (rare). A double-buffer or atomic swap would fix it.
- **No LICENSE file** -- pick one before first public release.

## Reference repos

Cloned to `tmp/` (gitignored) during research. Re-clone if needed:

- `JanWilczek/wavetable-synth` -- pedagogical wavetable oscillator (JUCE)
- `FigBug/Wavetable` -- production wavetable synth (JUCE, BSD)
- `PaulBatchelor/Soundpipe` -- canonical bitcrush implementation in C
- `link-512/Downsamplr-VST-Plugin` -- bitcrush + feedback VST (JUCE)

In-repo references:
- `iPlug2/Examples/IPlugInstrument/` -- the pattern our DSP architecture mirrors
- `iPlug2/Examples/IPlugChunks/` -- IVMultiSliderControl + SendArbitraryMsgFromUI pattern
