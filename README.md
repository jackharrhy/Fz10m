# Fz10m

Fz10m is a drawable-wavetable lo-fi synth plugin inspired by the Casio FZ series. It is built with [iPlug2](https://github.com/iPlug2/iPlug2) and uses the FZ as a reference rather than trying to emulate it.

The instrument has eight voices and a 128-point wavetable that can be drawn, generated from presets, or built with 16 harmonic controls. Each voice has amp and filter envelopes, a stepped multimode filter, and a lo-fi stage with sample-rate hold and bit-depth reduction. The lo-fi stage can run before or after the filter.

Fz10m builds as VST3, AUv2, and CLAP on macOS, and as VST3 and CLAP on Windows.

## Setup

macOS development requires Xcode and [just](https://github.com/casey/just).

```bash
git clone --recurse-submodules https://github.com/jackharrhy/Fz10m.git
cd Fz10m
just setup
```

On macOS, `just setup` initializes the iPlug2 submodule, downloads the VST3 and CLAP SDKs, and prepares the validator dependencies.

## Building

```bash
just build              # macOS VST3 Debug (default)
just build macOS-AUv2   # AUv2
just build macOS-CLAP   # CLAP
just app                # build and launch standalone app
just open               # open the workspace in Xcode
```

### Windows

Windows builds require Visual Studio 2022. Open `projects/Fz10m.sln` or build it from a developer command prompt:

```cmd
msbuild projects\Fz10m.sln /p:Configuration=Release /p:Platform=x64
```

## Validation

```bash
just test
```

This builds the macOS VST3, AUv2, and CLAP targets, then runs the Steinberg VST3 validator, clap-validator, pluginval, and `auval`.

## Releasing

Add the next version and its notes to `installer/changelog.txt`, commit the change to `main`, and make sure the working tree is clean. Then run:

```bash
just release
```

The recipe reads the next untagged version from the changelog and shows the release notes before making changes. It then updates the version files, commits, tags, and pushes. CI builds the macOS and Windows packages and creates a draft GitHub release.

## License

TBD.
