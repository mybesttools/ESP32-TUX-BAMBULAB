# MQTT Query Testing Guide

## Quick Test with Python Script

### Prerequisites

```bash
# Install paho-mqtt if not already installed
pip3 install paho-mqtt
```

### Get Your Printer Information

You need three pieces of information:

1. **Printer IP Address** - Find in printer settings or your router
2. **Access Code** - From printer: Settings → Network → MQTT → Access Code
3. **Serial Number** - From printer: Settings → Device → Serial Number

### Run the Test

```bash
cd /Users/mikevandersluis/source/ESP32-TUX-BAMBULAB

# Replace with your actual values
./scripts/test_mqtt.py <IP> <ACCESS_CODE> <SERIAL> --no-verify
```

**Example:**
```bash
./scripts/test_mqtt.py 192.168.1.100 12345678 01234567890ABCD --no-verify
```

### Expected Output

```
============================================================
Bambu Lab MQTT Connection Test
============================================================
Printer IP: 192.168.1.100
Port: 8883
Serial: 01234567890ABCD
SSL Verify: False
============================================================
⚠ SSL verification disabled

→ Connecting to 192.168.1.100:8883...
✓ Connected to Bambu printer MQTT broker
✓ Subscribed to device/+/report
✓ Subscription acknowledged (QoS: [1])

→ Sending query to device/01234567890ABCD/request
  Payload: {"pushing": {"sequence_id": "0", "command": "pushall"}}
✓ Query published (msg_id: 1)

✓ Received message on topic: device/01234567890ABCD/report
  Message length: 12584 bytes

✓ JSON parsed successfully
  Keys: ['print']

  Print status:
    State: IDLE
    Progress: 0%
    Nozzle temp: 24.5°C
    Bed temp: 24.1°C

✓ Full response saved to /tmp/bambu_response.json
```

The full JSON response will be saved to `/tmp/bambu_response.json` for inspection.

---

## ESP32 Configuration

### 1. Configure Printer in Settings

The ESP32 needs printer configuration in the settings JSON file. The settings are stored in SPIFFS at `/spiffs/config.json`.

**Example config.json structure:**

```json
{
  "devicename": "ESP32-TUX",
  "settings": {
    "brightness": 200,
    "theme": "dark",
    "timezone": "-5:00"
  },
  "printers": [
    {
      "name": "My X1C",
      "ip_address": "192.168.1.100",
      "token": "12345678",
      "serial": "01234567890ABCD",
      "enabled": true,
      "disable_ssl_verify": true
    }
  ]
}
```

### 2. Build and Flash

```bash
source ~/esp-idf/export.sh
idf.py build
idf.py -p /dev/tty.usbserial-* flash monitor
```

### 3. Monitor Logs

Look for these log messages:

```
I (12345) BambuHelper: Initializing Bambu Monitor helper
I (12346) BambuHelper: Using printer: My X1C at 192.168.1.100
I (12347) BambuMonitor: Bambu Monitor configured (MQTT will connect after WiFi is ready)
I (15678) BambuMonitor: Starting MQTT client connection to printer
I (15890) BambuMonitor: MQTT Connected to Bambu printer
I (15891) BambuMonitor: Subscribed to device/+/report (msg_id: 12345)
```

### 4. Test Query Button

Once connected, press the "Send Query" button on the Bambu printer page in the UI.

**Expected log:**
```
I (23456) BambuMonitor: Sending pushall query to device/01234567890ABCD/request
I (23457) BambuMonitor: Query sent successfully (msg_id: 67890)
I (23678) BambuMonitor: MQTT Data received on topic: device/01234567890ABCD/report (len: 41, data_len: 12584)
I (23680) BambuMonitor: Printer state: IDLE
I (23682) BambuMonitor: Temperatures - Nozzle: 24°C, Bed: 24°C
```

---

## Troubleshooting

### MQTT Client Not Initialized

**Error:**
```
E (12345) BambuMonitor: MQTT client not initialized
```

**Fix:** Check that printer is configured in settings and `bambu_helper_init()` succeeded.

### No Printer Configured

**Error:**
```
W (12345) BambuHelper: No printer configured in settings
```

**Fix:** Add printer configuration to `config.json` (see example above).

### Connection Timeout

**Error:**
```
E (12345) BambuMonitor: Failed to start MQTT client: ESP_ERR_TIMEOUT
```

**Fix:**
- Verify printer IP is correct and reachable
- Check that printer is on the same network
- Ping the printer: `ping <PRINTER_IP>`

### SSL Handshake Failed

**Error:**
```
E (12345) esp-tls-mbedtls: mbedtls_ssl_handshake returned -0x7100
```

**Fix:** Ensure `disable_ssl_verify` is set to `true` in printer config.

### Invalid Topic / Serial Not Set

**Error:**
```
I (12345) BambuMonitor: Sending pushall query to device/unknown/request
```

**Fix:** Serial number is not configured. Add `"serial"` field to printer config.

### Buffer Size Too Small

**Error:**
```
E (12345) MQTT_CLIENT: mqtt_message_receive: mqtt_message_buffer_length insufficient
```

**Fix:** Buffer size already increased to 16KB in current code. If still seeing this, check sdkconfig for memory constraints.

---

## MQTT Protocol Reference

### Topics

**Subscribe (ESP32 → Printer):**
- `device/+/report` - Wildcard to receive from any printer

**Publish (ESP32 → Printer):**
- `device/{SERIAL}/request` - Send commands/queries to specific printer

### Commands

**Request Full Status:**
```json
{
  "pushing": {
    "sequence_id": "0",
    "command": "pushall"
  }
}
```

**Pause Print:**
```json
{
  "print": {
    "command": "pause"
  }
}
```

**Resume Print:**
```json
{
  "print": {
    "command": "resume"
  }
}
```

**Stop Print:**
```json
{
  "print": {
    "command": "stop"
  }
}
```

### Response Structure

The printer responds on `device/{SERIAL}/report` with a large JSON payload containing:

```json
{
  "print": {
    "gcode_state": "IDLE",
    "mc_percent": 0,
    "nozzle_temper": 24.5,
    "bed_temper": 24.1,
    "chamber_temper": 23.8,
    "mc_remaining_time": 0,
    "layer_num": 0,
    "total_layer_num": 0,
    // ... many more fields
  }
}
```

See `/tmp/bambu_response.json` after running the Python test script for full structure.

---

## Next Steps

1. **Test with Python script** to verify printer connectivity
2. **Configure printer in settings** JSON file
3. **Build and flash** ESP32 firmware
4. **Monitor logs** to verify MQTT connection
5. **Test query button** in UI to confirm functionality
6. **Parse response** and update carousel with live data

## Files Modified for MQTT Query

- `components/BambuMonitor/BambuMonitor.cpp` - Query implementation
- `components/BambuMonitor/include/BambuMonitor.hpp` - API declarations
- `main/helpers/helper_bambu.hpp` - Initialization helper
- `main/gui.hpp` - "Send Query" button (line ~981)
- `main/main.cpp` - Bambu monitor startup
