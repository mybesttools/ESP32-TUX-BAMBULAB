# Flashing ESP32-TUX

## Quick Start (Recommended)

```bash
./flash_quick.sh /dev/cu.SLAB_USBtoUART
```

**That's it!** Your settings are automatically preserved. ✓

---

## Why This Works

When you flash new firmware using `idf.py flash`, it erases **all** partitions including SPIFFS (which contains your settings).

`flash_quick.sh` uses esptool to **only flash the bootloader, partition table, and application** - **skipping the SPIFFS partition entirely**.

**Result:** Your settings survive the firmware update! ✓

## What Gets Preserved

✓ Configured networks for printer discovery  
✓ Discovered printers and their settings  
✓ Weather locations  
✓ Weather API key  
✓ Timezone selection  
✓ Display brightness and theme  
✓ All other JSON config data  

## Building & Flashing Workflow

```bash
# Make changes to code
git add .
git commit -m "Your changes"

# Build
idf.py build

# Flash with settings preserved
./flash_quick.sh /dev/cu.SLAB_USBtoUART

# Monitor (optional)
idf.py monitor -p /dev/cu.SLAB_USBtoUART
```

## Technical Details

**ESP32 Partition Layout:**
```
0x1000:   Bootloader (8 KB)
0x8000:   Partition Table (4 KB)
0x10000:  Application (1.8 MB)
0x260000: SPIFFS Storage (1.5 MB) ← Contains your settings
```

`flash_quick.sh` only updates addresses 0x1000, 0x8000, and 0x10000, preserving SPIFFS data at 0x260000.

## If You Need to Reset Settings

To completely erase and reset:

```bash
idf.py erase_flash
idf.py -p /dev/cu.SLAB_USBtoUART flash
```

This will:
- Erase all partitions including SPIFFS
- Flash fresh firmware with default settings

## Troubleshooting

**Port not found?**
```bash
# List available ports
ls /dev/tty.* /dev/cu.*
```

**Permission denied?**
```bash
chmod +x flash_quick.sh
```

**Settings still lost?**
- Make sure you're using `./flash_quick.sh` (not `idf.py flash`)
- Check that SPIFFS partition exists: `idf.py partition_table-list`

