# Quick Test Guide - MQTT Query Mechanism

## Setup Summary

### What We Added Today

1. **`bambu_send_query()`** - Sends "pushall" command to request full printer status
2. **`bambu_send_command()`** - Sends custom commands to printer
3. **UI Test Button** - "Send Query" button on printer page

### Installation Status

ESP-IDF is currently being installed. The setup script will:
- Install ESP-IDF v5.1
- Install toolchains for ESP32 and ESP32-S3
- Configure your environment

## Once ESP-IDF Installation Completes

### 1. Activate ESP-IDF

```bash
. ~/esp-idf/export.sh
```

Or if you added the alias:
```bash
get_idf
```

### 2. Configure Your Printer

Edit `main/helpers/helper_bambu.hpp` (around line 64):

```cpp
bambu_printer_config_t printer_config = {
    .device_id = (char*)"YOUR_SERIAL_HERE",      // Get from printer settings
    .ip_address = (char*)"YOUR_PRINTER_IP",      // e.g., "192.168.1.100"
    .port = 8883,
    .access_code = (char*)"YOUR_ACCESS_CODE",    // 8-digit code from printer
    .tls_certificate = NULL,
    .disable_ssl_verify = true                    // true = easier setup
};
```

### 3. Build the Project

```bash
cd /Users/mike/source/ESP32-TUX-BAMBULAB
. ~/esp-idf/export.sh
idf.py build
```

### 4. Flash to Device

```bash
# Find your port
ls /dev/cu.*

# Flash (replace port as needed)
idf.py -p /dev/cu.SLAB_USBtoUART flash monitor
```

Or use the quick flash script:
```bash
./flash_quick.sh /dev/cu.SLAB_USBtoUART
```

### 5. Test the Query Mechanism

**Via UI:**
1. Navigate to Printer page on the display
2. Tap "Send Query" button
3. Watch for response in serial monitor

**Expected Serial Output:**
```
I (12000) GUI: Query button clicked - sending MQTT query
I (12001) BambuMonitor: Sending pushall query to device/00M09D530200738/request
I (12002) BambuMonitor: Query sent successfully (msg_id: 2)
I (12100) BambuMonitor: MQTT Data received on topic: device/00M09D530200738/report
I (12101) BambuMonitor: Printer state: IDLE
I (12102) BambuMonitor: Temperatures - Nozzle: 25°C, Bed: 24°C
```

## Files Modified

- `components/BambuMonitor/include/BambuMonitor.hpp` - Added query API declarations
- `components/BambuMonitor/BambuMonitor.cpp` - Implemented query functions
- `main/gui.hpp` - Added test button to printer page
- `MQTT_QUERY_TEST.md` - Complete test documentation

## Troubleshooting

### ESP-IDF Not Found After Installation

```bash
# Activate ESP-IDF
. ~/esp-idf/export.sh

# Verify
idf.py --version
```

### Build Errors

```bash
# Clean build
idf.py fullclean
idf.py build
```

### MQTT Query Fails

1. Verify printer is online and on same network
2. Check serial number matches exactly
3. Verify access code is correct
4. Ensure WiFi is connected on ESP32

### No Response from Printer

1. Check MQTT connection status in logs
2. Verify subscription to `device/+/report` succeeded
3. Try manual test from terminal:
   ```bash
   mosquitto_pub -h PRINTER_IP -p 8883 \
     -u bblp -P ACCESS_CODE \
     -t "device/SERIAL/request" \
     -m '{"pushing":{"sequence_id":"0","command":"pushall"}}'
   ```

## Next Steps After Successful Test

1. **Automatic Polling** - Send queries on timer
2. **Command Buttons** - Add pause/resume/stop
3. **Progress Bar** - Visual print progress
4. **Live Updates** - Real-time temperature display

## Documentation

- `MQTT_QUERY_TEST.md` - Detailed test documentation
- `BAMBU_QUICKSTART.md` - Integration overview
- `BAMBU_TECHNICAL_DESIGN.md` - Architecture details
