#!/bin/bash
#
# Flash firmware while preserving SPIFFS settings
# Usage: ./flash_preserve_settings.sh [port]
#

PORT=${1:-/dev/cu.SLAB_USBtoUART}
BAUD_RATE=460800

echo "================================"
echo "ESP32-TUX Flash with Settings Preservation"
echo "================================"
echo ""
echo "Port: $PORT"
echo "Baud Rate: $BAUD_RATE"
echo ""

# Check if idf.py is available
if ! command -v idf.py &> /dev/null; then
    echo "Error: idf.py not found. Please source ESP-IDF export.sh first:"
    echo "  . ~/esp-idf/export.sh"
    exit 1
fi

# Backup SPIFFS partition
echo "[1/3] Backing up SPIFFS settings from device..."
BACKUP_FILE="spiffs_backup_$(date +%s).bin"
python3 "$IDF_PATH/components/esptool_py/esptool/esptool.py" \
    -p "$PORT" \
    -b "$BAUD_RATE" \
    read_flash 0x260000 0x180000 "$BACKUP_FILE"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to backup SPIFFS!"
    exit 1
fi

echo "✓ Backed up to: $BACKUP_FILE"
echo ""

# Build project
echo "[2/3] Building project..."
idf.py build
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed!"
    exit 1
fi

echo "✓ Build successful"
echo ""

# Flash app and bootloader only (preserves SPIFFS)
echo "[3/3] Flashing firmware (preserving settings)..."
python3 "$IDF_PATH/components/esptool_py/esptool/esptool.py" \
    -p "$PORT" \
    -b "$BAUD_RATE" \
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

if [ $? -ne 0 ]; then
    echo "ERROR: Flash failed! Restoring from backup..."
    python3 "$IDF_PATH/components/esptool_py/esptool/esptool.py" \
        -p "$PORT" \
        -b "$BAUD_RATE" \
        write_flash 0x260000 "$BACKUP_FILE"
    exit 1
fi

echo "✓ Firmware flashed successfully"
echo ""

# Optionally restore SPIFFS if needed
read -p "Settings should be preserved. Restore from backup? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Restoring SPIFFS from backup..."
    python3 "$IDF_PATH/components/esptool_py/esptool/esptool.py" \
        -p "$PORT" \
        -b "$BAUD_RATE" \
        write_flash 0x260000 "$BACKUP_FILE"
    echo "✓ SPIFFS restored"
fi

echo ""
echo "================================"
echo "Flash complete!"
echo "Backup saved as: $BACKUP_FILE"
echo "================================"
