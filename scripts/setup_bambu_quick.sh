#!/bin/bash
#
# Quick Setup Script for Bambu Lab Printer Integration
#

set -e

echo "============================================================"
echo "Bambu Lab Printer - Quick Setup"
echo "============================================================"
echo ""

# Your configuration
PRINTER_IP="10.13.13.85"
ACCESS_CODE="5d35821c"
SERIAL="00M09D530200738"
ESP32_IP="10.13.13.68"

echo "Configuration:"
echo "  Printer IP:  $PRINTER_IP"
echo "  Access Code: $ACCESS_CODE"
echo "  Serial:      $SERIAL"
echo "  ESP32 IP:    $ESP32_IP"
echo ""

# Test MQTT connection
echo "→ Testing MQTT connection..."
if timeout 5s python3 scripts/test_mqtt.py $PRINTER_IP $ACCESS_CODE $SERIAL --no-verify >/dev/null 2>&1; then
    echo "✓ MQTT connection successful"
else
    echo "✗ MQTT connection failed"
    echo "  Check printer IP and access code"
    exit 1
fi

# Configure ESP32
echo ""
echo "→ Configuring ESP32 via web interface..."
python3 scripts/configure_esp32.py $ESP32_IP $PRINTER_IP $ACCESS_CODE $SERIAL

echo ""
echo "============================================================"
echo "Setup Complete!"
echo "============================================================"
echo ""
echo "Next steps:"
echo "1. If web API succeeded, restart ESP32 and monitor logs"
echo "2. If web API failed, use the web interface at http://$ESP32_IP"
echo "3. Check PRINTER_CONFIG.md for detailed instructions"
echo ""
echo "Monitor ESP32 logs for these messages:"
echo "  - 'Bambu Monitor configured'"
echo "  - 'MQTT Connected to Bambu printer'"
echo "  - 'Subscribed to device/+/report'"
echo ""
