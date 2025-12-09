# ESP-IDF Configuration Settings

This document lists all sdkconfig settings that differ from ESP-IDF defaults.

## Device and Hardware

| Setting | Value | Purpose |
|---------|-------|---------|
| `CONFIG_IDF_TARGET` | `esp32s3` | Target chip: ESP32-S3 |
| `CONFIG_TUX_DEVICE_WT32_SC01_PLUS` | `y` | WT32-SC01 Plus device configuration |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240` | `y` | 240MHz CPU frequency for performance |

## Flash and Storage

| Setting | Value | Purpose |
|---------|-------|---------|
| `CONFIG_ESPTOOLPY_FLASHSIZE_8MB` | `y` | 8MB flash size |
| `CONFIG_PARTITION_TABLE_CUSTOM` | `y` | Use custom partition table |
| `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` | `partitions/partition-8MB.csv` | 8MB partition layout |
| `CONFIG_FATFS_LFN_HEAP` | `y` | Long filenames allocated in heap |

## Memory (PSRAM)

| Setting | Value | Purpose |
|---------|-------|---------|
| `CONFIG_SPIRAM` | `y` | Enable PSRAM (external RAM) |
| `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` | `y` | WiFi/LWIP buffers in PSRAM to save internal RAM |

## TLS/SSL Configuration

These settings are critical for memory management and Bambu printer connectivity.

| Setting | Value | Purpose |
|---------|-------|---------|
| `CONFIG_ESP_TLS_INSECURE` | `y` | Allow insecure TLS connections |
| `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY` | `y` | Skip certificate verification (required for Bambu printers with self-signed certs) |
| `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC` | `y` | Allocate TLS buffers in external memory (PSRAM) |
| `CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN` | `2048` | Reduced TLS output buffer (default: 16384) |
| `CONFIG_MBEDTLS_DYNAMIC_BUFFER` | `y` | **Dynamic TLS buffers** - releases memory when connection closes |
| `CONFIG_MBEDTLS_DYNAMIC_FREE_CONFIG_DATA` | `y` | Free TLS config data after handshake completes |
| `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE` | `n` | Disable certificate bundle (saves ~60KB flash) |

### Why Dynamic TLS Buffers?

Without `CONFIG_MBEDTLS_DYNAMIC_BUFFER`, each TLS connection allocates a **16KB input buffer** that remains resident until the connection closes. With multiple concurrent connections (3 printers + weather API), this causes:

- **64KB+ of fragmented memory** that can't be reclaimed
- Memory allocation failures: `E (xxxx) esp-aes: Failed to allocate memory`
- SD card I/O failures: `E (xxxx) diskio_sdmmc: sdmmc_read_blocks failed (257)`

Enabling dynamic buffers allows the TLS stack to release memory after each operation, preventing fragmentation.

## Compiler and Performance

| Setting | Value | Purpose |
|---------|-------|---------|
| `CONFIG_COMPILER_OPTIMIZATION_SIZE` | `y` | Optimize for size (smaller binary) |
| `CONFIG_PERIPH_CTRL_FUNC_IN_IRAM` | `y` | Peripheral control functions in IRAM for speed |

## Localization

| Setting | Value | Purpose |
|---------|-------|---------|
| `CONFIG_TIMEZONE_STRING` | `WET0WEST,M3.5.0/1,M10.5.0/2` | Western European timezone with DST |

## LVGL

| Setting | Value | Purpose |
|---------|-------|---------|
| `CONFIG_LV_CONF_SKIP` | `n` | Use custom `lv_conf.h` instead of Kconfig |

## Applying These Settings

These settings are stored in `sdkconfig.defaults` and `sdkconfig.defaults.esp32s3`. When you run `idf.py set-target esp32s3`, ESP-IDF merges these defaults into the full `sdkconfig`.

To regenerate `sdkconfig.defaults` from current settings:

```bash
idf.py save-defconfig
```

To view current configuration:

```bash
idf.py menuconfig
```
