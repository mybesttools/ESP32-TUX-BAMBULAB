#!/bin/bash
# Build and package firmware for web installer
# Usage: ./scripts/build_webinstaller.sh [device]
# Devices: wt32-sc01-plus, wt32-sc01, makerfabs-s3, all

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
WEBINSTALLER_DIR="$PROJECT_DIR/webinstaller"
FIRMWARE_DIR="$WEBINSTALLER_DIR/firmware"

# Source ESP-IDF
if [ -f "$HOME/esp-idf/export.sh" ]; then
    . "$HOME/esp-idf/export.sh"
else
    echo "Error: ESP-IDF not found at ~/esp-idf"
    exit 1
fi

cd "$PROJECT_DIR"

# Device configurations
# Format: DEVICE_NAME:TARGET:CONFIG_DEFINE:FLASH_SIZE
declare -A DEVICES=(
    ["wt32-sc01-plus"]="esp32s3:CONFIG_TUX_DEVICE_WT32_SC01_PLUS:8MB"
    ["wt32-sc01"]="esp32:CONFIG_TUX_DEVICE_WT32_SC01:4MB"
    ["makerfabs-s3"]="esp32s3:CONFIG_TUX_DEVICE_MAKERFABS_S3_PARA:16MB"
)

build_device() {
    local device=$1
    local config=${DEVICES[$device]}
    
    if [ -z "$config" ]; then
        echo "Unknown device: $device"
        echo "Available: ${!DEVICES[@]}"
        return 1
    fi
    
    IFS=':' read -r target config_define flash_size <<< "$config"
    
    echo "========================================"
    echo "Building for: $device"
    echo "Target: $target"
    echo "Flash: $flash_size"
    echo "========================================"
    
    # Set target
    idf.py set-target "$target"
    
    # Configure device via sdkconfig
    # This creates a minimal sdkconfig with the device selection
    if [ -f "sdkconfig" ]; then
        # Update existing config
        sed -i.bak "s/CONFIG_TUX_DEVICE_.*=y/$config_define=y/" sdkconfig 2>/dev/null || true
    fi
    
    # Build
    idf.py build
    
    # Create output directory
    local output_dir="$FIRMWARE_DIR/$device"
    mkdir -p "$output_dir"
    
    # Copy firmware files
    echo "Copying firmware to $output_dir..."
    cp build/bootloader/bootloader.bin "$output_dir/"
    cp build/partition_table/partition-table.bin "$output_dir/"
    cp build/ota_data_initial.bin "$output_dir/"
    cp build/ESP32-TUX.bin "$output_dir/"
    cp build/storage.bin "$output_dir/"
    
    echo "✅ Built $device successfully"
    echo ""
}

update_manifests() {
    echo "Updating manifest versions..."
    local version=$(cat "$PROJECT_DIR/version.txt" 2>/dev/null || echo "1.0.0")
    
    for manifest in "$WEBINSTALLER_DIR"/manifest-*.json; do
        if [ -f "$manifest" ]; then
            # Update version in manifest using sed (portable)
            sed -i.bak "s/\"version\": \"[^\"]*\"/\"version\": \"$version\"/" "$manifest"
            rm -f "${manifest}.bak"
            echo "Updated: $(basename "$manifest") -> v$version"
        fi
    done
    
    # Copy version.txt to webinstaller
    cp "$PROJECT_DIR/version.txt" "$WEBINSTALLER_DIR/" 2>/dev/null || echo "1.0.0" > "$WEBINSTALLER_DIR/version.txt"
}

# Main
device="${1:-all}"

if [ "$device" = "all" ]; then
    echo "Building all devices..."
    for d in "${!DEVICES[@]}"; do
        build_device "$d"
    done
else
    build_device "$device"
fi

update_manifests

echo "========================================"
echo "🎉 Web installer firmware ready!"
echo "Files in: $FIRMWARE_DIR"
echo ""
echo "To test locally:"
echo "  cd webinstaller && python3 -m http.server 8080"
echo "  Open: http://localhost:8080"
echo ""
echo "To deploy: Upload webinstaller/ folder to your web hosting"
echo "========================================"
