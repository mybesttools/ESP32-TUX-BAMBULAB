#!/usr/bin/env python3
"""
Get Bambu Lab printer serial number via MQTT subscription
"""

import paho.mqtt.client as mqtt
import ssl
import json
import sys
import time

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✓ Connected to printer")
        client.subscribe("device/+/report")
        print("✓ Subscribed, waiting for printer message...")
    else:
        print(f"✗ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    # Extract serial from topic: device/{SERIAL}/report
    topic_parts = msg.topic.split('/')
    if len(topic_parts) >= 3:
        serial = topic_parts[1]
        print(f"\n✓ Found printer serial: {serial}")
        
        # Try to get printer name from message
        try:
            data = json.loads(msg.payload)
            if 'print' in data and 'name' in data['print']:
                print(f"  Printer name: {data['print']['name']}")
        except:
            pass
        
        userdata['serial'] = serial
        userdata['found'] = True

def main():
    if len(sys.argv) < 3:
        print("Usage: get_printer_serial.py <IP> <ACCESS_CODE>")
        sys.exit(1)
    
    ip = sys.argv[1]
    access_code = sys.argv[2]
    
    user_data = {'serial': None, 'found': False}
    
    client = mqtt.Client(client_id="SerialFinder", userdata=user_data)
    client.username_pw_set("bblp", access_code)
    client.tls_set(cert_reqs=ssl.CERT_NONE)
    client.tls_insecure_set(True)
    
    client.on_connect = on_connect
    client.on_message = on_message
    
    print(f"Connecting to {ip}:8883...")
    client.connect(ip, 8883, 60)
    
    client.loop_start()
    
    # Wait up to 10 seconds for a message
    for i in range(10):
        if user_data['found']:
            break
        time.sleep(1)
    
    client.loop_stop()
    client.disconnect()
    
    if user_data['serial']:
        print(f"\nSerial number: {user_data['serial']}")
        return user_data['serial']
    else:
        print("\n✗ No serial number found (printer may be idle)")
        print("  Try sending a command to the printer to wake it up")
        sys.exit(1)

if __name__ == "__main__":
    main()
