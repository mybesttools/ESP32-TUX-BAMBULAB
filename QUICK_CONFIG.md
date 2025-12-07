# Quick Configuration Reference

## Your Printer Details - Ready to Copy/Paste

### For Web Interface (http://10.13.13.68)

```
Name: Bambu X1C
IP Address: 10.13.13.85  
Access Code: 5d35821c
Serial Number: 00M09D530200738
Enable Printer: ✓ (checked)
Disable SSL Verify: ✓ (checked)
```

### For Direct Config File

If you need to manually edit `/spiffs/settings.json`:

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

## What Was Fixed

1. **Serial Number Field**: Now editable - you can type the serial directly
2. **Validation**: Serial is now optional - can add printer without it
3. **SSL Verification**: Properly defaults to disabled (checked)

## Steps to Configure

1. Open browser: `http://10.13.13.68`
2. Scroll to "Bambu Lab Printers" section
3. Fill in the form (copy values above)
4. Click "Add Printer"
5. Restart ESP32 or wait for auto-connect

## Testing After Configuration

Once configured, monitor the ESP32 serial output for:

```
I (xxxxx) BambuHelper: Using printer: Bambu X1C at 10.13.13.85
I (xxxxx) BambuMonitor: Bambu Monitor configured
I (xxxxx) BambuMonitor: MQTT Connected to Bambu printer
I (xxxxx) BambuMonitor: Subscribed to device/+/report
```

Then press the "Send Query" button in the UI to test!
