# ESP-IDF sdkconfig Settings Guide

This document describes the critical ESP-IDF configuration settings required for ESP32-TUX that are **not committed to the repository** and must be manually configured or preserved across builds.

## Overview

The `sdkconfig` file is generated during the build process and contains platform-specific configurations. While `sdkconfig.defaults*` files are committed, the actual `sdkconfig` adapts to your local environment and must be configured correctly for the firmware to work properly.

## Critical Settings

### 1. System Event Task Stack Size

**Setting:** `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`  
**Current Value:** `4096` (bytes)  
**Default Value:** `2304` (bytes)  
**Reason:** The WiFi event handler (`sys_evt` task) requires more stack space, especially when processing IP_EVENT_STA_GOT_IP and adding network information to settings. Stack overflow occurs at 2304 bytes when these operations complete.

**How to set:**

```bash
idf.py menuconfig
# Navigate to: Component config → ESP System Settings → Size of the system event task stack
# Change from 2304 to 4096
```

**Symptoms of insufficient stack:**

- Stack overflow panic in `sys_evt` task
- Device reboots after WiFi connects
- Error: `A stack overflow in task sys_evt has been detected`

---

### 2. WiFi Configuration

**Settings:**

- `CONFIG_ESP_WIFI_ENABLE=y` - WiFi support enabled
- `CONFIG_ESP_WIFI_STA_SUPPORT=y` - Station mode (client)
- `CONFIG_ESP_WIFI_AP_SUPPORT=y` - AP mode for provisioning

**How to verify:**

```bash
grep "CONFIG_ESP_WIFI" sdkconfig | head -20
```

---

### 3. NVS Flash Partition Size

**Setting:** `CONFIG_NVS_ENCRYPTION=y` (optional but recommended)  
**Used for:** Storing WiFi credentials, system settings, certificates

**How to verify:**

```bash
idf.py monitor
# Look for: "I (1228) SettingsConfig: Using settings from SPIFFS: /spiffs/settings.json"
```

---

### 4. Partition Table

**File:** `partitions/partition-*.csv` (committed)  
**Current:** Uses 8MB partition scheme for WT32-SC01-Plus (ESP32-S3 with 8MB PSRAM)

**Partition Layout:**

```
nvs             (WiFi data)        16 KB
otadata         (OTA data)          8 KB
phy_init        (RF calibration)    4 KB
factory         (factory app)    2084 KB
ota_0           (OTA app 0)     2084 KB
ota_1           (OTA app 1)     2084 KB
storage         (SPIFFS/FAT)     400 KB
```

**How to check active partition:**

```bash
idf.py menuconfig
# Navigate to: Partition Table → Partition Table → Select your device's partition scheme
```

---

### 5. Display & UI Configuration

**Settings:**

- `CONFIG_LV_COLOR_DEPTH_16=y` - 16-bit color for LovyanGFX
- `CONFIG_LV_MEM_CUSTOM=y` - Use PSRAM for LVGL memory
- `CONFIG_LV_MEM_SIZE_KILOBYTES=1024` - LVGL heap size (1 MB)

**How to verify:**

```bash
grep "CONFIG_LV" sdkconfig | grep -E "(COLOR|MEM)"
```

---

### 6. Logging Level

**Setting:** `CONFIG_LOG_DEFAULT_LEVEL`  
**Current:** `3` (INFO level)  
**Range:** 0 (NONE) to 5 (VERBOSE)

**How to adjust for debugging:**

```bash
idf.py menuconfig
# Navigate to: Component config → Log output → Default log verbosity
# Set to: DEBUG (4) for troubleshooting
```

**Recommended levels:**

- **Production:** 2 (WARN) - minimal log output
- **Testing:** 3 (INFO) - standard operation
- **Debugging:** 4 (DEBUG) - detailed troubleshooting

---

## Device-Specific Configuration

### WT32-SC01-Plus (ESP32-S3, 8MB)

Required settings for this device:

```bash
# Target & Flash
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_OCT_FLASH=y
CONFIG_ESPTOOLPY_FLASH_SIZE="8MB"
CONFIG_ESPTOOLPY_PSRAM_SIZE="2MB"

# Partition table for 8MB device
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/partition-8MB.csv"

# System event stack (critical for WiFi)
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096
```

### WT32-SC01 (ESP32, 4MB)

Use partition file: `partitions/partition-4MB.csv`

```bash
CONFIG_IDF_TARGET="esp32"
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/partition-4MB.csv"
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096
```

---

## Troubleshooting Configuration Issues

### Issue: WiFi connects but device reboots

**Cause:** Stack overflow in `sys_evt` task  
**Fix:** Increase `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` to 4096

```bash
idf.py menuconfig
# Component config → ESP System Settings → Size of the system event task stack
# Set to: 4096
idf.py build
idf.py flash
```

---

### Issue: "Failed to initialize the card" SD card error

**This is expected** if no SD card is present. The firmware falls back to SPIFFS.

```
E (1208) vfs_fat_sdmmc: sdmmc_card_init failed (0x107).
I (1228) ESP32-TUX: Using SPIFFS for config storage (SD card not available)
```

No configuration needed - this is normal operation.

---

### Issue: Insufficient memory for LVGL

**Cause:** `CONFIG_LV_MEM_SIZE_KILOBYTES` too small  
**Fix:**

```bash
idf.py menuconfig
# Component config → LVGL configuration → LVGL memory settings
# Increase CONFIG_LV_MEM_SIZE_KILOBYTES to 1024 or higher
```

---

## Resetting to Defaults

To reset configuration to project defaults:

```bash
# Clean everything and rebuild
rm -rf build sdkconfig
idf.py fullclean
idf.py build

# Or just reset sdkconfig
rm sdkconfig
idf.py reconfigure
```

This will use values from `sdkconfig.defaults` and any device-specific overrides.

---

## Verifying Current Configuration

```bash
# View all non-default settings
grep -E "^CONFIG_" sdkconfig | sort

# Check specific critical settings
grep "CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE" sdkconfig
grep "CONFIG_LV_MEM_SIZE_KILOBYTES" sdkconfig
grep "CONFIG_IDF_TARGET" sdkconfig

# Check what partition table is in use
grep "CONFIG_PARTITION_TABLE" sdkconfig
```

---

## When sdkconfig Changes Are Required

| Scenario | Action | Critical? |
|----------|--------|-----------|
| Switching devices (ESP32 ↔ ESP32-S3) | Update partition table, flash size | ✅ Yes |
| WiFi stack overflow crashes | Increase `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` | ✅ Yes |
| Adding features (logging, debugging) | Adjust `CONFIG_LOG_DEFAULT_LEVEL` | ❌ No |
| Optimizing memory usage | Tune `CONFIG_LV_MEM_SIZE_KILOBYTES` | ⚠️ Maybe |
| Changing UI colors | Adjust `CONFIG_LV_COLOR_DEPTH_*` | ❌ No |

---

## Related Documentation

- [ESP-IDF Configuration System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/kconfig.html)
- [LVGL Configuration](https://docs.lvgl.io/8.3/overview/integration.html)
- [FreeRTOS Task Stack Sizing](https://www.freertos.org/RTOS-task-stack-size.html)
