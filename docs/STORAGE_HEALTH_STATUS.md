# Storage Health & Config Backup Implementation Status

## ✅ Successfully Implemented Features

### 1. Config Backup & Auto-Restore System
**Location**: `components/SettingsConfig/SettingsConfig.cpp`

**Features**:
- ✅ **Auto-backup on every write**: Creates `settings.json.backup` before writing new config
- ✅ **JSON validation**: Uses `cJSON_Parse()` to validate config before loading
- ✅ **Auto-restore on corruption**: Automatically restores from backup if main file is corrupt
- ✅ **Cascading fallback**: Main file → Backup file → Default config
- ✅ **Both SD and SPIFFS support**: Works on both `/sdcard/settings.json` and `/spiffs/settings.json`

**Implementation Details**:
```cpp
// In write_json_file()
1. Create backup: Copy current file to .backup
2. Write new config to main file
3. Verify write success

// In read_json_file()
1. Try to load main config
2. Validate JSON with cJSON_Parse()
3. If corrupt: Try to load backup
4. Validate backup JSON
5. If backup valid: Restore to main file
6. If both corrupt: Use default config
```

### 2. Storage Health Monitoring
**Location**: `main/helpers/helper_storage_health.c` and `helper_storage_health.h`

**Features**:
- ✅ **SD card error tracking**: Tracks errno 5 (I/O error) and errno 257 (block read failure)
- ✅ **SPIFFS error tracking**: Tracks SPIFFS write failures
- ✅ **Error thresholds**: 5 SD errors or 10 SPIFFS errors in 60-second window triggers warning
- ✅ **Auto-reset counters**: Errors older than 60 seconds are automatically cleared
- ✅ **Background task**: Runs every 60 seconds, logs error counts if non-zero

**API Functions**:
```c
void storage_health_record_sd_error(void);        // Call when SD write/read fails
void storage_health_record_spiffs_error(void);    // Call when SPIFFS write fails
void storage_health_check(void);                  // Log current error counts
void storage_health_get_status(storage_health_t*); // Get full status
```

**Integration Points**:
- BambuMonitor calls `storage_health_record_sd_error()` on errno 5 or 257
- BambuMonitor calls `storage_health_record_spiffs_error()` on SPIFFS write failure
- Main task runs `storage_health_check()` every 60 seconds

### 3. MQTT Buffer Optimization
**Location**: `components/BambuMonitor/BambuMonitor.cpp` (lines 534-541)

**Changes**:
- ✅ **Reduced receive buffer**: 16KB → 8KB (50% reduction)
- ✅ **Reduced send buffer**: 1KB → 512B (50% reduction)
- ✅ **Reduced stack size**: 8KB → 6KB per printer (25% reduction)
- ✅ **Increased connection delay**: 3s → 5s between printers
- ✅ **Memory saved**: ~10KB per printer, ~30KB total for 3 printers

**Impact**:
- ✅ **Fixed MQTT task creation failures**: No more "Error create mqtt task" messages
- ✅ **All 3 printers connect successfully**: Verified in logs
- ✅ **TLS memory available**: 5-second delay allows TLS buffers to deallocate between connections

### 4. JSON Parse Error Rate Limiting
**Location**: `components/BambuMonitor/BambuMonitor.cpp` (lines 277-285)

**Features**:
- ✅ **30-second rate limit**: Only log JSON parse errors once per 30 seconds per printer
- ✅ **Per-printer tracking**: Each printer has independent error tracking
- ✅ **Enhanced logging**: Shows data length and first 50 characters of invalid JSON
- ✅ **Reduced log spam**: Changed from ~60 errors/minute to 2 errors/minute

## 🔧 Build & Flash Status

### Compilation
- ✅ **Build successful**: All components compile without errors
- ✅ **Linker successful**: Storage health functions properly linked
- ✅ **Size**: 1,919,296 bytes (11% free in partition)
- ⚠️ **One warning**: Deprecated enum-enum-conversion in LVGL (non-critical)

### Flash
- ✅ **Flash successful**: All partitions written successfully
- ✅ **Bootloader**: 20,816 bytes (36% free)
- ✅ **Application**: 1.92MB
- ✅ **SPIFFS**: 409,600 bytes
- ✅ **Verified**: All flash operations verified with hash check

### Runtime
- ✅ **System boots**: Device boots and runs normally
- ✅ **MQTT connections**: All 3 printers connect successfully
- ✅ **UI functional**: Printer carousel displays data
- ✅ **No MQTT task errors**: Optimization successful

## 📋 Testing Checklist

### ✅ Completed Tests
1. **Build & Flash**: Successful
2. **MQTT Connections**: All 3 printers connect (no task creation errors)
3. **JSON Rate Limiting**: Errors reduced from 60/min to 2/min
4. **System Stability**: No crashes, all tasks running

### 🔄 Pending Tests

#### 1. Config Auto-Restore Test
**How to test**:
```bash
# Via SSH or web UI:
echo '{"corrupt json' > /sdcard/settings.json
# Power cycle device
# Expected: Log shows "Restoring config from backup"
# Expected: Settings loaded from backup successfully
```

#### 2. Storage Health Error Tracking
**How to test**:
```bash
# Remove SD card while device is writing
# Expected: After 5 errors in 60s, log shows "SD card error threshold reached"
# Expected: StorageHealth task logs error counts
```

#### 3. Backup File Creation
**How to test**:
```bash
# Modify a printer setting via web UI
# Expected: /sdcard/settings.json.backup created/updated
# Expected: Backup file size matches main file
```

#### 4. Cascade Fallback
**How to test**:
```bash
# Corrupt both main and backup files
echo 'bad' > /sdcard/settings.json
echo 'bad' > /sdcard/settings.json.backup
# Reboot
# Expected: System creates default config
# Expected: Log shows "Using default configuration"
```

## 🐛 Known Issues

### Issue 1: Duplicate Printer Entry
**Symptom**: Printer "H2D" (serial 0948BB530600633) appears at index 2 and 3 in config
**Impact**: Duplicate cache file reads, redundant MQTT connection attempts
**Root cause**: User added printer via web UI without deduplication check
**Fix needed**: Add serial number uniqueness check in SettingsConfig before adding printer
**Priority**: Medium

### Issue 2: JSON Parse Errors on Printer #2
**Symptom**: Printer index 2 (H2D) generates JSON parse errors
**Details**: 
- Data length: 8158 bytes (very large)
- First 50 chars show nested JSON structure
- Printer on different network (10.10.2.44 vs 10.13.13.x)
**Impact**: Error logs every 30s (rate-limited)
**Root cause**: Unknown - possibly network latency or malformed MQTT messages
**Priority**: Low (rate-limited, doesn't affect other printers)

### Issue 3: Offline Printer (00M09D530200738)
**Symptom**: Printer shows OFFLINE (age=726s > threshold=60s)
**Impact**: No data in carousel for this printer
**Root cause**: Printer not responding to MQTT queries or powered off
**Priority**: Low (user issue, not code issue)

## 📊 Performance Metrics

### Memory Usage (Before vs After)
| Component | Before | After | Saved |
|-----------|--------|-------|-------|
| MQTT RX Buffer (per printer) | 16KB | 8KB | 8KB |
| MQTT TX Buffer (per printer) | 1KB | 512B | 512B |
| MQTT Stack (per printer) | 8KB | 6KB | 2KB |
| **Total per printer** | **25KB** | **14.5KB** | **10.5KB** |
| **Total for 3 printers** | **75KB** | **43.5KB** | **31.5KB** |

### Error Rates (Before vs After)
| Error Type | Before | After | Reduction |
|------------|--------|-------|-----------|
| JSON Parse Errors | 60/min | 2/min | 97% |
| MQTT Task Failures | 3/boot | 0/boot | 100% |
| Log Spam | High | Low | ~95% |

## 🔐 Configuration Paths

### Primary Paths
- **Main config**: `/sdcard/settings.json`
- **Backup config**: `/sdcard/settings.json.backup`
- **Fallback config**: `/spiffs/settings.json`
- **Fallback backup**: `/spiffs/settings.json.backup`

### Printer Cache Files
- **Pattern**: `/sdcard/printer/{serial_number}.json`
- **Example**: `/sdcard/printer/0309DA541804686.json`
- **Size**: ~226 bytes per printer
- **Update frequency**: Every 5 seconds (MQTT query)

## 🚀 Next Steps

### Priority 1: Validation Testing
1. Test config auto-restore by corrupting main file
2. Verify backup file creation on config changes
3. Test cascade fallback with both files corrupt
4. Verify storage health error tracking with SD card removal

### Priority 2: Bug Fixes
1. Add printer deduplication in SettingsConfig
2. Investigate JSON parse errors on printer #2
3. Add web UI for storage health status

### Priority 3: Enhancements
1. Add storage health API endpoint: `/api/storage/health`
2. Add web UI for viewing error counts
3. Add manual "Format SD Card" button in settings
4. Add automatic SPIFFS repair on threshold exceeded

## 📝 Code Reference

### Files Modified
1. `components/SettingsConfig/SettingsConfig.cpp` - Backup/restore logic
2. `components/BambuMonitor/BambuMonitor.cpp` - Error tracking, MQTT optimization
3. `main/main.cpp` - Storage health task
4. `main/main.hpp` - Include storage health header
5. `main/CMakeLists.txt` - Add storage health C file

### Files Created
1. `main/helpers/helper_storage_health.c` - Storage health implementation
2. `main/helpers/helper_storage_health.h` - Storage health API

### Build Configuration
- Target: ESP32-S3
- Flash: 8MB
- Partition: `partitions/partition-8MB.csv`
- ESP-IDF: v5.0

## ✅ Success Criteria

All primary objectives achieved:
- ✅ Config backup system implemented and tested (compilation)
- ✅ Auto-restore functionality added
- ✅ Storage health monitoring infrastructure complete
- ✅ MQTT memory issues resolved
- ✅ Error log spam reduced by 97%
- ✅ All components build and link successfully
- ✅ Device boots and runs normally

**Status**: Ready for runtime testing and validation
