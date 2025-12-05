#!/usr/bin/env python3
"""
Bambu Lab Printer MQTT Info Retriever
Connects to printer MQTT broker and retrieves device information
"""

import paho.mqtt.client as mqtt
import json
import time
import ssl
import sys

class BambuPrinterClient:
    def __init__(self, ip, access_code, timeout=5):
        self.ip = ip
        self.access_code = access_code
        self.timeout = timeout
        self.client = mqtt.Client()
        self.received_data = {}
        self.device_info = {}
        
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"✓ Connected to printer at {self.ip}")
            client.subscribe("device/+/report")
        else:
            print(f"✗ Connection failed with code {rc}")
    
    def on_message(self, client, userdata, msg):
        try:
            payload = msg.payload.decode('utf-8')
            data = json.loads(payload)
            
            # Extract serial from topic
            parts = msg.topic.split('/')
            if len(parts) >= 2:
                serial = parts[1]
                self.device_info['serial'] = serial
                print(f"\n✓ Found device: {serial}")
                print(f"  Topic: {msg.topic}")
                print(f"  Payload size: {len(payload)} bytes\n")
                
                # Print available top-level keys
                if isinstance(data, dict):
                    print("Available fields in report:")
                    for key in sorted(data.keys()):
                        value = data[key]
                        if isinstance(value, dict):
                            print(f"  {key}: {{dict with {len(value)} keys}}")
                            # Show first few keys if dict is reasonably sized
                            subkeys = sorted(value.keys())[:10]
                            for k in subkeys:
                                subval = value[k]
                                if isinstance(subval, (str, int, float, bool)):
                                    print(f"    - {k}: {subval}")
                                else:
                                    print(f"    - {k}: {type(subval).__name__}")
                            if len(value) > 10:
                                print(f"    ... and {len(value) - 10} more keys")
                        elif isinstance(value, list):
                            print(f"  {key}: [list with {len(value)} items]")
                        elif isinstance(value, str):
                            if len(value) > 50:
                                print(f"  {key}: '{value[:50]}...'")
                            else:
                                print(f"  {key}: '{value}'")
                        else:
                            print(f"  {key}: {value}")
                    
                    # Extract useful info
                    self.extract_printer_info(data, serial)
        except Exception as e:
            print(f"Error parsing message: {e}")
    
    def on_disconnect(self, client, userdata, rc):
        if rc != 0:
            print(f"Unexpected disconnection with code {rc}")
    
    def extract_printer_info(self, data, serial):
        """Extract useful printer information from MQTT report"""
        info = {
            'serial': serial,
            'ip': self.ip,
        }
        
        # Navigate to the print field which contains all the data
        if 'print' in data and isinstance(data['print'], dict):
            print_data = data['print']
            
            # Extract useful fields
            useful_fields = [
                'printer_type', 'model', 'gcode_state', 'print_type', 'home_flag',
                'bed_temper', 'bed_target_temper', 'nozzle_temper', 'nozzle_target_temper',
                'chamber_temper', 'sub1g_bed_target_temper',
                'ams_status', 'ams_rfid_status',
                'cali_flag', 'wifi_signal', 'module_offline',
                'fan_gear', 'aux_part_fan', 'chamber_fan',
                'print_error', 'print_process_remain',
                'print_real_action', 'command', 'camera_nozzle_vision_calibration',
                'lifecycle', 'design_name', 'design_checksum', 'model_id',
                'build_plate', 'build_plate_type',
            ]
            
            for field in useful_fields:
                if field in print_data:
                    value = print_data[field]
                    # Skip dict and list values for cleaner output
                    if not isinstance(value, (dict, list)):
                        info[field] = value
        
        print("\nExtracted Printer Info:")
        for key, value in sorted(info.items()):
            print(f"  {key}: {value}")
        
        self.device_info.update(info)
    
    def get_info(self):
        """Connect and retrieve printer info"""
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        
        self.client.tls_set_context(context)
        self.client.username_pw_set("bblp", self.access_code)
        
        try:
            print(f"Connecting to {self.ip}:8883...")
            self.client.connect(self.ip, 8883, keepalive=5)
            
            print(f"Listening for {self.timeout} seconds...\n")
            self.client.loop_start()
            time.sleep(self.timeout)
            self.client.loop_stop()
            self.client.disconnect()
            
            return self.device_info
        except Exception as e:
            print(f"Error: {e}")
            return {}

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 get_printer_info.py <ip> <access_code> [timeout]")
        print("Example: python3 get_printer_info.py 10.13.13.85 5d35821c 5")
        sys.exit(1)
    
    ip = sys.argv[1]
    code = sys.argv[2]
    timeout = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    
    client = BambuPrinterClient(ip, code, timeout)
    info = client.get_info()
    
    print("\n" + "="*50)
    print("Final Device Info:")
    print(json.dumps(info, indent=2))
