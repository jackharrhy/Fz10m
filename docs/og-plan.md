Here’s a clean spec-style doc you can hand to an LLM 👇

---

# FZ-Inspired Drawn Waveform Synth (Plugin Spec)

## Goal

Implement a simple VST/AU plugin inspired by the Casio FZ series:

* User-drawn waveform → wavetable oscillator
* Lo-fi digital character (bit depth + sample rate)
* Stepped / imperfect filter (FZ-inspired, not emulated)

**Important:**
Do NOT attempt hardware-accurate emulation. Focus on *behavioral similarity*.

---

# Architecture Overview

```text
[UI: waveform draw]
        ↓
[Wavetable Oscillator]
        ↓
[Lo-Fi Stage (optional)]
  - Bitcrush
  - Downsample
        ↓
[Filter (stepped / imperfect)]
        ↓
[Output]
```

---

# 1. Wavetable Oscillator

## Data Model

```cpp
std::vector<float> waveform; // size: 256–1024 samples
float phase = 0.0f;
float sampleRate;
```

## Processing

```cpp
float processOsc(float freq) {
  float phaseInc = freq / sampleRate * waveform.size();

  int i0 = (int)phase;
  int i1 = (i0 + 1) % waveform.size();

  float frac = phase - i0;

  // start with linear interpolation
  float sample = waveform[i0] * (1.0f - frac) + waveform[i1] * frac;

  phase += phaseInc;
  if (phase >= waveform.size()) phase -= waveform.size();

  return sample;
}
```

## Notes

* Start with **linear interpolation**
* Later experiment with:

  * no interpolation (lo-fi)
  * higher quality interpolation

---

# 2. Waveform Drawing (UI → DSP)

## UI Behavior

* User draws 1 cycle waveform
* Normalize to [-1, 1]
* Store into `waveform` buffer

## Constraints

* Fixed length (e.g. 512 samples)
* Always looped

---

# 3. Lo-Fi Stage

## Bit Depth Reduction

```cpp
float bitcrush(float x, int bits) {
  float levels = (1 << bits);
  return roundf(x * levels) / levels;
}
```

Use:

* 8-bit (main mode)
* optional 12-bit

---

## Downsampling

Simple approach:

```cpp
float downsample(float x) {
  static float held = 0.0f;
  static int counter = 0;

  if (counter++ >= factor) {
    held = x;
    counter = 0;
  }

  return held;
}
```

### Suggested modes

* 36 kHz (clean-ish)
* 18 kHz
* 9 kHz

---

# 4. Filter (FZ-Inspired)

## Important Constraints

The real FZ filter:

* digitally controlled
* low resolution
* clocked / stepped
* not perfectly smooth

We emulate **effects**, not implementation.

---

## Base Filter

Start with simple biquad low-pass.

---

## Parameter Quantization (critical)

```cpp
float quantize(float x, int steps) {
  return roundf(x * steps) / steps;
}
```

Apply to:

```cpp
cutoff = quantize(cutoff, 64);
resonance = quantize(resonance, 32);
```

---

## Stepped Parameter Updates (very important)

Simulate clocked behavior:

```cpp
if (frame % stepInterval == 0) {
  currentCutoff = targetCutoff;
  currentRes = targetRes;
}
```

Or smoother stepping:

```cpp
currentCutoff += (targetCutoff - currentCutoff) * 0.1f;
```

---

## Optional Imperfections

```cpp
currentCutoff += random(-0.001f, 0.001f);
```

Keep VERY subtle.

---

# 5. Signal Chain

```cpp
float process(float freq) {
  float x = processOsc(freq);

  if (lofiEnabled) {
    x = bitcrush(x, bitDepth);
    x = downsample(x);
  }

  x = filter.process(x);

  return x;
}
```

---

# 6. MIDI Handling

## Note → Frequency

```cpp
float midiToFreq(int note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}
```

## MVP

* monophonic
* last note priority

---

# 7. Parameters (MVP)

```text
Waveform (UI drawn)
Frequency (via MIDI)
Gain

Lo-fi:
- Enable
- Bit depth (8 / 12)
- Sample rate mode (36k / 18k / 9k)

Filter:
- Cutoff
- Resonance
```

---

# 8. Non-Goals (for now)

* No exact FZ filter emulation
* No polyphony
* No anti-aliasing
* No oversampling
* No modulation matrix

---

# 9. Future Extensions

```text
- Polyphony
- ADSR envelope
- Pre/post filter routing
- Better interpolation
- Additional “character” modes
```

---

# Key Design Principle

This is NOT about accuracy.

It is about recreating:

```text
low sample rate
+ low bit depth
+ stepped parameters
+ imperfect filtering
```

That combination produces the “FZ-like” sound.

---

If you want, I can next:

* generate a **minimal iPlug2OOS project skeleton wired to this**
* or implement the oscillator + filter classes directly for you to drop in

