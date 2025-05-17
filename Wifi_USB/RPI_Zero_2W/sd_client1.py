import os
import platform
import socket
import time
import json
import shutil
import logging
import magic
import threading
import requests
from zeroconf import Zeroconf, ServiceInfo

from flask import Flask, send_from_directory, request
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler, DirDeletedEvent, DirMovedEvent, FileDeletedEvent, FileModifiedEvent, FileMovedEvent

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# Webserver Configuration
RPI_IP = "0.0.0.0" # Listen on all interfaces
RPI_PORT = 5001

# File Storage Configuration
STORAGE_PATH = "/mnt/sdcard"

# Mass Storage Commands
CMD_MOUNT = "modprobe g_mass_storage file=/piusb.bin stall=0 ro=0 removable=1"
CMD_UNMOUNT = "modprobe -r g_mass_storage"
CMD_SYNC = "sync"

os.system(CMD_UNMOUNT)
logging.info("Unmounting USB via g_mass_storage...")
time.sleep(2)
os.system(CMD_SYNC)
logging.info("Syncing USB data...")
time.sleep(2)
os.system(CMD_MOUNT)
logging.info("USB remounted successfully.")
# Watchdog Settings
ACT_EVENTS = [DirDeletedEvent, DirMovedEvent, FileDeletedEvent, FileModifiedEvent, FileMovedEvent]
ACT_TIME_OUT = 10

app = Flask(__name__)

# -------------------- File System Observer (Watchdog) --------------------
class DirtyHandler(FileSystemEventHandler):
    def __init__(self):
        self.reset()

    def on_any_event(self, event):
        if type(event) in ACT_EVENTS:
            self._dirty = True
            self._dirty_time = time.time()
            self.remount_usb()

    @property
    def dirty(self):
        return self._dirty

    @property
    def dirty_time(self):
        return self._dirty_time

    def reset(self):
        self._dirty = False
        self._dirty_time = 0
        self._path = None

    def remount_usb(self):
        try:
            logging.info("File system changed, attempting to remount USB...")
            os.system(CMD_UNMOUNT)
            logging.info("Unmounting USB via g_mass_storage...")
            time.sleep(2)
            os.system(CMD_SYNC)
            logging.info("Syncing USB data...")
            time.sleep(2)
            os.system(CMD_MOUNT)
            logging.info("USB remounted successfully.")
        except Exception as e:
            logging.error(f"Error remounting USB: {e}")

# -------------------- Helper Functions --------------------
def get_rpi_id():
    """Attempts to read the serial number from /proc/cpuinfo, which should be unique."""
    try:
        with open('/proc/cpuinfo', 'r') as f:
            for line in f:
                if line.startswith('Serial'):
                    return line.split(':')[1].strip()
    except Exception as e:
        logging.error(f"Error getting RPi ID: {e}")
        return "UNKNOWN_RPI"

def get_local_ip():
    """Get the local IP address of the RPi."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))  # Connect to Google DNS to get local IP
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except Exception as e:
        logging.error(f"Error getting local IP: {e}")
        return "0.0.0.0"

RPI_ID = get_rpi_id()
RPI_LOCAL_IP = get_local_ip()

def get_space_info(mount_point=STORAGE_PATH):
    """Returns a dictionary of (used space, total space) in bytes."""
    try:
        total, used, free = shutil.disk_usage(mount_point)
        logging.info(f"Total space: {total}, Used space: {used}, Free space: {free}")
        return {"total": total, "used": used, "free": free}
    except Exception as e:
        logging.error(f"Error getting disk usage info: {e}")
        return {"total": 0, "used": 0, "free": 0}

# -------------------- Flask Routes --------------------
@app.route("/list_files")
def list_files():
    try:
        files = os.listdir(STORAGE_PATH)
        file_info = []
        for file in files:
            filepath = os.path.join(STORAGE_PATH, file)
            file_size_bytes = os.path.getsize(filepath)
            file_size_kb = round(file_size_bytes / 1024, 2)

            modified_time = os.path.getmtime(filepath)
            modified_date = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(modified_time))

            try:
                mime = magic.Magic(mime=True)
                file_type = mime.from_file(filepath)
            except Exception as e:
                file_type = "Unknown"

            file_info.append({
                "name": file,
                "modified": modified_date,
                "type": file_type,
                "size_kb": file_size_kb
            })
        logging.info(f"Files listed successfully: {file_info}")
        return json.dumps(file_info)
    except Exception as e:
        logging.error(f"Error listing files: {e}")
        return json.dumps({"error": str(e)})

@app.route("/upload/<filename>", methods=['POST'])
def upload_file(filename):
    try:
        if 'file' not in request.files:
            return "No file part"
        file = request.files['file']
        if file.filename == '':
            return "No selected file"

        filepath = os.path.join(STORAGE_PATH, filename)
        file.save(filepath)
        logging.info(f"File '{filename}' uploaded successfully to {STORAGE_PATH}")
        evh.remount_usb()
        return "File uploaded successfully"
    except Exception as e:
        logging.error(f"Error uploading file: {e}")
        return str(e), 500

@app.route("/download/<filename>")
def download_file(filename):
    try:
        logging.info(f"Download request received for {filename}")
        return send_from_directory(STORAGE_PATH, filename, as_attachment=True)
    except Exception as e:
        logging.error(f"Error downloading file '{filename}': {e}")
        return str(e), 404

@app.route("/delete/<filename>")
def delete_file(filename):
    try:
        os.remove(os.path.join(STORAGE_PATH, filename))
        evh.remount_usb()
        return f"Deleted {filename}"
    except Exception as e:
        logging.error(f"Error deleting file '{filename}': {e}")
        return str(e), 404

@app.route("/test_status")
def test_status():
    space_info = get_space_info()
    data = {
        "rpi_id": RPI_ID,
        "os_info": {"hostname": RPI_LOCAL_IP, "platform": platform.system()},  # Trả về IP thực tế
        "storage_path": STORAGE_PATH,
        "message": "RPi Client is running!",
        "files": list_files(),
        "space_info": space_info
    }
    return json.dumps(data)

# -------------------- mDNS Service Registration --------------------
def register_mdns_service():
    """Registers the service with Zeroconf."""
    try:
        zeroconf = Zeroconf()
        desc = {'version': '1.0', 'rpi_id': RPI_ID}
        ip_bytes = socket.inet_aton(RPI_LOCAL_IP)  # Correctly convert the IP
        service_name = f"rpi_sdcard_{RPI_ID}._http._tcp.local."
        info = ServiceInfo(
            "_http._tcp.local.",
            service_name,
            addresses=[ip_bytes],  # Use the byte representation
            port=RPI_PORT,
            properties=desc
        )
        zeroconf.register_service(info)
        logging.info(f"Service registered: {info}")
        return zeroconf, info  # Return zeroconf and info for cleanup

    except Exception as e:
        logging.error(f"Error registering Zeroconf service: {e}")
        return None, None

def unregister_mdns_service(zeroconf, info):
    """Unregisters the service and closes Zeroconf."""
    if zeroconf and info:
        logging.info("Unregistering Zeroconf service...")
        zeroconf.unregister_service(info)
        zeroconf.close()
        logging.info("Zeroconf service unregistered and closed.")
    else:
        logging.warning("Zeroconf service was not properly initialized, skipping unregister.")

# -------------------- Start Flask and Watchdog --------------------
if __name__ == "__main__":
    # Initialize and start the file system observer
    os.system(CMD_MOUNT)
    evh = DirtyHandler()
    observer = Observer()
    observer.schedule(evh, path=STORAGE_PATH, recursive=True)
    observer.start()

    logging.info(f"Starting RPi Client with ID: {RPI_ID} on {RPI_IP}:{RPI_PORT}")

    zeroconf, info = register_mdns_service()  # Register service and get instances

    try:
        app.run(debug=False, host=RPI_IP, port=RPI_PORT)

    except KeyboardInterrupt:
        logging.info("Keyboard interrupt received, stopping...")
        observer.stop()

    finally:
        logging.info("Cleaning up...")
        unregister_mdns_service(zeroconf, info)  # Unregister and close
        observer.join()