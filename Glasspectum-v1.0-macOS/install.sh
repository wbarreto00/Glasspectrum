#!/bin/bash
# Glasspectum OFX Plugin Installer for macOS
# Installs to /Library/OFX/Plugins/ (requires admin password)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUNDLE_NAME="Glasspectum.ofx.bundle"
BUNDLE_SRC="$SCRIPT_DIR/$BUNDLE_NAME"
INSTALL_DIR="/Library/OFX/Plugins"

echo "╔══════════════════════════════════════════╗"
echo "║     Glasspectum OFX Plugin Installer     ║"
echo "║     Lens Emulation for DaVinci Resolve   ║"
echo "╚══════════════════════════════════════════╝"
echo ""

if [ ! -d "$BUNDLE_SRC" ]; then
    echo "Error: $BUNDLE_NAME not found in $(pwd)"
    echo "Make sure install.sh is in the same folder as the bundle."
    exit 1
fi

echo "Installing to: $INSTALL_DIR/$BUNDLE_NAME"
echo ""

# Remove old installation if present
if [ -d "$INSTALL_DIR/$BUNDLE_NAME" ]; then
    echo "Removing previous installation..."
    sudo rm -rf "$INSTALL_DIR/$BUNDLE_NAME"
fi

# Create OFX directory if needed
sudo mkdir -p "$INSTALL_DIR"

# Copy bundle
sudo cp -R "$BUNDLE_SRC" "$INSTALL_DIR/$BUNDLE_NAME"

echo ""
echo "✅ Glasspectum installed successfully!"
echo ""
echo "Next steps:"
echo "  1. Open (or restart) DaVinci Resolve"
echo "  2. Go to Color page or Edit page"
echo "  3. Open OpenFX panel"
echo "  4. Find 'Glasspectum' in the effects list"
echo ""
echo "To uninstall:"
echo "  sudo rm -rf '$INSTALL_DIR/$BUNDLE_NAME'"
echo ""
