# Bambu Lab Component - Technical Design

This document provides the detailed technical design for integrating Bambu Lab printer monitoring into ESP32-TUX.

## Component Architecture

### BambuMonitor Component Structure

```text
components/BambuMonitor/
├── CMakeLists.txt
├── Kconfig
├── idf_component.yml
├── include/
│   ├── BambuMonitor.hpp           # Main interface
│   ├── printer_state.h             # State definitions
│   ├── mqtt_client.h               # MQTT wrapper
│   └── json_parser.h               # JSON utilities
└── BambuMonitor.cpp                # Implementation
```

### Core Classes

#### 1. PrinterState

```cpp
// printer_state.h
enum PrinterStatus {
    OFFLINE,
    IDLE,
    PREPARING,      // homing, probing
    HEATING,        // bed/nozzle warmup
    PRINTING,
    PAUSED,
    ERROR,
    COOLING
};

struct PrinterState {
    PrinterStatus status;
    uint8_t print_progress;           // 0-100%
    float bed_temp;
    float bed_target_temp;
    float nozzle_temp;
    float nozzle_target_temp;
    float chamber_temp;
    std::string current_file;         // print filename
    uint32_t print_time_remaining;    // seconds
    uint32_t print_time_elapsed;      // seconds
    std::string error_msg;
    time_t last_update;
};
```

#### 2. BambuMonitor Class

```cpp
// BambuMonitor.hpp
class BambuMonitor {
public:
    BambuMonitor();
    ~BambuMonitor();
    
    // Initialization
    esp_err_t init(const BambuConfig& config);
    esp_err_t connect();
    void disconnect();
    
    // Updates
    void update();  // Call regularly from main loop
    
    // State access
    const PrinterState& get_state() const;
    bool is_connected() const;
    
    // Callbacks
    void register_state_change_callback(StateChangeCallback callback);
    
private:
    void handle_mqtt_message(const char* topic, const uint8_t* payload, uint32_t length);
    void parse_printer_status_json(const JsonDocument& doc);
    void update_state(PrinterStatus new_status);
    void publish_request(const char* command);
    
    WiFiClientSecure wifi_client;
    PubSubClient mqtt_client;
    PrinterState state;
    BambuConfig config;
    StateChangeCallback callback;
    uint32_t last_update_ms;
};
```

#### 3. BambuConfig

```cpp
// From SettingsConfig integration
struct BambuConfig {
    std::string mqtt_server;        // Printer IP
    uint16_t mqtt_port = 8883;      // TLS port
    std::string device_serial;      // Printer serial number
    std::string access_code;        // 8-digit access code (password)
    bool enabled = false;
    uint32_t update_interval_ms = 1000;  // Poll rate
};
```

---

## MQTT Integration

### Bambu Lab MQTT Protocol

**Connection:**

```text
Server: <printer_ip>:8883 (TLS)
Username: bblp_<serial>
Password: <access_code>
```

**Topics:**

Subscribe to:

```text
device/{serial}/report
  └─ JSON with: status, temp_bed, temp_nozzle, print_progress, etc.
```

Publish to (optional):

```text
device/{serial}/request
  └─ JSON commands: { "print": { "command": "pause/resume/stop" } }
```

### Message Parsing Example

```cpp
// Expected JSON format (simplified)
{
  "print": {
    "bed_temper": 60.0,
    "nozzle_temper": 220.0,
    "chamber_temper": 45.0,
    "progress": 45,
    "remaining_time": 1800,
    "print_type": "print",
    "print_file": "test_model.3mf",
    "hw_ver": "AP04",
    "wifi_signal": -45,
    "gcode_state": "PRINTING"
  },
  "info": {
    "serial": "01S0006G..."
  }
}
```

---

## UI Integration

### New Screens

#### 1. Main Menu Item

```cpp
// In gui.hpp - add to tux_ui_init()
static void bambu_menu_item_create(lv_obj_t *parent) {
    // Add "Printer" menu option
}
```

#### 2. Printer Status Screen

```text
┌─────────────────────────────────┐
│  Bambu Printer Monitor          │
├─────────────────────────────────┤
│                                 │
│  Status: PRINTING         ███░░ │ 65%
│                                 │
│  Nozzle: 220°C / 220°C          │
│  Bed: 60°C / 60°C               │
│  Chamber: 45°C                  │
│                                 │
│  Time: 45m remaining            │
│  File: test_model.3mf           │
│                                 │
│  [Pause]  [Settings]  [Back]    │
└─────────────────────────────────┘
```

#### 3. Printer Config Screen

```text
┌─────────────────────────────────┐
│  Printer Configuration          │
├─────────────────────────────────┤
│                                 │
│  MQTT Server: 192.168.1.100     │
│  Serial: 01S0006G...            │
│  Access Code: [••••••••]        │
│  Update Interval: 1000ms        │
│                                 │
│  Status: CONNECTED ✓            │
│                                 │
│  [Edit]  [Test]  [Back]         │
└─────────────────────────────────┘
```

### Animation/GIF Integration (Optional)

Store printer state GIFs in SPIFFS (if space permits):

```text
spiffs/bambu_gifs/
├── printing.gif      (for PRINTING state)
├── heating.gif       (for HEATING state)
├── paused.gif        (for PAUSED state)
└── error.gif         (for ERROR state)
```

Display as animated image based on current status.

---

## Configuration Storage (NVS)

### Extend SettingsConfig Component

```cpp
// Add to SettingsConfig::load()
void load_bambu_config() {
    Preferences prefs;
    prefs.begin("bambu", false);
    
    bambu_config.enabled = prefs.getBool("enabled", false);
    bambu_config.mqtt_server = prefs.getString("mqtt_server", "");
    bambu_config.device_serial = prefs.getString("serial", "");
    bambu_config.access_code = prefs.getString("access_code", "");
    
    prefs.end();
}

void save_bambu_config() {
    Preferences prefs;
    prefs.begin("bambu", false);
    
    prefs.putBool("enabled", bambu_config.enabled);
    prefs.putString("mqtt_server", bambu_config.mqtt_server);
    prefs.putString("serial", bambu_config.device_serial);
    prefs.putString("access_code", bambu_config.access_code);
    
    prefs.end();
}
```

---

## WiFi Provisioning Integration

### Extended Provisioning Flow

Current flow (ESP32-TUX):

```text
1. WiFi not connected
2. → BLE provisioning manager
3. → WiFi SSID/Password entry
4. → Connect
5. → Done
```

Extended flow (with Bambu):

```text
1. WiFi not connected
2. → BLE provisioning manager
3. → WiFi SSID/Password entry
4. → Connect to WiFi
5. → (Optional) Bambu Printer config?
6. → Enter MQTT Server IP
7. → Enter Printer Serial
8. → Enter Access Code
9. → Done
```

Implementation:

```cpp
// In wifi_prov_mgr.hpp - add callback
void handle_wifi_provisioning_complete() {
    // After WiFi connected, check if Bambu enabled
    if (cfg->bambu_enabled) {
        // Transition to Bambu provisioning page
        lv_msg_send(MSG_BAMBU_PROVISIONING_START, NULL);
    }
}
```

---

## Main Integration Points

### 1. main.cpp Initialization

```cpp
#include "BambuMonitor.hpp"

// Global instance
static BambuMonitor* bambu_monitor = nullptr;

void app_main() {
    // ... existing code ...
    
    // Load Bambu config from NVS
    cfg->load_bambu_config();
    
    // Initialize Bambu monitor
    if (cfg->bambu_enabled) {
        bambu_monitor = new BambuMonitor();
        bambu_monitor->init(*cfg);
    }
    
    // ... rest of initialization ...
}
```

### 2. Main Loop Update

```cpp
// In timer callback or main loop
static void app_main_loop() {
    // Update weather
    timer_weather_callback(nullptr);
    
    // Update printer status
    if (bambu_monitor && bambu_monitor->is_connected()) {
        bambu_monitor->update();
    }
}
```

### 3. State Change Callbacks

```cpp
// Register callback
bambu_monitor->register_state_change_callback(
    [](const PrinterState& state) {
        // Trigger UI update
        lv_msg_send(MSG_BAMBU_STATUS_CHANGED, (void*)&state);
    }
);
```

---

## Build System Integration

### CMakeLists.txt Updates

```cmake
# components/BambuMonitor/CMakeLists.txt
idf_component_register(
    SRCS "BambuMonitor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event wifi_provisioning esp_https_client esp_tls
    PRIV_REQUIRES json nvs_flash
)

# GIFs are stored in SPIFFS partition (data/ folder)
# No C source files needed - LVGL loads GIFs directly from SPIFFS
```

### Kconfig Options

```text
menu "Bambu Lab Printer Monitoring"
    config BAMBU_ENABLE
        bool "Enable Bambu Lab Printer Support"
        default n
        help
            Enable real-time monitoring of Bambu Lab 3D printers via MQTT
    
    config BAMBU_UPDATE_INTERVAL
        int "MQTT Update Interval (ms)"
        default 1000
        depends on BAMBU_ENABLE
        help
            How often to update printer status from MQTT topic
    
    config BAMBU_GIF_ANIMATIONS
        bool "Enable Printer State GIF Animations"
        default n
        depends on BAMBU_ENABLE
        help
            Display animated GIFs based on printer state
            Requires ~1MB additional SPIFFS storage
    
    config BAMBU_MAX_RETRIES
        int "MQTT Connection Retries"
        default 5
        depends on BAMBU_ENABLE
endmenu
```

---

## Memory Considerations

### RAM Usage

- PrinterState struct: ~200 bytes
- JSON buffer: ~4KB (configurable)
- MQTT client internal: ~5KB
- Total overhead: ~10KB (acceptable)

### Flash Usage

- BambuMonitor component: ~30KB
- PubSubClient library: ~50KB
- ArduinoJson library: ~20KB
- Total: ~100KB (fits in 13% free space)

---

## Testing Strategy

### Unit Tests

```cpp
// Test JSON parsing
TEST_CASE("Parse printer status JSON") {
    BambuMonitor monitor;
    JsonDocument doc;
    // Parse sample JSON
    monitor.parse_printer_status_json(doc);
    
    REQUIRE(monitor.get_state().print_progress == 45);
    REQUIRE(monitor.get_state().status == PRINTING);
}
```

### Integration Tests

1. Connect to Bambu printer on local network
2. Verify MQTT connection establishes
3. Verify status updates received
4. Test state transitions
5. Verify UI updates in sync

### Edge Cases

- MQTT disconnection and reconnection
- Malformed JSON messages
- Network interruptions during updates
- Concurrent weather + printer updates

---

## Future Enhancements

1. **Print Control**
   - Pause/Resume/Stop buttons
   - Filament load/unload
   - Speed adjustment

2. **History & Analytics**
   - Track print times
   - Success/failure rates
   - Temperature graphs

3. **Multi-Printer Support**
   - Monitor multiple printers
   - Select active printer
   - Composite status view

4. **Notifications**
   - Print completion alert
   - Error notifications
   - Temperature warnings

5. **Camera Feed** (if device supports)
   - Live camera stream
   - Time-lapse capture
