#!/usr/bin/env bash
# Extract the changelog entry for a specific version from installer/changelog.txt.
# Usage: ./scripts/extract-changelog.sh <version>
# Example: ./scripts/extract-changelog.sh 0.2.1
#
# Prints the entry body (bullet lines) for that version, or exits 1 if not found.
# Used by `just release` for preview and by CI for the GitHub release body.

set -euo pipefail

version="${1:?usage: extract-changelog.sh <version>}"
changelog="${2:-installer/changelog.txt}"

# Find the section for this version, print only the indented bullet lines
# (skip the date/version header line itself), and strip leading whitespace.
awk -v ver="v${version}" '
  found && /^[0-9]/ { exit }
  found && /^  / { print }
  $0 ~ ver { found=1 }
' "$changelog" | sed 's/^  //' | sed '/^$/d'
