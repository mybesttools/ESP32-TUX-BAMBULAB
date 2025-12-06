#!/bin/bash
#
# Build and Flash ESP32-TUX-BAMBULAB
# This script builds the project and flashes it to your device
#

set -e

PORT=${1:-/dev/cu.usbserial-0251757F}
BAUD=${2:-460800}

echo "╔════════════════════════════════════════╗"
echo "║   ESP32-TUX Build & Flash              ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Check if ESP-IDF is installed
if [ ! -d "$HOME/esp-idf" ]; then
    echo "❌ ESP-IDF not found at $HOME/esp-idf"
    echo ""
    echo "Please run the installation script first:"
    echo "  ./setup_idf.sh"
    exit 1
fi

# Source ESP-IDF environment
echo "⚙️  Loading ESP-IDF environment..."
. ~/esp-idf/export.sh

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo "❌ Port $PORT not found"
    echo ""
    echo "Available ports:"
    ls -1 /dev/cu.* 2>/dev/null | grep -E "usbserial|SLAB" || echo "  (none found)"
    echo ""
    echo "Usage: $0 [port] [baud]"
    echo "Example: $0 /dev/cu.usbserial-0251757F 460800"
    exit 1
fi

echo "✓ ESP-IDF ready"
echo ""
echo "🔨 Building project..."
idf.py build

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Build failed!"
    exit 1
fi

echo ""
echo "📡 Flashing to $PORT at ${BAUD} baud..."
idf.py -p "$PORT" -b "$BAUD" flash

if [ $? -ne 0 ]; then
    echo ""
    echo "❌ Flash failed!"
    exit 1
fi

echo ""
echo "╔════════════════════════════════════════╗"
echo "║   ✓ Build & Flash Complete!            ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "To monitor serial output:"
echo "  ./monitor_simple.sh $PORT"
echo ""
echo "Or use idf.py monitor:"
echo "  . ~/esp-idf/export.sh"
echo "  idf.py -p $PORT monitor"
echo ""
