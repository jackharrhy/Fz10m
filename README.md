# Fz10m

A simple audio effect VST3/AU/CLAP/AAX plugin built with [iPlug2](https://github.com/iPlug2/iPlug2).

## Cloning

```bash
git clone --recurse-submodules https://github.com/<you>/Fz10m.git
# or, if you already cloned without --recurse-submodules:
git submodule update --init --recursive
```

After the submodule is populated, download the VST3 SDK (it's a separate set of Steinberg submodules fetched by a helper script):

```bash
cd iPlug2/Dependencies/IPlug
./download-vst3-sdk.sh
```

For CLAP and WAM builds you'll also want `./download-clap-sdks.sh` and `./download-wam-sdk.sh` in the same directory.

## Building

### macOS (Xcode)

Open the workspace:

```bash
open Fz10m.xcworkspace
```

…or build from the command line. Available schemes: `macOS-APP`, `macOS-AU`, `macOS-VST3`, `macOS-CLAP`, `macOS-AAX`, `macOS-AUv3`.

```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build
```

Built plugins install automatically to the standard macOS plug-in folders:

- VST3 → `~/Library/Audio/Plug-Ins/VST3/Fz10m.vst3`
- AU   → `~/Library/Audio/Plug-Ins/Components/Fz10m.component`
- CLAP → `~/Library/Audio/Plug-Ins/CLAP/Fz10m.clap`
- APP  → `~/Applications/Fz10m.app`

### iOS

```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme "iOS-APP with AUv3" -configuration Debug -destination 'platform=iOS Simulator,name=iPhone 16 Pro' build
```

### Windows

Open `Fz10m.sln` in Visual Studio, or from the command line:

```cmd
msbuild Fz10m.sln /p:Configuration=Release /p:Platform=x64
```

### CMake (Ninja / Xcode generator)

```bash
cmake --preset mac-ninja
cmake --build --preset mac-ninja
```

## Project layout

```
Fz10m/
├── Fz10m.cpp, Fz10m.h, config.h   Plugin sources and compile-time config
├── projects/                       Xcode projects and Visual Studio solution
├── resources/                      Info.plist files, icons, fonts, storyboards
├── config/                         Per-platform xcconfig / props / mk files
├── scripts/                        Build-phase helpers (prepare_resources, installer, etc.)
├── installer/                      Installer assets (.iss, license, readme RTF)
├── manual/                         Source for the user manual
└── iPlug2/                         iPlug2 framework (git submodule)
```

## License

TBD.
