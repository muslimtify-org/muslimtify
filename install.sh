#!/bin/bash
# Muslimtify — full installer
# Builds in release mode, installs the binary, and sets up the systemd user service.
#
# Usage: sudo ./install.sh

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

# -- helpers ------------------------------------------------------------------

step() { echo -e "\n${BLUE}${BOLD}[$1/$TOTAL_STEPS] $2${NC}"; }
ok()   { echo -e "${GREEN}✓${NC} $1"; }
warn() { echo -e "${YELLOW}!${NC} $1"; }
die()  { echo -e "${RED}Error: $1${NC}" >&2; exit 1; }

run_as_user() {
    sudo -u "$REAL_USER" \
        HOME="$REAL_HOME" \
        XDG_RUNTIME_DIR="$XDG_RT" \
        DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RT/bus" \
        "$@"
}

# Refuse to build from a tree an unprivileged third party can write. CMake reads
# CMakeLists.txt and every source file, and `cmake --install` later executes
# build-release/cmake_install.cmake as root -- so a planted file anywhere in here
# is root code execution. build-release is deliberately NOT pruned: `cmake --install`
# reads from it as root. Symlink modes are excluded because they are conventionally
# lrwxrwxrwx; the target is walked separately if it is in-tree.
#
# The two prunes below are conditional invariants, not standing facts. Re-check them
# if the build definition changes:
#   .git  -- safe only while nothing in the build invokes git. Add an
#            execute_process(COMMAND git ...) to CMakeLists.txt and this prune must
#            go, or a planted .git/config (core.fsmonitor, core.hooksPath) becomes
#            unreviewed code execution.
#   build -- safe only while the installer builds in build-release, never build.
validate_tree() {
    local offenders
    offenders=$(find "$SCRIPT_DIR" \
        \( -name .git -o -name build \) -prune -o \
        \( \( ! -type l -a -perm /022 \) -o \( ! -user root -a ! -user "$REAL_USER" \) \) \
        -print)
    if [ -n "$offenders" ]; then
        echo "$offenders" >&2
        die "Unsafe permissions in $SCRIPT_DIR (paths listed above).
       Every file must be owned by root or $REAL_USER, and must not be
       group- or world-writable. Fix them and re-run."
    fi
}

# -- pre-flight checks ---------------------------------------------------------

[ "$EUID" -eq 0 ] || die "Run with sudo: sudo ./install.sh"

REAL_USER="${SUDO_USER:-}"
[ -n "$REAL_USER" ] || die "Could not detect the invoking user. Run with sudo, not as root directly."
[ "$REAL_USER" != "root" ] || die "Do not run as root directly. Use: sudo ./install.sh"

REAL_UID=$(id -u "$REAL_USER")
REAL_HOME=$(getent passwd "$REAL_USER" | cut -d: -f6)
XDG_RT="/run/user/$REAL_UID"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_PREFIX="/usr/local"
BUILD_DIR="$SCRIPT_DIR/build-release"
TOTAL_STEPS=4

# An older version of this installer built as root, leaving a root-owned
# build-release/ that an unprivileged cmake cannot write. Discard it: a stale
# CMakeCache.txt from a root-context configure is not something to reuse anyway.
# This runs before validate_tree so a doomed directory cannot fail the check.
if [ -d "$BUILD_DIR" ] && [ "$(stat -c %u "$BUILD_DIR")" != "$REAL_UID" ]; then
    warn "Removing $BUILD_DIR left by a previous root build"
    rm -rf "$BUILD_DIR"
fi

validate_tree

echo -e "${BOLD}=== Muslimtify Installer ===${NC}"
echo "Installing for user: $REAL_USER"
echo "Install prefix:      $INSTALL_PREFIX"

# -- step 1: release build -----------------------------------------------------

step 1 "Building in release mode..."

# Configure and build unprivileged. Only the install step below needs root.
run_as_user cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
    --log-level=WARNING

run_as_user cmake --build "$BUILD_DIR" --parallel "$(nproc)"
ok "Build complete → $BUILD_DIR/bin/muslimtify"

# -- step 2: install binary and icons -----------------------------------------

step 2 "Installing binary and icons to $INSTALL_PREFIX..."

cmake --install "$BUILD_DIR"
ok "Binary installed to $INSTALL_PREFIX/bin/muslimtify"
ok "Icons installed to $INSTALL_PREFIX/share/"

# -- step 3: write systemd unit files -----------------------------------------

step 3 "Creating systemd user service for $REAL_USER..."

SYSTEMD_DIR="$REAL_HOME/.config/systemd/user"
BINARY_PATH="$INSTALL_PREFIX/bin/muslimtify"

mkdir -p "$SYSTEMD_DIR"
chown "$REAL_USER" "$SYSTEMD_DIR"

sed "s|@CMAKE_INSTALL_FULL_BINDIR@|$INSTALL_PREFIX/bin|" \
    "$SCRIPT_DIR/systemd/muslimtify.service.in" > "$SYSTEMD_DIR/muslimtify.service"
chown "$REAL_USER" "$SYSTEMD_DIR/muslimtify.service"
ok "Created $SYSTEMD_DIR/muslimtify.service"

# Remove a stale timer left by an older (timer-based) source install.
rm -f "$INSTALL_PREFIX/lib/systemd/user/muslimtify.timer"

# -- step 4: enable and start service -----------------------------------------

step 4 "Enabling systemd service..."

if [ ! -d "$XDG_RT" ]; then
    warn "XDG_RUNTIME_DIR $XDG_RT not found — user session may not be active."
    warn "After logging in run: systemctl --user enable --now muslimtify.service"
else
    run_as_user systemctl --user daemon-reload
    run_as_user systemctl --user disable --now muslimtify.timer 2>/dev/null || true
    run_as_user systemctl --user enable --now muslimtify.service
    ok "Service enabled and started"
fi

# -- done ---------------------------------------------------------------------

echo ""
echo -e "${GREEN}${BOLD}=== Installation complete! ===${NC}"
echo ""
echo "  Binary:  $INSTALL_PREFIX/bin/muslimtify"
echo "  Config:  $REAL_HOME/.config/muslimtify/config.json (created on first run)"
echo ""
echo "Quick start:"
echo "  muslimtify                   # show today's prayer times"
echo "  muslimtify location auto     # auto-detect location"
echo "  muslimtify next              # time until next prayer"
echo ""
echo "Systemd:"
echo "  systemctl --user status muslimtify.service"
echo "  journalctl --user -u muslimtify -f"
echo ""
echo "To uninstall:"
echo "  sudo ./uninstall.sh"
