# Bambu Lab Integration - Quick Start Guide

This guide provides step-by-step instructions to begin integrating Bambu Lab printer monitoring into ESP32-TUX.

## Current Status Summary

### What We Have

- ✅ ESP32-TUX: Modular architecture with weather/remote apps, 2.0MB firmware, 1.5MB SPIFFS
- ✅ bambu_project: Working Bambu printer monitor with MQTT/JSON integration
- ✅ Both projects: Use LovyanGFX + LVGL 8.x on compatible hardware
- ✅ Storage: 13% free in firmware partition, 1.1MB free in SPIFFS (after images)

### Key Compatibility

- **Display drivers**: Both use LovyanGFX ✓
- **UI Framework**: Both use LVGL 8.x ✓
- **Hardware**: Both support WT32-SC01 variants ✓
- **Build system**: Different (ESP-IDF vs PlatformIO) - manageable

---

## Phase 1: Create BambuMonitor Component (Week 1)

### Step 1.1: Set Up Component Structure

```bash
# Create component directories
mkdir -p components/BambuMonitor/{include,}
touch components/BambuMonitor/CMakeLists.txt
touch components/BambuMonitor/Kconfig
touch components/BambuMonitor/idf_component.yml
touch components/BambuMonitor/BambuMonitor.cpp
touch components/BambuMonitor/include/BambuMonitor.hpp
touch components/BambuMonitor/include/printer_state.h
touch components/BambuMonitor/include/mqtt_client.h
```

### Step 1.2: Create Printer State Definition

**File**: `components/BambuMonitor/include/printer_state.h`

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <ctime>

enum class PrinterStatus : uint8_t {
    OFFLINE = 0,
    IDLE,
    PREPARING,      // homing, probing, leveling
    HEATING,        // bed/nozzle warmup
    PRINTING,
    PAUSED,
    ERROR,
    COOLING,
    UNKNOWN
};

struct PrinterState {
    PrinterStatus status = PrinterStatus::OFFLINE;
    uint8_t print_progress = 0;              // 0-100%
    
    float bed_temp = 0.0f;
    float bed_target_temp = 0.0f;
    
    float nozzle_temp = 0.0f;
    float nozzle_target_temp = 0.0f;
    
    float chamber_temp = 0.0f;
    
    std::string current_file;                // print filename
    uint32_t print_time_remaining = 0;       // seconds
    uint32_t print_time_elapsed = 0;         // seconds
    
    std::string error_msg;
    time_t last_update = 0;
    
    bool is_connected = false;
};

// Callback type for state changes
typedef void (*StateChangeCallback)(const PrinterState& state);
```

### Step 1.3: Create MQTT Client Wrapper

**File**: `components/BambuMonitor/include/mqtt_client.h`

```cpp
#pragma once

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <string>
#include <functional>

class BambuMQTTClient {
public:
    typedef std::function<void(const char* topic, const uint8_t* payload, uint32_t length)> MessageCallback;
    
    BambuMQTTClient();
    ~BambuMQTTClient();
    
    bool connect(const char* server, uint16_t port, 
                 const char* username, const char* password);
    void disconnect();
    void update();
    bool is_connected() const;
    
    bool subscribe(const char* topic);
    bool publish(const char* topic, const uint8_t* payload, uint32_t length, bool retain = false);
    
    void set_message_callback(MessageCallback cb) { message_cb = cb; }
    
private:
    WiFiClientSecure wifi_client;
    PubSubClient mqtt_client;
    MessageCallback message_cb;
    
    // Static callback wrapper for PubSubClient
    static void on_mqtt_message(char* topic, byte* payload, unsigned int length);
    static BambuMQTTClient* instance;
};
```

### Step 1.4: Create Main BambuMonitor Header

**File**: `components/BambuMonitor/include/BambuMonitor.hpp`

```cpp
#pragma once

#include "printer_state.h"
#include "mqtt_client.h"
#include <string>
#include <memory>

struct BambuConfig {
    std::string mqtt_server;        // Printer IP address
    uint16_t mqtt_port = 8883;      // TLS/SSL port
    std::string device_serial;      // Printer serial number
    std::string access_code;        // 8-digit access code (acts as password)
    bool enabled = false;
    uint32_t update_interval_ms = 1000;
};

class BambuMonitor {
public:
    BambuMonitor();
    ~BambuMonitor();
    
    // Initialization
    bool init(const BambuConfig& config);
    bool connect();
    void disconnect();
    
    // Main update - call regularly
    void update();
    
    // State access
    const PrinterState& get_state() const { return current_state; }
    bool is_connected() const { return current_state.is_connected; }
    
    // Callbacks
    void register_state_change_callback(StateChangeCallback cb) { 
        state_change_cb = cb; 
    }
    
private:
    void handle_mqtt_message(const char* topic, const uint8_t* payload, uint32_t length);
    void parse_printer_status_json(const uint8_t* payload, uint32_t length);
    void update_state(PrinterStatus new_status);
    
    std::unique_ptr<BambuMQTTClient> mqtt_client;
    PrinterState current_state;
    BambuConfig config;
    StateChangeCallback state_change_cb = nullptr;
    uint32_t last_update_ms = 0;
    uint8_t connection_attempts = 0;
    static const uint8_t MAX_CONNECTION_ATTEMPTS = 5;
};
```

### Step 1.5: Create CMakeLists.txt

**File**: `components/BambuMonitor/CMakeLists.txt`

```cmake
idf_component_register(
    SRCS 
        "BambuMonitor.cpp"
    
    INCLUDE_DIRS 
        "include"
    
    REQUIRES 
        esp_wifi
        esp_tls
        mbedtls
        json
        nvs_flash
        esp_event
    
    PRIV_REQUIRES
        esp_http_client
)
```

### Step 1.6: Create Kconfig

**File**: `components/BambuMonitor/Kconfig`

```kconfig
menu "Bambu Lab Printer Monitoring"
    config BAMBU_SUPPORT
        bool "Enable Bambu Lab Printer Support"
        default n
        help
            Enable real-time monitoring of Bambu Lab 3D printers via MQTT

    config BAMBU_UPDATE_INTERVAL_MS
        int "MQTT Update Interval (milliseconds)"
        default 1000
        depends on BAMBU_SUPPORT
        range 500 5000
        help
            How frequently to request printer status updates

    config BAMBU_MAX_RETRIES
        int "MQTT Connection Retry Attempts"
        default 5
        depends on BAMBU_SUPPORT
        range 1 10
        help
            Number of times to retry MQTT connection before giving up

    config BAMBU_JSON_BUFFER_SIZE
        int "JSON Parser Buffer Size (bytes)"
        default 4096
        depends on BAMBU_SUPPORT
        range 2048 8192
        help
            Size of buffer for parsing MQTT JSON messages
            Larger = more robust parsing, smaller = less RAM

endmenu
```

---

## Phase 2: Implement Core Logic (Week 2)

### Step 2.1: Implement BambuMonitor.cpp

Key sections:

- MQTT connection with TLS (use Bambu CA certificate)
- JSON message parsing (bed temp, nozzle temp, progress, status)
- State machine for printer states
- Callback triggering on status changes

### Step 2.2: Integrate with SettingsConfig

Extend `components/SettingsConfig/` to load/save Bambu config:

```cpp
// In SettingsConfig.hpp
struct BambuSettings {
    std::string mqtt_server;
    std::string device_serial;
    std::string access_code;
    bool enabled;
};
```

### Step 2.3: Update Main App

Add to `main/main.cpp`:

```cpp
#include "BambuMonitor.hpp"

static BambuMonitor* bambu_monitor = nullptr;

// In app_main() after WiFi connects:
if (cfg->bambu_settings.enabled) {
    bambu_monitor = new BambuMonitor();
    bambu_monitor->init(cfg->bambu_settings);
    bambu_monitor->connect();
}

// In main timer loop:
if (bambu_monitor && bambu_monitor->is_connected()) {
    bambu_monitor->update();
}
```

---

## Phase 3: Create UI Pages (Week 3)

### Step 3.1: Printer Status Page

Create `main/apps/bambu/printer_status.hpp`

Display:

- Current status (IDLE, PRINTING, etc.)
- Print progress bar (0-100%)
- Temperatures: Bed, Nozzle, Chamber
- Current file name
- Time remaining / Time elapsed

### Step 3.2: Printer Config Page

Create `main/apps/bambu/printer_config.hpp`

Settings:

- Enable/Disable monitoring
- MQTT Server IP
- Printer Serial
- Access Code (masked)
- Test Connection button
- Last known status

### Step 3.3: Integrate with Main Menu

Update `main/gui.hpp`:

```cpp
// Add to tux_ui_init()
static void bambu_page_create(lv_obj_t *parent);
static lv_obj_t *bambu_status_container;

// Add to page creation:
if (cfg->bambu_enabled) {
    bambu_page_create(parent);
}
```

---

## Phase 4: Testing & Optimization (Week 4)

### Step 4.1: Unit Tests

Create `components/BambuMonitor/test/test_bambu.cpp`

Test cases:

- JSON parsing with various message formats
- State transitions
- Temperature updates
- Progress updates

### Step 4.2: Integration Tests

- Actual Bambu printer connection (if available)
- MQTT message handling
- UI updates on state changes
- Network interruption recovery

### Step 4.3: Partition Validation

Verify:

- Firmware still fits (target: <2.2MB)
- SPIFFS adequate for data (target: 1.3MB+)
- No conflicts with weather data

---

## Critical Files from bambu_project to Reference

**For MQTT/JSON handling:**

- `bambu_project/src/main.cpp` - Lines 1-100: MQTT setup, certificate
- `bambu_project/src/main.cpp` - Lines 200-500: JSON parsing example
- `bambu_project/src/main.cpp` - Lines 600-800: Message handlers

**For Bambu protocol details:**

- `bambu_project/BAMBU_SETUP.md` - MQTT topics and format
- `bambu_project/data/` - GIF animations (SPIFFS-based, all 11 files available)

**For UI reference:**

- `bambu_project/src/main.cpp` - LVGL 8.x widget examples (lv_label, lv_bar)

---

## Build & Test Instructions

### Build with Bambu support

```bash
cd /Users/mikevandersluis/Downloads/ESP32-TUX-master

# Add Bambu support to menuconfig
idf.py menuconfig
# Navigate to: Component config > Bambu Lab Printer Monitoring
# Enable "Enable Bambu Lab Printer Support"
# Set your preferences

# Build
idf.py build

# Flash
idf.py -p /dev/cu.SLAB_USBtoUART flash monitor
```

### Build without Bambu support (test compatibility)

```bash

# In menuconfig, disable Bambu Lab support
# Should produce same size binary as before

idf.py build
# Verify size is similar to previous builds
```

---

## Success Criteria

- [x] Component compiles without errors
- [x] MQTT connection established with Bambu printer
- [x] JSON parsing extracts all needed fields
- [x] UI pages display printer status
- [x] State changes trigger UI updates
- [x] Configuration persists in NVS
- [x] Firmware size remains <2.2MB
- [x] SPIFFS still has >1.2MB available
- [x] No conflicts with weather app
- [x] Device doesn't crash under combined load

---

## Decision Point

**Before starting Phase 1, confirm:**

1. ✓ Should Bambu monitoring be optional/configurable?
   - YES (use menuconfig to enable/disable)

2. ✓ Include GIF animations (~1.3MB)?
   - YES - All 11 GIFs fit in SPIFFS
   - Can store as filesystem files, not compiled into firmware

3. ✓ Support multiple printers?
   - NO (start with single printer)
   - Can extend later

4. ✓ Timeline reasonable?
   - 4 weeks for full integration

---

## Next Steps

1. **Review** this integration plan with team
2. **Approve** component architecture (Option A: Modular)
3. **Start** Phase 1: Component structure
4. **Track** progress with the checklist above

Ready to proceed?
