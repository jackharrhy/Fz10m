# Fz10m Gameplan

## Vision

Fz10m is a drawable-wavetable lo-fi synth inspired by the Casio FZ series. It is not an emulation. The FZ is a starting point for a small instrument with its own digital character.

The roadmap focuses on drawing waves, building them from harmonics, stepped filter movement, per-voice envelopes, and controllable sample-rate/bit-depth damage. It avoids the broad feature set of a general-purpose synth.

## Where we are (v0.7.0)

- 8-voice polyphony.
- Drawable 128-point wavetable with eight generated waveform presets.
- Additive editor with 16 harmonic amplitudes.
- Per-voice amp and filter ADSR envelopes.
- Per-voice lo-fi stage with character blend, sample-rate hold, and bit-depth reduction.
- Lo-fi stage can run before or after the filter.
- Stepped filter coefficient updates for FZ-inspired zipper movement.
- SVF filter modes: low-pass, high-pass, band-pass, notch, and peak.
- Versioned state stores the rendered wavetable, additive harmonics, and all parameters.
- State migration handles the parameter layouts used by earlier Fz10m releases.
- VST3, AU, and CLAP on macOS; VST3 and CLAP on Windows.
- Release automation builds macOS and Windows packages and creates a draft GitHub release.

### Current signal chain

The filter envelope advances every sample. Its current value changes the target cutoff, but the filter receives new cutoff and resonance coefficients only when the Step counter fires.

```text
Lo-fi Pre:
wavetable -> lo-fi -> stepped SVF -> amp envelope -> gain -> stereo out

Lo-fi Post:
wavetable -> stepped SVF -> lo-fi -> amp envelope -> gain -> stereo out
```

Each voice currently produces one mono sample and adds the same sample to the left and right outputs.

### Current parameters (20 total)

| Group | Parameters |
| --- | --- |
| Synth | Gain, Cutoff, Resonance, Step, Filter mode |
| Amp envelope | Attack, Decay, Sustain, Release |
| LoFi | Character, Rate, Bits, Pre/Post |
| Filter envelope | Attack, Decay, Sustain, Release, Amount |
| Wave | Preset, Draw/Additive mode |

Parameter IDs are host-visible and persisted. New parameters must be appended. Existing parameters must never be inserted, removed, or reordered.

## Design constraints

- Keep FM operators and algorithm routing out of Fz10m.
- Preserve the stepped, digital filter character.
- Replace the stock bar-graph wavetable editor with a custom plotting/blotting control.
- Use a pale hardware panel, pixel typography, hard inset/outset edges, a white drawing field, black plotted marks, and red control accents.
- Use the Ableton reference projects and recordings to reproduce sounds and tune the envelopes.
- Treat the two-filter idea as a design spike until the routing and control model sound worthwhile.

## Shipped milestones

| Version | Work |
| --- | --- |
| v0.4.0 | Filter envelope, waveform presets, and expanded layout |
| v0.5.0 | Filter modes and lo-fi Pre/Post routing |
| v0.6.0 | Additive harmonic editor and Draw/Additive mode |
| v0.7.0 | Robust drawn/additive state recall, legacy state migration, and host conformance fixes |

## Active roadmap

### P0: Finish project-recall validation

The v0.7 state fix prevents a recalled Wave preset from overwriting the serialized custom table. The remaining work is host-level regression testing and one editing decision.

- [ ] Test Draw state in Ableton VST3: save, close Ableton, reopen, and compare the waveform and sound.
- [ ] Test Additive state in Ableton VST3: compare the sound and all 16 harmonic controls after reopening.
- [ ] Verify parameter automation restores with custom state.
- [ ] Test preset-generated waves and Random separately.
- [ ] Test GUI close/reopen without closing the project.
- [ ] Decide whether drawing should change the Wave selector to `Custom`.
- [ ] Treat Wave preset selection as a one-shot edit, not the authoritative source of the current table.

### P1: Safe wavetable handoff

The UI/transport side currently writes the shared 128-sample table while voices may read it. Updates are user-driven and short, but a custom drawing control will send them more often.

- [ ] Replace in-place table writes with double-buffering or an atomic buffer swap.
- [ ] Keep buffer preparation off the audio thread.
- [ ] Swap complete tables at a bounded point without locks or allocation.
- [ ] Verify rapid drawing does not click, tear, or produce invalid samples.
- [ ] Preserve state serialization from the currently active table.

### P1: Custom wavetable drawing control

The drawing area should feel like plotting or blotting on an instrument display rather than editing a bar graph.

- [ ] Replace `IVMultiSliderControl<128>` in Draw mode with a custom `IControl`.
- [ ] Keep the DSP table at 128 samples in the `-1..+1` range.
- [ ] Draw a blank white inset field with black pixelated dots or dabs.
- [ ] Interpolate quick pointer movement across skipped sample positions.
- [ ] Decide whether separate strokes bridge gaps or leave existing points untouched.
- [ ] Draw a black pencil cursor over the field with an explicit bitmap and hotspot.
- [ ] Support touch, drag, high-DPI scaling, and plugin resizing.
- [ ] Keep generated, restored, and hand-drawn tables visually consistent.

### P1: Retro UI foundation

- [ ] Gather the source assets or settle the font, palette, line widths, bevel depth, and red slider treatment.
- [ ] Confirm the target aspect ratio and minimum usable size. The current UI is `1024x768`; the reference mockup is wider and shorter.
- [ ] Build reusable inset field, outset button, group frame, vertical slider, and pixel-label styles.
- [ ] Lay out Wave, Filter, Filter Envelope, Amp Envelope, and Character groups.
- [ ] Place Gain, Wave preset/mode, Reset, and the test keyboard intentionally; they are not all represented in the mockup.
- [ ] Define selected, pressed, hover, disabled, and automation-highlight states.
- [ ] Check readability and hit targets at the minimum window size.

### P1: Envelope reference pass

The one-second envelope limit needs to be checked against the long sampler recording. Amp and filter Attack, Decay, and Release currently reach 1000 ms.

- [ ] Get timestamps for the shortest and longest useful attacks, decays, and releases in the reference recording.
- [ ] Determine whether the issue is the one-second maximum, control resolution, curve, or envelope shape.
- [ ] Compare the references against the current amp and filter envelope ranges.
- [ ] If needed, extend the range while retaining precise short settings.
- [ ] Test held notes, short taps, retriggers, overlapping voices, and release tails in Ableton.

### P2: Dual-filter design spike

Reference: [Moog Matriarch filter modes](https://moogmusic-help.freshdesk.com/en/support/solutions/articles/69000877830-moog-matriarch-filter-modes)

Borrow the routing idea, not the Matriarch's ladder-filter model. The prototype should use two copies of the existing stepped FZ-style filter.

| Routing | Filter 1 | Filter 2 | Path | Result |
| --- | --- | --- | --- | --- |
| Off | Existing mode | Off | Current single-filter path | Existing sessions retain their sound |
| Series | HP | LP | voice -> HP -> LP | Movable band window |
| Parallel | HP | LP | voice -> both, then mix | Potential notch or hollow middle |
| Stereo | LP left | LP right | independent left/right paths | Width when the filters differ |

Recommended first control model:

- Shared Cutoff, Resonance, Step, and filter envelope.
- One bipolar Spacing control, preferably expressed in octaves.
- Routing selects HP/LP or LP/LP automatically.
- Existing Filter mode remains meaningful when dual routing is Off.
- Series and Parallel remain mono; Stereo calculates separate left and right voice samples.

Prototype risks:

- Complementary HP and LP branches may sum close to the dry signal in Parallel mode.
- Identical LP settings in Stereo mode will still sound centered.
- Cutoff spacing and envelope modulation must remain clamped to `20..20000 Hz`.
- Switching topology during a held note may click unless it is smoothed or crossfaded.
- Two filters per voice must remain comfortably within the CPU budget at eight active voices.

- [ ] Prototype the three routings before committing the final parameter/UI model.
- [ ] Audition whether spacing is symmetrical around Cutoff or offsets only filter 2.
- [ ] Start Parallel at `0.5 * (HP + LP)` and compare it with each branch and the dry signal at matched loudness.
- [ ] Test Stereo spacing in headphones and in mono fold-down.
- [ ] Compare Step values `1`, `64`, `128`, `256`, and `512` in every routing.
- [ ] Test positive and negative filter-envelope amounts at high resonance.
- [ ] Measure CPU with eight active voices.
- [ ] Append routing/spacing parameters and extend state only after the design survives the listening pass.

## Filter Step and Max/MSP reference

This question is resolved technically. Step controls time resolution, not frequency resolution.

The filter envelope and host automation produce a continuous target cutoff. Every `N` samples, the implementation copies the latest target cutoff and resonance into the filter. The filter processes audio continuously using those held coefficients until the next update.

```text
continuous cutoff/envelope signal
    -> sample-and-hold every N audio samples
    -> filter cutoff input

audio signal
    -> filter continuously
```

At sample rate `Fs` and Step value `N`:

```text
update interval (seconds) = N / Fs
update rate (Hz)          = Fs / N
```

At 48 kHz:

| Step | Update interval | Update rate | Character |
| ---: | ---: | ---: | --- |
| 1 | 0.021 ms | 48,000 Hz | Smooth/every sample |
| 64 | 1.333 ms | 750 Hz | Subtle stepping |
| 128 | 2.667 ms | 375 Hz | Noticeable |
| 256 | 5.333 ms | 187.5 Hz | Strong zipper movement |
| 512 | 10.667 ms | 93.75 Hz | Aggressive |

Do not round cutoff into `N` frequency values and do not sequence notes. A Max patch only needs a continuously running filter plus a sample/hold stage on its cutoff and resonance control signals.

The remaining question is musical: which Step settings and automation gestures best match the reference sound?

## Deferred and out of scope

- FM operators, algorithms, feedback, and routing. If pursued later, they should be a separate instrument or a new project.
- A large internal modulation matrix while Ableton can provide LFO and automation routing externally.
- Turning Filter Step into a note sequencer or cutoff-value quantizer.
- Importing the long reference recording as a sampler feature.
- Committing to dual filters before the prototype is auditioned.

## Known issues and technical debt

- `auval -stress` can produce infinity under concurrent parameter changes in iPlug2's SVF. The stress test is disabled in CI.
- Wavetable updates are not double-buffered yet.
- The Wave selector does not currently distinguish a hand-edited `Custom` table.
- The project still needs a license decision.

## Release discipline

- Every feature should result in a build that can be tested in Ableton.
- Run `just test` before release preparation.
- Run `just release-dry` to exercise macOS packaging.
- Add release notes to `installer/changelog.txt` before `just release`.
- Keep parameter IDs append-only and test old project state whenever state layout changes.

## References

In-repo references:

- `iPlug2/Examples/IPlugInstrument/` is the main voice/DSP architecture reference.
- `iPlug2/Examples/IPlugChunks/` shows custom state and arbitrary UI message patterns.

External references used during the initial work:

- `JanWilczek/wavetable-synth` for a pedagogical wavetable oscillator.
- `FigBug/Wavetable` for a production wavetable synth implementation.
- `PaulBatchelor/Soundpipe` for bitcrush behavior.
- `link-512/Downsamplr-VST-Plugin` for a lo-fi plugin reference.
