# Storage Health & Config Backup - Quick Reference

## 🎯 What We Fixed

### Problem 1: Config File Corruption
**Before**: Settings.json would get corrupted, causing "Failed to parse JSON" errors and system instability
**After**: Automatic backup created on every write; auto-restore when corruption detected

### Problem 2: MQTT Task Creation Failures
**Before**: "Error create mqtt task" ×3 on startup, printers couldn't connect
**After**: Optimized buffer sizes, all printers connect successfully

### Problem 3: JSON Parse Error Spam
**Before**: 60+ error messages per minute flooding logs
**After**: Rate-limited to 2 errors per minute with enhanced debugging info

## 🔧 How It Works

### Config Backup System
```
Write Config Flow:
1. User changes setting in web UI
2. SettingsConfig creates backup: settings.json → settings.json.backup
3. SettingsConfig writes new config to settings.json
4. Both files now exist on SD card

Load Config Flow (Normal):
1. System boots
2. SettingsConfig reads settings.json
3. Validates JSON with cJSON_Parse()
4. Config loaded successfully

Load Config Flow (Corrupted):
1. System boots
2. SettingsConfig tries settings.json → JSON INVALID
3. SettingsConfig reads settings.json.backup → JSON VALID
4. Backup restored to settings.json
5. Config loaded from backup
6. Log: "Restored config from backup"

Load Config Flow (Both Corrupted):
1. System boots
2. SettingsConfig tries settings.json → JSON INVALID
3. SettingsConfig tries settings.json.backup → JSON INVALID
4. SettingsConfig creates default config
5. Log: "Using default configuration"
```

### Storage Health Monitoring
```
Background Task (every 60 seconds):
1. Check error counters
2. If errors > 0: Log current counts
3. If SD errors >= 5: Log "SD card error threshold reached"
4. If SPIFFS errors >= 10: Log "SPIFFS error threshold reached"

On Storage Error:
1. BambuMonitor detects errno 5 or 257
2. Calls storage_health_record_sd_error()
3. Counter incremented
4. If threshold reached: Warning logged
5. After 60 seconds: Counter auto-resets
```

## 🧪 Testing Commands

### Test 1: Corrupt Config and Auto-Restore
```bash
# SSH to device or use web UI file editor
echo '{"invalid": "json' > /sdcard/settings.json

# Reboot device
# Watch serial monitor for:
# W (xxx) SettingsConfig: Failed to parse JSON config
# I (xxx) SettingsConfig: Restoring config from backup
# I (xxx) SettingsConfig: Config loaded successfully

# Verify settings still present in web UI
```

### Test 2: Force SD Card Errors
```bash
# While device is running, remove SD card
# Wait for 5 write attempts (watch serial output)
# Expected log:
# W (xxx) StorageHealth: SD card error threshold reached (5 errors in 60s)

# Re-insert SD card
# After 60 seconds, counter should reset
```

### Test 3: Verify Backup File Creation
```bash
# Via web UI: Add a new printer or change a setting
# SSH to device:
ls -lh /sdcard/settings.json*

# Should see both files:
# -rw-r--r-- 1 root root  1.2K Jan  1 12:00 settings.json
# -rw-r--r-- 1 root root  1.2K Jan  1 11:59 settings.json.backup

# Verify sizes are similar (backup is previous version)
```

### Test 4: Check Storage Health Status
```bash
# Watch serial monitor for periodic logs:
grep "StorageHealth" /dev/cu.usbmodem21201

# Expected every 60 seconds:
# I (xxx) StorageHealth: Status - SD errors: 0, SPIFFS errors: 0

# After errors:
# I (xxx) StorageHealth: Status - SD errors: 2, SPIFFS errors: 0
```

## 📊 Monitoring

### Key Log Messages to Watch

**Config Backup/Restore**:
```
I (xxx) SettingsConfig: Config loaded successfully          # Normal boot
W (xxx) SettingsConfig: Failed to parse JSON config        # Corruption detected
I (xxx) SettingsConfig: Restoring config from backup       # Auto-restore triggered
I (xxx) SettingsConfig: Config restored from backup        # Restore successful
W (xxx) SettingsConfig: Backup also corrupted             # Both files bad
I (xxx) SettingsConfig: Using default configuration        # Fallback to defaults
```

**Storage Health**:
```
I (xxx) ESP32-TUX: Storage health monitor task started     # Task started
I (xxx) StorageHealth: Status - SD errors: 0, SPIFFS errors: 0  # Periodic check
W (xxx) StorageHealth: SD card error threshold reached (5 errors in 60s)  # Threshold hit
W (xxx) BambuMonitor: SD write failed (errno=257)          # Individual error logged
```

**MQTT Connections**:
```
I (xxx) BambuMonitor: [0] Connecting to 10.13.13.107:8883  # Connection attempt
I (xxx) BambuMonitor: Waiting 5 seconds before next connection  # Stagger delay
I (xxx) BambuMonitor: [0] MQTT connected                   # Success
E (xxx) mqtt_client: Error create mqtt task                # FAILURE (should not appear)
```

**JSON Parse Errors** (rate-limited):
```
W (xxx) BambuMonitor: [1] Failed to parse JSON (data_len=8158, first 50 chars: {...})
# Only appears once per 30 seconds per printer
```

## 🔍 Debugging

### Config Not Loading?
1. Check if settings.json exists: `ls -l /sdcard/settings.json`
2. Check if backup exists: `ls -l /sdcard/settings.json.backup`
3. Try manual validation: `cat /sdcard/settings.json | python -m json.tool`
4. If both corrupt, delete both files and reboot (will create defaults)

### Storage Errors Appearing?
1. Check SD card health: Try different card
2. Check connections: Reseat SD card
3. Check free space: `df -h /sdcard`
4. Monitor error patterns: Are errors clustered or random?

### MQTT Not Connecting?
1. Check heap memory: Look for memory allocation errors
2. Check connection stagger: Should see "Waiting 5 seconds" between attempts
3. Check network: Are printers reachable from device?
4. Check certificates: Verify Bambu certs are valid

## 📱 Web UI Integration (Future)

Planned API endpoint for storage health:
```javascript
GET /api/storage/health

Response:
{
  "sd_errors": 0,
  "spiffs_errors": 0,
  "sd_mounted": true,
  "spiffs_mounted": true,
  "last_sd_error_time": 0,
  "last_spiffs_error_time": 0,
  "uptime_seconds": 3600
}
```

Planned settings page section:
```
[Storage Health]
SD Card:     ✓ Healthy (0 errors)
SPIFFS:      ✓ Healthy (0 errors)
Config:      ✓ Valid (backup available)
Last Check:  2 minutes ago

[Actions]
[Check Health Now]  [Format SD Card]  [Restore from Backup]
```

## 🚨 Troubleshooting

### "Failed to parse JSON" on every boot
**Solution**: Both config files corrupted. Delete and recreate:
```bash
rm /sdcard/settings.json /sdcard/settings.json.backup
# Reboot - will create default config
```

### "SD card error threshold reached" frequently
**Solution**: SD card failing. Replace card with high-quality industrial-grade SD card.

### MQTT connections fail intermittently
**Solution**: Check heap usage. May need to reduce number of simultaneous printers or increase connection delay further.

### Backup file not being created
**Solution**: Check SD card write permissions and free space. Ensure SettingsConfig is being called for all config changes.

## 📚 Related Documentation

- **Technical Details**: `STORAGE_HEALTH_STATUS.md`
- **Bambu Integration**: `BAMBU_TECHNICAL_DESIGN.md`
- **Build Instructions**: `FLASHING_GUIDE.md`
- **Overall Status**: `BAMBU_INTEGRATION_STATUS.md`
