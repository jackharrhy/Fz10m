# Fz10m — common dev recipes. Run `just` to list.

workspace := "Fz10m.xcworkspace"
plugin_name := "Fz10m"

# List available recipes
default:
    @just --list

# Build a scheme (default: macOS-VST3 Debug)
build scheme="macOS-VST3" config="Debug":
    xcodebuild -workspace {{workspace}} -scheme {{scheme}} -configuration {{config}} build

# Build and run the standalone app (ad-hoc signed for local dev)
app config="Debug":
    xcodebuild -workspace {{workspace}} -scheme macOS-APP -configuration {{config}} build \
        CODE_SIGN_IDENTITY="-" CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO
    open ~/Applications/{{plugin_name}}.app

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

# Validate a built bundle with pluginval only (faster than `just test`)
pluginval scheme="macOS-VST3":
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

# One-shot post-clone setup: init submodules, fetch SDKs, download test tools
setup:
    #!/usr/bin/env bash
    set -euo pipefail

    git submodule update --init --recursive

    cd iPlug2/Dependencies/IPlug
    # Build the VST3 validator tool alongside the SDK so `just test` can use it.
    ./download-vst3-sdk.sh v3.8.0_build_66 build-validator
    ./download-clap-sdks.sh
    cd ../../..

    # Local tools dir (gitignored).
    mkdir -p .local-tools

    # clap-validator (Bitwig's CLAP test harness)
    if [ ! -x .local-tools/clap-validator ]; then
        echo "fetching clap-validator..."
        cd .local-tools
        CLAP_VER="0.3.2"
        curl -fsSL "https://github.com/free-audio/clap-validator/releases/download/${CLAP_VER}/clap-validator-${CLAP_VER}-macos-universal.tar.gz" -o clap-validator.tar.gz
        tar -xzf clap-validator.tar.gz
        mv binaries/clap-validator ./clap-validator
        rm -rf binaries clap-validator.tar.gz
        cd ..
    fi

    # pluginval (reminder, since it's a brew cask)
    if [ ! -x /Applications/pluginval.app/Contents/MacOS/pluginval ]; then
        echo "pluginval not installed. run: brew install --cask pluginval"
    fi

# Run the same validator suite that CI runs. Builds VST3/AU/CLAP first.
test:
    #!/usr/bin/env bash
    set -eo pipefail

    pluginval="/Applications/pluginval.app/Contents/MacOS/pluginval"
    vst3_validator="iPlug2/Dependencies/IPlug/VST3_SDK/validator"
    clap_validator=".local-tools/clap-validator"

    missing=0
    [ -x "$pluginval" ] || { echo "missing: pluginval (brew install --cask pluginval)"; missing=1; }
    [ -x "$vst3_validator" ] || { echo "missing: VST3 validator (run: just setup)"; missing=1; }
    [ -x "$clap_validator" ] || { echo "missing: clap-validator (run: just setup)"; missing=1; }
    [ "$missing" -eq 0 ] || exit 1

    vst3="$HOME/Library/Audio/Plug-Ins/VST3/{{plugin_name}}.vst3"
    clap="$HOME/Library/Audio/Plug-Ins/CLAP/{{plugin_name}}.clap"
    auv2="$HOME/Library/Audio/Plug-Ins/Components/{{plugin_name}}.component"

    echo "=== building VST3 / AUv2 / CLAP ==="
    just build macOS-VST3  >/dev/null
    just build macOS-AUv2  >/dev/null
    just build macOS-CLAP  >/dev/null

    echo ""
    echo "=== VST3 validator (Steinberg) ==="
    "$vst3_validator" "$vst3"

    echo ""
    echo "=== CLAP validator (Bitwig) ==="
    "$clap_validator" validate "$clap"

    echo ""
    echo "=== pluginval on VST3 ==="
    "$pluginval" --skip-gui-tests --validate-in-process --output-dir .local-tools/pluginval-out --validate "$vst3"

    echo ""
    echo "=== pluginval on AUv2 ==="
    pgrep -x AudioComponentRegistrar >/dev/null && killall -9 AudioComponentRegistrar || true
    "$pluginval" --skip-gui-tests --validate-in-process --output-dir .local-tools/pluginval-out --validate "$auv2"

    echo ""
    echo "=== auval ==="
    # validate_audiounit.sh does 'cd $BASEDIR' at startup, so a relative
    # 'config.h' stops working. Stage a flat copy next to the script,
    # mimicking what CI sees inside Fz10m-v*-mac-auval.zip, then invoke it.
    cp config.h iPlug2/Scripts/config.h
    ./iPlug2/Scripts/validate_audiounit.sh config.h
    rm -f iPlug2/Scripts/config.h

    echo ""
    echo "✅ all validators passed"

# Dry-run the release-native CI pipeline locally (mac only; no push/tag)
release-dry:
    #!/usr/bin/env bash
    set -eo pipefail

    echo "=== step 1: get_archive_name (matches release-native.yml Get archive name step) ==="
    name=$(python3 iPlug2/Scripts/get_archive_name.py . mac full)
    echo "archive name: ${name}"

    echo ""
    echo "=== step 2: makedist-mac.sh full installer ==="
    cd scripts && ./makedist-mac.sh full installer && cd ..

    echo ""
    echo "=== step 3: verify expected artefacts (Stage macOS artefacts step) ==="
    for f in "build-mac/out/${name}-dSYMs.zip" "build-mac/out/${name}.dmg"; do
        if [ -f "$f" ]; then
            echo "✓ $f"
        else
            echo "✗ MISSING: $f"
            exit 1
        fi
    done

    echo ""
    echo "✅ release-native.yml would succeed for macOS"

# Bump version, tag, and push to trigger a release build + GitHub release
release:
    #!/usr/bin/env bash
    set -euo pipefail

    # Must be on main with a clean working tree.
    branch=$(git rev-parse --abbrev-ref HEAD)
    if [ "$branch" != "main" ]; then
        echo "release must be run from main (currently on '$branch')" >&2
        exit 1
    fi
    if ! git diff-index --quiet HEAD -- || [ -n "$(git ls-files --others --exclude-standard)" ]; then
        echo "working tree is not clean. commit or stash first." >&2
        git status --short
        exit 1
    fi

    # Ask for bump type.
    current_version=$(grep -E '^#define PLUG_VERSION_STR' config.h | sed -E 's/.*"(.+)".*/\1/')
    echo "=== Fz10m Release ==="
    echo ""
    echo "  current version: v${current_version}"
    echo ""
    echo "  1) patch"
    echo "  2) minor"
    echo "  3) major"
    echo ""
    read -rp "bump type [1/2/3]: " choice
    case "$choice" in
        1) bump="patch" ;;
        2) bump="minor" ;;
        3) bump="major" ;;
        *) echo "invalid choice."; exit 1 ;;
    esac

    # Show what's about to happen.
    echo ""
    echo "  bump type:        ${bump}"
    echo "  branch:           ${branch}"
    echo ""
    echo "this will:"
    echo "  1. bump version (${bump}) in config.h + Info.plists"
    echo "  2. commit the version bump"
    echo "  3. create a git tag"
    echo "  4. push to origin (triggers CI build + GitHub release)"
    echo ""
    echo "commits since last tag:"
    last_tag=$(git describe --tags --abbrev=0 2>/dev/null || echo "")
    if [ -n "$last_tag" ]; then
        git log --oneline "${last_tag}..HEAD"
    else
        git log --oneline -10
    fi
    echo ""
    read -rp "proceed? [y/N] " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        echo "aborted."
        exit 0
    fi

    # Bump version + regenerate Info.plists.
    ./bump_version.py "$bump"

    # Read the new version back so we can tag it.
    new_version=$(grep -E '^#define PLUG_VERSION_STR' config.h | sed -E 's/.*"(.+)".*/\1/')

    # Verify changelog has an entry for this version.
    # installer/changelog.txt is used as the GitHub release body.
    if ! grep -q "v${new_version}" installer/changelog.txt; then
        echo ""
        echo "error: installer/changelog.txt has no entry for v${new_version}" >&2
        echo "add a changelog entry before releasing. reverting version bump..." >&2
        git checkout -- config.h resources/ installer/
        exit 1
    fi

    echo ""
    echo "tagging as v${new_version}"

    # Commit everything bump_version.py touched + tag + push.
    git add config.h resources/ installer/
    git commit -m "release: v${new_version}"
    git tag "v${new_version}"
    git push origin main
    git push origin "v${new_version}"

    echo ""
    echo "pushed v${new_version}. CI will now build + publish the release."
    echo "watch progress: gh run watch  (or check the Actions tab on GitHub)"
