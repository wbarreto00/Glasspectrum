#!/bin/bash
# ═══════════════════════════════════════════════════════════════
# Glasspectrum — macOS .pkg Installer Builder
# Creates a native macOS installer package that users double-click.
# ═══════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# --- Configuration ---
BUNDLE_NAME="Glasspectrum.ofx.bundle"
PKG_IDENTIFIER="com.glasspectrum.lensemulator"
PKG_VERSION="${1:-1.0.0}"
INSTALL_LOCATION="/Library/OFX/Plugins"

# --- Locate the bundle ---
# Try common build locations
if [ -n "${BUNDLE_PATH:-}" ] && [ -d "$BUNDLE_PATH" ]; then
    BUNDLE_SRC="$BUNDLE_PATH"
elif [ -d "$PROJECT_ROOT/$BUNDLE_NAME" ]; then
    BUNDLE_SRC="$PROJECT_ROOT/$BUNDLE_NAME"
elif [ -d "$PROJECT_ROOT/build2/$BUNDLE_NAME" ]; then
    BUNDLE_SRC="$PROJECT_ROOT/build2/$BUNDLE_NAME"
elif [ -d "$PROJECT_ROOT/build/$BUNDLE_NAME" ]; then
    BUNDLE_SRC="$PROJECT_ROOT/build/$BUNDLE_NAME"
else
    echo "✗ Error: $BUNDLE_NAME not found."
    echo "  Set BUNDLE_PATH or place the bundle in the project root."
    exit 1
fi

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║   Glasspectrum — macOS .pkg Builder      ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""
echo "  Bundle:  $BUNDLE_SRC"
echo "  Version: $PKG_VERSION"
echo ""

# --- Stage the payload ---
STAGING_DIR=$(mktemp -d)
SCRIPTS_DIR=$(mktemp -d)
trap "rm -rf '$STAGING_DIR' '$SCRIPTS_DIR'" EXIT

mkdir -p "$STAGING_DIR/$INSTALL_LOCATION"
cp -R "$BUNDLE_SRC" "$STAGING_DIR/$INSTALL_LOCATION/$BUNDLE_NAME"

# --- Create postinstall script ---
cat > "$SCRIPTS_DIR/postinstall" << 'EOF'
#!/bin/bash
# 1. Clear quarantine
xattr -rc "$2/Library/OFX/Plugins/Glasspectrum.ofx.bundle" || true
# 2. Re-sign the binary locally (Apple Silicon AMFI workaround for Hardened Runtime)
codesign --force --deep --sign - "$2/Library/OFX/Plugins/Glasspectrum.ofx.bundle" || true
exit 0
EOF
chmod +x "$SCRIPTS_DIR/postinstall"

# --- Build the .pkg ---
OUTPUT_NAME="Glasspectrum-v${PKG_VERSION}-macOS.pkg"
OUTPUT_PATH="$PROJECT_ROOT/$OUTPUT_NAME"

pkgbuild \
    --root "$STAGING_DIR" \
    --scripts "$SCRIPTS_DIR" \
    --identifier "$PKG_IDENTIFIER" \
    --version "$PKG_VERSION" \
    --install-location "/" \
    "$OUTPUT_PATH"

echo ""
echo "  ✅ Package created: $OUTPUT_PATH"
echo "     Double-click to install, then restart DaVinci Resolve."
echo ""
