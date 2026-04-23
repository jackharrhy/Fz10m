# Fz10m Phase 1 — playable wavetable synth

**Status:** Approved
**Date:** 2026-04-22
**Repo:** `/Users/jack/repos/personal/Fz10m`
**Related:** `plan.md` (long-range vision), `AGENTS.md`

## Goal

Turn the current Fz10m gain plugin into a monophonic, MIDI-driven, drawable-wavetable synth with ADSR amp envelope and a clean low-pass filter. No lo-fi stage, no FZ-style stepped-parameter filter quirks yet — those land in a subsequent phase. This is the "prove the end-to-end path" release.

## Non-goals (explicit Phase 1 exclusions)

- Bit-depth reduction (bitcrush)
- Sample-rate reduction (downsample)
- FZ-style stepped filter parameter updates
- Parameter quantization on the filter
- Preset / additive / noise wavetable source modes (drawn only)
- Polyphony (single voice via `MidiSynth`)
- Filter envelope (separate from amp env)
- Any UI polish beyond stock iPlug2 vector controls laid out in a grid
- Unit tests (iPlug2 projects have no test harness conventions; verification is manual/audible)

None of these are out of long-range scope — they're deferred to keep Phase 1 achievable.

## Architecture

```
MIDI ─────────────▶ MidiSynth (1 voice)
                        │
wavetable[128] ◀─ UI ───┤
                        ▼
                      Voice:
                        WavetableOscillator
                            ↓
                        SVF (low-pass)
                            ↓
                        × ADSR envelope
                            ↓
                        × per-voice gain
                        │
                        ▼
                     Accumulate to stereo out
                        │
                        ▼
                    × Master gain
                        │
                        ▼
                  Plugin output
```

**Shape rationale:** Uses iPlug2's `MidiSynth` + `SynthVoice` + `VoiceAllocator` pattern (see `iPlug2/Examples/IPlugInstrument/IPlugInstrument_DSP.h`). Every piece of DSP state lives on the Voice, so adding polyphony later requires only changing `nVoices` in the constructor. The wavetable buffer is plugin-owned and read-only from voices. The UI thread writes into it sporadically; torn reads of a single sample are inaudible, so no locking is required.

## Components

### WavetableOscillator (new, in `Fz10m_DSP.h`)

Templated `class WavetableOscillator : public IOscillator<T>`.

**State:**
- `const T* mTable` — non-owning pointer to the plugin's wavetable buffer
- `int mTableSize` — 128 (constexpr)
- Inherits `mPhase`, `mPhaseIncr`, `mSampleRate` from `IOscillator<T>`

**Interface:**
- `void SetTable(const T* table, int size)` — called once at Voice construction
- `T Process(double freqHz)` override — advances phase, does linear interpolation between `mTable[i0]` and `mTable[i1]`, returns one sample

**Phase semantics:** `mPhase` runs 0.0–1.0 like the rest of iPlug2's oscillators. Index computation: `i0 = int(mPhase * mTableSize) % mTableSize`.

### Fz10mVoice (new, in `Fz10m_DSP.h`, subclass of `iplug::SynthVoice`)

**Owns:**
- `WavetableOscillator<T> mOsc`
- `ADSREnvelope<T> mAmpEnv` (constructed with `mResetFunc = [&]() { mOsc.Reset(); }`)
- `SVF<T> mFilter` — state-variable filter (default `NC=1` channel), low-pass mode
- `T mGain = 1.0`

**Overrides:**
- `bool GetBusy() const` → `mAmpEnv.GetBusy()`
- `void Trigger(double level, bool isRetrigger)` — reset osc, start/retrigger env
- `void Release()` → `mAmpEnv.Release()`
- `void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs, int startIdx, int nFrames)` — read pitch from `mInputs[kVoiceControlPitch]`, compute freq in Hz, for each frame: `sample = osc.Process(freq)`, filter it, multiply by env and gain, accumulate into both output channels
- `void SetSampleRateAndBlockSize(double sampleRate, int blockSize)` — forward to osc, env, filter

### Fz10mDSP (new, in `Fz10m_DSP.h`)

Owns the synth, the wavetable buffer, and param smoothers.

**State:**
- `MidiSynth mSynth { VoiceAllocator::kPolyModeMono, MidiSynth::kDefaultBlockSize }` (mono mode; voice-stealing becomes "steal the one voice" ≡ last-note priority)
- `std::array<T, 128> mWavetable` — single-cycle waveform, initialized to sine
- Per-voice filter cutoff / Q / env params cached here, pushed to voices in `SetParam`
- Master gain smoother (`LogParamSmooth<T, 1>`)

**Interface:**
- Constructor: seeds `mWavetable` with `sin(2π · i/128)`, creates 1 Voice, passes `mWavetable.data()` to it
- `void ProcessBlock(T** outputs, int nOutputs, int nFrames)` — delegate to `mSynth.ProcessBlock`, apply smoothed master gain to outputs
- `void Reset(double sampleRate, int blockSize)` — propagate to synth
- `void ProcessMidiMsg(const IMidiMsg& msg)` → `mSynth.AddMidiMsgToQueue(msg)`
- `void SetParam(int paramIdx, double value)` — switch on param, forward to env/filter of every voice using `mSynth.ForEachVoice(...)`
- `void SetWavetablePoint(int i, float value01)` — called from UI thread. Writes `mWavetable[i] = value01 * 2 - 1` (remap 0..1 from slider to -1..+1). No lock needed; single-sample torn reads are inaudible.

### Plugin (Fz10m.cpp / Fz10m.h)

**Params** (enum in `Fz10m.h`):
```
kParamGain, kParamAttack, kParamDecay, kParamSustain, kParamRelease,
kParamCutoff, kParamResonance, kNumParams
```

`config.h` update: `kNumParams` auto-follows (via `MakeConfig(kNumParams, kNumPresets)`), but the existing `#define PLUG_LATENCY 0` / `PLUG_TYPE 0` need to stay; set `PLUG_TYPE 1` (instrument) and `PLUG_DOES_MIDI_IN 1`.

**UI layout** (stock controls only; user will polish later):
- Top strip: `IVMultiSliderControl<128>` labeled "Wavetable", full width. Action function: on any value change at index i, call `mDSP.SetWavetablePoint(i, newValue)`.
- Middle row of knobs: Gain, Attack, Decay, Sustain, Release (as in `IPlugInstrument`)
- Right of ADSR: Cutoff knob, Resonance knob
- Bottom: `IVKeyboardControl` for mouse/QWERTY testing without a DAW
- No LFO, no meters, no LFO visualization, no pitch-bend wheel — minimal for Phase 1

**MIDI routing:** copy the `ProcessMidiMsg` switch from `IPlugInstrument.cpp:111-136` verbatim — same message types matter.

**Initial wavetable state:** sine wave. User sees a sine drawn in the multi-slider on first load and hears a sine when they press a key.

## Parameter details

| Param | Range | Default | Shape | Notes |
|---|---|---|---|---|
| Gain | 0 – 100% | 100% | linear | master output |
| Attack | 1 – 1000 ms | 10 ms | power-3 curve | matches IPlugInstrument convention |
| Decay | 1 – 1000 ms | 10 ms | power-3 | |
| Sustain | 0 – 100% | 50% | linear | |
| Release | 2 – 1000 ms | 10 ms | power-3 | min 2ms to avoid clicks |
| Cutoff | 20 – 20000 Hz | 20000 Hz | log/frequency | via `InitFrequency` |
| Resonance | 0.5 – 10 | 0.707 | linear | Q value for SVF |

## Error handling

- **Wavetable never populated:** cannot happen — constructor seeds with sine.
- **All-zero wavetable:** user drags every bar to center; plays silence. Acceptable.
- **Invalid MIDI:** already handled by `MidiSynth`.
- **Sample-rate change mid-session:** `OnReset` calls `mSynth.SetSampleRateAndBlockSize`, which propagates into every voice. Filter coefficients recomputed automatically via `SVF::SetSampleRate`.
- **Host passes `nOutputs != 2`:** iPlug2 guarantees stereo for this plugin class given `PLUG_CHANNEL_IO "0-2"`. If it ever happens, the accumulator loop still works (writes to what's there).

## Testing / verification

No automated tests. After each piece is wired up:

| Check | How | Expected |
|---|---|---|
| Builds | `just build` | `** BUILD SUCCEEDED **` |
| Codesign still sealed | `codesign -dvvv ~/Library/Audio/Plug-Ins/VST3/Fz10m.vst3` | `Sealed Resources version=2` |
| Loads in Ableton | Rescan, instantiate on a MIDI track | Appears, no crashes |
| Oscillator works | Draw a sine (leave default), play C4 | Pure tone ≈ 261 Hz |
| Drawing works | Drag the multi-slider to a sawtooth shape, play C4 | Brighter tone with harmonics |
| ADSR works | Long attack (1 s), hit a key | Audibly ramps in |
| Filter works | Sweep cutoff from 20 kHz to 100 Hz while holding a note | Tone audibly darkens |
| Resonance works | Raise resonance to ~5, sweep cutoff | Whistling/resonant peak |
| No clicks on note-on | Hit notes rapidly | No zipper/click artifacts |
| `just validate` passes | Run pluginval level-5 | `SUCCESS` |

## File changes

```
modified:   Fz10m.h          — add 7 params, keep existing ctrl tag pattern
modified:   Fz10m.cpp        — new UI layout, MIDI routing, wavetable wiring
modified:   config.h         — PLUG_TYPE 1, PLUG_DOES_MIDI_IN 1
created:    Fz10m_DSP.h      — WavetableOscillator, Fz10mVoice, Fz10mDSP
```

No changes to the project.pbxproj needed: `Fz10m_DSP.h` is header-only (template classes) and gets picked up by `#include "Fz10m_DSP.h"` in `Fz10m.h`. Matches how `IPlugInstrument_DSP.h` is structured — Xcode finds it via the project's source groups automatically since it sits next to `Fz10m.h`.

**Wait** — verify that claim at plan-time. If Xcode doesn't auto-discover the file, we need to add it to the project. Note this risk; either way the fix is mechanical.

## Risks

- **IVMultiSliderControl<128> usability:** 128 bars at typical plugin width (~1024 px) is ~8 px per bar — drawable but potentially jittery. **Mitigation:** trivially tunable by changing the template arg; fall back to 64 if it feels bad.
- **Wavetable aliasing on high notes:** No band-limiting. At high pitch with a sawtooth-shaped wavetable, aliasing will be audible. **Mitigation:** this matches the lo-fi aesthetic; if it's painfully harsh, add a simple pitch-tracking one-pole lowpass post-osc in Phase 2.
- **Phase reset clicks:** Calling `mOsc.Reset()` on every `Trigger` can click with certain waveforms. **Mitigation:** use the `ADSREnvelope` `mResetFunc` lambda to reset the oscillator only when the env has faded to zero (same pattern as `IPlugInstrument_DSP.h:28`).
- **Xcode project file discovery:** `Fz10m_DSP.h` might need to be added to `Fz10m-macOS.xcodeproj`. Verify during implementation; add to project if needed.
- **Reading `iPlug2/IPlug/Extras/Synth/SynthVoice.h` correctly:** `kVoiceControlPitch` is a "1v/oct" value, so frequency = `440 * pow(2, pitch + pitchBend)`. Confirmed by `IPlugInstrument_DSP.h:63-64`.

## Out of scope / follow-ups

Tracked here so we don't forget:

- Phase 2: lo-fi stage (continuous-knob bit depth + sample rate), FZ-style stepped filter parameter updates
- Phase 3: preset / additive / noise wavetable sources; polyphony; filter envelope
- UI polish (done by user in a separate pass)
- Unit tests / DSP test harness (not conventional in iPlug2)
- LICENSE file for the repo (already a known follow-up from the extraction project)
