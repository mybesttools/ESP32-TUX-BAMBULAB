# MQTT Query Mechanism Test

## Overview

This document describes the MQTT query mechanism added to the BambuMonitor component and how to test it.

## What Was Added

### 1. New API Functions

#### `bambu_send_query()`
Sends a "pushall" command to request complete printer status.

```c
esp_err_t bambu_send_query(void);
```

**What it does:**
- Publishes to topic: `device/{serial}/request`
- Sends JSON: `{"pushing":{"sequence_id":"0","command":"pushall"}}`
- Returns `ESP_OK` on success, `ESP_FAIL` if MQTT not connected

#### `bambu_send_command(const char* command)`
Sends custom MQTT command to printer.

```c
esp_err_t bambu_send_command(const char* command);
```

**Example commands:**
- Pause: `{"print":{"command":"pause"}}`
- Resume: `{"print":{"command":"resume"}}`
- Stop: `{"print":{"command":"stop"}}`

### 2. Internal Changes

**File: `components/BambuMonitor/BambuMonitor.cpp`**

- Added `saved_config` static variable to store printer configuration
- Modified `bambu_monitor_init()` to save device_id and other config for later use
- Implemented query publishing using `esp_mqtt_client_publish()`

**File: `components/BambuMonitor/include/BambuMonitor.hpp`**

- Added function declarations for query APIs

**File: `main/gui.hpp`**

- Added "Send Query" button to Bambu printer page
- Button click sends MQTT query via `bambu_send_query()`

## How It Works

### MQTT Flow

1. **Connect** (on WiFi ready):
   ```
   ESP32 → MQTT CONNECT → Printer (port 8883)
   ```

2. **Subscribe** (automatic):
   ```
   ESP32 → SUBSCRIBE device/+/report → Printer
   ```

3. **Query** (manual or automatic):
   ```
   ESP32 → PUBLISH device/{serial}/request → Printer
           {"pushing":{"sequence_id":"0","command":"pushall"}}
   ```

4. **Response** (from printer):
   ```
   Printer → PUBLISH device/{serial}/report → ESP32
             {full JSON status with temps, progress, state, etc.}
   ```

### Bambu MQTT Protocol

**Topics:**
- **Subscribe**: `device/{serial}/report` - Receives printer status updates
- **Publish**: `device/{serial}/request` - Sends commands/queries

**Authentication:**
- Username: `bblp`
- Password: `{access_code}` (8-digit code from printer screen)

**Commands:**
```json
// Request full status
{"pushing":{"sequence_id":"0","command":"pushall"}}

// Pause print
{"print":{"command":"pause"}}

// Resume print
{"print":{"command":"resume"}}

// Stop print
{"print":{"command":"stop"}}
```

## Testing Instructions

### Prerequisites

1. Bambu Lab printer on same network
2. Access code from printer (Settings → Network → MQTT)
3. Printer serial number (found in settings)
4. ESP32 device flashed with this code

### Test Steps

#### 1. Configure Connection

Edit `main/helpers/helper_bambu.hpp`:

```cpp
bambu_printer_config_t printer_config = {
    .device_id = (char*)"YOUR_SERIAL_HERE",      // e.g., "00M09D530200738"
    .ip_address = (char*)"YOUR_PRINTER_IP",      // e.g., "192.168.1.100"
    .port = 8883,
    .access_code = (char*)"YOUR_ACCESS_CODE",    // e.g., "12345678"
    .tls_certificate = NULL,
    .disable_ssl_verify = true                    // Set to true for testing
};
```

#### 2. Build and Flash

```bash
# Set target (if not already set)
idf.py set-target esp32s3  # or esp32

# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

#### 3. Monitor Serial Output

Watch for these log messages:

```
I (12345) BambuHelper: Initializing Bambu Monitor helper
I (12346) BambuMonitor: Initializing Bambu Monitor for device: 00M09D530200738 at 10.13.13.85:8883
I (12347) BambuMonitor: SSL verification disabled - using insecure connection
I (15000) BambuMonitor: Starting MQTT client connection to printer
I (15500) BambuMonitor: MQTT Connected to Bambu printer
I (15501) BambuMonitor: Subscribed to device/+/report (msg_id: 1)
```

#### 4. Test Manual Query

1. Navigate to Printer page on the display
2. Tap "Send Query" button
3. Check serial monitor:

```
I (20000) GUI: Query button clicked - sending MQTT query
I (20001) BambuMonitor: Sending pushall query to device/00M09D530200738/request
I (20002) BambuMonitor: Query sent successfully (msg_id: 2)
I (20003) BambuMonitor: MQTT Message published (msg_id: 2)
I (20100) BambuMonitor: MQTT Data received on topic: device/00M09D530200738/report
I (20101) BambuMonitor: Printer state: IDLE
I (20102) BambuMonitor: Temperatures - Nozzle: 25°C, Bed: 24°C
```

#### 5. Verify Response Parsing

Check that the display updates with:
- Current printer status (Idle, Printing, etc.)
- Temperature readings
- Progress (if printing)

### Expected Behavior

**Success Indicators:**
- ✅ MQTT connection established
- ✅ Subscription successful
- ✅ Query publishes without errors
- ✅ Response received on report topic
- ✅ JSON parsed correctly
- ✅ UI updates with new data

**Failure Scenarios:**
- ❌ "MQTT client not initialized" → Call `bambu_monitor_init()` first
- ❌ "Failed to publish query" → Check MQTT connection status
- ❌ No response → Verify printer IP, access code, serial number
- ❌ TLS errors → Set `disable_ssl_verify = true` for testing

## Code Integration Example

### From Application Code

```cpp
#include "BambuMonitor.hpp"

// Initialize (once, at startup)
bambu_printer_config_t config = {
    .device_id = "00M09D530200738",
    .ip_address = "192.168.1.100",
    .port = 8883,
    .access_code = "12345678",
    .disable_ssl_verify = true
};
bambu_monitor_init(&config);

// Start MQTT (after WiFi connected)
bambu_monitor_start_mqtt();

// Send query (when needed)
if (bambu_send_query() == ESP_OK) {
    ESP_LOGI(TAG, "Query sent");
}

// Send custom command
const char* pause_cmd = "{\"print\":{\"command\":\"pause\"}}";
bambu_send_command(pause_cmd);

// Get current state
bambu_printer_state_t state = bambu_get_printer_state();

// Get full JSON status
cJSON* status = bambu_get_status_json();
if (status) {
    char* json_str = cJSON_Print(status);
    ESP_LOGI(TAG, "Status: %s", json_str);
    free(json_str);
    cJSON_Delete(status);
}
```

## Troubleshooting

### Query Not Sending

**Symptom:** `bambu_send_query()` returns `ESP_FAIL`

**Solutions:**
1. Check MQTT connection: `esp_mqtt_client_get_state()` 
2. Verify `bambu_monitor_init()` was called
3. Ensure `bambu_monitor_start_mqtt()` was called after WiFi connected

### No Response Received

**Symptom:** Query sends but no data on report topic

**Solutions:**
1. Verify serial number matches printer exactly
2. Check access code is correct
3. Ensure printer is on and connected to network
4. Check subscription succeeded (msg_id > 0)

### TLS/SSL Errors

**Symptom:** Connection fails with TLS errors

**Solutions:**
1. Set `disable_ssl_verify = true` for testing
2. Extract printer certificate: 
   ```bash
   openssl s_client -connect PRINTER_IP:8883 -showcerts < /dev/null 2>/dev/null | openssl x509 -outform PEM
   ```
3. Provide certificate in config

### Memory Issues

**Symptom:** Task creation fails or crashes

**Solutions:**
1. Reduce `mqtt_cfg.buffer.size` (currently 4096)
2. Increase `mqtt_cfg.task.stack_size` (currently 6144)
3. Check free heap: `esp_get_free_heap_size()`

## Performance Metrics

- **Query latency**: ~100-200ms (printer response time)
- **MQTT overhead**: ~2KB RAM for client
- **Message size**: Typical response 2-8KB JSON
- **CPU impact**: Minimal (handled by MQTT task, priority 4)

## Next Steps

### Planned Enhancements

1. **Automatic Polling**: Send queries on timer (every 1-5 seconds)
2. **Command UI**: Add pause/resume/stop buttons
3. **Print Progress**: Visual progress bar
4. **File List**: Query and display available files
5. **Camera Feed**: MJPEG stream integration (if supported)

### Related Documentation

- `BAMBU_QUICKSTART.md` - Integration guide
- `BAMBU_TECHNICAL_DESIGN.md` - Component architecture
- `BAMBU_INTEGRATION_STATUS.md` - Current status
- `components/BambuMonitor/BambuMonitor.cpp` - Implementation

## References

- Bambu Lab MQTT Protocol (reverse-engineered)
- Home Assistant Bambu Lab integration
- ESP-IDF MQTT Client documentation
