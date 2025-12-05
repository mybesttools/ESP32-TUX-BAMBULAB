#!/bin/bash

# ESP32-TUX Monitor Script
# Starts the serial monitor with optional log filtering
# Usage: ./start_monitor.sh [port] [filter_keyword]
# Example: ./start_monitor.sh /dev/cu.SLAB_USBtoUART Discovery

PORT=${1:-/dev/cu.SLAB_USBtoUART}
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
echo "Press Ctrl+C to exit"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Source ESP-IDF environment
source "$HOME/esp-idf/export.sh" > /dev/null 2>&1

if [ -n "$FILTER" ]; then
    idf.py -p "$PORT" -b "$BAUD" monitor | grep -E "$FILTER|error|ERROR|failed|FAILED"
else
    idf.py -p "$PORT" -b "$BAUD" monitor
fi
