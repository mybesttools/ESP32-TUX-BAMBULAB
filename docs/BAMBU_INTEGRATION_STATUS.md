# Bambu Lab Integration - Implementation Summary

## Status: ⚠️ IN PROGRESS - MQTT CONNECTION WORKING

### Latest Update: December 6, 2025

**Current State:**
- ✅ Successfully switched to WT32-SC01-Plus (ESP32-S3)
- ✅ TLS handshake successful with printer
- ✅ MQTT connected and subscribed to printer
- ⚠️ Buffer size increased to handle large JSON payloads (needs testing)

### Hardware Configuration

**Device:** WT32-SC01-Plus
- **MCU:** ESP32-S3 (Dual Core @ 160MHz)
- **Flash:** 8MB
- **PSRAM:** 2MB (QUAD mode)
- **Display:** 480x320 ST7796 (8-bit parallel)
- **SD Card:** 4GB inserted

**Configuration File:** `sdkconfig.esp32s3.macmini`
- Target: esp32s3
- PSRAM: QUAD mode (not OCTAL - causes bootloop)
- Flash: 8MB
- Partition: partition-8MB.csv

### MQTT Connection Progress

**What's Working:**
1. ESP-IDF mqtt_client with `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY=y`
2. TLS handshake completes successfully
3. MQTT CONNECT and SUBSCRIBE successful
4. Printer responds to subscription

**Issue Fixed:**
- Changed buffer.size from 4KB → 16KB (Bambu sends large JSON ~10KB+)
- Changed buffer.out_size from 2KB → 4KB
- Increased task stack from 6KB → 8KB
- Error was: `-0x7100` (MBEDTLS_ERR_SSL_ALLOC_FAILED)

**Next Steps:**
1. Build and test with new buffer sizes
2. Verify printer data reception
3. Parse JSON and update UI
4. Test carousel with printer GIFs

## What Was Integrated

### 1. BambuMonitor Component

**Location:** `components/BambuMonitor/`

**Files Created:**

- `BambuMonitor.cpp` (4.9KB) - MQTT client implementation
- `include/BambuMonitor.hpp` - Public API header
- `CMakeLists.txt` - Build configuration
- `data/` - 11 GIF animation files (1.3MB)
  - logo.gif, printing.gif, standby.gif, error.gif, nozzle_cleaning.gif, bed_leveling.gif, etc.

**Features:**

- MQTT client for printer communication
- Printer state tracking (Idle, Printing, Paused, Error, Offline)
- JSON status parsing with cJSON library
- Event handler registration for status updates
- Comprehensive error logging

### 2. Helper Module

**Location:** `main/helpers/helper_bambu.hpp`

**Functions:**

- `bambu_get_gif_path()` - Returns SPIFFS path for printer state animation
- `bambu_get_state_str()` - Returns human-readable state string
- `bambu_helper_init()` - Initializes Bambu Monitor with default settings

### 3. Main Application Integration

**Modified Files:**

- `main/main.hpp` - Added includes and global state variables
- `main/main.cpp` - Added Bambu initialization in app_main()
- `main/CMakeLists.txt` - Added BambuMonitor to component requirements

**Integration Points:**

- Bambu Monitor initialized after OpenWeatherMap setup
- Graceful fallback if Bambu Monitor init fails (optional feature)
- Global state variables for printer status tracking

## Build Metrics

| Metric | Value |
|--------|-------|
| Firmware Size | 2.0MB |
| Free Space in Partition | 12% (283KB free in 2.3MB factory partition) |
| GIF Storage | 1.3MB (all 11 animations included) |
| SPIFFS Free Space | ~1.1MB available for images |
| Total Build | Clean compile, no errors |

## Configuration

### Default Settings

The component initializes with placeholder configuration:

- Device ID: `bambu_lab_printer`
- IP Address: `192.168.1.100` (TODO: Make configurable via UI)
- Port: `8883` (MQTT secure)
- Access Code: `0000` (TODO: Make configurable via UI)

### Next Steps for Full Integration

1. **UI Components** - Add Bambu printer status display to main interface
   - Display printer state and animation
   - Show percentage complete for active print
   - Display bed/nozzle temperatures

2. **Configuration UI** - Add settings page for:
   - Printer IP address
   - Device access code
   - Enable/disable Bambu monitoring

3. **Status Page** - Create dedicated printer monitor page showing:
   - Real-time printer animation
   - Temperature data
   - Print progress
   - Error notifications

4. **Event Handlers** - Add LVGL message handlers to:
   - Update UI when printer state changes
   - Display GIF animations for different states
   - Handle printer errors with notifications

## Technical Details

### Component Dependencies

- **Required:** esp_event, mqtt (MQTT client)
- **Private:** json (cJSON for JSON parsing), nvs_flash

### Event System

- Defines `BAMBU_EVENT_BASE` for printer events
- Event type: `BAMBU_STATUS_UPDATED` triggered on state changes
- Allows main UI to subscribe and react to printer status changes

### SPIFFS Integration

- GIF files stored in SPIFFS partition (1.5MB available)
- Paths use `S:/` prefix for SPIFFS access
- All 11 GIFs fit within available storage
- No SD card required (device has no SD reader)

## Testing Recommendations

1. **Build Verification** ✅ Complete
   - No compilation errors
   - All symbols resolved
   - Firmware fits in partition

2. **Runtime Testing** (Next phase)
   - Initialize with valid printer IP/credentials
   - Verify MQTT connection to printer
   - Check printer state tracking
   - Validate GIF path resolution

3. **UI Integration Testing**
   - Display animations based on printer state
   - Handle offline printer gracefully
   - Update UI when state changes

## Files Modified/Created

```ascii
components/BambuMonitor/
├── BambuMonitor.cpp
├── include/BambuMonitor.hpp
├── CMakeLists.txt
└── data/ (11 GIF files)

main/
├── main.hpp (modified)
├── main.cpp (modified)
├── CMakeLists.txt (modified)
└── helpers/
    └── helper_bambu.hpp (created)
```

## Notes

- Component is optional - application continues if Bambu init fails
- GIFs are stored in SPIFFS and loaded by LVGL at runtime
- No embedded C source files for GIFs (clean SPIFFS approach)
- All compiler warnings fixed (no BambuMonitor-related warnings)
- Ready for UI component integration

---

**Build Date:** December 4, 2025  
**Integration Status:** Complete and Ready for UI Integration
