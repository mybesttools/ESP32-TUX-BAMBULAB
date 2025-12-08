#!/usr/bin/env python3
"""
Test MQTT connection to Bambu Lab printer
Usage: python test_bambu_mqtt.py <IP> <ACCESS_CODE>
"""

import sys
import ssl
import json
import time
import paho.mqtt.client as mqtt

# Printer details
PRINTER_IP = "10.13.13.141"
ACCESS_CODE = "10235137"
PORT = 8883

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print(f"✓ Connected successfully!")
        
        # For A1 Mini, we need to know the serial number
        # Let's try several approaches
        serial = userdata.get('serial', '')
        
        if serial:
            # If serial provided, subscribe to specific topic
            topic = f"device/{serial}/report"
            client.subscribe(topic, qos=0)
            print(f"✓ Subscribed to: {topic}")
            
            # Send push_all to request status
            request_topic = f"device/{serial}/request"
            push_cmd = json.dumps({
                "pushing": {
                    "sequence_id": "0", 
                    "command": "pushall"
                }
            })
            client.publish(request_topic, push_cmd)
            print(f"→ Sent push_all to: {request_topic}")
        else:
            print("! No serial provided - connection will stay idle")
            print("  To get serial: check Bambu app or printer display")
            print("  Re-run with: python3 test_bambu_mqtt.py <IP> <ACCESS_CODE> <SERIAL>")
    else:
        print(f"✗ Connection failed with code: {rc}")
        # RC codes:
        # 0 = Success
        # 1 = Incorrect protocol version
        # 2 = Invalid client identifier
        # 3 = Server unavailable
        # 4 = Bad username or password
        # 5 = Not authorized
        if rc == 4:
            print("  *** WRONG ACCESS CODE ***")
        elif rc == 5:
            print("  *** NOT AUTHORIZED ***")

def on_message(client, userdata, msg):
    print(f"\n{'='*60}")
    print(f"Topic: {msg.topic}")
    
    # Extract serial from topic
    parts = msg.topic.split('/')
    if len(parts) >= 2:
        serial = parts[1]
        print(f"Serial: {serial}")
    
    # Try to parse JSON
    try:
        data = json.loads(msg.payload.decode('utf-8'))
        # Print key info
        if 'print' in data:
            p = data['print']
            print(f"\n--- Printer Status ---")
            print(f"State: {p.get('gcode_state', 'unknown')}")
            print(f"Progress: {p.get('mc_percent', 0)}%")
            print(f"Layer: {p.get('layer_num', 0)}/{p.get('total_layer_num', 0)}")
            print(f"Remaining: {p.get('mc_remaining_time', 0)} min")
            print(f"Nozzle: {p.get('nozzle_temper', 0)}°C / {p.get('nozzle_target_temper', 0)}°C")
            print(f"Bed: {p.get('bed_temper', 0)}°C / {p.get('bed_target_temper', 0)}°C")
            print(f"File: {p.get('gcode_file', 'none')}")
            print(f"WiFi: {p.get('wifi_signal', 'unknown')}")
        else:
            # Print raw (truncated)
            payload_str = json.dumps(data, indent=2)
            if len(payload_str) > 500:
                print(f"Payload (truncated): {payload_str[:500]}...")
            else:
                print(f"Payload: {payload_str}")
    except Exception as e:
        print(f"Raw payload ({len(msg.payload)} bytes): {msg.payload[:200]}...")
    
    print(f"{'='*60}\n")
    
    # Exit after first message
    userdata['received'] = True

def on_disconnect(client, userdata, rc, properties=None):
    print(f"Disconnected with code: {rc}")
    if rc != 0:
        print("  Unexpected disconnection - printer may have closed connection")

def on_log(client, userdata, level, buf):
    if level <= mqtt.MQTT_LOG_WARNING:
        print(f"[MQTT] {buf}")

def main():
    global PRINTER_IP, ACCESS_CODE
    
    serial = ""
    
    if len(sys.argv) >= 3:
        PRINTER_IP = sys.argv[1]
        ACCESS_CODE = sys.argv[2]
    if len(sys.argv) >= 4:
        serial = sys.argv[3]
    
    print(f"Testing MQTT connection to Bambu printer")
    print(f"  IP: {PRINTER_IP}")
    print(f"  Port: {PORT}")
    print(f"  Access Code: {ACCESS_CODE}")
    print(f"  Serial: {serial if serial else '(not provided)'}")
    print(f"  Username: bblp")
    print()
    
    # Create MQTT client
    userdata = {'received': False, 'serial': serial}
    client = mqtt.Client(
        client_id="python_test",
        protocol=mqtt.MQTTv311,
        userdata=userdata
    )
    
    # Set credentials
    client.username_pw_set("bblp", ACCESS_CODE)
    
    # Configure TLS - skip certificate verification (self-signed)
    client.tls_set(cert_reqs=ssl.CERT_NONE)
    client.tls_insecure_set(True)
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    # client.on_log = on_log  # Uncomment for debug
    
    print("Connecting...")
    try:
        client.connect(PRINTER_IP, PORT, keepalive=60)
    except Exception as e:
        print(f"✗ Connection error: {e}")
        return 1
    
    # Run for up to 15 seconds waiting for a message
    start = time.time()
    timeout = 15
    
    while time.time() - start < timeout:
        client.loop(timeout=1.0)
        if userdata['received']:
            print("\n✓ Successfully received printer data!")
            break
    else:
        print(f"\n✗ Timeout after {timeout} seconds - no data received")
    
    client.disconnect()
    return 0

if __name__ == "__main__":
    sys.exit(main())
