# Carousel Feature - Quick Start

## What You See

When you boot ESP32-TUX and navigate to the home page, you'll see a horizontal carousel (slider) displaying:

- **Weather location cards** with city, country, temperature, and conditions
- **Printer status cards** with printer name, status, progress, and temperature

## How to Use

### Navigate Between Slides

**Swipe left/right** on the carousel to scroll through different locations and printers.

The **orange dots** at the bottom show:

- Your current position in the carousel
- Total number of slides available

### Add a New Weather Location

**Via Web Interface:**

1. Open http://esp32-tux.local in a browser
2. Click "Weather Settings" 
3. Click "Add Location"
4. Enter location name (e.g., "Paris")
5. Click "Search" to find city coordinates (future feature)
6. Or manually enter latitude/longitude
7. Save

**Via Settings on Device:**

1. Tap **Settings** button on device
2. Navigate to Weather section
3. (Future: Built-in UI for location management)

### Add a Printer

**Via Web Interface:**

1. Open http://esp32-tux.local
2. Click "Printer Management"
3. Click "Add Printer"
4. Enter printer name (e.g., "Bambu Lab X1")
5. Enter IP address or MQTT broker
6. Enter authentication token
7. Save

**Via Settings on Device:**

1. Tap **Settings** button
2. Navigate to Printer section
3. Click "Add Printer"
4. (Future: Built-in configuration)

## Default Setup

On first boot, the system comes with two weather locations:

- **Home** - Kleve, Germany
- **Reference** - Amsterdam, Netherlands

You can replace these with your own locations via the web interface.

## Real-Time Updates (Coming Soon)

Currently the carousel shows placeholder data:

- Weather: "24°C", "Partly Cloudy"
- Printers: "Status: Idle", "0%", "0°C"

**Next steps will:**

1. Fetch real weather from OpenWeatherMap API for each location
2. Get live printer status from BambuMonitor over MQTT
3. Auto-update carousel every 5-10 minutes

## Screen Layout

```text
┌─────────────────────────────────────┐
│   ESP32-TUX Home                    │  ← Header (time, WiFi, battery)
├─────────────────────────────────────┤
│                                     │
│  ┌─────────────────────────────┐    │
│  │ Home                        │    │
│  │ Germany                     │    │
│  │        24°C                 │    │  ← Carousel with
│  │     Partly Cloudy           │    │     swipeable slides
│  └─────────────────────────────┘    │ 
│  ● ○ ○ ○ ○                          │  ← Page indicators
│                                     │
├─────────────────────────────────────┤
│  [Home] [Printer] [Settings] [OTA]  │  ← Menu buttons
└─────────────────────────────────────┘
```

## Carousel Features

- **Horizontal scrolling** - Swipe to browse all locations and printers
- **Page indicators** - See which slide you're on and how many total
- **Automatic wrapping** - Last slide wraps to first slide
- **Smooth animations** - Fluid transitions between slides
- **Color-coded slides**:
  - Blue background = Weather location
  - Purple background = Printer status

## Limits

- Maximum **5 weather locations**
- Maximum **5 printers**
- Maximum **10 total slides** (5 + 5)

If you need more, you can edit the config file directly in SPIFFS.

## Customization

### Edit Colors

Edit in `main/gui.hpp` function `update_carousel_slides()`:

```cpp
slide.bg_color = lv_color_hex(0x1e3a5f);  // Blue for weather
slide.bg_color = lv_color_hex(0x3a1e2f);  // Purple for printers
```

### Change Default Locations

Edit in `main/main.cpp`:

```cpp
cfg->add_weather_location("Home", "Kleve", "Germany", 51.7934f, 6.1368f);
cfg->add_weather_location("Reference", "Amsterdam", "Netherlands", 52.3676f, 4.9041f);
```

Rebuild and flash to apply changes.

## Troubleshooting

### I don't see the carousel

- Make sure you're on the **Home** page (press Home button if not)
- Check that the device has booted completely (look for splash screen first)
- Try navigating to another page and back to home

### The carousel is blank / shows "Welcome"

- You probably haven't added any weather locations or printers
- Use the web interface at http://esp32-tux.local to add some
- Or check the device console for error messages

### Can't swipe between slides

- Make sure you're swiping horizontally (left/right)
- Try swiping closer to the middle of the carousel area
- Not on buttons or text labels

### Weather/Printer data isn't updating

- This is normal for now - data is static placeholders
- Real-time updates coming in next phase
- Check http://esp32-tux.local → Weather Settings to verify API key

## Web Interface

Access full configuration and status at **http://esp32-tux.local**:

### Weather Settings
- Add/remove/edit weather locations
- Set OpenWeatherMap API key
- Validate coordinates with map view (coming soon)

### Printer Management  
- Add/remove printers
- Test MQTT connection
- View printer status history (coming soon)

### System Settings
- Adjust screen brightness
- Set timezone
- View device information

## Need Help?

Check these files for more info:
- **CAROUSEL_FEATURE.md** - Detailed technical documentation
- **README.md** - General ESP32-TUX information
- **BAMBU_QUICKSTART.md** - Bambu printer setup
- **BAMBU_TECHNICAL_DESIGN.md** - Printer integration details

Happy monitoring! 🚀

