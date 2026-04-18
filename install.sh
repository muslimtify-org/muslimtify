#!/bin/bash
# Muslimtify — full installer
# Builds in release mode, installs the binary, and sets up the systemd user timer.
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
        XDG_RUNTIME_DIR="$XDG_RT" \
        DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RT/bus" \
        "$@"
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

echo -e "${BOLD}=== Muslimtify Installer ===${NC}"
echo "Installing for user: $REAL_USER"
echo "Install prefix:      $INSTALL_PREFIX"

# -- step 1: release build -----------------------------------------------------

step 1 "Building in release mode..."

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
    --log-level=WARNING

cmake --build "$BUILD_DIR" --parallel "$(nproc)"
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

cat > "$SYSTEMD_DIR/muslimtify.service" <<EOF
[Unit]
Description=Prayer Time Notification Check
After=network-online.target

[Service]
Type=oneshot
ExecStart=$BINARY_PATH check
StandardOutput=journal
StandardError=journal
EOF

cat > "$SYSTEMD_DIR/muslimtify.timer" <<EOF
[Unit]
Description=Check prayer times every minute
After=network-online.target

[Timer]
OnCalendar=*:*:00
Persistent=true
AccuracySec=1s

[Install]
WantedBy=timers.target
EOF

chown "$REAL_USER" "$SYSTEMD_DIR/muslimtify.service" "$SYSTEMD_DIR/muslimtify.timer"
ok "Created $SYSTEMD_DIR/muslimtify.service"
ok "Created $SYSTEMD_DIR/muslimtify.timer"

# -- step 4: enable and start timer -------------------------------------------

step 4 "Enabling systemd timer..."

if [ ! -d "$XDG_RT" ]; then
    warn "XDG_RUNTIME_DIR $XDG_RT not found — user session may not be active."
    warn "After logging in run: systemctl --user enable --now muslimtify.timer"
else
    run_as_user systemctl --user daemon-reload
    run_as_user systemctl --user enable muslimtify.timer
    run_as_user systemctl --user start  muslimtify.timer
    ok "Timer enabled and started"
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
echo "  systemctl --user status muslimtify.timer"
echo "  journalctl --user -u muslimtify -f"
echo ""
echo "To uninstall:"
echo "  sudo ./uninstall.sh"
