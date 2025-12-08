#!/bin/bash

# Simple Serial Monitor (no ESP-IDF required)
# Uses screen to monitor serial output
# Usage: ./monitor_simple.sh [port] [baud]

# Auto-detect USB port if not provided
if [ -n "$1" ]; then
    PORT="$1"
elif [ -e /dev/cu.usbmodem21201 ]; then
    PORT="/dev/cu.usbmodem21201"
elif [ -e /dev/cu.usbmodem1101 ]; then
    PORT="/dev/cu.usbmodem1101"
else
    echo "Error: No USB device found. Connect your ESP32 or specify port manually."
    exit 1
fi
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
