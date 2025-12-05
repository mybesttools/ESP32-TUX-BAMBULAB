# Reference Code Extraction from bambu_project

This document identifies which sections of bambu_project source code should be referenced during Bambu Lab component development.

## File-by-File Reference Guide

### 1. MQTT Connection & SSL/TLS Setup

**Source:** `bambu_project/src/main.cpp` (Lines 45-100)

**What to extract:**

- Bambu CA certificate (required for secure connection)
- MQTT connection parameters
- TLS/SSL configuration

**Code snippet location:**

```cpp
// Bambu Lab CA Certificate
const char* bambu_ca_cert = 
"-----BEGIN CERTIFICATE-----\n"
...
"-----END CERTIFICATE-----\n";

// MQTT configuration
#define MQTT_MAX_PACKET_SIZE 4096
#include <PubSubClient.h>
```

**Use for:** `components/BambuMonitor/include/mqtt_client.h`

---

### 2. WiFi Event Handling

**Source:** `bambu_project/src/main.cpp` (Lines 200-350)

**What to extract:**

- WiFi event callbacks (`wifi_event_handler`)
- Connection state tracking
- Network status updates

**Code patterns:**

- `WiFi.begin(SSID, PASSWORD)`
- Event handling for WIFI_EVENT_STA_CONNECTED
- Connection retry logic

**Use for:** Integrate with existing WiFi provisioning in ESP32-TUX

---

### 3. MQTT Message Handler

**Source:** `bambu_project/src/main.cpp` (Lines 400-600)

**What to extract:**

- `onMqttMessageReceived()` callback function
- Topic parsing
- Message routing logic

**Key elements:**

- Subscribe to `device/{SERIAL}/report`
- Route to JSON parser
- Handle connection/disconnection

**Use for:** `components/BambuMonitor/BambuMonitor.cpp` message handling

---

### 4. JSON Parsing - Printer Status

**Source:** `bambu_project/src/main.cpp` (Lines 700-900)

**What to extract:**

- JSON document creation and parsing
- Field extraction patterns
- ArduinoJson library usage

**Expected JSON structure:**

```json
{
  "print": {
    "bed_temper": 60.0,
    "nozzle_temper": 220.0,
    "chamber_temper": 45.0,
    "progress": 45,
    "remaining_time": 1800,
    "print_file": "test_model.3mf",
    "gcode_state": "PRINTING"
  },
  "info": {
    "serial": "01S0006G..."
  }
}
```

**JSON parsing example:**

```cpp
StaticJsonDocument<4096> doc;
DeserializationError error = deserializeJson(doc, payload, length);

if (!error) {
    int progress = doc["print"]["progress"];
    float bed_temp = doc["print"]["bed_temper"];
    // ... more fields
}
```

**Use for:** `components/BambuMonitor/BambuMonitor.cpp` parse_printer_status_json()

---

### 5. State Machine / Status Tracking

**Source:** `bambu_project/src/main.cpp` (Lines 1000-1200)

**What to extract:**

- Printer state definitions
- State transition logic
- Status mapping from gcode_state strings

**State mappings to implement:**

| gcode_state | Mapped to |
|------------|-----------|
| "PRINTING" | PRINTING |
| "PAUSING" | PAUSED (transitional) |
| "PAUSED" | PAUSED |
| "IDLE" | IDLE |
| "ERROR" | ERROR |
| "HEATING" | HEATING |
| "HOMING" | PREPARING |
| "PROBING" | PREPARING |

**Use for:** `components/BambuMonitor/include/printer_state.h`

---

### 6. Configuration Management

**Source:** `bambu_project/src/main.cpp` (Lines 100-200, config.h equivalent)

**What to extract:**

- Configuration structure definition
- Settings names and types
- Default values

**Configuration items needed:**

```cpp
#define MQTT_SERVER         "192.168.1.100"      // Printer IP
#define DEVICE_SERIAL       "01S00A123456789"    // Printer serial
#define MQTT_PASSWORD       "12345678"           // Access code
#define MQTT_PORT           8883                 // TLS port
```

**Use for:** Integrate with `components/SettingsConfig/SettingsConfig.hpp`

---

### 7. Error Handling & Reconnection

**Source:** `bambu_project/src/main.cpp` (Lines 1300-1400)

**What to extract:**

- Connection error handling
- Retry logic with backoff
- Error logging patterns

**Patterns:**

- Track connection attempts
- Exponential backoff for retries
- Graceful degradation when disconnected

**Use for:** `components/BambuMonitor/BambuMonitor.cpp` error handling

---

### 8. LVGL UI Widget Examples

**Source:** `bambu_project/src/main.cpp` (Lines 1500-2000)

**What to extract:**

- LVGL widget creation patterns
- Label formatting
- Bar/progress widgets
- Event handling

**Example patterns:**

```cpp
// Create progress bar
lv_obj_t *progress_bar = lv_bar_create(parent);
lv_bar_set_value(progress_bar, progress_percent, LV_ANIM_ON);

// Create temperature label
lv_obj_t *temp_label = lv_label_create(parent);
lv_label_set_text_fmt(temp_label, "Nozzle: %.1f°C", temp);

// Create event handler
lv_obj_add_event_cb(btn, button_event_handler, LV_EVENT_CLICKED, NULL);
```

**Use for:** `main/apps/bambu/printer_status.hpp` UI creation

---

### 9. Documentation & Setup

**Source:** `bambu_project/BAMBU_SETUP.md`

**What to extract:**

- MQTT protocol details
- Printer authentication method
- Topic structure
- Sample messages

**Key information:**

- How to get printer serial number
- How to get access code
- MQTT server address (printer IP)
- TLS certificate requirement

**Use for:** Component documentation and user guides

---

### 10. GIF Handling (SPIFFS-Based)

**Source:** `bambu_project/data/` folder (11 GIF files)

**Storage Approach:**

- GIFs stored in SPIFFS partition (data/ folder during build)
- LVGL loads GIFs directly from SPIFFS at runtime
- No C source files needed
- Total size: 1.3MB (all 11 GIFs fit comfortably in 1.5MB SPIFFS)

**Available GIFs:**

- logo.gif (16KB)
- printing.gif (157KB)
- standby.gif (64KB)
- error.gif (48KB)
- nozzle_cleaning.gif (164KB)
- bed_leveling.gif (82KB)
- And 5 more state animations

**Implementation:**
Add to CMakeLists.txt:

```cmake
spiffs_create_partition_image(storage ${CMAKE_CURRENT_SOURCE_DIR}/data FLASH_IN_PROJECT)
```

**Reference:** `bambu_project/data/` folder structure

---

## Code Extraction Checklist

### Phase 1: Infrastructure

- [ ] Extract CA certificate
- [ ] Extract MQTT connection code
- [ ] Extract configuration structure
- [ ] Extract state definitions

### Phase 2: Implementation

- [ ] Extract message handler pattern
- [ ] Extract JSON parsing logic
- [ ] Extract state machine code
- [ ] Extract error handling

### Phase 3: Integration

- [ ] Extract UI widget patterns
- [ ] Extract event handling examples
- [ ] Review BAMBU_SETUP.md for protocol details

---

## Important Notes

### License

Ensure code extraction respects bambu_project licensing (verify LICENSE file in bambu_project/)

### Version Compatibility

- bambu_project uses LVGL 8.4.0
- ESP32-TUX uses LVGL 8.3.3
- Minor version difference is compatible
- API differences are minimal (mostly in display/touch initialization)

### Build System

- bambu_project: PlatformIO
- ESP32-TUX: ESP-IDF
- Both use same libraries (just different build systems)
- Component code should be build-system agnostic

### Testing Considerations

When extracting code:

1. Test extraction in isolation first
2. Verify against actual Bambu printer if possible
3. Validate JSON parsing with sample messages
4. Check error handling with simulated failures

---

## Direct File References

| File | Purpose | Extract | Copy As |
|------|---------|---------|---------|
| `bambu_project/src/main.cpp` | Full implementation | Lines 1-3597 | Reference |
| `bambu_project/BAMBU_SETUP.md` | Protocol docs | Full | Reference |
| `bambu_project/src/ui/ui.h` | UI definitions | Full | Reference |
| `bambu_project/platformio.ini` | Build config | Full | Reference |
| `bambu_project/src/lv_conf.h` | LVGL config | Full | Reference |

---

## Quick Copy-Paste Sections

### Bambu CA Certificate

```ascii
Location: bambu_project/src/main.cpp
Search for: "-----BEGIN CERTIFICATE-----"
Length: ~1.5KB
Paste into: components/BambuMonitor/include/mqtt_client.h
```

### MQTT Message Format

```ascii
Location: bambu_project/BAMBU_SETUP.md
Section: "MQTT Topics"
Content: Topic structure and JSON format
```

### State Mapping Table

```ascii
Location: bambu_project/src/main.cpp
Search for: function handling gcode_state
Extract: All state string comparisons
```

---

## Development Notes

When referencing bambu_project code:

- Code style may differ (adapt to ESP32-TUX conventions)
- Remove PlatformIO-specific code (use ESP-IDF equivalents)
- Remove hardcoded configs (use NVS/dynamic config)
- Add error checking (especially MQTT operations)
- Log state transitions for debugging
- Handle network failures gracefully

---

## Next Steps

1. **Read** bambu_project/BAMBU_SETUP.md thoroughly
2. **Review** bambu_project/src/main.cpp sections 1-100 (setup)
3. **Extract** CA certificate and MQTT configuration
4. **Analyze** JSON message format and parsing
5. **Plan** component API based on extracted code patterns

Ready to begin extraction?
