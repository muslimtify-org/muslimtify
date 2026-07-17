#!/usr/bin/env bash
# bump-version.sh — Single source of truth for version updates.
# Updates CMakeLists.txt + all packaging files in one shot, resets distro
# revisions, refreshes changelog headers/timestamps, and regenerates the AUR
# .SRCINFO.
#
# The ONE value this cannot set is the AUR sha256sums — it is the hash of
# GitHub's archive/vX.Y.Z.tar.gz, which only exists after you push the tag.
# Run .packages/refresh-aur-hash.sh after tagging to fill it.
#
# This does NOT touch the Fedora spec %changelog or the Debian changelog at all.
# Both are hand-maintained: write the new top stanza yourself before tagging.
# Debian has no Version: field — its changelog IS the version source — so
# check-versions.sh fails until that stanza exists. That failure is the reminder.
#
# Usage:
#   ./bump-version.sh 0.2.0            # release version
#   ./bump-version.sh 0.2.0 rc.1       # pre-release suffix
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <version> [suffix]"
    echo "  e.g. $0 0.2.0"
    echo "  e.g. $0 0.2.0 rc.1"
    exit 1
fi

VERSION="$1"
SUFFIX="${2:-}"

if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: version must be in X.Y.Z format (got '$VERSION')"
    exit 1
fi

if [ -n "$SUFFIX" ]; then
    FULL_VERSION="${VERSION}-${SUFFIX}"
    CMAKE_SUFFIX_LINE="set(FULL_PROJECT_VERSION \"\${PROJECT_VERSION}-${SUFFIX}\")"
else
    FULL_VERSION="$VERSION"
    CMAKE_SUFFIX_LINE="set(FULL_PROJECT_VERSION \"\${PROJECT_VERSION}\")"
fi

echo "Bumping version to ${FULL_VERSION}"

# -- CMakeLists.txt -----------------------------------------------------------

sed -i "s/^project(muslimtify VERSION [^ ]*/project(muslimtify VERSION ${VERSION}/" CMakeLists.txt
sed -i "s/^set(FULL_PROJECT_VERSION .*)/${CMAKE_SUFFIX_LINE}/" CMakeLists.txt
echo "  updated CMakeLists.txt"

# -- Fedora spec --------------------------------------------------------------
# Reset Release to 1 for the new upstream version. The %changelog is not touched.

SPEC=".packages/fedora/muslimtify.spec"
if [ -f "$SPEC" ]; then
    sed -i "s/^Version:        .*/Version:        ${VERSION}/" "$SPEC"
    sed -i -E "s/^Release:[[:space:]]+.*/Release:        1%{?dist}/" "$SPEC"
    echo "  updated ${SPEC}"
fi

# -- AUR PKGBUILD -------------------------------------------------------------
# Reset pkgrel to 1; sha256sums is refreshed post-tag by refresh-hash.sh.

PKGBUILD=".packages/aur/PKGBUILD"
if [ -f "$PKGBUILD" ]; then
    sed -i "s/^pkgver=.*/pkgver=${VERSION}/" "$PKGBUILD"
    sed -i "s/^pkgrel=.*/pkgrel=1/" "$PKGBUILD"
    echo "  updated ${PKGBUILD}"
fi

# -- Winget / Inno Setup ------------------------------------------------------

ISS=".packages/winget/muslimtify.iss"
if [ -f "$ISS" ]; then
    # The define sits indented inside an #ifndef block; preserve the leading
    # whitespace so the match isn't anchored to column 0.
    sed -i -E "s/^([[:space:]]*)#define MyAppVersion \".*\"/\1#define MyAppVersion \"${VERSION}\"/" "$ISS"
    echo "  updated ${ISS}"
fi

# -- Regenerate AUR .SRCINFO --------------------------------------------------
# .SRCINFO is generated, never hand-edited. It still carries the OLD sha256sums
# until refresh-hash.sh runs post-tag (which updates the hash and regenerates
# .SRCINFO again).

if [ -f "$PKGBUILD" ]; then
    if command -v makepkg >/dev/null 2>&1; then
        ( cd "$(dirname "$PKGBUILD")" && makepkg --printsrcinfo > .SRCINFO )
        echo "  regenerated .packages/aur/.SRCINFO"
    else
        echo "  WARNING: makepkg not found — .SRCINFO NOT regenerated."
        echo "           Run: (cd .packages/aur && makepkg --printsrcinfo > .SRCINFO)"
    fi
fi

echo ""
echo "Done. Verify consistency with:"
echo "  ./.packages/check-versions.sh"
echo ""
echo "Next:"
echo "  1. Write the new changelog stanzas by hand (not done by this script):"
echo "       .packages/fedora/muslimtify.spec   %changelog"
echo "         * $(date '+%a %b %d %Y') <maintainer> - ${VERSION}-1"
echo "       .packages/debian/debian/changelog  new top stanza"
echo "         muslimtify (${VERSION}-1) unstable; urgency=medium"
echo "          -- <maintainer>  $(date -R)"
echo "     The debian date must be RFC-2822 ('date -R'); a malformed one fails"
echo "     the PPA build at dh_installchangelogs."
echo "  2. Commit, then: git tag v${VERSION} && git push --tags"
echo "  3. After the tag is on GitHub, refresh the AUR source hash:"
echo "       ./.packages/refresh-aur-hash.sh"
