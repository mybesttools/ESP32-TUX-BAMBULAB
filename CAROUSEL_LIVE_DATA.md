# Carousel Live Data Integration Guide

## Overview

This document shows how to connect live weather and printer data to the carousel display, replacing the placeholder values with real data.

## Architecture

The carousel uses a **message-based update system** similar to existing weather and printer monitoring:

1. **Data Source** (OpenWeatherMap API, BambuMonitor MQTT) fetches data
2. **Message Posted** via `lv_msg_send()` to LVGL message system
3. **Carousel Callback** receives message and updates slide display
4. **User Sees** live temperature, status, progress updates

## Weather Data Integration

### Step 1: Define Weather Slide Message Type

Add to `main/events/gui_events.hpp`:

```cpp
// Existing message
#define MSG_WEATHER_CHANGED 0x1001  // Already exists

// New - for per-location weather updates
#define MSG_CAROUSEL_WEATHER_UPDATED 0x1010
#define MSG_CAROUSEL_PRINTER_UPDATED 0x1011
```

### Step 2: Create Weather Slide Data Structure

In `main/gui.hpp`, add:

```cpp
struct carousel_weather_update_t {
    int location_index;      // Which location this update is for
    float temperature;       // Current temperature
    const char *condition;   // Weather condition (e.g., "Partly Cloudy")
    const char *icon;        // Font Awesome icon code
    float humidity;          // Optional: humidity percentage
    float wind_speed;        // Optional: wind speed
};
```

### Step 3: Fetch Weather for Each Location

In the existing `timer_weather_callback()` or weather task in `main/main.cpp`:

```cpp
// After fetching weather for current location, also fetch for other locations
static void timer_weather_callback(lv_timer_t *timer) {
    // ... existing code fetches weather for current location ...
    
    // NEW: Fetch weather for carousel locations
    if (cfg && owm) {
        int loc_count = cfg->get_weather_location_count();
        for (int i = 0; i < loc_count; i++) {
            weather_location_t loc = cfg->get_weather_location(i);
            if (loc.enabled) {
                // Fetch weather at this location
                float lat = loc.latitude;
                float lon = loc.longitude;
                
                // Call your OpenWeatherMap API here
                // For example: owm->get_weather_by_coords(lat, lon);
                
                carousel_weather_update_t update;
                update.location_index = i;
                update.temperature = 24.5f;  // From API
                update.condition = "Partly Cloudy";  // From API
                
                // Send update to carousel
                lv_msg_send(MSG_CAROUSEL_WEATHER_UPDATED, &update);
            }
        }
    }
}
```

### Step 4: Update Carousel on Weather Message

Add to `main/gui.hpp`:

```cpp
static void carousel_weather_event_cb(lv_event_t *e) {
    carousel_weather_update_t *update = (carousel_weather_update_t *)lv_event_get_msg_data(e);
    if (!carousel_widget || !update) return;
    
    // Update the weather slide
    int weather_count = cfg ? cfg->get_weather_location_count() : 0;
    if (update->location_index < weather_count) {
        // Slide index is location_index (weather slides come first)
        carousel_slide_t new_slide = carousel_widget->slides[update->location_index];
        
        // Update values
        new_slide.value1 = fmt::format("{:.1f}°C", update->temperature);
        new_slide.value2 = update->condition;
        
        // Update slide in widget (need to add method for this)
        // For now, recreate all slides
        update_carousel_slides();
    }
}

// Add message subscription in create_page_home():
lv_obj_add_event_cb(carousel_widget->container, carousel_weather_event_cb, 
                   LV_EVENT_MSG_RECEIVED, NULL);
lv_msg_subsribe_obj(MSG_CAROUSEL_WEATHER_UPDATED, carousel_widget->container, NULL);
```

## Printer Status Integration

### Step 1: Define Printer Slide Message Type

In `main/events/gui_events.hpp` (already done above):

```cpp
#define MSG_CAROUSEL_PRINTER_UPDATED 0x1011
```

### Step 2: Create Printer Slide Data Structure

In `main/gui.hpp`, add:

```cpp
struct carousel_printer_update_t {
    int printer_index;       // Which printer this update is for
    const char *status;      // "Idle", "Printing", "Error", "Offline"
    float progress;          // Print progress 0-100%
    float nozzle_temp;       // Current nozzle temperature
    float bed_temp;          // Current bed temperature
    int time_remaining;      // Time remaining in seconds (if printing)
};
```

### Step 3: Subscribe to BambuMonitor Messages

In `main.cpp` main task or BambuMonitor callback:

```cpp
// Existing BambuMonitor callbacks (in gui.hpp)
static void bambu_progress_cb(void *s, lv_msg_t *m) {
    uint8_t *progress = (uint8_t *)lv_msg_get_payload(m);
    
    // Update carousel for this printer
    // You'll need to map which printer this status belongs to
    carousel_printer_update_t update;
    update.printer_index = 0;  // Assuming first printer
    update.progress = (float)(*progress);
    
    lv_msg_send(MSG_CAROUSEL_PRINTER_UPDATED, &update);
}

static void bambu_temps_cb(void *s, lv_msg_t *m) {
    // Bambu temperature update
    // Extract nozzle_temp and bed_temp and send to carousel
}
```

### Step 4: Update Carousel on Printer Message

Add to `main/gui.hpp`:

```cpp
static void carousel_printer_event_cb(lv_event_t *e) {
    carousel_printer_update_t *update = (carousel_printer_update_t *)lv_event_get_msg_data(e);
    if (!carousel_widget || !update) return;
    
    int weather_count = cfg ? cfg->get_weather_location_count() : 0;
    int slide_index = weather_count + update->printer_index;
    
    if (slide_index < (int)carousel_widget->slides.size()) {
        carousel_slide_t new_slide = carousel_widget->slides[slide_index];
        
        // Update values
        new_slide.subtitle = fmt::format("Status: {}", update->status);
        new_slide.value1 = fmt::format("{:.0f}%", update->progress);
        new_slide.value2 = fmt::format("N:{:.0f}° B:{:.0f}°", 
                                       update->nozzle_temp, update->bed_temp);
        
        // Update slide (need to add method for this)
        update_carousel_slides();
    }
}

// Add message subscription in create_page_home():
lv_obj_add_event_cb(carousel_widget->container, carousel_printer_event_cb,
                   LV_EVENT_MSG_RECEIVED, NULL);
lv_msg_subsribe_obj(MSG_CAROUSEL_PRINTER_UPDATED, carousel_widget->container, NULL);
```

## Optimized Update Method

Instead of recreating all slides, add an update method to CarouselWidget:

In `main/widgets/carousel_widget.hpp`, add:

```cpp
class CarouselWidget {
public:
    // ... existing code ...
    
    void update_slide_data(int index, const carousel_slide_t &slide) {
        if (index < 0 || index >= (int)slide_panels.size()) return;
        
        // Update the slide panel labels with new data
        // Find title label
        lv_obj_t *title = lv_obj_get_child(slide_panels[index], 0);
        if (title) lv_label_set_text(title, slide.title.c_str());
        
        // Find subtitle label
        lv_obj_t *subtitle = lv_obj_get_child(slide_panels[index], 1);
        if (subtitle) lv_label_set_text(subtitle, slide.subtitle.c_str());
        
        // Find value1 label
        lv_obj_t *value1 = lv_obj_get_child(slide_panels[index], 2);
        if (value1) lv_label_set_text(value1, slide.value1.c_str());
        
        // Find value2 label
        lv_obj_t *value2 = lv_obj_get_child(slide_panels[index], 3);
        if (value2) lv_label_set_text(value2, slide.value2.c_str());
    }
};
```

Then use in callbacks:

```cpp
static void carousel_weather_event_cb(lv_event_t *e) {
    carousel_weather_update_t *update = (carousel_weather_update_t *)lv_event_get_msg_data(e);
    if (!carousel_widget || !update) return;
    
    carousel_slide_t updated = carousel_widget->slides[update->location_index];
    updated.value1 = fmt::format("{:.1f}°C", update->temperature);
    updated.value2 = update->condition;
    
    carousel_widget->update_slide_data(update->location_index, updated);
}
```

## Integration Timeline

### Phase 1 (Current) ✅
- ✅ Carousel widget framework
- ✅ Multiple location support
- ✅ Placeholder data display
- ✅ Touch navigation
- ✅ Web UI configuration

### Phase 2 (Next)
- [ ] OpenWeatherMap API integration
- [ ] Fetch weather for each location
- [ ] Display real temperatures and conditions
- [ ] Weather icon mapping

### Phase 3
- [ ] BambuMonitor integration
- [ ] Real printer status display
- [ ] Live progress updates
- [ ] Temperature monitoring

### Phase 4
- [ ] Auto-refresh timer (5-10 minute intervals)
- [ ] Error handling and offline fallback
- [ ] Performance optimization
- [ ] Caching of API responses

## Code Example: Complete Weather Update Flow

```cpp
// In a background task or timer
void fetch_carousel_weather_task(void *param) {
    while (true) {
        if (cfg && owm) {
            int loc_count = cfg->get_weather_location_count();
            for (int i = 0; i < loc_count; i++) {
                weather_location_t loc = cfg->get_weather_location(i);
                if (loc.enabled) {
                    // Fetch weather
                    // weather_data_t weather = owm->get_by_coords(loc.latitude, loc.longitude);
                    
                    carousel_weather_update_t update;
                    update.location_index = i;
                    // update.temperature = weather.temp;
                    // update.condition = weather.main;
                    
                    // Thread-safe message send
                    lv_msg_send(MSG_CAROUSEL_WEATHER_UPDATED, &update);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(600000));  // Every 10 minutes
    }
}
```

## Testing

### Manual Test Steps

1. **Build and Flash** with carousel feature
2. **Add 2+ weather locations** via web UI
3. **Verify carousel displays** with placeholder data
4. **Manually send message:**
   ```cpp
   carousel_weather_update_t test;
   test.location_index = 0;
   test.temperature = 18.5f;
   test.condition = "Cloudy";
   lv_msg_send(MSG_CAROUSEL_WEATHER_UPDATED, &test);
   ```
5. **Observe carousel updates** on device

### Serial Monitor Debug

```cpp
ESP_LOGI(TAG, "Carousel weather update: loc=%d, temp=%.1f, condition=%s", 
         update->location_index, update->temperature, update->condition);
```

## Performance Notes

- **Message size**: ~30 bytes (very small)
- **Update frequency**: Can handle 1/sec without issues
- **Memory**: No extra heap allocation needed
- **Recommended update rate**: 1 per location per 10 minutes (weather)
- **Printer updates**: Can be frequent (1/sec) since using MQTT

## Files to Modify

1. `main/gui.hpp` - Add callbacks and integration
2. `main/events/gui_events.hpp` - Add new message constants
3. `main/main.cpp` - Add weather fetching task
4. `main/widgets/carousel_widget.hpp` - Add update_slide_data() method (optional)

## References

- LVGL Message System: `lv_msg_send()`, `lv_msg_subsribe_obj()`
- OpenWeatherMap API: `components/OpenWeatherMap/`
- BambuMonitor: `components/BambuMonitor/`
- Existing weather integration: `main/gui.hpp` ~line 300+

