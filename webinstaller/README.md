# ESP32-TUX Web Installer

Browser-based firmware installer for ESP32-TUX BambuLab Monitor. No software installation required!

## For End Users

### Requirements

- **Browser**: Google Chrome or Microsoft Edge (desktop only)
- **USB Cable**: Data cable (not charge-only)
- **Device**: Supported ESP32 display (WT32-SC01, WT32-SC01 Plus, or Makerfabs)

### Installation Steps

1. Connect your ESP32 device to your computer via USB
2. Visit the web installer page (hosted version or local)
3. Select your device type
4. Click "Install Firmware"
5. Select the serial port when prompted
6. Wait 2-3 minutes for installation
7. After reboot, connect to "ESP32-TUX" WiFi to configure

### Troubleshooting

**"No serial port found"**
- Try a different USB cable (must be data cable, not charge-only)
- Install USB drivers:
  - [CP210x Driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) (most common)
  - [CH340 Driver](https://www.wch-ic.com/downloads/CH341SER_ZIP.html)

**"Failed to connect"**
- Hold the BOOT button while clicking Install
- Try unplugging and replugging the device

**Browser not supported**
- Use Chrome or Edge on desktop (not mobile, not Firefox/Safari)

## For Developers

### Building Firmware for Web Installer

```bash
# Build for all devices
./scripts/build_webinstaller.sh all

# Build for specific device
./scripts/build_webinstaller.sh wt32-sc01-plus
```

### Testing Locally

```bash
cd webinstaller
python3 -m http.server 8080
# Open http://localhost:8080 in Chrome
```

### Deploying

Upload the entire `webinstaller/` folder to any static web hosting:
- GitHub Pages
- Netlify
- Vercel
- Any web server

### File Structure

```
webinstaller/
├── index.html                    # Main installer page
├── manifest-wt32-sc01-plus.json  # ESP32-S3 8MB device manifest
├── manifest-wt32-sc01.json       # ESP32 4MB device manifest
├── manifest-makerfabs-s3.json    # ESP32-S3 16MB device manifest
├── version.txt                   # Current firmware version
└── firmware/
    ├── wt32-sc01-plus/
    │   ├── bootloader.bin
    │   ├── partition-table.bin
    │   ├── ota_data_initial.bin
    │   ├── ESP32-TUX.bin
    │   └── storage.bin
    ├── wt32-sc01/
    │   └── ... (same files)
    └── makerfabs-s3/
        └── ... (same files)
```

### Manifest Format

Each device needs a manifest JSON file that tells ESP Web Tools how to flash:

```json
{
  "name": "ESP32-TUX BambuLab Monitor",
  "version": "1.0.0",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "firmware/device/bootloader.bin", "offset": 0 },
        { "path": "firmware/device/partition-table.bin", "offset": 32768 },
        { "path": "firmware/device/ota_data_initial.bin", "offset": 53248 },
        { "path": "firmware/device/ESP32-TUX.bin", "offset": 65536 },
        { "path": "firmware/device/storage.bin", "offset": 6553600 }
      ]
    }
  ]
}
```

**Important offsets:**
- ESP32: bootloader at `0x1000` (4096)
- ESP32-S3: bootloader at `0x0`
- Partition table: `0x8000` (32768)
- OTA data: `0xd000` (53248)
- App: `0x10000` (65536)
- Storage: varies by partition scheme

## Technology

Powered by [ESP Web Tools](https://esphome.github.io/esp-web-tools/) which uses the WebSerial API available in Chromium-based browsers.
