#!/usr/bin/env python3
"""
Bambu Lab Printer Discovery Script
Scans the local network for Bambu Lab printers
"""

import socket
import json
import sys
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed

# Bambu Lab uses these ports
BAMBU_MQTT_PORT = 8883
BAMBU_FTP_PORT = 990

def check_port(ip, port, timeout=0.5):
    """Check if a port is open on a given IP"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((ip, port))
        sock.close()
        return result == 0
    except:
        return False

def scan_ip(ip, verbose=False):
    """Scan a single IP for Bambu printer"""
    if verbose:
        print(f"  Scanning {ip}...", end='\r')
    
    # Check MQTT port (8883)
    mqtt_open = check_port(ip, BAMBU_MQTT_PORT)
    
    if mqtt_open:
        # Bambu printer found!
        return {
            'ip': ip,
            'mqtt_port': BAMBU_MQTT_PORT,
            'ftp_port': BAMBU_FTP_PORT if check_port(ip, BAMBU_FTP_PORT) else None
        }
    
    return None

def scan_subnet(subnet, threads=50):
    """Scan a subnet for Bambu printers"""
    # Parse subnet (e.g., "192.168.1.0/24")
    if '/' not in subnet:
        print(f"Error: Invalid subnet format. Use CIDR notation (e.g., 192.168.1.0/24)")
        return []
    
    network, mask = subnet.split('/')
    mask = int(mask)
    
    if mask != 24:
        print(f"Warning: Only /24 subnets are currently supported")
        return []
    
    # Get base IP
    parts = network.split('.')
    base = '.'.join(parts[:3])
    
    print(f"\nScanning {subnet} for Bambu Lab printers...")
    print(f"This may take 30-60 seconds...\n")
    
    printers = []
    
    # Scan all IPs in the subnet
    with ThreadPoolExecutor(max_workers=threads) as executor:
        futures = {executor.submit(scan_ip, f"{base}.{i}", False): i for i in range(1, 255)}
        
        for future in as_completed(futures):
            result = future.result()
            if result:
                printers.append(result)
                print(f"✓ Found Bambu printer at {result['ip']}")
    
    return printers

def get_local_subnet():
    """Try to determine the local subnet"""
    try:
        # Get local IP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        
        # Convert to /24 subnet
        parts = local_ip.split('.')
        subnet = '.'.join(parts[:3]) + '.0/24'
        
        return subnet
    except:
        return None

def main():
    parser = argparse.ArgumentParser(description='Discover Bambu Lab printers on the network')
    parser.add_argument('--subnet', help='Subnet to scan in CIDR notation (e.g., 192.168.1.0/24)')
    parser.add_argument('--threads', type=int, default=50, help='Number of concurrent threads (default: 50)')
    
    args = parser.parse_args()
    
    print("=" * 60)
    print("Bambu Lab Printer Discovery")
    print("=" * 60)
    
    # Determine subnet to scan
    subnet = args.subnet
    if not subnet:
        subnet = get_local_subnet()
        if subnet:
            print(f"Auto-detected local subnet: {subnet}")
            response = input(f"Scan this subnet? [Y/n]: ")
            if response.lower() == 'n':
                subnet = input("Enter subnet to scan (e.g., 192.168.1.0/24): ")
        else:
            subnet = input("Enter subnet to scan (e.g., 192.168.1.0/24): ")
    
    # Scan the subnet
    printers = scan_subnet(subnet, args.threads)
    
    # Display results
    print("\n" + "=" * 60)
    print(f"Discovery Complete - Found {len(printers)} printer(s)")
    print("=" * 60)
    
    if printers:
        print("\nPrinter(s) found:\n")
        for i, printer in enumerate(printers, 1):
            print(f"{i}. IP Address: {printer['ip']}")
            print(f"   MQTT Port: {printer['mqtt_port']}")
            if printer['ftp_port']:
                print(f"   FTP Port: {printer['ftp_port']}")
            print()
        
        print("\nNext steps:")
        print("1. Get the access code from your printer:")
        print("   Settings → Network → MQTT → Access Code")
        print()
        print("2. Get the serial number from your printer:")
        print("   Settings → Device → Serial Number")
        print()
        print("3. Test MQTT connection:")
        print(f"   ./scripts/test_mqtt.py {printers[0]['ip']} <ACCESS_CODE> <SERIAL> --no-verify")
        print()
        print("4. Add to config.json:")
        
        config_example = {
            "printers": [
                {
                    "name": "My Bambu Printer",
                    "ip_address": printers[0]['ip'],
                    "token": "YOUR_ACCESS_CODE",
                    "serial": "YOUR_SERIAL_NUMBER",
                    "enabled": True,
                    "disable_ssl_verify": True
                }
            ]
        }
        print(json.dumps(config_example, indent=2))
        
    else:
        print("\nNo Bambu Lab printers found on this subnet.")
        print("\nTroubleshooting:")
        print("- Ensure printer is powered on and connected to WiFi")
        print("- Check that printer is on the same network")
        print("- Try scanning a different subnet")
        print("- Check printer network settings for IP address")
    
    print()

if __name__ == "__main__":
    main()
