#!/usr/bin/env bash
# bump-version.sh — Single source of truth for version updates.
# Updates CMakeLists.txt + all packaging files in one shot.
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

SPEC=".packages/fedora/muslimtify.spec"
if [ -f "$SPEC" ]; then
    sed -i "s/^Version:        .*/Version:        ${VERSION}/" "$SPEC"
    echo "  updated ${SPEC}"
fi

# -- AUR PKGBUILD -------------------------------------------------------------

PKGBUILD=".packages/aur/PKGBUILD"
if [ -f "$PKGBUILD" ]; then
    sed -i "s/^pkgver=.*/pkgver=${VERSION}/" "$PKGBUILD"
    echo "  updated ${PKGBUILD}"
fi

# -- Winget / Inno Setup ------------------------------------------------------

ISS=".packages/winget/muslimtify.iss"
if [ -f "$ISS" ]; then
    sed -i "s/^#define MyAppVersion \".*\"/#define MyAppVersion \"${VERSION}\"/" "$ISS"
    echo "  updated ${ISS}"
fi

# -- Debian changelog ---------------------------------------------------------

CHANGELOG=".packages/debian/debian/changelog"
if [ -f "$CHANGELOG" ]; then
    sed -i "1s/muslimtify ([^)]*)/muslimtify (${VERSION}-1)/" "$CHANGELOG"
    echo "  updated ${CHANGELOG}"
fi

echo ""
echo "Done. Verify with:"
echo "  grep -n 'VERSION\|Version\|pkgver\|MyAppVersion\|muslimtify (' CMakeLists.txt ${SPEC} ${PKGBUILD} ${ISS} ${CHANGELOG}"
