# MQTT Query Quick Start

## 1. Discovery (Find Your Printer)

```bash
./scripts/discover_printer.py
```

This will scan your network and find Bambu printers automatically.

## 2. Test Connection

```bash
./scripts/test_mqtt.py <IP> <ACCESS_CODE> <SERIAL> --no-verify
```

**Get these from printer:**

- **Access Code:** Settings → Network → MQTT → Access Code
- **Serial Number:** Settings → Device → Serial Number

**Example:**

```bash
./scripts/test_mqtt.py 192.168.1.100 12345678 01234567890ABCD --no-verify
```

## 3. Configure ESP32

Add to `config.json` (in SPIFFS):

```json
{
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

## 4. Build & Flash

```bash
source ~/esp-idf/export.sh
idf.py build
idf.py -p /dev/tty.usbserial-* flash monitor
```

## 5. Expected Behavior

**Startup logs:**

```
I BambuHelper: Using printer: My X1C at 192.168.1.100
I BambuMonitor: Bambu Monitor configured
I BambuMonitor: Starting MQTT client connection
I BambuMonitor: MQTT Connected to Bambu printer
I BambuMonitor: Subscribed to device/+/report
```

**Query logs (when button pressed):**

```
I BambuMonitor: Sending pushall query to device/01234567890ABCD/request
I BambuMonitor: Query sent successfully
I BambuMonitor: MQTT Data received (len: 12584)
I BambuMonitor: Printer state: IDLE
I BambuMonitor: Temperatures - Nozzle: 24°C, Bed: 24°C
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "No printer configured" | Add printer to `config.json` |
| "MQTT client not initialized" | Check `config.json` format and printer enabled |
| Connection timeout | Verify IP address and network connectivity |
| SSL handshake failed | Ensure `disable_ssl_verify: true` |
| "device/unknown/request" | Add `serial` field to config |

## Files Reference

- **Python test:** `scripts/test_mqtt.py`
- **Discovery:** `scripts/discover_printer.py`
- **Full guide:** `docs/MQTT_TESTING_GUIDE.md`
- **Code:** `components/BambuMonitor/BambuMonitor.cpp`
- **Helper:** `main/helpers/helper_bambu.hpp`
- **UI button:** `main/gui.hpp` (line ~981)
