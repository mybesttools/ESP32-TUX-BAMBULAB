#!/bin/bash
#
# Quick flash without erasing SPIFFS
# This preserves your settings across flashes
#

PORT=${1:-/dev/cu.SLAB_USBtoUART}

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
    --chip esp32 \
    write_flash \
    --flash_mode dio \
    --flash_size 4MB \
    --flash_freq 40m \
    0x1000 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0x10000 build/ESP32-TUX.bin

echo "✓ Flash complete! Settings preserved."
