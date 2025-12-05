# Bambu Lab Integration Plan for ESP32-TUX

## Project Analysis

### Current State

- **ESP32-TUX**: Generic IoT device with weather/remote control apps, LVGL 8.3.3 UI framework
- **bambu_project**: Bambu Lab 3D Printer monitor with real-time MQTT status, animated GIFs for printer states
- **Both use**: LovyanGFX display driver, LVGL UI, WT32-SC01 hardware variants

### Key Differences

| Aspect | ESP32-TUX | bambu_project |
|--------|-----------|---------------|
| **Primary Purpose** | General dashboard (weather, media) | Printer monitoring |
| **Display** | WT32-SC01 variants (all models) | WT32-SC01_V3.3 specific |
| **Architecture** | Component-based (weather, OTA, settings) | Monolithic main.cpp |
| **UI Framework** | Custom LVGL pages | SquareLine Studio exports + GIF animations |
| **Network** | WiFi + provisioning | WiFi + MQTT to printer |
| **Storage** | SPIFFS for images (1.5MB) | Needs GIF files in SPIFFS |
| **Build System** | ESP-IDF | PlatformIO |

### File Size Comparison

**ESP32-TUX vs bambu_project:**

- GIF files: ~1.3 MB total (in data/)
- Source code: ~70 KB

**Current Storage Allocation:**

- Factory partition: 2.3MB (firmware: 2.0MB, 13% free)
- SPIFFS partition: 1.5MB (for images/data)

## Integration Options

### Option A: Modular Integration (Recommended)

Add Bambu Lab monitoring as a new app within ESP32-TUX architecture.

**Pros:**

- Keeps existing functionality (weather, remote control)
- Reuses existing WiFi provisioning, NVS config storage
- Easy to enable/disable features
- Cleaner codebase

**Cons:**

- Requires refactoring bambu_project code
- Need to integrate MQTT library properly

**Implementation Steps:**

1. Create `components/BambuMonitor/` component
2. Extract MQTT connection, JSON parsing, printer state machine
3. Create LVGL UI pages for printer status
4. Add to main app menu system
5. Share WiFi config between apps

**Code locations:**

- New component: `components/BambuMonitor/BambuMonitor.cpp/.hpp`
- UI pages: `main/apps/bambu/`
- Config: Extend `main/Kconfig.projbuild` with Bambu settings

---

### Option B: Firmware Variant

Create separate firmware build for printer monitoring vs general dashboard.

**Pros:**

- Minimal code changes to both projects
- Independent optimization for each use case
- Easier maintenance

**Cons:**

- Duplicate code
- Must choose one feature set per device
- More complex build system

---

### Option C: Full Project Replacement

Use bambu_project as base, add weather/remote features.

**Pros:**

- Single, cohesive codebase
- Simpler than maintaining two builds

**Cons:**

- Loses existing functionality
- More refactoring needed

---

## Technical Challenges

### 1. Storage Space

- Current: 1.5MB SPIFFS available
- Bambu GIF files: ~1.3MB total (stored in SPIFFS filesystem)
- **Solution**: GIFs can be stored directly in SPIFFS, no compilation needed

### 2. Firmware Size

- Current firmware: 2.0MB / 2.3MB partition
- Adding MQTT + JSON = ~100KB
- **Solution**: Already have 13% free space; removing unused fonts/widgets saved space

### 3. Different Hardware Targets

- ESP32-TUX: WT32-SC01, WT32-SC01_V3.3, Makerfabs S3 PTFT/STFT
- bambu_project: WT32-SC01_V3.3 specific
- **Solution**: Create hardware abstraction layer for displays

### 4. Configuration Management

- ESP32-TUX: NVS with SettingsConfig component
- bambu_project: Hardcoded config.h values
- **Solution**: Extend SettingsConfig to support Bambu printer MQTT credentials

## Recommended Approach: Option A (Modular Integration)

### Phase 1: Prepare Infrastructure

1. Create `components/BambuMonitor/` component
2. Create NVS schema for Bambu settings (MQTT server, printer serial, access code)
3. Add Bambu config menu to WiFi provisioning page

### Phase 2: Add MQTT Core

1. Copy MQTT connection logic from bambu_project/src/main.cpp
2. Create state machine for printer status updates
3. Store printer state in global struct accessible to UI

### Phase 3: Create UI Pages

1. Add printer status page to main menu
2. Display: Print progress, temps, current status
3. Optional: GIF animations for states (if SPIFFS space allows)

### Phase 4: Integration Testing

1. Test concurrent weather + Bambu updates
2. Verify WiFi provisioning with Bambu settings
3. Optimize SPIFFS for both weather images and printer GIFs

---

## File Structure After Integration

```ascii
ESP32-TUX/
├── components/
│   ├── BambuMonitor/
│   │   ├── CMakeLists.txt
│   │   ├── BambuMonitor.cpp
│   │   ├── include/
│   │   │   ├── BambuMonitor.hpp
│   │   │   └── printer_state.h
│   │   └── Kconfig
│   ├── OpenWeatherMap/       (existing)
│   ├── SettingsConfig/       (extend for Bambu)
│   └── ...
├── main/
│   ├── apps/
│   │   ├── weather/          (existing)
│   │   ├── remote/           (existing)
│   │   └── bambu/            (NEW)
│   │       ├── bambu_ui.hpp
│   │       ├── bambu_status.hpp
│   │       └── bambu_config.hpp
│   ├── main.cpp              (add Bambu initialization)
│   ├── gui.hpp               (add Bambu pages to menu)
│   └── helpers/
│       └── helper_bambu_mqtt.hpp  (MQTT utilities)
└── bambu_project/            (reference/archive)
```

---

## Storage Strategy

### Current: 1.5MB SPIFFS

- Weather images: ~400KB (bg images, icons)
- Remaining: ~1.1MB
- Target: Keep ~256KB free for configuration/logging

### For Bambu GIFs (1.1MB available)

- **Option 1**: Store essential GIFs only (printing, standby, error) = ~350KB
- **Option 2**: Store all 11 GIFs = ~1.3MB (tight but fits)
- **Option 3**: GIFs can be added later if SPIFFS is expanded

---

## Dependencies to Add

From bambu_project:

- `PubSubClient.h` - MQTT client (already in esp-idf)
- `ArduinoJson.h` - JSON parsing (need to add)

These will add ~200KB to firmware (still fits in 13% free space).

---

## Next Steps

1. **Confirm requirements**:
   - Do you want Bambu as a new app within ESP32-TUX? (Option A)
   - Or separate firmware build? (Option B)
   - Or complete replacement? (Option C)

2. **Gather printer details**:
   - Bambu printer model(s) to support
   - Required monitoring data (just status, or also history?)
   - UI complexity needs

3. **Storage planning**:
   - Which weather images are essential?
   - Can any GIFs be removed/compressed?
   - Accept 1.1MB for printer animations?

4. **Start implementation**:
   - Create BambuMonitor component structure
   - Begin MQTT integration
   - Design unified configuration UI

---

## Quick Implementation Checklist

- [ ] Create `components/BambuMonitor/` directory structure
- [ ] Copy MQTT connection code from bambu_project
- [ ] Extract printer state parsing from JSON
- [ ] Create LVGL UI pages for printer status
- [ ] Integrate with NVS configuration system
- [ ] Test MQTT connection and status updates
- [ ] Optimize storage for GIFs/images
- [ ] Partition table validation for combined workload
