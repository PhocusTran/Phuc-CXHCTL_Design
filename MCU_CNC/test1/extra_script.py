import serial
import webbrowser
import re

ser = serial.Serial('COM8', 115200)  # Thay COM3 bằng cổng của bạn

while True:
    line = ser.readline().decode('utf-8').strip()
    match = re.search(r"Địa chỉ IP:\s*(\d+\.\d+\.\d+\.\d+)", line)
    if match:
        ip_address = match.group(1)
        webbrowser.open(f"http://{ip_address}")
        break

ser.close()