#!/bin/bash

# ESP32-TUX Monitor Script
# Starts the serial monitor with optional log filtering
# Usage: ./start_monitor.sh [port] [filter_keyword]
# Example: ./start_monitor.sh /dev/cu.SLAB_USBtoUART Discovery

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
FILTER=${2:-}
BAUD=${3:-115200}

echo "╔════════════════════════════════════════╗"
echo "║   ESP32-TUX Serial Monitor             ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Port:     $PORT"
echo "Baud:     $BAUD"
[ -n "$FILTER" ] && echo "Filter:   $FILTER" || echo "Filter:   (none)"
echo ""

# Check if ESP-IDF is installed
if [ ! -d "$HOME/esp-idf" ]; then
    echo "❌ ESP-IDF not found at $HOME/esp-idf"
    echo ""
    echo "Please run the installation script first:"
    echo "  ./setup_idf.sh"
    echo ""
    echo "Or wait for the current installation to complete."
    exit 1
fi

# Source ESP-IDF environment
if [ -f "$HOME/esp-idf/export.sh" ]; then
    echo "Loading ESP-IDF environment..."
    source "$HOME/esp-idf/export.sh" > /dev/null 2>&1
else
    echo "❌ ESP-IDF export.sh not found"
    exit 1
fi

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo "❌ Port $PORT not found"
    echo ""
    echo "Available ports:"
    ls -1 /dev/cu.* 2>/dev/null || echo "  (none found)"
    echo ""
    exit 1
fi

echo "Press Ctrl+C to exit"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if [ -n "$FILTER" ]; then
    idf.py -p "$PORT" -b "$BAUD" monitor | grep -E "$FILTER|error|ERROR|failed|FAILED"
else
    idf.py -p "$PORT" -b "$BAUD" monitor
fi
