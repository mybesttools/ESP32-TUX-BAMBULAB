# ESP32-TUX Copilot Instructions

## Markdown Quality Standards

Wheever creating Markdown, make sure no linting errrors occur!!!
All generated markdown must follow these linting rules to prevent errors:

### Heading Hierarchy
- Maintain proper progression (H1 → H2 → H3, no skipping levels)
- Single H1 per document maximum
- Blank lines before and after headings
- No emphasis in headings

### Code & References
- Use triple backticks with language specification: ` ```language `
- Wrap all file paths in backticks: `` `path/to/file.ext` ``
- Use markdown link syntax: `[text](url)` (no bare URLs)
- Inline code: `` `variable_name` ``

### Lists & Tables
- Consistent bullet markers (all `-`, `*`, or `+`)
- Proper indentation for nested items (2-4 spaces)
- Blank lines before lists if following text
- Table syntax: proper headers, separator rows, aligned columns

### Format Standards
- Lines wrapped reasonably (~100 characters)
- Single blank lines between sections (never multiple)
- No trailing whitespace
- Use spaces, not tabs

---

## Project Overview

**ESP32-TUX** is a responsive touch UI template for embedded systems built on:
- **Display drivers**: LovyanGFX (hardware-accelerated graphics)
- **UI framework**: LVGL 8.x (lightweight GUI library)
- **Platform**: ESP-IDF (Espressif's official framework)

Supported devices: WT32-SC01 variants (4-8MB Flash), Makerfabs ESP32S3 boards (16MB). Single codebase adapts to different resolutions via device headers.

## Critical Architecture Patterns

### 1. Device Abstraction Layer
Device selection happens at **compile-time** via `main/main.hpp` conditional includes:

```cpp
#if defined(CONFIG_TUX_DEVICE_WT32_SC01)
  #include "conf_WT32SCO1.h"              // ESP32, 4MB, SPI display
#elif defined(CONFIG_TUX_DEVICE_WT32_SC01_PLUS)
  #include "conf_WT32SCO1-Plus.h"         // ESP32-S3, 8MB, 8-bit parallel
// ... more device configs
```

Each `conf_*.h` in `main/devices/` defines:
- Display resolution, driver chip, interface (SPI/parallel)
- Touch controller configuration
- GPIO pin mappings
- Partition table path

To add a device: Create new `conf_*.h`, add Kconfig entry in `main/Kconfig.projbuild`, define `CONFIG_TUX_DEVICE_*`.

### 2. LVGL Thread Safety (Critical for Extensions)
LVGL is **not thread-safe**. All GUI operations must be serialized:

**Pattern**: Use `lvgl_acquire()` / `lvgl_release()` mutex wrappers:
```cpp
// From another task (NOT the LVGL task)
lvgl_acquire();
lv_obj_set_style_bg_color(widget, color, 0);
lvgl_release();
```

Implementation in `helper_display.hpp`:
- LVGL task runs on core 1 (or 0 if unicore) with priority 3
- FreeRTOS binary semaphore (`xGuiSemaphore`) protects all LVGL calls
- `lv_msg_send()` for asynchronous UI updates (preferred for background tasks)

**Never call LVGL functions directly from background tasks without acquire/release**.

### 3. Event-Driven Communication
Decoupled component communication via two mechanisms:

**a) LVGL Messages** (gui-bound events):
```cpp
// In background task
lv_msg_send(MSG_WEATHER_CHANGED, &weather_data);

// In gui_events.hpp callback
lv_msg_subscribe(MSG_WEATHER_CHANGED, weather_callback, NULL);
```

**b) ESP Events** (system-level, cross-component):
```cpp
// Defined in events/tux_events.hpp
esp_event_post_to(TUX_EVENTS, TUX_EVENT_OTA_STARTED, NULL, 0, portMAX_DELAY);
```

Use LVGL messages for UI updates; ESP events for lifecycle/state changes.

### 4. Build System Quirks
- **Single project, multi-target**: CMakeLists.txt hardcoded (no cmake presets)
- **Partition tables**: 4MB, 8MB, 16MB variants in `partitions/partition-*.csv`
- **SPIFFS image**: `fatfs/` directory auto-compiled to partition via `idf_build_set_property`
- **Fonts pre-selected**: Only referenced fonts (main/CMakeLists.txt) avoid bloat

When modifying CMakeLists, always update font list incrementally; don't add all fonts at once.

### 5. Component Dependency Graph
Core system stack (bottom-up):
```
LovyanGFX (display/touch driver)
└─ lvgl (UI framework)
   ├─ OpenWeatherMap (weather API client)
   ├─ BambuMonitor (3D printer MQTT monitor) [new]
   └─ SettingsConfig (JSON-based persistent storage)
```

Helper modules (task-specific):
- `helper_sntp.hpp`: Time sync
- `helper_spiff.hpp`: SPIFFS mounting
- `helper_lv_fs.hpp`: LVGL filesystem mapping (F: = SPIFFS, S: = SD card)
- `helper_display.hpp`: LVGL + LovyanGFX initialization, GUI task, theme setup

## Common Developer Workflows

### Build for Specific Device
```bash
idf.py set-target esp32                    # Select SoC
idf.py menuconfig                          # Select device (CONFIG_TUX_DEVICE_*)
idf.py build
idf.py -p /dev/ttyUSB0 -b 460800 flash    # Adjust baud/port
```

### Modify UI
Edit `main/gui.hpp`. Large file organized as:
- Lines 1-200: Setup/theme definitions
- Lines 600-1300: Page widget creation (`tux_panel_*()` functions)
- Lines 1000+: Button/event callbacks

LVGL patterns:
- Use `tux_panel_create(parent, title, height)` for consistent card widgets
- Messages use `lv_msg_send(MSG_*, data)` convention (see `gui_events.hpp`)
- Theme switching: `theme_current` global, toggle via settings page

### Add Background Task (e.g., weather polling)
Template in `main/main.cpp`:
1. Create task with `xTaskCreatePinnedToCore()`
2. Use `lv_msg_send()` for UI updates (safe from any core)
3. For frequent updates, wrap in `lvgl_acquire()` / `lvgl_release()`
4. Register ESP event handler if state needs cross-task visibility

### OTA Updates
- Flow: Flash binary to update partition → reboot → bootloader selects active partition
- Configured in `components/ota/` and main.cpp
- Triggered from settings page; Azure cloud support planned

## Project-Specific Conventions

### Documentation
- **All documentation must be placed in the `docs/` folder**
- No markdown files in project root except README.md and LICENSE
- Use descriptive filenames (e.g., `BAMBU_QUICKSTART.md`, `CAROUSEL_IMPLEMENTATION.md`)
- Create index files for related documentation groups

### Naming
- **Functions**: `snake_case`, suffix with `_task`, `_handler`, `_init` for clarity
- **Global state**: `g_` prefix (e.g., `g_lvgl_task_handle`)
- **LVGL objects**: `lbl_`, `btn_`, `img_`, `pnl_` prefixes (label, button, image, panel)
- **Config defines**: `CONFIG_TUX_*` for device selection, `TUX_*` for app-level

### File Organization
```
main/
  main.cpp              # Entry point, task creation, event handlers
  gui.hpp               # All UI code (single large file—intentional)
  devices/conf_*.h      # Device-specific configs
  helpers/              # Utilities (SPIFFS, storage, display setup)
  pages/                # Page-specific UI (BLE, WiFi, settings)
  widgets/              # Custom widgets (tux_panel)
  events/               # Event definitions, callbacks
```

### Configuration
- **Menuconfig**: Device + timezone in Kconfig.projbuild
- **Runtime settings**: JSON file in SPIFFS (`SettingsConfig` component)
- **sdkconfig**: ESP-IDF build options; use provided defaults in `sdkconfig.defaults*`

## Integration Points for New Features

### Add Weather-Like Service (e.g., Bambu Monitor)
1. **Create component** in `components/BambuMonitor/` with:
   - Header in `include/` with struct definitions and public API
   - Implementation (C++ or C) handling MQTT/polling
   - ESP event definitions for state changes
2. **Register in main** (main.cpp):
   - Create task, subscribe to key events
   - Use `lv_msg_send()` for UI pushes
3. **UI in gui.hpp**:
   - Create page via `tux_panel_*()` functions
   - Subscribe to component's messages (e.g., `MSG_BAMBU_*`)
4. **Partition/SPIFFS**: Ensure adequate free space (`df -h` on device)

### Add Custom Widget
1. Place in `main/widgets/` (C-based, LVGL convention)
2. Register in `main/CMakeLists.txt` SRCS
3. Expose header, declare in `gui.hpp`, use in page creation

## Known Constraints & Workarounds

| Constraint | Workaround |
|-----------|-----------|
| LVGL not thread-safe | Always use `lvgl_acquire()/release()` or `lv_msg_send()` |
| SPIFFS small (2MB max) | Use SD card for large assets; map as S: in helper_lv_fs.hpp |
| Single gui.hpp (1300 lines) | Feature toggle via #define; refactor pages to separate .hpp files if needed |
| No GPU acceleration | LovyanGFX handles DMA; avoid complex LVGL animations on slow displays |
| Device compile-time selection | Rebuild entire project for device switch; no runtime switching |

## Debugging Tips

- **Logs**: Enable via menuconfig → Component config → ESP-IDF → Logging
  - Set log level to DEBUG for LVGL/LovyanGFX
  - Search for `ESP_LOG*` calls to add instrumentation
- **Monitor**: `idf.py monitor` shows serial output; use to track tasks/memory
- **LVGL Simulator**: Not integrated; web installer (tux.sukesh.me) useful for quick testing
- **Memory**: Check free heap with `ESP_LOGI(TAG, "Free heap: %ld", esp_get_free_heap_size())`

## External References
- LVGL 8.x docs: https://docs.lvgl.io/8.3/
- LovyanGFX: https://github.com/lovyan03/LovyanGFX
- ESP-IDF build system: https://docs.espressif.com/projects/esp-idf/
- Bambu integration docs: See `BAMBU_QUICKSTART.md`, `BAMBU_TECHNICAL_DESIGN.md`
