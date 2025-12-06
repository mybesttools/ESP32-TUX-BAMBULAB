# Multi-Location Weather & Printer Status Carousel

## Overview

The ESP32-TUX now features an interactive carousel/slider display on the home page that shows:
- **Multiple weather locations** - Each with local time, city, country, and current conditions
- **Printer status cards** - Each configured printer displays status, temperature, and progress
- **Touch-friendly navigation** - Swipe horizontally to navigate between slides
- **Page indicators** - Visual dots showing current slide position and total slides

## Architecture

### Components

#### 1. CarouselWidget (`main/widgets/carousel_widget.hpp`)
Header-only widget class that manages the carousel display:

```cpp
class CarouselWidget {
public:
    // Create carousel with parent, width, and height
    CarouselWidget(lv_obj_t *parent, int width, int height);
    
    // Slide management
    void add_slide(const carousel_slide_t &slide);
    void update_slides();
    void show_slide(int index);
    void next_slide();
    void prev_slide();
};
```

**Slide Structure:**
```cpp
struct carousel_slide_t {
    std::string title;           // Location/Printer name
    std::string subtitle;        // City, country or status
    std::string value1;          // Temperature or progress
    std::string value2;          // Weather description or details
    lv_color_t bg_color;         // Slide background color
    uint32_t icon_code;          // Font icon code (reserved)
};
```

#### 2. SettingsConfig Extensions (`components/SettingsConfig/`)
New methods for managing weather locations and printers:

```cpp
// Weather location management
void add_weather_location(const std::string &name, const std::string &city, 
                         const std::string &country, float lat, float lon);
void remove_weather_location(int index);
weather_location_t get_weather_location(int index);
int get_weather_location_count();

// Printer configuration (already existed)
void add_printer(const std::string &name, const std::string &ip, const std::string &token);
void remove_printer(int index);
printer_config_t get_printer(int index);
int get_printer_count();
```

**Data Structures:**
```cpp
struct weather_location_t {
    std::string name;        // Display name (e.g., "Home", "Office")
    std::string city;        // City name
    std::string country;     // Country
    float latitude;          // For OpenWeather API queries
    float longitude;         // For OpenWeather API queries
    bool enabled;            // Whether to display this location
};

struct printer_config_t {
    std::string name;        // Printer display name
    std::string ip_address;  // MQTT broker or IP
    std::string token;       // Authentication token
    bool enabled;            // Whether to display this printer
};
```

### Home Page Integration

The home page (`create_page_home()` in `main/gui.hpp`) now creates and manages the carousel:

1. **Initialization**: Carousel created on first page load
2. **Slide Population**: `update_carousel_slides()` populates from SettingsConfig
3. **Layout**: 
   - Carousel container: 480×250px (full width, reduced height for footer)
   - Scrollable slides area: 480×200px (horizontal scrolling)
   - Page indicator: 480×40px (bottom with dots)

### Visual Design

**Slide Styling:**
- Dark background theme (0x1e1e1e)
- Weather slides: Blue background (0x1e3a5f)
- Printer slides: Purple background (0x3a1e2f)
- Title: White text, 24pt font
- Subtitle: Gray text (0xaaaaaa), 16pt font
- Main value: Orange text (0xffa500), 32pt font
- Secondary value: Light gray (0xcccccc), 16pt font

**Page Indicators:**
- Dots at bottom showing current slide position
- Active dot: Orange (0xffa500)
- Inactive dots: Gray (0x666666)
- Circular design with 20px spacing

## Usage

### Adding Weather Locations

**Via Code (in main.cpp):**
```cpp
cfg->add_weather_location("Home", "Kleve", "Germany", 51.7934f, 6.1368f);
cfg->add_weather_location("Office", "Berlin", "Germany", 52.5200f, 13.4050f);
cfg->save_config();
```

**Via Web UI:**
1. Navigate to http://esp32-tux.local
2. Go to "Weather Settings"
3. Click "Add Location"
4. Enter city name, coordinates
5. Save

### Adding Printers

**Via Code:**
```cpp
cfg->add_printer("Bambu Lab X1", "192.168.1.100", "your_token_here");
cfg->save_config();
```

**Via Web UI:**
1. Navigate to http://esp32-tux.local
2. Go to "Printer Management"
3. Click "Add Printer"
4. Enter printer name, IP, authentication token
5. Save

### Updating Carousel Display

After adding/removing locations or printers, refresh the carousel:

```cpp
// In LVGL task or via message
update_carousel_slides();
```

Or force refresh from settings page by navigating away and back to home.

## Data Persistence

All weather locations and printers are stored in `/spiffs/settings.json` with format:

```json
{
  "devicename": "ESP32-TUX",
  "settings": {
    "brightness": 250,
    "theme": "dark",
    "timezone": "+1:00"
  },
  "weather_locations": [
    {
      "name": "Home",
      "city": "Kleve",
      "country": "Germany",
      "latitude": 51.7934,
      "longitude": 6.1368,
      "enabled": true
    }
  ],
  "printers": [
    {
      "name": "Bambu Lab X1",
      "ip_address": "192.168.1.100",
      "token": "your_token",
      "enabled": true
    }
  ]
}
```

## Integration with Weather & Printer Data

### Weather Updates
The carousel slide values are placeholder-populated with:
- Title: Location name
- Subtitle: Country
- Value1: "24°C" (will update from OpenWeatherMap API)
- Value2: "Partly Cloudy" (will update from OpenWeatherMap API)

Future enhancement: Wire `MSG_WEATHER_CHANGED` LVGL message to update carousel slides in real-time.

### Printer Status Updates
Similar placeholders for printer slides:
- Title: Printer name
- Subtitle: "Status: Idle" (updates from BambuMonitor)
- Value1: "0%" (print progress)
- Value2: "Nozzle: 0°C" (temperature)

Future enhancement: Subscribe to `MSG_BAMBU_*` messages to update printer status live.

## Touch Interaction

**Horizontal Swipe:**
- Swipe left to go to next slide
- Swipe right to go to previous slide
- Slides wrap around (last → first, first → last)

**Visual Feedback:**
- Page indicator dots update to show current position
- Smooth scroll animation to target slide (LV_ANIM_ON)

## Configuration Limits

```cpp
#define MAX_PRINTERS 5               // Maximum 5 printers
#define MAX_WEATHER_LOCATIONS 5      // Maximum 5 weather locations
```

Maximum total slides: 10 (5 locations + 5 printers)

## Default Initialization

On first boot, if no weather locations exist, the system initializes:
1. **Home** - Kleve, Germany (51.7934°, 6.1368°)
2. **Reference** - Amsterdam, Netherlands (52.3676°, 4.9041°)

These can be modified via web UI or code.

## Performance Considerations

- **Memory**: Each slide object tree (~5-6 LVGL objects) uses minimal heap
- **Flash**: Carousel widget header-only implementation (~300 lines)
- **Rendering**: Hardware-accelerated by LovyanGFX, smooth 60 FPS expected
- **Scroll Snapback**: Automatic centering on nearest slide

## Future Enhancements

1. **Live Weather Data**: Integrate OpenWeatherMap API for real-time updates
2. **Live Printer Status**: Subscribe to BambuMonitor MQTT messages
3. **Location Search**: OpenWeather API location validation/autocomplete
4. **Custom Slide Ordering**: User-configurable order of locations/printers
5. **Slide Timer**: Auto-advance carousel (currently manual/swipe only)
6. **Animation Effects**: Fade transitions between slides (LVGL anim_set)
7. **Weather Icons**: Use FontAwesome weather icons per condition
8. **Tap to Details**: Single tap on slide to expand details
9. **Settings UI**: In-device carousel management (no web UI needed)
10. **Multi-Language**: Localized strings for slide text

## Troubleshooting

### Carousel Not Showing
- Check SettingsConfig initialization: `cfg->get_weather_location_count()`
- Verify carousel widget is created: `if (carousel_widget) { ... }`
- Check SPIFFS has enough space for slides (no image data needed currently)

### Slides Not Updating
- Ensure `update_carousel_slides()` is called after adding locations
- Check LVGL task is running and carousel_widget is thread-safe
- Verify `cfg` pointer is valid when populating slides

### Touch Navigation Issues
- Ensure parent container has proper size/position
- Check `lv_obj_set_scroll_dir()` allows LV_DIR_HOR
- Verify scroll event callback is properly registered

## Code References

- **Carousel Widget**: `main/widgets/carousel_widget.hpp`
- **Home Page Creation**: `main/gui.hpp` ~lines 775-830
- **SettingsConfig**: `components/SettingsConfig/include/SettingsConfig.hpp`
- **Implementation**: `components/SettingsConfig/SettingsConfig.cpp`
- **Initialization**: `main/main.cpp` ~lines 285-295

