#!/usr/bin/env bash
# check-versions.sh — Assert the upstream version (X.Y.Z) is identical across
# every packaging file. Run locally before tagging, and in CI on every push.
#
# Compares ONLY the upstream X.Y.Z. Distro revisions (pkgrel, RPM Release, the
# Debian -N suffix) intentionally differ and are ignored.
#
# Exit 0 if all agree; exit 1 (listing mismatches) otherwise.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CMAKELISTS="CMakeLists.txt"
SPEC=".packages/fedora/muslimtify.spec"
PKGBUILD=".packages/aur/PKGBUILD"
SRCINFO=".packages/aur/.SRCINFO"
CHANGELOG=".packages/debian/debian/changelog"
ISS=".packages/winget/muslimtify.iss"

SEMVER='[0-9]+\.[0-9]+\.[0-9]+'

# Extract the version from a file with a Perl-regex lookbehind.
#  - Missing file -> SKIP (the AUR dir, .packages/aur/, is gitignored and
#    absent in CI checkouts; it is still checked locally where it exists).
#  - File present but pattern unmatched -> hard error (the file changed shape and
#    this check has gone blind to it).
# Sets the global EXTRACTED to the version, or empty when skipped.
EXTRACTED=""
extract() {
    local label="$1" file="$2" pattern="$3" value
    EXTRACTED=""
    if [ ! -f "$file" ]; then
        printf '    skip %-20s (not present: %s)\n' "$label" "$file" >&2
        return 0
    fi
    value="$(grep -oP "$pattern" "$file" | head -1 || true)"
    if [ -z "$value" ]; then
        echo "ERROR: $label: could not extract a version from $file" >&2
        echo "       (pattern may need updating: $pattern)" >&2
        exit 2
    fi
    EXTRACTED="$value"
}

# label  file  pattern  — order matters only for display.
checks=(
    "CMakeLists.txt|$CMAKELISTS|project\(muslimtify VERSION \K${SEMVER}"
    "spec|$SPEC|^Version:\s*\K${SEMVER}"
    "PKGBUILD|$PKGBUILD|^pkgver=\K${SEMVER}"
    ".SRCINFO|$SRCINFO|^\s*pkgver = \K${SEMVER}"
    "debian/changelog|$CHANGELOG|muslimtify \(\K${SEMVER}"
    "winget.iss|$ISS|#define MyAppVersion \"\K${SEMVER}"
)

# Reference = CMakeLists.txt (the project's primary version source).
extract 'CMakeLists.txt' "$CMAKELISTS" "project\(muslimtify VERSION \K${SEMVER}"
REF="$EXTRACTED"
if [ -z "$REF" ]; then
    echo "ERROR: CMakeLists.txt missing or unparseable — cannot establish a reference version" >&2
    exit 2
fi
mismatch=0

echo "==> Packaging version check (reference: CMakeLists.txt = ${REF})"
for entry in "${checks[@]}"; do
    IFS='|' read -r label file pattern <<<"$entry"
    extract "$label" "$file" "$pattern"
    v="$EXTRACTED"
    [ -z "$v" ] && continue   # skipped (file absent)
    if [ "$v" = "$REF" ]; then
        printf '    ok   %-20s %s\n' "$label" "$v"
    else
        printf '    DIFF %-20s %s  (expected %s)\n' "$label" "$v" "$REF"
        mismatch=1
    fi
done

if [ "$mismatch" -ne 0 ]; then
    echo "" >&2
    echo "==> FAIL: packaging versions disagree. Run ./bump-version.sh <version>" >&2
    echo "    to resync every file, then re-run this check." >&2
    exit 1
fi

echo "==> All packaging files agree on ${REF}"
