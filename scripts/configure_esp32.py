#!/usr/bin/env python3
"""
Configure Bambu Lab printer via ESP32-TUX web interface
"""

import requests
import json
import sys

def update_printer_config(esp_ip, printer_ip, access_code, serial):
    """Add printer configuration to ESP32-TUX via web API"""
    
    # Configuration to send
    printer_config = {
        "name": "Bambu X1C",
        "ip_address": printer_ip,
        "token": access_code,
        "serial": serial,
        "enabled": True,
        "disable_ssl_verify": True
    }
    
    print(f"Configuring ESP32-TUX at {esp_ip}")
    print(f"Printer: {printer_ip} (Serial: {serial})")
    print()
    
    # Try to add printer via web API
    url = f"http://{esp_ip}/api/printers"
    
    try:
        # First, get existing printers
        get_response = requests.get(url, timeout=5)
        if get_response.status_code == 200:
            print(f"✓ Web API available at {esp_ip}")
            
            # Add the printer
            post_response = requests.post(url, json=printer_config, timeout=5)
            if post_response.status_code == 200:
                print("✓ Printer configured successfully via web API")
                print("\nRestart the ESP32 or wait for automatic MQTT connection")
                return True
            else:
                print(f"⚠ Failed to add printer (HTTP {post_response.status_code})")
                print(f"  Response: {post_response.text}")
        else:
            print(f"⚠ Web API returned {get_response.status_code}")
    except requests.exceptions.ConnectionError:
        print(f"⚠ Cannot connect to ESP32 at {esp_ip}")
        print(f"  Make sure ESP32 is on the network and web server is running")
    except Exception as e:
        print(f"⚠ API call failed: {e}")
    
    # Alternative: Print manual configuration
    print("\nManual Configuration:")
    print("=" * 60)
    print("\nAdd this to your config.json file:")
    print()
    
    full_config = {
        "printers": [printer_config]
    }
    
    print(json.dumps(full_config, indent=2))
    print()
    print("=" * 60)
    print(f"\nTo apply manually:")
    print(f"1. Connect to ESP32-TUX web interface: http://{esp_ip}")
    print(f"2. Navigate to Printer Settings")
    print(f"3. Add printer with the following details:")
    print(f"   - Name: Bambu X1C")
    print(f"   - IP: {printer_ip}")
    print(f"   - Access Code: {access_code}")
    print(f"   - Serial: {serial}")
    print(f"   - SSL Verify: Disabled")
    
    return False

def main():
    if len(sys.argv) < 5:
        print("Usage: configure_esp32.py <ESP_IP> <PRINTER_IP> <ACCESS_CODE> <SERIAL>")
        print()
        print("Example:")
        print("  ./configure_esp32.py 10.13.13.68 10.13.13.85 5d35821c 00M09D530200738")
        sys.exit(1)
    
    esp_ip = sys.argv[1]
    printer_ip = sys.argv[2]
    access_code = sys.argv[3]
    serial = sys.argv[4]
    
    update_printer_config(esp_ip, printer_ip, access_code, serial)

if __name__ == "__main__":
    main()
