#!/bin/bash
# Trigger a COPR build from the git repository using .copr/Makefile
set -euo pipefail

COPR_PROJECT="rizukirr/muslimtify"
GIT_URL="https://github.com/rizukirr/muslimtify"

# --- Check copr-cli ---
if ! command -v copr-cli &>/dev/null; then
    echo "copr-cli not found. Install it with:"
    echo "  sudo dnf install copr-cli"
    echo "  or: pip install copr-cli"
    echo ""
    echo "Then configure your API token:"
    echo "  Visit https://copr.fedorainfracloud.org/api/"
    echo "  Save the token to ~/.config/copr"
    exit 1
fi

# --- Determine git ref ---
# Default to main so packaging-only changes (Release bumps) build without a new tag.
# Pass an explicit ref to build something else, e.g. ./upload-copr.sh v0.2.4
COMMITTISH="${1:-main}"

echo "==> Triggering COPR build for ${COPR_PROJECT}"
echo "    Source: ${GIT_URL}"
echo "    Ref:    ${COMMITTISH}"
echo ""

read -p "Submit build? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    copr-cli buildscm "$COPR_PROJECT" \
        --clone-url "$GIT_URL" \
        --commit "$COMMITTISH" \
        --method make_srpm \
        --type git
    echo "==> Build submitted! Check status at:"
    echo "    https://copr.fedorainfracloud.org/coprs/${COPR_PROJECT}/builds/"
fi
