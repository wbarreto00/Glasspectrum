#!/bin/bash
# Glasspectum OFX Plugin Installer — Linux
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUNDLE="$SCRIPT_DIR/Glasspectum.ofx.bundle"
DEST="/usr/OFX/Plugins"

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║     Glasspectum — Linux Installer        ║"
echo "  ║     Lens Emulation for DaVinci Resolve   ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""

if [ ! -d "$BUNDLE" ]; then
    echo "  ✗ Error: Glasspectum.ofx.bundle not found."
    echo "    Make sure this script is next to the bundle folder."
    exit 1
fi

echo "  Installing to $DEST ..."
[ -d "$DEST/Glasspectum.ofx.bundle" ] && sudo rm -rf "$DEST/Glasspectum.ofx.bundle"
sudo mkdir -p "$DEST"
sudo cp -R "$BUNDLE" "$DEST/Glasspectum.ofx.bundle"

echo ""
echo "  ✅ Installed! Restart DaVinci Resolve to use Glasspectum."
echo ""
