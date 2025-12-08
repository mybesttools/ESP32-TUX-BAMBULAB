# Web UI Internationalization (i18n)

## Overview

The ESP32-TUX web configuration interface now supports full internationalization in 5 languages:
- English (en)
- German (de) 
- Dutch (nl)
- Polish (pl)
- Russian (ru)

Language selection is persistent across sessions using browser localStorage and automatically translates all UI text including labels, buttons, placeholders, and status messages.

## Architecture

### Translation System Components

1. **Translation Dictionary** (`translations` object)
   - Located at the top of the `<script>` section in `WebServer.cpp`
   - Contains all UI strings organized by language code
   - Each language has identical keys but translated values
   - Currently supports ~60 translation keys

2. **Translation Function** (`t(key)`)
   - Simple lookup function: `t(key)` returns translation for current language
   - Falls back to English if key not found in current language
   - Falls back to the key itself if missing entirely

3. **Language State Management**
   - `currentLang` global variable tracks active language
   - Synchronized with `<select id="language">` dropdown
   - Persisted to `localStorage` on change
   - Restored from `localStorage` on page load (defaults to 'en')

4. **Page Translation** (`translatePage()` function)
   - Called on page load and language change
   - Updates document title
   - Processes all elements with `data-i18n` attributes
   - Updates select option text for theme/timezone/printer dropdowns
   - Handles both text content and input placeholders

### HTML Integration Pattern

Elements are marked for translation using the `data-i18n` attribute:

```html
<!-- Labels/headings use textContent -->
<label data-i18n="brightness">Brightness:</label>
<h2 data-i18n="systemSettings">⚙️ System Settings</h2>

<!-- Input placeholders -->
<input type="text" id="cityName" data-i18n="cityPlaceholder" placeholder="e.g., Kleve">

<!-- Buttons -->
<button onclick="saveSettings()" data-i18n="saveSettings">💾 Save Settings</button>
```

The `translatePage()` function automatically determines whether to update `textContent` or `placeholder` based on the element type.

### Dynamic Message Translation

Status messages use the `t()` function directly in JavaScript:

```javascript
// Success message
showStatus('settingsStatus', t('settingsSaved'), true);

// Error message with dynamic content
showStatus('settingsStatus', t('errorLoading') + ': ' + e, false);

// Progress message with interpolation
statusElem.textContent = `${t('scanning')}: ${d.progress}% complete`;
```

## Translation Keys Reference

### Core UI

| Key | English | Usage |
|-----|---------|-------|
| `title` | ESP32-TUX Configuration | Page title and H1 |
| `systemSettings` | System Settings | Panel heading |
| `brightness` | Brightness | Slider label |
| `theme` | Theme | Dropdown label |
| `themeDark` | Dark | Theme option |
| `themeLight` | Light | Theme option |
| `timezone` | Timezone | Dropdown label |
| `selectTimezone` | -- Select Timezone -- | Default option |
| `language` | Language | Dropdown label |

### Actions

| Key | English | Usage |
|-----|---------|-------|
| `saveSettings` | Save Settings | Button text |
| `loadSettings` | Load Settings | Button text |
| `saveWeatherSettings` | Save Weather Settings | Button text |
| `loadWeatherSettings` | Load Weather Settings | Button text |
| `addLocationBtn` | Add Location | Button text |
| `addPrinterBtn` | Add Printer | Button text |
| `deleteBtn` | Delete | Delete button |
| `discoverPrintersBtn` | Discover Printers | Discovery button |

### Weather Settings

| Key | English | Usage |
|-----|---------|-------|
| `weatherSettings` | Weather Settings | Panel heading |
| `apiKey` | API Key | Input label |
| `apiKeyPlaceholder` | Enter OpenWeatherMap API key | Input placeholder |
| `manageWeatherLocations` | Manage Weather Locations | Panel heading |
| `addLocation` | Add Location | Subheading |
| `locationName` | Location Name | Input label |
| `locationNamePlaceholder` | e.g., Home, Office | Input placeholder |
| `city` | City | Input label |
| `cityPlaceholder` | e.g., Kleve | Input placeholder |
| `country` | Country | Input label |
| `countryPlaceholder` | e.g., Germany | Input placeholder |
| `latitude` | Latitude | Input label |
| `longitude` | Longitude | Input label |
| `configuredLocations` | Configured Locations | Subheading |

### Printer Configuration

| Key | English | Usage |
|-----|---------|-------|
| `printerConfiguration` | Printer Configuration | Panel heading |
| `autoDiscover` | Auto-Discover Printers | Subheading |
| `autoDiscoverDesc` | Searches for Bambu Lab printers... | Help text |
| `selectDiscovered` | Select from discovered printers | Dropdown label |
| `choosePrinter` | -- Choose a printer -- | Default option |
| `addManually` | Add Printer Manually | Subheading |
| `printerName` | Printer Name | Input label |
| `printerNamePlaceholder` | e.g., Bambu Lab X1 | Input placeholder |
| `ipAddress` | IP Address | Input label |
| `ipPlaceholder` | 192.168.1.100 | Input placeholder |
| `printerCode` | Printer Code | Input label |
| `printerCodePlaceholder` | Enter printer access code | Input placeholder |
| `serialNumber` | Serial Number | Input label |
| `serialPlaceholder` | e.g., 0309DA541804686... | Input placeholder |
| `configuredPrinters` | Configured Printers | Subheading |

### Status Messages

| Key | English | Usage |
|-----|---------|-------|
| `settingsSaved` | Settings saved! | Success message |
| `weatherSettingsSaved` | Weather settings saved! | Success message |
| `locationAdded` | Location added! | Success message |
| `printerAdded` | Printer added! | Success message |
| `error` | Error | Error prefix |
| `errorLoading` | Error loading | Error message |
| `scanning` | Scanning | Progress indicator |
| `starting` | Starting | Progress indicator |
| `noDiscoveredPrinters` | No printers discovered | Discovery result |
| `discoveryComplete` | Discovery complete | Discovery result |

## Adding New Translations

To add a new translatable string:

1. **Add translation keys to all languages** in the `translations` object:

```javascript
en: {
    // ... existing keys
    newFeature: 'New Feature Label',
    newButton: 'Click Me'
},
de: {
    // ... existing keys
    newFeature: 'Neue Funktion Bezeichnung',
    newButton: 'Klick mich'
},
// ... repeat for nl, pl, ru
```

2. **Add `data-i18n` attribute to HTML elements**:

```html
<label data-i18n="newFeature">New Feature Label:</label>
<button onclick="doThing()" data-i18n="newButton">Click Me</button>
```

3. **Use `t()` function for dynamic messages**:

```javascript
showStatus('statusDiv', t('newFeature') + ' completed!', true);
```

## Translation Best Practices

### Key Naming Convention

- Use camelCase for keys: `saveSettings`, `printerName`
- Group related keys with prefixes: `theme`, `themeDark`, `themeLight`
- Placeholder keys end with `Placeholder`: `cityPlaceholder`
- Button action keys end with `Btn` if ambiguous: `addLocationBtn`

### Text Content Guidelines

- **Labels**: Include colon in translation: `"Brightness:"` not `"Brightness"`
- **Placeholders**: Include "e.g." examples: `"e.g., Kleve"`
- **Buttons**: Include emojis in HTML, not translation: `<button data-i18n="save">💾 Save</button>`
- **Status messages**: Keep punctuation: `"Settings saved!"` not `"Settings saved"`

### HTML vs JavaScript Patterns

| Use Case | Pattern | Example |
|----------|---------|---------|
| Static label | `data-i18n` attribute | `<label data-i18n="brightness">` |
| Static button | `data-i18n` attribute | `<button data-i18n="saveSettings">` |
| Input placeholder | `data-i18n` attribute | `<input data-i18n="cityPlaceholder">` |
| Status message | `t()` in JS | `showStatus('id', t('settingsSaved'))` |
| Dynamic message | `t()` + concat | `t('errorLoading') + ': ' + error` |
| Progress text | Template literal | `` `${t('scanning')}: ${progress}%` `` |

## Language Dropdown Behavior

The language selector in the web UI:
- Saves selection to device configuration (persisted to settings.json)
- Saves selection to browser localStorage (immediate UI update)
- Changes language instantly without page reload
- Updates all text elements via `translatePage()`

**Important**: The language setting affects BOTH:
1. Web UI language (via JavaScript i18n)
2. Device UI language (via `settings.json` → `SettingsConfig` → `TR()` macro)

## Font Requirements

**Russian language requires Cyrillic font support!**

The current web UI uses system fonts, so Russian text displays correctly in browsers. However, the **device LCD UI** uses custom embedded fonts that currently lack Cyrillic glyphs (U+0400-U+04FF).

To enable full Russian support on the device display, see `docs/FONT_GENERATION.md` (to be created) for instructions on generating Montserrat fonts with Cyrillic character ranges.

## Testing i18n Implementation

### Manual Testing Checklist

1. **Language Persistence**
   - [ ] Select German → reload page → still German
   - [ ] Select Russian → close browser → reopen → still Russian
   - [ ] Clear localStorage → reload → defaults to English

2. **Static Text Translation**
   - [ ] Panel headings update
   - [ ] Labels update  
   - [ ] Button text updates
   - [ ] Input placeholders update

3. **Dynamic Message Translation**
   - [ ] Save settings → see translated success message
   - [ ] Trigger error → see translated error message
   - [ ] Run printer discovery → see translated progress messages

4. **Select Option Translation**
   - [ ] Theme dropdown shows "Dark"/"Light" in selected language
   - [ ] Timezone first option shows translated "Select Timezone"
   - [ ] Printer dropdown shows translated "Choose a printer"

5. **Cross-Language Consistency**
   - [ ] Same keys exist in all 5 languages
   - [ ] No missing translations cause fallback to English
   - [ ] Status message timing works in all languages

### Browser Console Testing

```javascript
// Test translation function
t('brightness')  // Should return "Brightness" in English

// Switch language programmatically
currentLang = 'de';
translatePage();  // UI should switch to German

// Check translation keys
Object.keys(translations.en).length === Object.keys(translations.ru).length
// Should be true (all languages have same keys)
```

## Implementation File Locations

- **Translation dictionary**: `components/WebServer/WebServer.cpp` lines ~260-550
- **Translation functions**: `components/WebServer/WebServer.cpp` lines ~555-600
- **HTML with i18n attributes**: `components/WebServer/WebServer.cpp` lines ~36-250
- **Status message calls**: Throughout JavaScript section (lines ~650-1050)
- **Language initialization**: Window load event handler (lines ~1060-1075)

## Related Documentation

- `BAMBU_QUICKSTART.md` - Web UI usage guide
- `README_BAMBULAB.md` - Overall project documentation  
- Main device UI i18n: `main/i18n/lang.hpp` with `TR()` macro
- Settings persistence: `components/SettingsConfig/SettingsConfig.cpp`

## Future Enhancements

Potential improvements to the i18n system:

1. **Automatic key validation**: Build-time check for missing keys across languages
2. **External translation files**: Move translations from C++ to JSON for easier editing
3. **Translation contribution workflow**: Allow community translation submissions
4. **Compact notation**: Consider using shorter key names to reduce code size
5. **RTL language support**: Add Arabic/Hebrew with right-to-left layout
6. **Number/date formatting**: Use `Intl` APIs for locale-specific formatting

---

**Last Updated**: December 2024  
**Status**: Fully implemented and tested for EN/DE/NL/PL/RU
