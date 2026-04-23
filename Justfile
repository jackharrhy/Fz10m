# Fz10m — common dev recipes. Run `just` to list.

workspace := "Fz10m.xcworkspace"
plugin_name := "Fz10m"

# List available recipes
default:
    @just --list

# Build a scheme (default: macOS-VST3 Debug)
build scheme="macOS-VST3" config="Debug":
    xcodebuild -workspace {{workspace}} -scheme {{scheme}} -configuration {{config}} build

# Open the Xcode workspace
open:
    open {{workspace}}

# Remove build artefacts and installed plugin bundles
clean:
    rm -rf ~/Library/Developer/Xcode/DerivedData/{{plugin_name}}-*
    rm -rf ~/Library/Audio/Plug-Ins/VST3/{{plugin_name}}.vst3
    rm -rf ~/Library/Audio/Plug-Ins/Components/{{plugin_name}}.component
    rm -rf ~/Library/Audio/Plug-Ins/CLAP/{{plugin_name}}.clap
    rm -rf ~/Applications/{{plugin_name}}.app

# Validate the built VST3 (or another scheme) with pluginval
validate scheme="macOS-VST3":
    #!/usr/bin/env bash
    set -euo pipefail
    pluginval="/Applications/pluginval.app/Contents/MacOS/pluginval"
    if [ ! -x "$pluginval" ]; then
        echo "pluginval not found. Install with: brew install --cask pluginval"
        exit 1
    fi
    case "{{scheme}}" in
        *VST3*) bundle="$HOME/Library/Audio/Plug-Ins/VST3/{{plugin_name}}.vst3" ;;
        *AU*)   bundle="$HOME/Library/Audio/Plug-Ins/Components/{{plugin_name}}.component" ;;
        *CLAP*) bundle="$HOME/Library/Audio/Plug-Ins/CLAP/{{plugin_name}}.clap" ;;
        *)      echo "unknown scheme for validation: {{scheme}}"; exit 1 ;;
    esac
    if [ ! -e "$bundle" ]; then
        echo "bundle not found: $bundle"
        echo "build it first: just build {{scheme}}"
        exit 1
    fi
    "$pluginval" --validate "$bundle" --strictness-level 5

# One-shot post-clone setup: init submodules and fetch VST3 SDK
setup:
    git submodule update --init --recursive
    cd iPlug2/Dependencies/IPlug && ./download-vst3-sdk.sh
