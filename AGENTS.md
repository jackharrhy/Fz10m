# CLAUDE.md

## Fz10m

Standalone iPlug2 plugin repo. Single plugin, not a template/workspace.

## Layout

- Plugin sources at repo root: `Fz10m.cpp`, `Fz10m.h`, `config.h`
- `projects/` — Xcode projects (macOS, iOS) and Visual Studio solution
- `resources/` — Info.plists, icons, fonts, storyboards
- `config/` — per-platform xcconfig / props / mk
- `scripts/` — build-phase Python and shell helpers
- `iPlug2/` — framework (git submodule; pinned)

## Building

macOS VST3 Debug:
```bash
xcodebuild -workspace Fz10m.xcworkspace -scheme macOS-VST3 -configuration Debug build
```

Other schemes: `macOS-APP`, `macOS-AU`, `macOS-VST2`, `macOS-CLAP`, `macOS-AAX`, `macOS-AUv3`.

Install locations are standard macOS plugin folders (`~/Library/Audio/Plug-Ins/...`).

## Dependencies

After cloning:
1. `git submodule update --init --recursive` (populates `iPlug2/`)
2. `iPlug2/Dependencies/IPlug/download-vst3-sdk.sh` (populates `VST3_SDK/`)

For CLAP/WAM builds, run `download-clap-sdks.sh` / `download-wam-sdk.sh` in the same directory.

## iPlug2 framework notes

- Three main files per project: `Fz10m.cpp`, `Fz10m.h`, `config.h`
- `ProcessBlock` must be realtime-safe (no allocations, locks, file I/O)
- Parameters: fixed count at compile time, indexed by enum, non-normalized
- Access via `GetParam(kIndex)->Value()`
- See [iPlug2 wiki](https://github.com/iPlug2/iPlug2/wiki) and [docs](https://iplug2.github.io/docs) for the full API

## Codesigning note

All Debug builds are ad-hoc signed (`CODE_SIGN_IDENTITY = "-"` in `projects/Fz10m-macOS.xcodeproj/project.pbxproj`) so that bundle Resources are sealed. Without this, Ableton Live 12's plugin scanner rejects the bundle with "code has no resources but signature indicates they must be present". For Release / AUv3 delivery, change to a real identity.

## Code style

- 2-space indent, no tabs, Unix line endings (Windows files exempt)
- Member vars: `mCamelCase` · pointer args: `pCamelCase` · internal methods: `_methodName`
- C++17: `override`, `final`, `auto`, `std::optional`, `std::string_view`
- Prefer WDL (`iPlug2/WDL`) over STL in hot paths
