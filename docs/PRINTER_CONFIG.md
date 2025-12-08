# Bambu Lab Printer Configuration

## Your Printer Details

- **Printer IP:** 10.13.13.85
- **Access Code:** 5d35821c
- **Serial Number:** 00M09D530200738
- **Current Status:** FINISH (100% complete, cooling down)
- **ESP32-TUX IP:** 10.13.13.68

## ✅ MQTT Connection Verified

The MQTT connection test was successful! Your printer is responding correctly to queries.

## Configuration Options

### Option 1: Web Interface (Recommended)

1. Open browser: `http://10.13.13.68`
2. Navigate to **Printer Settings** or **Configuration**
3. Click **Add Printer**
4. Enter the following:
   - **Name:** Bambu X1C
   - **IP Address:** 10.13.13.85
   - **Access Code:** 5d35821c
   - **Serial Number:** 00M09D530200738
   - **Enable:** ✓ (checked)
   - **Disable SSL Verify:** ✓ (checked)
5. Save and restart ESP32

### Option 2: Direct Config File Edit

If the ESP32 is accessible via serial or you can modify the SPIFFS partition:

**File:** `/spiffs/settings.json`

Add this section to the JSON file:

```json
{
  "devicename": "ESP32-TUX",
  "settings": {
    "brightness": 250,
    "theme": "dark",
    "timezone": "-5:00"
  },
  "printers": [
    {
      "name": "Bambu X1C",
      "ip_address": "10.13.13.85",
      "token": "5d35821c",
      "serial": "00M09D530200738",
      "enabled": true,
      "disable_ssl_verify": true
    }
  ],
  "networks": [
    {
      "name": "Local Network",
      "subnet": "10.13.13.0/24",
      "enabled": true
    }
  ]
}
```

### Option 3: Build with Pre-configured Settings

Before building, create the config file in `fatfs/settings.json` with the above content, then build and flash.

## Expected Behavior After Configuration

### 1. On Boot

```
I BambuHelper: Using printer: Bambu X1C at 10.13.13.85
I BambuMonitor: Bambu Monitor configured
I BambuMonitor: MQTT will connect after WiFi is ready
```

### 2. After WiFi Connect

```
I BambuMonitor: Starting MQTT client connection to printer
I BambuMonitor: MQTT Connected to Bambu printer
I BambuMonitor: Subscribed to device/+/report (msg_id: 12345)
```

### 3. When Query Button Pressed

```
I BambuMonitor: Sending pushall query to device/00M09D530200738/request
I BambuMonitor: Query sent successfully (msg_id: 67890)
I BambuMonitor: MQTT Data received (len: 15611)
I BambuMonitor: Printer state: FINISH
I BambuMonitor: Temperatures - Nozzle: 69°C, Bed: 58°C
```

## Testing Scripts Available

All scripts are in `scripts/` directory:

### 1. Get Serial Number
```bash
./scripts/get_printer_serial.py 10.13.13.85 5d35821c
```

### 2. Test MQTT Connection
```bash
./scripts/test_mqtt.py 10.13.13.85 5d35821c 00M09D530200738 --no-verify
```

### 3. Configure ESP32 (via web API)
```bash
./scripts/configure_esp32.py 10.13.13.68 10.13.13.85 5d35821c 00M09D530200738
```

### 4. Discover Printers on Network
```bash
./scripts/discover_printer.py --subnet 10.13.13.0/24
```

## Current Printer Status

Last query response (from `/tmp/bambu_response.json`):

- **State:** FINISH (print completed)
- **Progress:** 100%
- **Nozzle Temperature:** 69°C (cooling)
- **Bed Temperature:** 58°C (cooling)
- **Full JSON:** 15,611 bytes (detailed status available)

## Next Steps

1. **Configure the printer** using Option 1 (web interface) above
2. **Restart ESP32** or wait for automatic connection
3. **Monitor logs** to verify MQTT connection
4. **Test query button** in the Bambu page of the UI
5. **Update carousel** with live printer data

## Troubleshooting

### Web Interface Not Loading
- Ping ESP32: `ping 10.13.13.68`
- Check serial monitor for errors
- Verify web server started: look for "Web server started successfully"

### MQTT Not Connecting
- Verify printer IP hasn't changed
- Check access code is correct
- Ensure `disable_ssl_verify` is `true`
- Look for error logs with tag "BambuMonitor"

### No Data Received
- Press "Send Query" button manually
- Check serial number is correct in config
- Verify printer is powered on and connected to network

## Documentation

- **Quick Start:** `docs/MQTT_QUICK_START.md`
- **Full Testing Guide:** `docs/MQTT_TESTING_GUIDE.md`
- **Integration Status:** `docs/BAMBU_INTEGRATION_STATUS.md`
- **Technical Design:** `docs/BAMBU_TECHNICAL_DESIGN.md`
