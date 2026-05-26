#!/bin/bash
# Build muslimtify .rpm package using a Podman (or Docker) Fedora container on Arch Linux
set -euo pipefail

FEDORA_VERSION="${1:-44}"
PKG_NAME="muslimtify"
PROJECT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
PKG_VERSION="$(grep '^Version:' "$PROJECT_DIR/.packages/fedora/muslimtify.spec" | awk '{print $2}')"
OUTPUT_DIR="$PROJECT_DIR/.packages/fedora"

# Prefer podman, fall back to docker
if command -v podman &>/dev/null; then
    CONTAINER_CMD="podman"
elif command -v docker &>/dev/null; then
    CONTAINER_CMD="docker"
else
    echo "Error: podman or docker is required."
    echo "  sudo pacman -S podman"
    exit 1
fi

echo "==> Building ${PKG_NAME}-${PKG_VERSION} RPM for Fedora ${FEDORA_VERSION}"
echo "==> Using: ${CONTAINER_CMD}"
echo "==> Project: ${PROJECT_DIR}"

$CONTAINER_CMD run --rm \
    -v "${PROJECT_DIR}:/src:z" \
    -w /src \
    "fedora:${FEDORA_VERSION}" \
    bash -c "
set -euo pipefail

echo '==> Installing build dependencies...'
dnf install -y \
    cmake \
    gcc \
    pkgconfig \
    libnotify-devel \
    libcurl-devel \
    rpm-build \
    rpmdevtools

# Set up rpmbuild tree
rpmdev-setuptree

# Create source tarball (rpmbuild expects Source0 format)
TOPDIR=\$(rpm --eval '%{_topdir}')
cd /tmp
cp -a /src ${PKG_NAME}-${PKG_VERSION}
tar czf \"\${TOPDIR}/SOURCES/${PKG_NAME}-${PKG_VERSION}.tar.gz\" \
    --exclude=.git \
    --exclude=build \
    --exclude='.packages/debian/*.deb' \
    --exclude='.packages/debian/*.tar.*' \
    --exclude='.packages/debian/*.dsc' \
    ${PKG_NAME}-${PKG_VERSION}
rm -rf ${PKG_NAME}-${PKG_VERSION}

# Copy spec file
cp /src/.packages/fedora/muslimtify.spec \"\${TOPDIR}/SPECS/\"

# Build binary RPM
rpmbuild -bb \"\${TOPDIR}/SPECS/muslimtify.spec\"

# Copy results back
cp \"\${TOPDIR}/RPMS/\"*/*.rpm /src/.packages/fedora/

echo ''
echo '==> Build complete!'
ls -lh \"\${TOPDIR}/RPMS/\"*/*.rpm
"

echo ""
echo "==> Output RPM files:"
ls -lh "$OUTPUT_DIR/"*.rpm 2>/dev/null || echo "Check ${OUTPUT_DIR}/ for output"
