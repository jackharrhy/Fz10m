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

## Next up

### Filter envelope

Currently filter cutoff is static (set by knob). Aaron specifically wants filter frequency controlled by an ADSR envelope per note.

- Add a second `ADSREnvelope<T>` to `Fz10mVoice` (`mFilterEnv`)
- New params: FilterEnvAttack, FilterEnvDecay, FilterEnvSustain, FilterEnvRelease, FilterEnvAmount
- In `ProcessSamplesAccumulating`: `effectiveCutoff = baseCutoff + filterEnv.Process(sustain) * envAmount`
- Trigger/release in sync with amp envelope

### Lo-fi pre/post filter toggle

Aaron wants signal degradation toggleable to be before or after the filter in the signal path. Currently hardcoded as pre-filter.

- Add a toggle param (pre/post)
- Move the `mLoFi.Process(s)` call based on the toggle

### Filter exploration

Aaron wants something musically interesting, not just a stock filter. Different filter types with character.

> A big component of this I think will be work on getting the filter to behave in a musical / interesting way. There's a lot of nuance with different filters behavior / "flavor" (i.e moog ladder filters lose bass response as resonance increases).

> It'd be interesting to tweak stuff with the filter to find something idiosyncratic we find interesting rather than strictly going after emulation.

Reference: http://www.buchty.net/casio/dcf.html (Casio DCF filter info). The real FZ had a hybrid digital/analog filter -- the digital character came before the analog filtering.

The SVF already supports multiple modes (low-pass, high-pass, band-pass, notch, peak, bell, shelves). Could expose a mode selector as a starting point.

### Additive / harmonic mode

The FZ-1 had 48-harmonic additive synthesis alongside waveform drawing.

- 16 or 48 harmonic amplitude sliders
- Render into the wavetable via direct summation: `sum(amplitude[k] * sin(2pi * k * i / tableSize))`
- UI: a second multi-slider control that appears when "Additive" mode is selected
- Shares the same wavetable buffer -- just a different way to populate it

## Longer term (Aaron's wishlist)

### 4-voice FM synthesis

> next level would be having 4 voices of these custom waveforms that can then do FM and reconfigured in different ways

- 4 oscillators with FM routing between them
- Coarse & fine tune control per oscillator
- Algorithm selector (different FM routing topologies, like DX7-style)
- Feedback per operator
- Independent filters per oscillator
- Independent envelopes per oscillator
- Independent signal quality (lo-fi) per oscillator

### Modulation

- LFO to modulate parameters within the VST (low priority since Ableton has built-in LFOs users can route)
- Sequencer routable to modulate different parameters
- Modulation matrix

### GUI

Aaron is designing the GUI himself. Motivated by annoyance with other VST interfaces.

> Id definitely like to cook some up!! Big interest for me in this actually also came in part from being annoyed by other vst interfaces out

Current UI is stock iPlug2 controls (placeholder until Aaron's design is ready).

### Display modes

- Toggle between spectrogram and classic wave display mode for the wavetable

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
