import socket

def get_esp_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("", 8888))  # Cổng UDP phải khớp với cổng trong code Arduino
    data, addr = s.recvfrom(1024)
    esp_ip = data.decode()
    return esp_ip

esp_ip = get_esp_ip()
if esp_ip:
    print(f"ESP32 IP: http://{esp_ip}/")