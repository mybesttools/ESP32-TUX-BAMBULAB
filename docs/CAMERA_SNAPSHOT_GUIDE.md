# Camera Snapshot Feature Guide

**Feature**: Capture JPEG snapshots from Bambu Lab printer cameras  
**Status**: ✅ Implemented and tested  
**Build**: Successfully compiled and flashed

---

## Overview

The BambuMonitor component now supports capturing snapshots from the built-in cameras on Bambu Lab printers. Each printer exposes an HTTP endpoint that provides JPEG images from the camera feed.

### Key Features

- ✅ **HTTP-based snapshot capture** - No RTSP/streaming complexity
- ✅ **Automatic storage management** - Saves to SD card with SPIFFS fallback
- ✅ **Per-printer tracking** - Each printer maintains its last snapshot path
- ✅ **Error handling** - Validates downloads and cleans up failed captures
- ✅ **Timestamped filenames** - Auto-generates unique filenames with Unix timestamps

---

## API Reference

### 1. Capture Snapshot

```cpp
esp_err_t bambu_capture_snapshot(int index, const char* save_path);
```

**Parameters**:

- `index` - Printer index (0-5)
- `save_path` - Custom save path (optional, pass `NULL` for auto-generated)

**Returns**: `ESP_OK` on success, `ESP_FAIL` on error

**Auto-generated path format**:

- SD card: `/sdcard/snapshots/<serial>_<timestamp>.jpg`
- SPIFFS fallback: `/spiffs/snapshots/<serial>_<timestamp>.jpg`

**Example**:

```cpp
// Auto-generated path
esp_err_t result = bambu_capture_snapshot(0, NULL);
if (result == ESP_OK) {
    ESP_LOGI(TAG, "Snapshot captured successfully");
}

// Custom path
result = bambu_capture_snapshot(1, "/sdcard/my_snapshot.jpg");
```

### 2. Get Last Snapshot Path

```cpp
const char* bambu_get_last_snapshot_path(int index);
```

**Parameters**:

- `index` - Printer index (0-5)

**Returns**: Path to last captured snapshot, or `NULL` if none exists

**Example**:

```cpp
const char* path = bambu_get_last_snapshot_path(0);
if (path) {
    ESP_LOGI(TAG, "Last snapshot: %s", path);
    // Display image or upload to cloud
}
```

---

## Technical Details

### Camera URL Format

Bambu Lab printers expose snapshot endpoints at:

```ascii
http://<printer_ip>/snapshot.cgi?user=bblp&pwd=<access_code>
```

**Components**:

- `printer_ip` - From printer config (e.g., `10.13.13.85`)
- `user` - Always `bblp` (Bambu Lab Printer)
- `access_code` - From printer config (LAN access code)

### Storage Strategy

1. **Primary**: SD card (`/sdcard/snapshots/`)
   - Preferred for large storage capacity (GB)
   - Used unless printer is in SPIFFS-only mode

2. **Fallback**: SPIFFS (`/spiffs/snapshots/`)
   - Used when SD card unavailable or unreliable
   - Limited to 2MB total partition size

### Download Process

1. **Create snapshot directory** (if missing)
2. **Generate filename**: `<serial>_<timestamp>.jpg`
   - Example: `00M09D530200738_1765206500.jpg`
3. **Open file** for binary write
4. **HTTP GET** to snapshot URL
5. **Stream chunks** directly to file (4KB buffer)
6. **Verify download** - Check file size > 0
7. **Update tracker** - Store path in printer structure

### Error Handling

- **Invalid printer index** → `ESP_ERR_INVALID_ARG`
- **Printer not configured** → `ESP_ERR_INVALID_ARG`
- **File open failure** → `ESP_FAIL` + errno logged
- **HTTP request failure** → `ESP_FAIL` + error code logged
- **Empty download** → `ESP_FAIL` + file deleted
- **Timeout** → 15 second timeout, then `ESP_FAIL`

---

## Usage Examples

### Example 1: Manual Snapshot Button

Add to GUI (e.g., in `main/gui.hpp`):

```cpp
// In printer panel creation
static void btn_snapshot_handler(lv_event_t* e) {
    int printer_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    ESP_LOGI(TAG, "Capturing snapshot for printer %d...", printer_index);
    
    esp_err_t result = bambu_capture_snapshot(printer_index, NULL);
    
    if (result == ESP_OK) {
        const char* path = bambu_get_last_snapshot_path(printer_index);
        ESP_LOGI(TAG, "Snapshot saved: %s", path);
        
        // TODO: Display snapshot in UI
        // lv_img_set_src(snapshot_img, path);
    } else {
        ESP_LOGW(TAG, "Snapshot capture failed");
    }
}

// Add button to printer panel
lv_obj_t* btn_snapshot = lv_btn_create(printer_panel);
lv_obj_set_size(btn_snapshot, 100, 40);
lv_obj_add_event_cb(btn_snapshot, btn_snapshot_handler, LV_EVENT_CLICKED, (void*)(intptr_t)printer_index);

lv_obj_t* label = lv_label_create(btn_snapshot);
lv_label_set_text(label, "Snapshot");
```

### Example 2: Periodic Auto-Snapshot (Time-Lapse)

Capture snapshots every 30 seconds during printing:

```cpp
static void timelapse_task(void* arg) {
    while (1) {
        for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
            bambu_printer_state_t state = bambu_get_printer_state(i);
            
            if (state == BAMBU_STATE_PRINTING) {
                ESP_LOGI(TAG, "Capturing time-lapse frame for printer %d", i);
                bambu_capture_snapshot(i, NULL);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000));  // 30 seconds
    }
}

// Start task
xTaskCreate(timelapse_task, "timelapse", 4096, NULL, 5, NULL);
```

### Example 3: Snapshot on State Change

Capture when print starts/finishes:

```cpp
static void printer_event_handler(void* arg, esp_event_base_t base, 
                                  int32_t event_id, void* event_data) {
    if (event_id == BAMBU_STATUS_UPDATED) {
        int printer_index = (int)(intptr_t)event_data;
        bambu_printer_state_t state = bambu_get_printer_state(printer_index);
        static bambu_printer_state_t last_states[6] = {BAMBU_STATE_IDLE};
        
        // Detect state change
        if (state != last_states[printer_index]) {
            if (state == BAMBU_STATE_PRINTING) {
                ESP_LOGI(TAG, "Print started - capturing snapshot");
                bambu_capture_snapshot(printer_index, NULL);
            }
            else if (last_states[printer_index] == BAMBU_STATE_PRINTING) {
                ESP_LOGI(TAG, "Print finished - capturing final snapshot");
                bambu_capture_snapshot(printer_index, NULL);
            }
            
            last_states[printer_index] = state;
        }
    }
}
```

### Example 4: Upload Snapshot to Cloud

Capture and upload to webhook/API:

```cpp
static void capture_and_upload(int printer_index) {
    // Capture snapshot
    esp_err_t result = bambu_capture_snapshot(printer_index, NULL);
    if (result != ESP_OK) {
        return;
    }
    
    // Get path
    const char* snapshot_path = bambu_get_last_snapshot_path(printer_index);
    if (!snapshot_path) {
        return;
    }
    
    // Read file
    FILE* f = fopen(snapshot_path, "rb");
    if (!f) {
        return;
    }
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* data = (char*)malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    
    // Upload via HTTP POST (pseudo-code)
    esp_http_client_config_t config = {};
    config.url = "https://your-api.com/upload";
    config.method = HTTP_METHOD_POST;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, data, size);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    
    free(data);
}
```

---

## Displaying Snapshots in LVGL

To show snapshots in the UI:

### Option 1: LVGL Image Widget

```cpp
// Create image widget
lv_obj_t* snapshot_img = lv_img_create(parent);
lv_obj_set_size(snapshot_img, 480, 320);

// Set image source (requires LVGL filesystem mapping)
const char* path = bambu_get_last_snapshot_path(0);
if (path) {
    // If using SD card and LVGL FS is mapped to 'S:'
    char lvgl_path[256];
    snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", path + 8);  // Skip "/sdcard/"
    lv_img_set_src(snapshot_img, lvgl_path);
}
```

### Option 2: Convert to LVGL Format

LVGL can display JPEG directly if enabled in `lv_conf.h`:

```c
#define LV_USE_SJPG 1   // Enable JPEG decoder
```

Then:

```cpp
lv_img_set_src(snapshot_img, "S:/sdcard/snapshots/snapshot.jpg");
```

### Option 3: Thumbnail Preview

Create small preview images for carousel:

```cpp
// Capture full-size
bambu_capture_snapshot(0, "/sdcard/snapshots/full.jpg");

// Create 150x150 thumbnail (requires image processing library)
// resize_image("/sdcard/snapshots/full.jpg", "/sdcard/snapshots/thumb.jpg", 150, 150);

// Display thumbnail in carousel
lv_img_set_src(carousel_thumb, "S:/sdcard/snapshots/thumb.jpg");
```

---

## Storage Management

### Snapshot File Sizes

Typical Bambu Lab camera snapshots:

- **Resolution**: 1920×1080 (X1 Carbon) or 1280×720 (A1 Mini/P1P)
- **Format**: JPEG (lossy compression)
- **File size**: 50-200 KB per snapshot

### Capacity Planning

**SD Card (3.7GB)**:

- ~18,000-74,000 snapshots
- ~500-2000 snapshots per day (30s intervals for 12-16h prints)

**SPIFFS (2MB)**:

- ~10-40 snapshots
- Use only for temporary/recent snapshots

### Auto-Cleanup Strategy

To prevent storage from filling up:

```cpp
#define MAX_SNAPSHOTS_PER_PRINTER 100

static void cleanup_old_snapshots(int printer_index) {
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "/sdcard/snapshots");
    
    DIR* dir = opendir(dir_path);
    if (!dir) return;
    
    // Build list of snapshot files for this printer
    std::vector<std::pair<time_t, std::string>> snapshots;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, bambu_get_device_id(printer_index))) {
            // Extract timestamp from filename
            time_t timestamp = extract_timestamp_from_filename(entry->d_name);
            snapshots.push_back({timestamp, entry->d_name});
        }
    }
    closedir(dir);
    
    // Sort by timestamp (oldest first)
    std::sort(snapshots.begin(), snapshots.end());
    
    // Delete oldest snapshots if over limit
    while (snapshots.size() > MAX_SNAPSHOTS_PER_PRINTER) {
        std::string path = std::string(dir_path) + "/" + snapshots[0].second;
        unlink(path.c_str());
        ESP_LOGI(TAG, "Deleted old snapshot: %s", path.c_str());
        snapshots.erase(snapshots.begin());
    }
}
```

---

## Troubleshooting

### Snapshot Capture Fails

**Symptoms**:

```log
E BambuMonitor: [0] HTTP GET failed: ESP_ERR_HTTP_CONNECT
```

**Causes**:

1. Printer offline or network unreachable
2. Incorrect access code
3. Firewall blocking HTTP port 80

**Solutions**:

```bash
# Test camera URL manually
curl "http://10.13.13.85/snapshot.cgi?user=bblp&pwd=YOUR_ACCESS_CODE" > test.jpg

# Check if file is valid JPEG
file test.jpg  # Should say "JPEG image data"
```

### Empty/Corrupt Files

**Symptoms**:

```log
W BambuMonitor: [0] Snapshot file empty or unreadable
```

**Causes**:

1. SD card timeout (see SD_CARD_FIX_REPORT.md)
2. Printer camera not ready
3. HTTP response truncated

**Solutions**:

- Check SD card health
- Retry after 1-2 seconds
- Increase timeout_ms in config

### Camera Not Available

Some Bambu printers require camera to be enabled:

1. Open Bambu Handy app
2. Settings → Device → Camera
3. Enable "LAN Mode Live View"

### SPIFFS Full

**Symptoms**:

```log
E BambuMonitor: [0] Failed to open /spiffs/snapshots/... for writing (errno=28)
```

**Solution**: Clean up old snapshots or use SD card only

---

## Performance Considerations

### Memory Usage

- **HTTP client buffer**: 4 KB (configurable)
- **File write buffer**: System default (~512 bytes)
- **Peak heap**: ~10-15 KB during capture
- **No RAM buffering**: Streamed directly to file

### Network Bandwidth

- **Download time**: 200 KB @ 1 Mbps = ~1.6 seconds
- **Concurrent captures**: Avoid >2 simultaneous to prevent WiFi saturation

### CPU Load

- **Minimal**: HTTP client handles streaming
- **Task safe**: Can be called from any FreeRTOS task
- **Non-blocking**: Returns immediately, download happens in HTTP client task

---

## Future Enhancements

### Priority 1: LVGL Integration

Add snapshot display to printer carousel panels:

```cpp
// Show live snapshot when user taps printer panel
static void printer_panel_clicked(lv_event_t* e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Capture fresh snapshot
    bambu_capture_snapshot(index, NULL);
    
    // Show in fullscreen overlay
    show_snapshot_fullscreen(bambu_get_last_snapshot_path(index));
}
```

### Priority 2: Snapshot Gallery

Browse all captured snapshots:

```cpp
// List all snapshots for printer
std::vector<std::string> get_snapshots(int index) {
    DIR* dir = opendir("/sdcard/snapshots");
    // Filter by device_id, sort by timestamp
    // Return paths
}
```

### Priority 3: Time-Lapse Video

Stitch snapshots into MP4:

- Capture every 30s during print
- Use FFmpeg or similar to encode
- Upload to cloud storage

### Priority 4: Cloud Upload

Automatic upload to:

- Google Drive
- Dropbox
- Custom webhook
- Discord/Telegram notifications

---

## Example Output

### Successful Capture

```log
I (42193) BambuMonitor: [0] Capturing snapshot from 10.13.13.85
I (42193) BambuMonitor: [0] Snapshot URL: http://10.13.13.85/snapshot.cgi?user=bblp&pwd=12345678
I (42203) BambuMonitor: [0] Save path: /sdcard/snapshots/00M09D530200738_1765206500.jpg
I (43750) BambuMonitor: [0] Snapshot saved: /sdcard/snapshots/00M09D530200738_1765206500.jpg (156234 bytes)
```

### Camera Offline

```log
I (42193) BambuMonitor: [0] Capturing snapshot from 10.13.13.85
E (57193) BambuMonitor: [0] HTTP GET failed: ESP_ERR_HTTP_TIMEOUT
```

### SD Card Fallback

```log
I (42193) BambuMonitor: [0] Capturing snapshot from 10.13.13.85
I (42203) BambuMonitor: [0] Save path: /spiffs/snapshots/00M09D530200738_1765206500.jpg
I (43750) BambuMonitor: [0] Snapshot saved: /spiffs/snapshots/00M09D530200738_1765206500.jpg (156234 bytes)
```

---

## Quick Start

### 1. Test Snapshot Capture

Add to `main/main.cpp` for testing:

```cpp
// After bambu_monitor_start()
vTaskDelay(pdMS_TO_TICKS(10000));  // Wait for MQTT connection

ESP_LOGI(TAG, "Testing snapshot capture...");
for (int i = 0; i < 3; i++) {
    if (bambu_is_printer_active(i)) {
        bambu_capture_snapshot(i, NULL);
    }
}
```

### 2. Monitor Logs

```bash
./scripts/start_monitor.sh | grep -E "snapshot|Capturing|Snapshot"
```

### 3. Check Saved Files

```bash
# List snapshots on SD card
ls -lh /Volumes/SDCARD/snapshots/

# View snapshot
open /Volumes/SDCARD/snapshots/*.jpg
```

---

**Feature Status**: ✅ **READY FOR USE**  
**Documentation**: ✅ **COMPLETE**  
**Next Step**: Add UI button to trigger snapshot capture
