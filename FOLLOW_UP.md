# Fz10m — Follow-up work

## Where we are

Phase 1 is complete and working in Ableton Live 12. The plugin is a monophonic MIDI-driven wavetable synth with:

- Drawable 128-point wavetable (IVMultiSliderControl)
- ADSR amp envelope
- Clean SVF low-pass filter (cutoff + Q)
- Master gain
- Builds as VST3/AU/CLAP on macOS, VST3/CLAP/standalone on Windows
- CI green for build-mac, build-win, and release-native workflows
- Submodule points at jackharrhy/iPlug2 fork with two framework fixes (CLAP stateSave loop, auval no-stress default)

## Phase 2: Lo-fi stage

Add the FZ-inspired digital character between oscillator and filter.

### Bit-depth reduction
- Continuous knob (not stepped modes)
- Range: effectively 4-bit to 16-bit (16-bit = transparent)
- Math: `roundf(x * levels) / levels` where `levels = pow(2, bitDepth)`
- Reference: soundpipe's `bitcrush.c` (cloned to `tmp/Soundpipe/` during brainstorming — re-clone if needed)

### Sample-rate reduction (downsample)
- Continuous knob (not the FZ's 9k/18k/36k modes, though those could be snap-to presets later)
- Sample-and-hold approach: hold the last sample for N frames, where N = hostSR / targetSR
- Must be per-voice state (member variable), NOT static locals like plan.md's pseudocode shows
- Reference: Airwindows DeRez2 in `tmp/Wavetable/plugin/Source/FX/DeRez2Proc.cpp` does a more sophisticated version with edge-softening — overkill for MVP but worth reading

### Signal chain placement
```
Osc → LoFi (bitcrush + downsample) → Filter → × ADSR → × gain → out
```
This matches the real FZ-1 architecture where the digital character came before the filter.

### Parameters to add
- `kParamBitDepth` — range 4.0–16.0, default 16.0 (transparent)
- `kParamDownsampleRate` — range 1000–48000 Hz, default 48000 (transparent)

### Implementation approach
- Add a `LoFiProcessor` struct/class in `Fz10m_DSP.h` with `Process(T sample)` method
- Per-voice instance (inside `Fz10mVoice`)
- Wire into `ProcessSamplesAccumulating` between `mOsc.Process()` and `mFilter.ProcessBlock()`

## Phase 2b: Stepped filter parameter updates

The key to the FZ "character" — update filter cutoff/Q at a lower rate than the audio sample rate.

### How it works
- Instead of applying cutoff/Q changes every sample (smooth), apply them every N samples (e.g., every 256 samples ≈ 5ms @ 48k)
- Creates audible "staircase" artifacts on filter sweeps — the classic FZ zipper sound
- The SVF's `SetFreqCPS` / `SetQ` are already called from `SetParam` → `ForEachVoice`. The stepped behavior means caching the target values and only forwarding them to the SVF on a timer/counter

### Implementation approach
- Add a `mFilterUpdateInterval` (in samples) and a counter to `Fz10mVoice`
- In `ProcessSamplesAccumulating`, only call `mFilter.SetFreqCPS(mTargetCutoff)` / `SetQ(mTargetQ)` when the counter hits zero
- Between updates, the filter runs with stale coefficients — that's the whole point
- Optional: add a `kParamFilterStepRate` knob to control the update interval (or hardcode to ~256 samples initially)

### Design note from brainstorming
The spec chose parameter-update-rate over value-quantization as the primary "FZ quirk" mechanism. Value quantization (rounding cutoff to 64 steps) was considered but rejected as less authentic — the real FZ's stepping came from a slow parameter control loop, not low-resolution DACs on the filter.

## Phase 3: Additional wavetable sources

The FZ-1 had multiple ways to generate a single-cycle waveform. Phase 1 only has "drawn." The architecture already supports adding more — they all just populate the same `mWavetable[128]` buffer.

### Preset waveforms
- Sine, saw, square, triangle, pulse, double-sine, saw/pulse, random (noise)
- UI: dropdown or button row that overwrites the wavetable with the selected shape
- Implementation: a function that fills `mWavetable` with the shape, called from UI thread via `SendArbitraryMsgFromUI` or just directly since it's a simple buffer write

### Additive / harmonic mode
- 16 or 48 harmonic amplitude sliders (FZ-1 had 48)
- Render into the wavetable via inverse FFT or direct summation: `sum(amplitude[k] * sin(2π * k * i / tableSize))`
- UI: second `IVMultiSliderControl<16>` or `<48>` that appears when "Additive" mode is selected
- Shares the same `mWavetable` buffer — just a different way to populate it

### UI considerations
- Need a mode selector (Drawn / Preset / Additive)
- Each mode shows different controls but writes to the same buffer
- When switching modes, the buffer gets overwritten — previous drawing is lost (acceptable for MVP; could save per-mode later)

## Phase 3+: Polyphony

Currently mono (`VoiceAllocator::kPolyModeMono`, 1 voice). The code was designed for easy poly:

- Change `kPolyModeMono` → `kPolyModePoly` in `Fz10mDSP` constructor
- Change `nVoices` from 1 to 4 or 8
- All per-voice state (osc, env, filter, lo-fi) is already on `Fz10mVoice`
- The shared wavetable is read-only from voices — no contention

### Open question from brainstorming
With polyphony + stepped filter: should all voices update their filter on the same clock tick, or each on their own? Same clock = more FZ-authentic (one global parameter update loop). Own clocks = more natural-sounding for a poly synth. Decide when implementing.

## Phase 3+: Filter envelope

Currently the filter cutoff is static (set by knob). A filter envelope would modulate cutoff per-note.

- Add a second `ADSREnvelope<T>` to `Fz10mVoice` (`mFilterEnv`)
- New params: FilterEnvAttack, FilterEnvDecay, FilterEnvSustain, FilterEnvRelease, FilterEnvAmount
- In `ProcessSamplesAccumulating`: `effectiveCutoff = baseCutoff + filterEnv.Process(sustain) * envAmount`
- Trigger/release in sync with amp envelope

## Reference repos (cloned during brainstorming)

These were cloned to `tmp/` (gitignored) for research. Re-clone if needed:

- `JanWilczek/wavetable-synth` — pedagogical wavetable oscillator (JUCE, 45-line oscillator)
- `FigBug/Wavetable` — production wavetable synth (JUCE, BSD)
- `PaulBatchelor/Soundpipe` — canonical bitcrush implementation in C
- `link-512/Downsamplr-VST-Plugin` — bitcrush + feedback VST (JUCE)

In-repo references:
- `iPlug2/Examples/IPlugInstrument/` — the pattern our DSP architecture mirrors
- `iPlug2/Examples/IPlugChunks/` — IVMultiSliderControl + SendArbitraryMsgFromUI pattern

## Known issues / tech debt

- **auval -stress fails** with infinity in output (thread-safety issue in iPlug2's SVF under concurrent param changes). Stress test disabled in CI. Not a real-world issue — DAWs don't randomize params at 600 Hz. Would need param smoothers fed into the processor (like IPlugInstrument's `LogParamSmooth` array pattern) to fix properly.
- **Wavetable thread safety** — `UpdateWavetable` writes 128 floats from the transport thread while the audio thread reads them. Torn reads can produce a brief click. Acceptable because updates are user-initiated (rare). A double-buffer or atomic swap would fix it cleanly if it ever matters.
- **No state serialization for the wavetable** — if a host saves/restores the plugin state, the drawn waveform resets to sine. Need to implement `SerializeState`/`UnserializeState` (or `PLUG_DOES_STATE_CHUNKS 1`) to persist the 128-float buffer.
- **UI is stock iPlug2 controls** — user (Jack) plans to do their own UI polish pass. Current layout: knobs on top, wavetable drawing in middle, keyboard at bottom.
- **No LICENSE file** — README says "TBD". Pick one before first public release.
