#!/usr/bin/env python3
"""
Bambu Lab FTP Camera Snapshot Fetcher

Downloads the most recent camera snapshot from the printer's SD card.
This works for ALL Bambu printer types that have SD cards.

The printer saves IP camera recordings every ~5 minutes with thumbnails.
This script grabs the most recent thumbnail as a "snapshot".

Thumbnails are 480x480 JPEG, ~15KB - perfect for ESP32/LVGL.

IMPORTANT: Only downloads if the snapshot is from the current print session,
determined by checking the timestamp is within the print duration.
"""

import ftplib
import ssl
import os
import re
from datetime import datetime, timedelta

# Printer configuration - UPDATE THESE VALUES
HOSTNAME = "10.13.13.85"
ACCESS_CODE = "5d35821c"
PORT = 990
USERNAME = "bblp"

# Output directory
OUTPUT_DIR = "/Users/mike/source/ESP32-TUX-BAMBULAB/scripts/ftp_downloads"

# Print job info (would come from MQTT in real implementation)
# Set to None to disable time filtering (download any recent snapshot)
PRINT_START_TIME = None  # datetime object when print started
PRINT_DURATION_MINUTES = None  # Total expected print time in minutes


class ImplicitFTPS(ftplib.FTP_TLS):
    """FTP_TLS with implicit TLS and session reuse for Bambu printers."""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._sock = None
    
    @property
    def sock(self):
        return self._sock
    
    @sock.setter
    def sock(self, value):
        if value is not None and not isinstance(value, ssl.SSLSocket):
            value = self.context.wrap_socket(value, server_hostname=self.host)
        self._sock = value
    
    def ntransfercmd(self, cmd, rest=None):
        conn, size = ftplib.FTP.ntransfercmd(self, cmd, rest)
        if self._prot_p:
            conn = self.context.wrap_socket(
                conn,
                server_hostname=self.host,
                session=self.sock.session
            )
        return conn, size


def parse_timestamp(filename):
    """Extract timestamp from ipcam filename.
    
    Format: ipcam-record.YYYY-MM-DD_HH-MM-SS.X.jpg
    """
    match = re.search(r'(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})', filename)
    if match:
        try:
            return datetime.strptime(match.group(1), '%Y-%m-%d_%H-%M-%S')
        except:
            pass
    return None


def is_snapshot_valid(snapshot_time, print_start=None, print_duration_min=None):
    """Check if snapshot is from the current print session.
    
    Args:
        snapshot_time: datetime of the snapshot
        print_start: datetime when print started (None = use "today" heuristic)
        print_duration_min: expected print duration in minutes
        
    Returns:
        True if snapshot is valid (recent enough to be from current print)
    """
    if snapshot_time is None:
        return False
    
    now = datetime.now()
    
    # If we have print start time and duration, use precise check
    if print_start is not None and print_duration_min is not None:
        # Snapshot must be after print started
        if snapshot_time < print_start:
            return False
        # Snapshot must be within the print duration window (with some margin)
        max_age = print_start + timedelta(minutes=print_duration_min + 10)
        if snapshot_time > max_age:
            return False
        return True
    
    # Fallback: snapshot must be from today and not too old
    # Check if same day
    if snapshot_time.date() != now.date():
        return False
    
    # Check if not older than 24 hours (safety check)
    age = now - snapshot_time
    if age > timedelta(hours=24):
        return False
    
    return True


def get_latest_snapshot(hostname, access_code, port=990, 
                        print_start=None, print_duration_min=None):
    """Connect to printer and get the most recent camera snapshot.
    
    Args:
        hostname: Printer IP address
        access_code: Printer access code
        port: FTP port (default 990)
        print_start: datetime when print started (for validation)
        print_duration_min: expected print duration in minutes
    
    Returns tuple: (image_data: bytes, filename: str, timestamp: datetime) 
                   or (None, None, None) on error.
    """
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    
    ftp = ImplicitFTPS(context=ctx)
    ftp.connect(hostname, port, timeout=15)
    ftp.login(USERNAME, access_code)
    ftp.prot_p()
    
    # Check /ipcam/thumbnail/ for camera recording thumbnails
    latest_file = None
    latest_time = None
    
    try:
        ftp.cwd("/ipcam/thumbnail")
        lines = []
        ftp.dir(lines.append)
        
        for line in lines:
            parts = line.split()
            if len(parts) >= 9:
                name = " ".join(parts[8:])
                if name.endswith('.jpg'):
                    timestamp = parse_timestamp(name)
                    # Check if this snapshot is valid (from current print)
                    if is_snapshot_valid(timestamp, print_start, print_duration_min):
                        if latest_time is None or timestamp > latest_time:
                            latest_time = timestamp
                            latest_file = f"/ipcam/thumbnail/{name}"
    except Exception as e:
        print(f"Warning: Could not access /ipcam/thumbnail: {e}")
    
    # Also check /timelapse/thumbnail/ as fallback
    if latest_file is None:
        try:
            ftp.cwd("/timelapse/thumbnail")
            lines = []
            ftp.dir(lines.append)
            
            for line in lines:
                parts = line.split()
                if len(parts) >= 9:
                    name = " ".join(parts[8:])
                    if name.endswith('.jpg'):
                        timestamp = parse_timestamp(name)
                        if is_snapshot_valid(timestamp, print_start, print_duration_min):
                            if latest_time is None or timestamp > latest_time:
                                latest_time = timestamp
                                latest_file = f"/timelapse/thumbnail/{name}"
        except:
            pass
    
    if latest_file is None:
        ftp.quit()
        return None, None, None
    
    # Download the file
    from io import BytesIO
    buffer = BytesIO()
    ftp.retrbinary(f'RETR {latest_file}', buffer.write)
    ftp.quit()
    
    return buffer.getvalue(), os.path.basename(latest_file), latest_time


def main():
    print("=" * 60)
    print("Bambu Lab FTP Camera Snapshot Fetcher")
    print("=" * 60)
    print(f"Printer: {HOSTNAME}")
    print(f"Date filter: Only snapshots from today")
    if PRINT_START_TIME and PRINT_DURATION_MINUTES:
        print(f"Print started: {PRINT_START_TIME}")
        print(f"Print duration: {PRINT_DURATION_MINUTES} min")
    print("=" * 60)
    
    print("\nFetching most recent camera snapshot...")
    
    try:
        data, filename, timestamp = get_latest_snapshot(
            HOSTNAME, ACCESS_CODE, PORT,
            print_start=PRINT_START_TIME,
            print_duration_min=PRINT_DURATION_MINUTES
        )
        
        if data:
            now = datetime.now()
            age = now - timestamp
            age_str = f"{int(age.total_seconds() // 60)} minutes ago"
            
            print(f"✅ Found: {filename}")
            print(f"   Timestamp: {timestamp.strftime('%Y-%m-%d %H:%M:%S')} ({age_str})")
            print(f"   Size: {len(data)} bytes ({len(data)/1024:.1f} KB)")
            
            # Save it
            os.makedirs(OUTPUT_DIR, exist_ok=True)
            output_path = os.path.join(OUTPUT_DIR, "latest_snapshot.jpg")
            with open(output_path, 'wb') as f:
                f.write(data)
            print(f"✅ Saved to: {output_path}")
            
            # Also save with original name
            original_path = os.path.join(OUTPUT_DIR, filename)
            with open(original_path, 'wb') as f:
                f.write(data)
            print(f"✅ Also saved as: {original_path}")
            
            # Show image info
            if data[:2] == b'\xff\xd8':  # JPEG magic
                print("   Format: JPEG")
        else:
            print("❌ No valid camera snapshots found")
            print("   Possible reasons:")
            print("   - No snapshots from today")
            print("   - Printer hasn't recorded any camera footage yet")
            print("   - No SD card in printer")
            
    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()
