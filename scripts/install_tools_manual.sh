#!/bin/bash
#
# Minimal ESP-IDF setup - manually download required tools
# Workaround for SSL certificate issues
#

set -e

echo "╔════════════════════════════════════════╗"
echo "║   ESP-IDF Manual Tool Installation     ║"
echo "╚════════════════════════════════════════╝"
echo ""

ESPRESSIF_DIR="$HOME/.espressif"
DIST_DIR="$ESPRESSIF_DIR/dist"
TOOLS_DIR="$ESPRESSIF_DIR/tools"

mkdir -p "$DIST_DIR"
mkdir -p "$TOOLS_DIR"

# Detect platform
PLATFORM="aarch64-apple-darwin21.1"

echo "Platform: macOS ARM64"
echo "Installing to: $ESPRESSIF_DIR"
echo ""

# Use curl with --insecure to bypass SSL issues
CURL_CMD="curl -L -k --progress-bar"

# ESP32 Toolchain
TOOL1_NAME="xtensa-esp32-elf-gcc11_2_0-esp-2022r1-RC1-macos-arm64.tar.xz"
TOOL1_URL="https://github.com/espressif/crosstool-NG/releases/download/esp-2022r1-RC1/$TOOL1_NAME"

# ESP32-S3 Toolchain
TOOL2_NAME="xtensa-esp32s3-elf-gcc11_2_0-esp-2022r1-RC1-macos-arm64.tar.xz"
TOOL2_URL="https://github.com/espressif/crosstool-NG/releases/download/esp-2022r1-RC1/$TOOL2_NAME"

# Download and install ESP32 toolchain
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Installing: xtensa-esp32-elf"

if [ ! -f "$DIST_DIR/$TOOL1_NAME" ]; then
    echo "Downloading $TOOL1_NAME..."
    $CURL_CMD "$TOOL1_URL" -o "$DIST_DIR/$TOOL1_NAME"
else
    echo "✓ Already downloaded"
fi

if [ ! -d "$TOOLS_DIR/xtensa-esp32-elf" ]; then
    echo "Extracting..."
    tar -xf "$DIST_DIR/$TOOL1_NAME" -C "$TOOLS_DIR/"
    echo "✓ Extracted"
else
    echo "✓ Already installed"
fi
echo ""

# Download and install ESP32-S3 toolchain
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Installing: xtensa-esp32s3-elf"

if [ ! -f "$DIST_DIR/$TOOL2_NAME" ]; then
    echo "Downloading $TOOL2_NAME..."
    $CURL_CMD "$TOOL2_URL" -o "$DIST_DIR/$TOOL2_NAME"
else
    echo "✓ Already downloaded"
fi

if [ ! -d "$TOOLS_DIR/xtensa-esp32s3-elf" ]; then
    echo "Extracting..."
    tar -xf "$DIST_DIR/$TOOL2_NAME" -C "$TOOLS_DIR/"
    echo "✓ Extracted"
else
    echo "✓ Already installed"
fi
echo ""

echo "╔════════════════════════════════════════╗"
echo "║   ✓ Tools Installed!                   ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Next steps:"
echo "1. Source ESP-IDF environment:"
echo "   . ~/esp-idf/export.sh"
echo ""
echo "2. Build your project:"
echo "   cd /Users/mike/source/ESP32-TUX-BAMBULAB"
echo "   idf.py build"
echo ""
