#!/usr/bin/env bash
# refresh-aur-hash.sh — Fill the AUR sha256sums AFTER the git tag exists on
# GitHub.
#
# bump-version.sh sets every other packaging field offline, but the AUR source
# checksum is the hash of GitHub's archive/vX.Y.Z.tar.gz, which only exists once
# the tag is pushed. Run this then: it updates PKGBUILD's sha256sums and
# regenerates .SRCINFO.
#
# NOTE: the AUR packaging (.packages/muslimtify/) is gitignored in this repo and
# kept locally / in the AUR git repo; this script operates on those local files.
#
# Usage (after 'git push --tags'):
#   ./.packages/refresh-aur-hash.sh
set -euo pipefail

AUR_DIR="$(cd "$(dirname "$0")/muslimtify" && pwd)"
cd "$AUR_DIR"

PKGVER="$(grep -oP '^pkgver=\K[0-9.]+' PKGBUILD)"
URL="https://github.com/rizukirr/muslimtify/archive/v${PKGVER}.tar.gz"
echo "==> Refreshing AUR sha256sums for v${PKGVER}"
echo "    Source: ${URL}"

if command -v updpkgsums >/dev/null 2>&1; then
    # pacman-contrib: downloads sources and rewrites the sums in PKGBUILD.
    updpkgsums
else
    echo "    (updpkgsums not found; install pacman-contrib for the clean path)"
    SUM="$(curl -fsSL "$URL" | sha256sum | cut -d' ' -f1)"
    if [ -z "$SUM" ]; then
        echo "ERROR: failed to download or hash ${URL}" >&2
        echo "       Is the tag v${PKGVER} pushed to GitHub yet?" >&2
        exit 1
    fi
    sed -i -E "s/^sha256sums=\('[0-9a-f]+'\)/sha256sums=('${SUM}')/" PKGBUILD
fi

if command -v makepkg >/dev/null 2>&1; then
    makepkg --printsrcinfo > .SRCINFO
    echo "==> Regenerated .SRCINFO"
else
    echo "WARNING: makepkg not found — .SRCINFO NOT regenerated." >&2
fi

echo "==> Done:"
grep -E '^pkgver=|^pkgrel=|^sha256sums=' PKGBUILD
