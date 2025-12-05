# ESP32-TUX-BAMBULAB - Bambu Lab Printer Monitor

ESP32-TUX fork with integrated Bambu Lab 3D printer monitoring via MQTT.

## Features

### Original ESP32-TUX Features
- Touch UI template for ESP32 devices
- Graphics & Touch Driver: [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- UI/Widgets: [LVGL 8.x](https://github.com/lvgl/lvgl)
- Framework: [ESP-IDF v5.0](https://github.com/espressif/esp-idf/)
- WiFi provisioning with fallback credentials
- Web server for configuration
- OTA updates
- Weather integration (OpenWeatherMap)

### Bambu Lab Integration (New!)
- **Real-time printer monitoring** via MQTT over TLS
- **Live telemetry display**: temperatures, print progress, state
- **Printer discovery** on local network (sequential scan)
- **Secure connection** with access code authentication
- **Carousel view** for multiple printer status
- Support for all Bambu Lab printers (P1P, P1S, X1C, A1, A1 Mini)

## Supported Devices

- [WT32-SC01](https://bit.ly/wt32-sc01) - SPI TFT 3.5" ST7796 (ESP32)
- [WT32-SC01 Plus](https://bit.ly/wt32-sc01-plus) - 8Bit Parallel 3.5" ST7796UI (ESP32-S3)
- [Makerfabs ESP32-S3](https://bit.ly/ESP32S335D) - 16Bit Parallel TFT 3.5" ILI9488
- [Makerfabs ESP32-S3 SPI](https://bit.ly/ESP32S3SPI35) - SPI TFT 3.5" ILI9488

## Quick Start

### Prerequisites
- ESP-IDF v5.0 installed
- ESP32 development board (see supported devices above)
- Bambu Lab 3D printer on same network

### Configuration

1. **WiFi Credentials** (in `main/helpers/wifi_prov_mgr.hpp`):
```cpp
#define FALLBACK_WIFI_SSID      "YourWiFiSSID"
#define FALLBACK_WIFI_PASS      "YourWiFiPassword"
```

2. **Printer Settings** (in `main/helpers/helper_bambu.hpp`):
```cpp
bambu_printer_config_t printer_config = {
    .device_id = (char*)"00M09D530200738",     // Your printer serial
    .ip_address = (char*)"10.13.13.85",        // Your printer IP
    .port = 8883,                               // MQTT TLS port
    .access_code = (char*)"5d35821c",          // Your access code
};
```

### Build & Flash

```bash
# Set target (esp32 or esp32s3)
idf.py set-target esp32

# Configure device via menuconfig
idf.py menuconfig
# Navigate to: ESP32-TUX Configuration -> Select Device

# Build
idf.py build

# Flash (with preserved settings)
./flash_quick.sh /dev/cu.SLAB_USBtoUART

# Or full flash (erases everything)
idf.py -p /dev/cu.SLAB_USBtoUART flash
```

### Finding Your Bambu Lab Access Code

1. Open Bambu Handy app or Bambu Studio
2. Go to printer settings
3. Enable "LAN Mode"
4. Access code is displayed (8 characters)

## Architecture

### BambuMonitor Component
- **Location**: `components/BambuMonitor/`
- **MQTT Client**: Connects to printer on port 8883 (TLS)
- **Authentication**: Username "bblp" + access code
- **Topics**: Subscribes to `device/+/report` for telemetry
- **Self-signed cert handling**: Skip cert verification enabled

### Printer Discovery
- **Network scanning**: Sequential IP scan (10.13.13.0/24)
- **Port detection**: TCP port 8883 check
- **Speed**: ~75 seconds for 254 addresses
- **Configurable**: Add networks via Web UI

### UI Integration
- **Bambu page**: Dedicated screen showing printer status
- **Live updates**: LVGL messages for real-time display
- **Status indicators**: Temperatures, progress, state
- **Carousel support**: Multiple printers (future)

## Documentation

See the following guides for more details:
- [BAMBU_QUICKSTART.md](BAMBU_QUICKSTART.md) - Quick setup guide
- [BAMBU_TECHNICAL_DESIGN.md](BAMBU_TECHNICAL_DESIGN.md) - Architecture details
- [CAROUSEL_QUICKSTART.md](CAROUSEL_QUICKSTART.md) - Multi-printer setup
- [FLASHING_GUIDE.md](FLASHING_GUIDE.md) - Detailed flash instructions

## Troubleshooting

### No WiFi Connection
- Check fallback credentials in `wifi_prov_mgr.hpp`
- Device creates AP `TUX_XXXXXX` if not provisioned
- Web UI available at device IP after connection

### MQTT Connection Issues
- Verify printer IP is reachable: `ping 10.13.13.85`
- Check access code (8 hex characters)
- Ensure LAN mode enabled on printer
- Monitor logs: `idf.py monitor`

### Bootloop Issues
- Port 80 conflict between WebServer and provisioning
- Graceful error handling implemented (continues on failure)
- Check SPIFFS partition flashed: `idf.py flash` (not flash_quick.sh)

## Credits

- **Original ESP32-TUX**: [sukesh-ak/ESP32-TUX](https://github.com/sukesh-ak/esp32-tux)
- **Bambu Lab Integration**: Community-driven enhancement
- **MQTT Protocol**: Bambu Lab MQTT specification

## License

MIT License (same as original ESP32-TUX project)

## Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Test thoroughly on real hardware
4. Submit pull request with clear description

## Support

- **Issues**: [GitHub Issues](https://github.com/mybesttools/ESP32-TUX-BAMBULAB/issues)
- **Original Project**: [ESP32-TUX](https://github.com/sukesh-ak/esp32-tux)
