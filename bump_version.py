#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["semver>=3.0,<4"]
# ///
"""Bump Fz10m's version in config.h and regenerate Info.plist files.

Usage:
    ./bump_version.py <major|minor|patch>

Non-interactive. Exits non-zero on invalid input. Designed to be called
from `just release`, but also usable standalone. Does NOT commit, tag, or
push — the caller is responsible for those.

Uses uv's inline-script metadata (PEP 723) so the semver dependency is
managed automatically without requiring a venv.
"""

import fileinput
import glob
import os
import subprocess
import sys

import semver

# Layout: everything at repo root. iPlug2 is a git submodule at ./iPlug2.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
IPLUG2_ROOT = os.path.join(SCRIPT_DIR, "iPlug2")
CONFIG_H = os.path.join(SCRIPT_DIR, "config.h")
UPDATE_SCRIPTS_DIR = os.path.join(SCRIPT_DIR, "scripts")

sys.path.insert(0, os.path.join(IPLUG2_ROOT, "Scripts"))

from parse_config import parse_config  # noqa: E402 — after sys.path tweak


def replace_in_file(path, old, new):
    for line in fileinput.input(path, inplace=True):
        sys.stdout.write(line.replace(old, new))


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("major", "minor", "patch"):
        print("usage: bump_version.py <major|minor|patch>", file=sys.stderr)
        sys.exit(2)

    bump_type = sys.argv[1]

    # parse_config expects a directory containing config.h
    config = parse_config(SCRIPT_DIR)
    current = semver.VersionInfo.parse(config["FULL_VER_STR"])
    print(f"current version: v{current}")

    new = getattr(current, f"bump_{bump_type}")()
    new_hex = "0x{:08x}".format(
        (new.major << 16 & 0xFFFF0000)
        + (new.minor << 8 & 0x0000FF00)
        + (new.patch & 0x000000FF)
    )

    print(f"new version:     v{new}  (hex: {new_hex})")

    # Update config.h
    replace_in_file(
        CONFIG_H,
        f'#define PLUG_VERSION_STR "{current}"',
        f'#define PLUG_VERSION_STR "{new}"',
    )
    replace_in_file(
        CONFIG_H,
        f'#define PLUG_VERSION_HEX {config["PLUG_VERSION_HEX"]}',
        f"#define PLUG_VERSION_HEX {new_hex}",
    )

    # Regenerate Info.plist files — run each helper from scripts/ because
    # they use os.getcwd()-relative paths to locate iPlug2 Scripts.
    for script in ("update_version-mac.py", "update_version-ios.py"):
        subprocess.run(
            ["python3", script],
            cwd=UPDATE_SCRIPTS_DIR,
            check=True,
        )

    # Windows installer text. `0` disables the demo-mode banner.
    subprocess.run(
        ["python3", "update_installer-win.py", "0"],
        cwd=UPDATE_SCRIPTS_DIR,
        check=True,
    )

    print(f"\nBumped to v{new}. Review the diff before committing:")
    print("  git diff config.h resources/ installer/")


if __name__ == "__main__":
    main()
