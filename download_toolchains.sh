#!/bin/bash
# Manual ESP32/ESP32-S3 toolchain download using curl
# Bypasses Python SSL issues

set -e

DIST_DIR="$HOME/.espressif/dist"
TOOLS_DIR="$HOME/.espressif/tools"

mkdir -p "$DIST_DIR"
mkdir -p "$TOOLS_DIR"

echo "Downloading ESP32 and ESP32-S3 toolchains..."
echo ""

# ESP32 toolchain
URL1="https://github.com/espressif/crosstool-NG/releases/download/esp-2022r1-RC1/xtensa-esp32-elf-gcc11_2_0-esp-2022r1-RC1-macos-arm64.tar.xz"
FILE1="xtensa-esp32-elf-gcc11_2_0-esp-2022r1-RC1-macos-arm64.tar.xz"

if [ ! -f "$DIST_DIR/$FILE1" ]; then
    echo "Downloading ESP32 toolchain..."
    curl -L "$URL1" -o "$DIST_DIR/$FILE1"
    echo "✓ Downloaded"
else
    echo "✓ ESP32 toolchain already downloaded"
fi

# ESP32-S3 toolchain  
URL2="https://github.com/espressif/crosstool-NG/releases/download/esp-2022r1-RC1/xtensa-esp32s3-elf-gcc11_2_0-esp-2022r1-RC1-macos-arm64.tar.xz"
FILE2="xtensa-esp32s3-elf-gcc11_2_0-esp-2022r1-RC1-macos-arm64.tar.xz"

if [ ! -f "$DIST_DIR/$FILE2" ]; then
    echo "Downloading ESP32-S3 toolchain..."
    curl -L "$URL2" -o "$DIST_DIR/$FILE2"
    echo "✓ Downloaded"
else
    echo "✓ ESP32-S3 toolchain already downloaded"
fi

echo ""
echo "Extracting toolchains..."

if [ ! -d "$TOOLS_DIR/xtensa-esp32-elf" ]; then
    tar -xf "$DIST_DIR/$FILE1" -C "$TOOLS_DIR/"
    echo "✓ ESP32 extracted"
else
    echo "✓ ESP32 already extracted"
fi

if [ ! -d "$TOOLS_DIR/xtensa-esp32s3-elf" ]; then
    tar -xf "$DIST_DIR/$FILE2" -C "$TOOLS_DIR/"
    echo "✓ ESP32-S3 extracted"
else
    echo "✓ ESP32-S3 already extracted"
fi

echo ""
echo "✓ Toolchains ready!"
echo ""
echo "Now try building:"
echo "  cd ~/esp-idf && . ./export.sh"
echo "  cd /Users/mike/source/ESP32-TUX-BAMBULAB"
echo "  idf.py build"
