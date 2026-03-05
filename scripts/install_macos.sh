#!/bin/bash
# Glasspectrum OFX Plugin Installer — macOS
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUNDLE="$SCRIPT_DIR/Glasspectrum.ofx.bundle"
DEST="/Library/OFX/Plugins"

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║     Glasspectrum — macOS Installer        ║"
echo "  ║     Lens Emulation for DaVinci Resolve   ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""

if [ ! -d "$BUNDLE" ]; then
    echo "  ✗ Error: Glasspectrum.ofx.bundle not found."
    echo "    Make sure this script is next to the bundle folder."
    exit 1
fi

echo "  Installing to $DEST ..."
[ -d "$DEST/Glasspectrum.ofx.bundle" ] && sudo rm -rf "$DEST/Glasspectrum.ofx.bundle"
sudo mkdir -p "$DEST"
sudo cp -R "$BUNDLE" "$DEST/Glasspectrum.ofx.bundle"

echo ""
echo "  ✅ Installed! Restart DaVinci Resolve to use Glasspectrum."
echo ""
