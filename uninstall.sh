#!/bin/bash
# Muslimtify — full uninstaller
# Stops the systemd timer, removes the binary and icons.
# Config file is preserved unless --purge is passed.
#
# Usage:
#   sudo ./uninstall.sh           # remove everything except config
#   sudo ./uninstall.sh --purge   # also remove ~/.config/muslimtify

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

# -- helpers -------------------------------------------------------------------

step() { echo -e "\n${BLUE}${BOLD}[$1/$TOTAL_STEPS] $2${NC}"; }
ok()   { echo -e "${GREEN}✓${NC} $1"; }
warn() { echo -e "${YELLOW}!${NC} $1"; }
die()  { echo -e "${RED}Error: $1${NC}" >&2; exit 1; }
skip() { echo -e "  (skipped — $1)"; }

run_as_user() {
    sudo -u "$REAL_USER" \
        XDG_RUNTIME_DIR="$XDG_RT" \
        DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RT/bus" \
        "$@"
}

# -- pre-flight checks ---------------------------------------------------------

[ "$EUID" -eq 0 ] || die "Run with sudo: sudo ./uninstall.sh"

REAL_USER="${SUDO_USER:-}"
[ -n "$REAL_USER" ] || die "Could not detect the invoking user. Run with sudo, not as root directly."
[ "$REAL_USER" != "root" ] || die "Do not run as root directly. Use: sudo ./uninstall.sh"

REAL_UID=$(id -u "$REAL_USER")
REAL_HOME=$(getent passwd "$REAL_USER" | cut -d: -f6)
XDG_RT="/run/user/$REAL_UID"

INSTALL_PREFIX="/usr/local"
PURGE=false
[ "${1:-}" = "--purge" ] && PURGE=true

TOTAL_STEPS=3

echo -e "${BOLD}=== Muslimtify Uninstaller ===${NC}"
echo "Removing installation for user: $REAL_USER"
$PURGE && echo -e "${YELLOW}--purge: config directory will also be removed${NC}"

# -- step 1: stop and remove systemd units ------------------------------------

step 1 "Stopping and removing systemd units..."

SYSTEMD_DIR="$REAL_HOME/.config/systemd/user"

if [ -d "$XDG_RT" ]; then
    if run_as_user systemctl --user is-active --quiet muslimtify.service 2>/dev/null; then
        run_as_user systemctl --user stop muslimtify.service
        ok "Stopped muslimtify.service"
    else
        skip "service not running"
    fi

    if run_as_user systemctl --user is-enabled --quiet muslimtify.service 2>/dev/null; then
        run_as_user systemctl --user disable muslimtify.service
        ok "Disabled muslimtify.service"
    else
        skip "service not enabled"
    fi
else
    warn "User session not active — skipping systemctl commands"
fi

for unit in muslimtify.service muslimtify.timer; do
    if [ -f "$SYSTEMD_DIR/$unit" ]; then
        rm "$SYSTEMD_DIR/$unit"
        ok "Removed $SYSTEMD_DIR/$unit"
    else
        skip "$unit not found"
    fi
done

if [ -d "$XDG_RT" ]; then
    run_as_user systemctl --user daemon-reload
    ok "Reloaded systemd"
fi

# -- step 2: remove binary and icons ------------------------------------------

step 2 "Removing binary and icons from $INSTALL_PREFIX..."

BINARY="$INSTALL_PREFIX/bin/muslimtify"
if [ -f "$BINARY" ]; then
    rm "$BINARY"
    ok "Removed $BINARY"
else
    skip "binary not found at $BINARY"
fi

for icon in \
    "$INSTALL_PREFIX/share/icons/hicolor/128x128/apps/muslimtify.png" \
    "$INSTALL_PREFIX/share/pixmaps/muslimtify.png"; do
    if [ -f "$icon" ]; then
        rm "$icon"
        ok "Removed $icon"
    fi
done

# -- step 3: config directory --------------------------------------------------

step 3 "Handling config files..."

CONFIG_DIR="$REAL_HOME/.config/muslimtify"
if [ -d "$CONFIG_DIR" ]; then
    if $PURGE; then
        rm -rf "$CONFIG_DIR"
        ok "Removed $CONFIG_DIR"
    else
        warn "Config preserved at $CONFIG_DIR"
        echo "     Run with --purge to also remove it:"
        echo "     sudo ./uninstall.sh --purge"
    fi
else
    skip "config directory not found"
fi

# -- done ----------------------------------------------------------------------

echo ""
echo -e "${GREEN}${BOLD}=== Uninstallation complete! ===${NC}"
echo ""

# -- manual uninstall reference ------------------------------------------------

cat <<'MANUAL'
-----------------------------------------------------
Manual uninstall reference (if you prefer doing it yourself):

  # 1. Stop and disable the timer
  systemctl --user stop    muslimtify.timer
  systemctl --user disable muslimtify.timer

  # 2. Remove systemd unit files
  rm -f ~/.config/systemd/user/muslimtify.service
  rm -f ~/.config/systemd/user/muslimtify.timer
  systemctl --user daemon-reload

  # 3. Remove the binary (requires sudo)
  sudo rm -f /usr/local/bin/muslimtify

  # 4. Remove icons (requires sudo)
  sudo rm -f /usr/local/share/icons/hicolor/128x128/apps/muslimtify.png
  sudo rm -f /usr/local/share/pixmaps/muslimtify.png

  # 5. Remove config (optional)
  rm -rf ~/.config/muslimtify
-----------------------------------------------------
MANUAL
