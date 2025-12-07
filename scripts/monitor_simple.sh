#!/bin/bash

# Simple Serial Monitor (no ESP-IDF required)
# Uses screen to monitor serial output
# Usage: ./monitor_simple.sh [port] [baud]

PORT=${1:-/dev/cu.usbmodem1401}
BAUD=${2:-115200}

echo "╔════════════════════════════════════════╗"
echo "║   ESP32-TUX Simple Monitor             ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Port:     $PORT"
echo "Baud:     $BAUD"
echo ""

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo "❌ Port $PORT not found"
    echo ""
    echo "Available ports:"
    ls -1 /dev/cu.* 2>/dev/null || echo "  (none found)"
    echo ""
    echo "Common ESP32 ports on macOS:"
    echo "  /dev/cu.SLAB_USBtoUART    (CP2102 USB-to-UART)"
    echo "  /dev/cu.usbserial-*       (Generic USB serial)"
    echo "  /dev/cu.wchusbserial*     (CH340 USB serial)"
    echo ""
    exit 1
fi

echo "Press Ctrl+A then K to exit, or Ctrl+C"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Use screen for serial monitoring
screen "$PORT" "$BAUD"
