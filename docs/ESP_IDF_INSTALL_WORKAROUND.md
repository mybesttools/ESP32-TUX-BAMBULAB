# ESP-IDF Installation - Workaround for SSL Issues

## Problem

The ESP-IDF installation is failing due to Python SSL certificate verification issues when downloading toolchains from GitHub.

## Quick Solution (When You Have Stable Internet)

### Option 1: Fix Python Certificates (Recommended)

```bash
# Run with sudo
sudo "/Applications/Python 3.10/Install Certificates.command"
```

Then retry:
```bash
cd ~/esp-idf
./install.sh esp32,esp32s3
```

### Option 2: Use Pre-Built ESP-IDF (Docker)

If you have Docker installed:

```bash
docker pull espressif/idf:v5.1
docker run --rm -v $PWD:/project -w /project espressif/idf:v5.1 idf.py build
```

### Option 3: Manual Download (Current Workaround)

The toolchains can be manually downloaded and installed:

1. **Download ESP32 Toolchain:**
   - Visit: https://github.com/espressif/crosstool-NG/releases/tag/esp-2022r1-RC1
   - Download: `xtensa-esp32-elf-gcc11_2_0-esp-2022r1-RC1-macos-arm64.tar.xz`
   - Extract to: `~/.espressif/tools/`

2. **Download ESP32-S3 Toolchain:**
   - From same page, download: `xtensa-esp32s3-elf-gcc11_2_0-esp-2022r1-RC1-macos-arm64.tar.xz`
   - Extract to: `~/.espressif/tools/`

3. **Set up environment:**
   ```bash
   . ~/esp-idf/export.sh
   ```

## Current Status

✅ **Code Changes Complete:**
- SSL/TLS fix for Bambu MQTT
- Query mechanism implemented
- Test button added to GUI
- Monitor scripts working

❌ **ESP-IDF Installation Blocked:**
- Python SSL certificate issues
- Can't download toolchains automatically

## What to Test Once ESP-IDF Works

```bash
cd /Users/mike/source/ESP32-TUX-BAMBULAB

# Build
. ~/esp-idf/export.sh
idf.py build

# Flash
idf.py -p /dev/cu.usbserial-0251757F flash

# Monitor
./monitor_simple.sh /dev/cu.usbserial-0251757F
```

## Temporary Alternative

If you just want to test the MQTT query changes on hardware you already have flashed, you can skip the build and just monitor the current firmware to verify network connectivity and configuration.

The SSL fix and query functions are ready in the code - they just need to be compiled and flashed to test.
