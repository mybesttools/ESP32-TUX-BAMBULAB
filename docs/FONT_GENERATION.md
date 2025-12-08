# Font Generation with Cyrillic Support

## Problem Statement

The ESP32-TUX device UI currently uses `font_montserrat_pl_*` fonts that include:
- **Basic Latin**: U+0020-U+007F (English)
- **Latin Extended-A**: U+0100-U+017F (Polish characters: ą, ć, ę, ł, ń, ó, ś, ź, ż)

To support Russian language (`ru` locale), we need to add:
- **Cyrillic**: U+0400-U+04FF (Russian alphabet and extended characters)

## Required Fonts

Generate these 4 font files with Cyrillic support:

| Font File | Size | Usage | Approximate Size |
|-----------|------|-------|------------------|
| `font_montserrat_14.c` | 14pt | Small text, labels | ~30-40KB |
| `font_montserrat_16.c` | 16pt | Body text, buttons | ~35-45KB |
| `font_montserrat_24.c` | 24pt | Headings, emphasis | ~50-65KB |
| `font_montserrat_32.c` | 32pt | Large titles, numbers | ~70-90KB |

**Total impact**: ~200-250KB additional flash space (acceptable on 8MB/16MB devices)

## Method 1: LVGL Online Font Converter (Recommended)

### Step 1: Access the Converter

Visit: <https://lvgl.io/tools/fontconverter>

### Step 2: Configure Font Settings

1. **Name**: `font_montserrat_14` (change number for each size)
2. **Size**: `14` (or 16, 24, 32)
3. **Bpp**: `4 bit-per-pixel` (good quality, reasonable size)
4. **TTF/OTF font**: Click "Select" and upload `Montserrat-Regular.ttf`
   - Download from: <https://fonts.google.com/specimen/Montserrat>
   - Use the Regular (400) weight

### Step 3: Set Character Ranges

In the "Range" section, add these ranges (one per line):

```
0x20-0x7F
0x100-0x17F
0x400-0x4FF
```

Explanation:
- `0x20-0x7F`: Basic Latin (A-Z, a-z, 0-9, punctuation)
- `0x100-0x17F`: Latin Extended-A (ą, ć, ę, ł, ń, ó, ś, ź, ż for Polish)
- `0x400-0x4FF`: Cyrillic (А-Я, а-я, Ё, ё, etc. for Russian)

### Step 4: Optional Settings

- **Format**: `LVGL` (default, correct for this project)
- **Try different bpp**: Leave at 4 unless file size is a concern
  - 1 bpp = smallest, blocky
  - 2 bpp = small, acceptable
  - 4 bpp = smooth, recommended
  - 8 bpp = smoothest, large

### Step 5: Generate and Download

1. Click "Convert"
2. Wait for processing (may take 10-30 seconds for large fonts)
3. Download the `.c` file

### Step 6: Repeat for All Sizes

Generate 4 files total:
- `font_montserrat_14.c` (Size: 14)
- `font_montserrat_16.c` (Size: 16)
- `font_montserrat_24.c` (Size: 24)
- `font_montserrat_32.c` (Size: 32)

## Method 2: LVGL Font Converter CLI (Advanced)

If you need to automate the process or generate many fonts:

### Install lv_font_conv

```bash
npm install -g lv_font_conv
```

### Download Montserrat Font

```bash
cd ~/Downloads
curl -L https://fonts.google.com/download?family=Montserrat -o montserrat.zip
unzip montserrat.zip
cd Montserrat
```

### Generate Font Files

Run these commands to create all 4 fonts:

```bash
# 14pt
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 14 \
  --font Montserrat-Regular.ttf -r 0x20-0x7F,0x100-0x17F,0x400-0x4FF \
  --format lvgl -o font_montserrat_14.c

# 16pt
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 16 \
  --font Montserrat-Regular.ttf -r 0x20-0x7F,0x100-0x17F,0x400-0x4FF \
  --format lvgl -o font_montserrat_16.c

# 24pt  
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 24 \
  --font Montserrat-Regular.ttf -r 0x20-0x7F,0x100-0x17F,0x400-0x4FF \
  --format lvgl -o font_montserrat_24.c

# 32pt
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 32 \
  --font Montserrat-Regular.ttf -r 0x20-0x7F,0x100-0x17F,0x400-0x4FF \
  --format lvgl -o font_montserrat_32.c
```

**Options explained**:
- `--no-compress`: Faster rendering (important for ESP32)
- `--no-prefilter`: Faster rendering
- `--bpp 4`: 4 bits per pixel (16 gray levels)
- `--size N`: Font size in points
- `--font`: Source TTF file
- `-r`: Character ranges (Unicode hex ranges)
- `--format lvgl`: Output format
- `-o`: Output filename

## Integration into Project

### Step 1: Place Font Files

Copy the generated `.c` files to:

```
main/fonts/
├── font_montserrat_14.c
├── font_montserrat_16.c
├── font_montserrat_24.c
└── font_montserrat_32.c
```

**Important**: Replace the existing `font_montserrat_pl_*` files or rename them to `_pl_backup` for safety.

### Step 2: Update CMakeLists.txt

Edit `main/CMakeLists.txt` to reference the new font files:

```cmake
set(COMPONENT_SRCS
    # ... other files
    "fonts/font_montserrat_14.c"
    "fonts/font_montserrat_16.c"
    "fonts/font_montserrat_24.c"
    "fonts/font_montserrat_32.c"
    # ... other files
)
```

### Step 3: Update Font Declarations

In `main/gui.hpp`, ensure font declarations match (around line 30-40):

```cpp
// External font declarations
LV_FONT_DECLARE(font_montserrat_14);
LV_FONT_DECLARE(font_montserrat_16);
LV_FONT_DECLARE(font_montserrat_24);
LV_FONT_DECLARE(font_montserrat_32);
LV_FONT_DECLARE(font_fa_14);  // FontAwesome (unchanged)
```

### Step 4: Update Font References

Search for any references to the old `_pl` suffix and remove it:

```bash
cd main/
grep -r "font_montserrat_pl" .
```

Replace instances like:
- `&font_montserrat_pl_16` → `&font_montserrat_16`
- `font_montserrat_pl_14` → `font_montserrat_14`

Key locations to check:
- `main/gui.hpp`: Font assignments for labels, buttons
- `main/pages/*.hpp`: Any page-specific font usage
- `main/widgets/*.hpp`: Custom widget fonts

### Step 5: Clean and Rebuild

```bash
cd ~/source/ESP32-TUX-BAMBULAB
idf.py fullclean
idf.py build
```

**Note**: Full clean is required because CMake caches font files.

### Step 6: Flash and Test

```bash
# Flash with settings preservation
./scripts/flash_quick.sh

# OR standard flash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Testing Cyrillic Rendering

### Test Plan

1. **Change language to Russian**:
   - Web UI: Settings → Language → Русский → Save Settings
   - Device UI: Settings → Language → Русский

2. **Verify Russian text on screen**:
   - Home screen labels
   - Weather panel (if configured)
   - Settings page
   - Status messages

3. **Check for missing glyphs**:
   - Look for `?` or `□` characters (indicates missing glyph)
   - Common Russian letters to check: А, Б, В, Г, Д, Е, Ё, Ж, З, И, Й...

4. **Verify font sizes**:
   - Small text (14pt): Readable but not too small
   - Body text (16pt): Comfortable reading
   - Headings (24pt): Clear emphasis
   - Large numbers (32pt): Temperature, progress, etc.

### Expected Results

Before (Polish fonts only):
```
Настройки      → ????????
Температура    → ????????????
Принтер        → ???????
```

After (with Cyrillic):
```
Настройки      → Настройки       (Settings)
Температура    → Температура     (Temperature)
Принтер        → Принтер         (Printer)
```

## Troubleshooting

### Issue: Build fails with "undefined reference to font_montserrat_14"

**Cause**: Font files not added to CMakeLists.txt or named incorrectly.

**Solution**:
1. Verify files exist in `main/fonts/`
2. Check `main/CMakeLists.txt` includes them in `COMPONENT_SRCS`
3. Ensure filenames match exactly (case-sensitive)
4. Run `idf.py fullclean && idf.py build`

### Issue: Russian text still shows as `?????`

**Cause**: Fonts don't include Cyrillic range or translation strings missing.

**Solution**:
1. Check font was generated with `0x400-0x4FF` range
2. Verify `main/i18n/lang.hpp` has Russian translations
3. Confirm language set to `ru` in settings
4. Check serial monitor for font warnings

### Issue: Binary too large / Flash overflow

**Cause**: All 4 fonts + Cyrillic characters exceed flash partition.

**Solution**:
1. Use 4MB partition table → 8MB partition table
2. Reduce font sizes (use only 14pt and 16pt)
3. Lower bpp from 4 to 2 (smaller but less smooth)
4. Remove unused character ranges if not using Polish

### Issue: Fonts look pixelated/blocky

**Cause**: bpp setting too low.

**Solution**:
1. Regenerate with `--bpp 4` (or higher if flash space available)
2. Use `--no-compress` for faster rendering
3. Consider using font_montserrat-Medium.ttf for better rendering

## Character Range Reference

### Full Unicode Blocks

If you need additional characters beyond Cyrillic:

```
0x20-0x7F       Basic Latin (English)
0xA0-0xFF       Latin-1 Supplement (Western European)
0x100-0x17F     Latin Extended-A (Polish, Czech, etc.)
0x180-0x24F     Latin Extended-B (Romanian, etc.)
0x2000-0x206F   General Punctuation (em dash, quotes)
0x20A0-0x20CF   Currency Symbols (€, £, ¥, ₽)
0x400-0x4FF     Cyrillic (Russian, Ukrainian, etc.)
0x2190-0x21FF   Arrows (→, ←, ↑, ↓)
0x2600-0x26FF   Miscellaneous Symbols (☀, ☁, ❄, ⚡)
```

### Russian Essential Characters

Minimum Cyrillic set for Russian support:

```
0x410-0x44F     А-Я, а-я (main alphabet)
0x401, 0x451    Ё, ё (io letter)
```

Full Cyrillic range recommended for compatibility with Ukrainian, Belarusian, etc.

## Flash Size Recommendations

### 4MB Flash (WT32-SC01)
- **Status**: May be tight with all 4 fonts + Cyrillic
- **Recommendation**: Use 14pt and 16pt only, or switch to 8MB device

### 8MB Flash (WT32-SC01 Plus)  
- **Status**: Plenty of space
- **Recommendation**: Use all 4 fonts with full Cyrillic range

### 16MB Flash (Makerfabs ESP32S3)
- **Status**: Abundant space
- **Recommendation**: Use all fonts, consider adding more languages

## Alternative Fonts

If Montserrat doesn't meet requirements:

### Roboto
- **Source**: <https://fonts.google.com/specimen/Roboto>
- **Pros**: Very readable, good Cyrillic coverage
- **Cons**: Less stylish than Montserrat

### Open Sans
- **Source**: <https://fonts.google.com/specimen/Open+Sans>
- **Pros**: Excellent readability, wide language support
- **Cons**: Larger file size

### Liberation Sans
- **Source**: <https://github.com/liberationfonts/liberation-fonts>
- **Pros**: Compact, good screen rendering
- **Cons**: Less modern aesthetic

## Related Documentation

- `WEB_UI_I18N.md` - Web interface translations
- `BAMBU_QUICKSTART.md` - Device usage guide
- `README_BAMBULAB.md` - Project overview
- Main device UI translations: `main/i18n/lang.hpp`

## References

- LVGL Font Converter: <https://docs.lvgl.io/master/overview/font.html>
- Montserrat Font: <https://fonts.google.com/specimen/Montserrat>
- Unicode Cyrillic Chart: <https://unicode.org/charts/PDF/U0400.pdf>
- lv_font_conv GitHub: <https://github.com/lvgl/lv_font_conv>

---

**Last Updated**: December 2024  
**Status**: Documentation complete, fonts pending generation
