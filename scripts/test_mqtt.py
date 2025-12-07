#!/usr/bin/env python3
"""
Bambu Lab MQTT Test Script
Tests MQTT connection to Bambu printer and validates query mechanism
"""

import paho.mqtt.client as mqtt
import ssl
import json
import sys
import time
import argparse

def on_connect(client, userdata, flags, rc):
    """Callback when connected to MQTT broker"""
    if rc == 0:
        print("✓ Connected to Bambu printer MQTT broker")
        # Subscribe to printer status topic
        client.subscribe("device/+/report")
        print("✓ Subscribed to device/+/report")
        
        # Send pushall query after 1 second
        time.sleep(1)
        send_query(client, userdata['serial'])
    else:
        print(f"✗ Connection failed with code {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when message is received"""
    print(f"\n✓ Received message on topic: {msg.topic}")
    print(f"  Message length: {len(msg.payload)} bytes")
    
    try:
        data = json.loads(msg.payload)
        print("\n✓ JSON parsed successfully")
        print(f"  Keys: {list(data.keys())}")
        
        # Extract key information
        if 'print' in data:
            print_data = data['print']
            print(f"\n  Print status:")
            if 'gcode_state' in print_data:
                print(f"    State: {print_data['gcode_state']}")
            if 'mc_percent' in print_data:
                print(f"    Progress: {print_data['mc_percent']}%")
            if 'nozzle_temper' in print_data:
                print(f"    Nozzle temp: {print_data['nozzle_temper']}°C")
            if 'bed_temper' in print_data:
                print(f"    Bed temp: {print_data['bed_temper']}°C")
        
        # Save full response to file
        with open('/tmp/bambu_response.json', 'w') as f:
            json.dump(data, f, indent=2)
        print(f"\n✓ Full response saved to /tmp/bambu_response.json")
        
    except json.JSONDecodeError as e:
        print(f"✗ Failed to parse JSON: {e}")
        print(f"  Raw payload (first 500 chars): {msg.payload[:500]}")

def on_subscribe(client, userdata, mid, granted_qos):
    """Callback when subscription is acknowledged"""
    print(f"✓ Subscription acknowledged (QoS: {granted_qos})")

def on_publish(client, userdata, mid):
    """Callback when message is published"""
    print(f"✓ Query published (msg_id: {mid})")

def send_query(client, serial):
    """Send pushall query to printer"""
    topic = f"device/{serial}/request"
    payload = json.dumps({
        "pushing": {
            "sequence_id": "0",
            "command": "pushall"
        }
    })
    
    print(f"\n→ Sending query to {topic}")
    print(f"  Payload: {payload}")
    
    result = client.publish(topic, payload, qos=1)
    if result.rc != mqtt.MQTT_ERR_SUCCESS:
        print(f"✗ Publish failed with code {result.rc}")

def main():
    parser = argparse.ArgumentParser(description='Test Bambu Lab MQTT connection')
    parser.add_argument('ip', help='Printer IP address')
    parser.add_argument('access_code', help='Printer access code')
    parser.add_argument('serial', help='Printer serial number')
    parser.add_argument('--port', type=int, default=8883, help='MQTT port (default: 8883)')
    parser.add_argument('--no-verify', action='store_true', help='Skip SSL certificate verification')
    
    args = parser.parse_args()
    
    print("=" * 60)
    print("Bambu Lab MQTT Connection Test")
    print("=" * 60)
    print(f"Printer IP: {args.ip}")
    print(f"Port: {args.port}")
    print(f"Serial: {args.serial}")
    print(f"SSL Verify: {not args.no_verify}")
    print("=" * 60)
    
    # Create MQTT client
    client = mqtt.Client(client_id="ESP32-TUX-Test")
    client.user_data_set({'serial': args.serial})
    
    # Set username (required: 'bblp') and password (access code)
    client.username_pw_set("bblp", args.access_code)
    
    # Configure TLS
    if args.no_verify:
        client.tls_set(cert_reqs=ssl.CERT_NONE)
        client.tls_insecure_set(True)
        print("⚠ SSL verification disabled")
    else:
        client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_subscribe = on_subscribe
    client.on_publish = on_publish
    
    try:
        print(f"\n→ Connecting to {args.ip}:{args.port}...")
        client.connect(args.ip, args.port, keepalive=60)
        
        # Start loop
        print("→ Starting MQTT loop (waiting for messages)...")
        print("  Press Ctrl+C to stop\n")
        client.loop_forever()
        
    except KeyboardInterrupt:
        print("\n\n→ Stopping test...")
        client.disconnect()
        print("✓ Test completed")
        
    except Exception as e:
        print(f"\n✗ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
