#!/bin/bash
# Build muslimtify .deb package using debootstrap chroot on Arch Linux
set -euo pipefail

DISTRO="${1:-resolute}"  # Default: Ubuntu (resolute)
ARCH="amd64"
CHROOT_DIR="${HOME}/.cache/muslimtify-deb-chroot"
PROJECT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
PKG_VERSION="$(grep -oP '\(\K[^)]+' "$PROJECT_DIR/.packages/debian/debian/changelog" | head -1 | cut -d- -f1)"
PKG_NAME="muslimtify"

echo "==> Building ${PKG_NAME}_${PKG_VERSION} for ${DISTRO} (${ARCH})"
echo "==> Project: ${PROJECT_DIR}"

# --- Step 1: Create chroot ---
if [ ! -d "$CHROOT_DIR" ]; then
    echo "==> Creating debootstrap chroot at ${CHROOT_DIR}..."
    sudo debootstrap --arch="$ARCH" "$DISTRO" "$CHROOT_DIR" http://archive.ubuntu.com/ubuntu
else
    echo "==> Reusing existing chroot at ${CHROOT_DIR}"
fi

# --- Step 2: Mount necessary filesystems ---
cleanup() {
    echo "==> Cleaning up mounts..."
    sudo umount "$CHROOT_DIR/proc" 2>/dev/null || true
    sudo umount "$CHROOT_DIR/sys" 2>/dev/null || true
    sudo umount "$CHROOT_DIR/dev/pts" 2>/dev/null || true
    sudo umount "$CHROOT_DIR/dev" 2>/dev/null || true
    sudo umount "$CHROOT_DIR/build" 2>/dev/null || true
}
trap cleanup EXIT

sudo mount --bind /proc "$CHROOT_DIR/proc"
sudo mount --bind /sys "$CHROOT_DIR/sys"
sudo mount --bind /dev "$CHROOT_DIR/dev"
sudo mount --bind /dev/pts "$CHROOT_DIR/dev/pts"

# --- Step 3: Copy source into chroot ---
sudo mkdir -p "$CHROOT_DIR/build"
sudo mount --bind "$PROJECT_DIR" "$CHROOT_DIR/build"

# --- Step 4: Install build deps and build ---
LOG_FILE="${PROJECT_DIR}/.packages/debian/build-${DISTRO}.log"
echo "==> Full build output -> ${LOG_FILE}"

set +e  # capture the chroot exit status ourselves so we can surface the cause
sudo chroot "$CHROOT_DIR" /bin/bash -c "
set -euo pipefail

# Enable main + universe repos
cat > /etc/apt/sources.list <<SOURCES
deb http://archive.ubuntu.com/ubuntu ${DISTRO} main universe
deb http://archive.ubuntu.com/ubuntu ${DISTRO}-updates main universe
deb http://archive.ubuntu.com/ubuntu ${DISTRO}-security main universe
SOURCES

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential \
    gcc-13 \
    debhelper \
    cmake \
    pkg-config \
    libnotify-dev \
    libcurl4-openssl-dev \
    devscripts \
    fakeroot \
    lsb-release

# Prepare source tree
cd /tmp
rm -rf ${PKG_NAME}-build
# Drop stale build outputs from previous runs (chroot /tmp persists across builds),
# otherwise the copy-back below re-copies every version ever built here.
rm -f ${PKG_NAME}_*
cp -a /build ${PKG_NAME}-build
cd ${PKG_NAME}-build

# Copy debian/ directory into source root
cp -a .packages/debian/debian .

# Pin gcc-13: this chroot defaults to gcc-15, but its binutils predates the
# '.base64' assembler directive gcc-15 emits, so 'as' aborts with
# 'unknown pseudo-op: .base64'. gcc-13 matches the chroot's binutils.
export CC=gcc-13

# Build the .deb; on failure, surface the CMake configure logs that dh hides
if ! dpkg-buildpackage -us -uc -b; then
    echo '!!! dpkg-buildpackage failed -- dumping CMake error log (compiler/toolchain checks):'
    for f in obj-*-linux-gnu/CMakeFiles/CMakeError.log; do
        [ -f \"\$f\" ] && { echo \"===== \$f =====\"; cat \"\$f\"; }
    done
    exit 1
fi

# Refresh output dir: prune all old muslimtify artifacts, then copy back only
# this build's version (PKG_VERSION) so the directory holds just the latest.
rm -f /build/.packages/debian/${PKG_NAME}_*.deb \
      /build/.packages/debian/${PKG_NAME}_*.buildinfo \
      /build/.packages/debian/${PKG_NAME}_*.changes
cp /tmp/${PKG_NAME}_${PKG_VERSION}-*.deb /build/.packages/debian/
cp /tmp/${PKG_NAME}_${PKG_VERSION}-*.buildinfo /build/.packages/debian/ 2>/dev/null || true
cp /tmp/${PKG_NAME}_${PKG_VERSION}-*.changes /build/.packages/debian/ 2>/dev/null || true

echo '==> Build complete!'
ls -lh /tmp/${PKG_NAME}_${PKG_VERSION}-*.deb
" 2>&1 | tee "$LOG_FILE"
build_status=${PIPESTATUS[0]}
set -e

if [ "$build_status" -ne 0 ]; then
    echo
    echo "==> BUILD FAILED (exit ${build_status}). Root-cause configure error:"
    echo "--------------------------------------------------------------------"
    grep -nE -B30 'CMake Error|Could NOT find|dh_auto_configure: error' "$LOG_FILE" | tail -60 || true
    echo "--------------------------------------------------------------------"
    echo "==> Full log saved at: ${LOG_FILE}"
    exit "$build_status"
fi

echo "==> Output .deb files:"
ls -lh "$PROJECT_DIR/.packages/debian/"*.deb 2>/dev/null || echo "Check ${PROJECT_DIR}/.packages/debian/ for output"
