# Fz10m

A drawable-wavetable lo-fi synth plugin inspired by the Casio FZ series, built with [iPlug2](https://github.com/iPlug2/iPlug2).

8-voice polyphony, per-voice lo-fi stage (sample-rate hold + bit-depth quantization), stepped filter coefficient updates, ADSR envelope, and a drawable 128-point wavetable. Builds as VST3/AU/CLAP on macOS and VST3/CLAP on Windows.

## Getting started

```bash
git clone --recurse-submodules https://github.com/jackharrhy/Fz10m.git
cd Fz10m
just setup
```

`just setup` initializes submodules, downloads the VST3 and CLAP SDKs, and fetches test tools.

## Building

Requires [just](https://github.com/casey/just) and Xcode (macOS).

```bash
just build              # macOS VST3 Debug (default)
just build macOS-AU     # AU
just build macOS-CLAP   # CLAP
just app                # build and launch standalone app
```

Or open the workspace directly:

```bash
just open               # opens Fz10m.xcworkspace in Xcode
```

Built plugins install to standard macOS plug-in folders:

- VST3 → `~/Library/Audio/Plug-Ins/VST3/Fz10m.vst3`
- AU → `~/Library/Audio/Plug-Ins/Components/Fz10m.component`
- CLAP → `~/Library/Audio/Plug-Ins/CLAP/Fz10m.clap`
- APP → `~/Applications/Fz10m.app`

### Windows

Open `projects/Fz10m.sln` in Visual Studio:

```cmd
msbuild projects\Fz10m.sln /p:Configuration=Release /p:Platform=x64
```

## Releasing

```bash
just release
```

Prompts for major/minor/patch, shows changelog preview, then bumps version, tags, and pushes. CI builds and publishes a draft GitHub release.

## Project layout

```
Fz10m/
├── Fz10m.cpp, Fz10m.h, config.h   Plugin sources and compile-time config
├── Fz10m_DSP.h                     DSP: wavetable oscillator, lo-fi stage, voice, synth
├── projects/                       Xcode projects and Visual Studio solution
├── resources/                      Info.plist files, icons, fonts, storyboards
├── config/                         Per-platform xcconfig / props / mk files
├── scripts/                        Build-phase helpers (prepare_resources, installer, etc.)
├── installer/                      Installer assets (.iss, license, readme, changelog)
├── docs/                           Design specs and implementation plans
└── iPlug2/                         iPlug2 framework (git submodule)
```

## License

TBD.
