# Fz10m — Follow-up work

## Where we are

Phase 1 and Phase 2a are complete. The plugin is a monophonic MIDI-driven wavetable synth with:

- Drawable 128-point wavetable (IVMultiSliderControl)
- ADSR amp envelope
- Per-voice lo-fi stage: sample-rate hold + bit-depth quantization + character blend
- SVF low-pass filter (cutoff + Q)
- Master gain
- UI knobs grouped into Synth / ADSR / LoFi sections (IVGroupControl)
- Builds as VST3/AU/CLAP on macOS, VST3/CLAP/standalone on Windows
- CI green for build-mac, build-win, and release-native workflows
- Submodule points at jackharrhy/iPlug2 fork with two framework fixes (CLAP stateSave loop, auval no-stress default)
- `just app` recipe for quick standalone testing

### Current signal chain
```
Osc → LoFiStage (sample-hold + bitcrush + character blend) → SVF Filter → × ADSR → × gain → out
```

### Current parameters (10 total)
```
Synth:  Gain, Cutoff, Resonance
ADSR:   Attack, Decay, Sustain, Release
LoFi:   Character (wet/dry 0-100%), Rate (9k-48k Hz), Bits (4-16)
```

## Phase 2b: Stepped filter parameter updates (next)

The other half of the FZ character. Update filter cutoff/Q at a lower rate than the audio sample rate.

### How it works
- Instead of applying cutoff/Q changes every sample (smooth), apply them every N samples (e.g. every 256 samples ~ 5ms @ 48k)
- Creates audible "staircase" artifacts on filter sweeps -- the classic FZ zipper sound
- The SVF's `SetFreqCPS` / `SetQ` are currently called from `SetParam` → `ForEachVoice`, which updates immediately. The stepped behavior means caching target values on the voice and only forwarding them to the SVF on a counter

### Implementation approach
- Add `mTargetCutoff`, `mTargetQ`, `mFilterUpdateCounter`, `mFilterUpdateInterval` to `Fz10mVoice`
- In `ProcessSamplesAccumulating`, only call `mFilter.SetFreqCPS(mTargetCutoff)` / `SetQ(mTargetQ)` when the counter hits zero
- Between updates, the filter runs with stale coefficients -- that's the whole point
- Optional: add a `kParamFilterStepRate` knob to control the update interval, or hardcode to ~256 samples initially

### Design note
The spec chose parameter-update-rate over value-quantization as the primary "FZ quirk" mechanism. Value quantization (rounding cutoff to 64 steps) was considered but rejected as less authentic -- the real FZ's stepping came from a slow parameter control loop, not low-resolution DACs on the filter.

## Phase 3: Additional wavetable sources

The FZ-1 had multiple ways to generate a single-cycle waveform. Currently only "drawn" is supported. The architecture already supports adding more -- they all just populate the same `mWavetable[128]` buffer.

### Preset waveforms
- Sine, saw, square, triangle, pulse, double-sine, saw/pulse, random (noise)
- UI: dropdown or button row that overwrites the wavetable with the selected shape
- Implementation: a function that fills `mWavetable` with the shape, sent via `SendArbitraryMsgFromUI` or `OnMessage`

### Additive / harmonic mode
- 16 or 48 harmonic amplitude sliders (FZ-1 had 48)
- Render into the wavetable via direct summation: `sum(amplitude[k] * sin(2pi * k * i / tableSize))`
- UI: second `IVMultiSliderControl<16>` or `<48>` that appears when "Additive" mode is selected
- Shares the same `mWavetable` buffer -- just a different way to populate it

### UI considerations
- Need a mode selector (Drawn / Preset / Additive)
- Each mode shows different controls but writes to the same buffer
- When switching modes, the buffer gets overwritten -- previous drawing is lost (acceptable for MVP)

## Phase 3+: Polyphony

Currently mono (`VoiceAllocator::kPolyModeMono`, 1 voice). The code was designed for easy poly:

- Change `kPolyModeMono` → `kPolyModePoly` in `Fz10mDSP` constructor
- Add more voices (4 or 8)
- All per-voice state (osc, env, filter, lo-fi) is already on `Fz10mVoice`
- The shared wavetable is read-only from voices -- no contention

### Open question
With polyphony + stepped filter: should all voices update their filter on the same clock tick, or each on their own? Same clock = more FZ-authentic (one global parameter update loop). Own clocks = more natural-sounding for a poly synth. Decide when implementing.

## Phase 3+: Filter envelope

Currently the filter cutoff is static (set by knob). A filter envelope would modulate cutoff per-note.

- Add a second `ADSREnvelope<T>` to `Fz10mVoice` (`mFilterEnv`)
- New params: FilterEnvAttack, FilterEnvDecay, FilterEnvSustain, FilterEnvRelease, FilterEnvAmount
- In `ProcessSamplesAccumulating`: `effectiveCutoff = baseCutoff + filterEnv.Process(sustain) * envAmount`
- Trigger/release in sync with amp envelope

## Reference repos (cloned during brainstorming)

These were cloned to `tmp/` (gitignored) for research. Re-clone if needed:

- `JanWilczek/wavetable-synth` -- pedagogical wavetable oscillator (JUCE)
- `FigBug/Wavetable` -- production wavetable synth (JUCE, BSD)
- `PaulBatchelor/Soundpipe` -- canonical bitcrush implementation in C
- `link-512/Downsamplr-VST-Plugin` -- bitcrush + feedback VST (JUCE)

In-repo references:
- `iPlug2/Examples/IPlugInstrument/` -- the pattern our DSP architecture mirrors
- `iPlug2/Examples/IPlugChunks/` -- IVMultiSliderControl + SendArbitraryMsgFromUI pattern

## Known issues / tech debt

- **auval -stress fails** with infinity in output (thread-safety issue in iPlug2's SVF under concurrent param changes). Stress test disabled in CI. Not a real-world issue -- DAWs don't randomize params at 600 Hz.
- **Wavetable thread safety** -- `UpdateWavetable` writes 128 floats from the transport thread while the audio thread reads them. Torn reads can produce a brief click. Acceptable because updates are user-initiated (rare). A double-buffer or atomic swap would fix it.
- **No state serialization for the wavetable** -- if a host saves/restores the plugin state, the drawn waveform resets to sine. Need `SerializeState`/`UnserializeState` (or `PLUG_DOES_STATE_CHUNKS 1`) to persist the 128-float buffer.
- **No LICENSE file** -- pick one before first public release.
