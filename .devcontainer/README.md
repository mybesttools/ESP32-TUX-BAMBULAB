# ESP32-TUX Development Container

This `.devcontainer` configuration provides a complete ESP-IDF v5.0 development environment for the ESP32-TUX project.

## Prerequisites

- Docker Desktop installed and running
- VS Code with "Dev Containers" extension
- USB serial adapter (for device flashing)

## Quick Start

1. **Open in Container:**
   - Open this project in VS Code
   - Press `Ctrl+Shift+P` (or `Cmd+Shift+P` on Mac)
   - Select "Dev Containers: Reopen in Container"
   - Wait for container to build and start

2. **Build the Project:**

   ```bash
   idf.py build
   ```

3. **Flash to Device:**

   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```

4. **Monitor Serial Output:**

   ```bash
   idf.py -p /dev/ttyUSB0 monitor
   ```

## Container Features

- **ESP-IDF v5.0** - Latest stable version
- **Python 3.10** - With all required dependencies
- **IDF Component Manager** - For managing project dependencies
- **QEMU Support** - Optional virtual device testing
- **USB Device Support** - Direct access to serial ports for flashing

## Device Connection

On **macOS**:

- Device port: `/dev/cu.SLAB_USBtoUART` or `/dev/cu.usbserial-*`
- Update `idf.port` in `devcontainer.json` if different

On **Linux**:

- Device port: `/dev/ttyUSB0` or `/dev/ttyACM0`
- May need to add user to `dialout` group: `sudo usermod -aG dialout $USER`

On **Windows**:

- Device port: `COM3`, `COM4`, etc.
- Check Device Manager for the correct port

## Environment Variables

The following are automatically configured:

- `IDF_PATH` - Points to `/opt/esp/idf`
- `IDF_TOOLS_PATH` - Points to `/opt/esp`
- `PATH` - Includes ESP-IDF tools

## VS Code Extensions

The container automatically installs:

- C/C++ IntelliSense
- ESP-IDF Extension
- Python
- GitHub Copilot (optional)

## Troubleshooting

### Container won't start

- Ensure Docker Desktop is running
- Check available disk space (build requires ~5GB)
- Try: `Dev Containers: Rebuild Container Without Cache`

### Can't flash device

- Verify USB cable and device connection
- Check device is in `/dev/` (Linux/Mac) or Device Manager (Windows)
- Update `idf.port` in `devcontainer.json`
- Try: `idf.py chip_id` to verify connection

### Serial monitor shows garbage

- Verify baud rate: 115200 (default)
- Check cable connections
- Try: `idf.py -p <PORT> monitor --baud 115200`

## Documentation

- [ESP-IDF Official Documentation](https://docs.espressif.com/projects/esp-idf/en/v5.0/)
- [Dev Containers Documentation](https://containers.dev/)
- [Project README](../README.md)

## Notes

- The container runs as the `esp` user (non-root)
- Device `/dev` is mounted for USB serial access
- Build artifacts are stored in `/workspace/build`
- Container can be removed/rebuilt without affecting code
