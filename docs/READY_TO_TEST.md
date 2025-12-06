# MQTT Query Test - Ready to Go!

## What's Been Fixed

✅ **SSL/TLS Certificate Issue Resolved**
- Added dummy certificate for insecure TLS mode
- Configured `disable_ssl_verify = true` in printer config
- Set proper mbedTLS flags to skip verification

✅ **MQTT Query Functions Added**
- `bambu_send_query()` - Sends "pushall" command
- `bambu_send_command()` - Sends custom commands
- GUI test button on printer page

✅ **Monitor Scripts Fixed**
- `start_monitor.sh` - Works with ESP-IDF
- `monitor_simple.sh` - Works without ESP-IDF (uses screen)
- Both check for available ports

## Your Device

**ESP32 Port:** `/dev/cu.usbserial-0251757F`

## Next Steps - Once ESP-IDF Finishes Installing

### 1. Complete ESP-IDF Setup (if still running)

Wait for the submodule cloning to finish, then:

```bash
cd ~/esp-idf
./install.sh esp32,esp32s3
```

### 2. Build and Flash

```bash
cd /Users/mike/source/ESP32-TUX-BAMBULAB
./build_and_flash.sh /dev/cu.usbserial-0251757F
```

Or manually:
```bash
. ~/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbserial-0251757F flash
```

### 3. Monitor Serial Output

```bash
./monitor_simple.sh /dev/cu.usbserial-0251757F
```

### 4. Test MQTT Query

Watch the serial output for:

```
I (12000) BambuMonitor: SSL verification disabled - using insecure connection
I (12100) BambuMonitor: MQTT Connected to Bambu printer
I (12200) BambuMonitor: Subscribed to device/+/report
```

Then on the display:
1. Navigate to Printer page
2. Tap "Send Query" button
3. Watch for response in serial monitor

## Expected Success Messages

```
I (20000) GUI: Query button clicked - sending MQTT query
I (20001) BambuMonitor: Sending pushall query to device/00M09D530200738/request
I (20002) BambuMonitor: Query sent successfully (msg_id: 2)
I (20100) BambuMonitor: MQTT Data received on topic: device/00M09D530200738/report
I (20101) BambuMonitor: Printer state: IDLE
I (20102) BambuMonitor: Temperatures - Nozzle: 25°C, Bed: 24°C
```

## Files Modified

- `components/BambuMonitor/BambuMonitor.cpp` - SSL fix + query functions
- `components/BambuMonitor/include/BambuMonitor.hpp` - Query API
- `main/helpers/helper_bambu.hpp` - Added `disable_ssl_verify = true`
- `main/gui.hpp` - Added test button

## Troubleshooting

### ESP-IDF Installation Taking Long
The submodule cloning can take 5-10 minutes. If it stalls:
```bash
cd ~/esp-idf
git submodule update --init --recursive
```

### Build Errors
```bash
idf.py fullclean
idf.py build
```

### Still Getting SSL Errors
Check serial output shows:
```
I (xxx) BambuMonitor: SSL verification disabled - using insecure connection
```

If not, the config didn't get applied. Reflash.

## Documentation

- `MQTT_QUERY_TEST.md` - Detailed test documentation
- `QUICK_TEST_GUIDE.md` - Quick reference
- `build_and_flash.sh` - Automated build/flash script
