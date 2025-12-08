#!/bin/bash
#
# Quick flash without erasing SPIFFS
# This preserves your settings across flashes
#

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

echo "Using port: $PORT"

# Source ESP-IDF if needed
if [ -z "$IDF_PATH" ]; then
    . ~/esp-idf/export.sh
fi

echo "Flashing ESP32-TUX (preserving SPIFFS settings)..."
python3 "$IDF_PATH/components/esptool_py/esptool/esptool.py" \
    -p "$PORT" \
    -b 460800 \
    --before default_reset \
    --after hard_reset \
    --chip esp32s3 \
    write_flash \
    --flash_mode dio \
    --flash_size 8MB \
    --flash_freq 80m \
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0x10000 build/ESP32-TUX.bin

echo "✓ Flash complete! Settings preserved."
