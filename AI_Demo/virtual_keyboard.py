import time
from threading import Thread
import socket
import requests


def send_command(esp_ip:str, button_name:str):
    """Gửi lệnh đến ESP32 theo tên nút."""
    print(f"Sending command: {button_name}")
    try:
        url = f"http://{esp_ip}/button?name={button_name}"
        response = requests.get(url, timeout=2)  # timeout để tránh treo chương trình
        response.raise_for_status()  # Kiểm tra lỗi HTTP
        # print(f"Response from ESP32: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"Error sending command to ESP32: {e}")

def get_esp_ip():
    """Tìm địa chỉ IP của ESP32 trên mạng LAN."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("", 8888))  # Cổng UDP phải khớp với cổng trong code Arduino

    data, _ = s.recvfrom(1024)
    esp_ip = data.decode()
    return esp_ip

class virtual_keyboard_FC_O_t:
    """ the old one"""
    def __init__(self):
        self.esp_ip = "192.168.4.1"
        print("using the old cnc machine")
        # Danh sách tên nút và nhãn hiển thị trên giao diện
        # self.button_names = ["resetkey", "helpkey", "deletekey", "inputkey", "cankey", "insertkey", "shiftkey", "alterkey", "offsetkey", "graphkey",
        #                 "progkey", "messagekey", "poskey", "systemkey", "pageupkey", "pagedownkey", "upkey", "downkey", "leftkey", "rightkey",
        #                 "mkey", "tkey", "key0", "key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8", "key9",
        #                 "eobkey", "wvkey", "uhkey", "tjkey", "skkey", "mikey", "flkey", "zykey", "xckey", "grkey", "nqkey", "opkey",
        #                 "f1key", "f2key", "f3key", "f4key", "f5key", "f6key", "f7key"]

        # # Text hiển thị cho các nút (khớp với thứ tự trong button_names)
        # self.button_labels = ["RESET", "HELP", "DELETE", "INPUT", "CAN", "INSERT", "SHIFT", "ALTER", "OFFSET", "CSTM/GR",
        #                 "PROG", "MESSAGE", "POS", "SYSTEM", "PAGE UP", "PAGE DOWN", "UP", "DOWN", "LEFT", "RIGHT",
        #                 ". /", "- +", "0 *", "1 ,", "2 #", "3 =", "4 [", "5 ]", "6 SP", "7 A", "8 B", "9 D",
        #                 "EOB E", "W V", "U H", "T J", "S K", "M I", "F L", "Z Y", "X C", "G R", "N Q", "O P",
        #                 "F1", "F2", "F3", "F4", "F5", "F6", "F7"]
    
    def send_gcode_commands(self, line:str):
        """Gửi các lệnh G-code từ danh sách các dòng đến ESP32."""
        #send_command("progkey")  # Nhấn nút program
        #time.sleep(0.5)
        if line:
            for char in line:
                if char.isdigit():
                    send_command(self.esp_ip, "key" + char)
                elif char == '.':
                    send_command(self.esp_ip, "tkey")
                elif char == '-':
                    send_command(self.esp_ip, "mkey")
                elif char == ' ':
                    send_command(self.esp_ip, "insertkey")

                elif char == 'A':
                    for _ in range(2):  # Nhấn 2 lần - CAB
                        send_command(self.esp_ip, "backey")
                        time.sleep(0.1)
                elif char == 'B':
                    for _ in range(3):  # Nhấn 3 lần - CAB
                        send_command(self.esp_ip, "backey")
                        time.sleep(0.1)
                elif char == 'C': # Nhấn 1 lần
                    send_command(self.esp_ip, "backey")

                elif char == 'K':
                    for _ in range(4):  # Nhấn 2 lần - CAB
                        send_command(self.esp_ip, "kihkey")
                        time.sleep(0.1)
                elif char == 'I':
                    for _ in range(3):  # Nhấn 3 lần - CAB
                        send_command(self.esp_ip, "kihkey")
                        time.sleep(0.1)
                elif char == 'H': # Nhấn 1 lần
                    for _ in range(3):  # Nhấn 3 lần - CAB
                        send_command(self.esp_ip, "kihkey")
                        time.sleep(0.1)
                elif char == 'Y': # Nhấn 1 lần
                    send_command(self.esp_ip, "kihkey")

                elif char == 'V':
                    for _ in range(4):  # Nhấn 2 lần - CAB
                        send_command(self.esp_ip, "jqpkey")
                        time.sleep(0.1)
                elif char == 'J':
                    for _ in range(3):  # Nhấn 3 lần - CAB
                        send_command(self.esp_ip, "jqpkey")
                        time.sleep(0.1)
                elif char == 'Q': # Nhấn 1 lần
                    for _ in range(3):  # Nhấn 3 lần - CAB
                        send_command(self.esp_ip, "jqpkey")
                        time.sleep(0.1)
                elif char == 'P': # Nhấn 1 lần
                    send_command(self.esp_ip, "jqpkey")

                elif char == 'S':
                    send_command(self.esp_ip, "key0")
                elif char == 'U':
                    send_command(self.esp_ip, "key1")
                elif char == 'W':
                    send_command(self.esp_ip, "key2")
                elif char == 'R':
                    send_command(self.esp_ip, "key3")
                elif char == 'X':
                    send_command(self.esp_ip, "key4")
                elif char == 'Z':
                    send_command(self.esp_ip, "key5")
                elif char == 'F':
                    send_command(self.esp_ip, "key6")
                elif char == 'O':
                    send_command(self.esp_ip, "key7")
                elif char == 'N':
                    send_command(self.esp_ip, "key8")
                elif char == 'G':
                    send_command(self.esp_ip, "key9")
                elif char == 'T':
                    send_command(self.esp_ip, "tkey")
                elif char == 'M':
                    send_command(self.esp_ip, "mkey")

                else:
                    send_command(self.esp_ip, char.lower() + "key") # Các lệnh ký tự chữ
                time.sleep(0.1)

            send_command(self.esp_ip, "eobkey")  # Kết thúc dòng
            time.sleep(0.1)
            send_command(self.esp_ip, "insertkey")  # Reset sau khi xong dòng
            time.sleep(0.1)
        else:
            print("Không có code để gửi.")

    def pressit(self, x, z):
        """Nhập giá trị X và Z từ ô nhập liệu."""
        if self.esp_ip is None:
            print("ESP32 IP is None. Please check the connection")
            return
        if not x and not z:
            raise ValueError('Both inputs are empty.')
        Thread(target=self._pressit_thread, args=(x, z)).start()
    
    def _pressit_thread(self, x, z):
        send_command(self.esp_ip, "menukey")  # Vào menu offset
        time.sleep(0.2)

        if x:
            send_command(self.esp_ip, "key4")  # X
            time.sleep(0.2)
            for c in x:
                if c == '.':
                    send_command(self.esp_ip, "tkey")
                    time.sleep(0.2)
                elif c == '-':
                    send_command(self.esp_ip, "mkey")
                    time.sleep(0.2)
                else:
                    send_command(self.esp_ip, "key" + c)
                    time.sleep(0.2)
            send_command(self.esp_ip, "inputkey")

        if z:
            send_command(self.esp_ip, "key5")  # Z
            time.sleep(0.2)
            for c in z:
                if c == '.':
                    send_command(self.esp_ip, "tkey")
                    time.sleep(0.2)
                elif c == '-':
                    send_command(self.esp_ip, "mkey")
                    time.sleep(0.2)
                else:
                    send_command(self.esp_ip, "key" + c)
                    time.sleep(0.2)
            send_command(self.esp_ip, "inputkey")


    def send_offset(self, value):
        """Xử lý đầu vào từ người dùng để nhập offset."""
        if value is not None:
            try:
                self.pressit(value, None)  # Chỉ nhận X
            except Exception as error:
                print(repr(error))
        else:
            print("No value entered")


class virtual_keyboard_FC_Oi:
    """ The new one """
    def __init__(self):
        self.esp_ip = "192.168.4.1"

    def send_offset(self, value):
        """Xử lý đầu vào từ người dùng để nhập offset."""
        if value is not None:
            try:
                self.pressit(value, None)  # Chỉ nhận X
            except Exception as error:
                print(repr(error))
        else:
            print("No value entered")
    
    def pressit(self, x, z):
        if not x and not z:
            print("x n z")
            return

        send_command(self.esp_ip, "offsetkey")  # Vào menu offset
        time.sleep(0.1)

        if x:
            send_command(self.esp_ip, "xckey")  # X
            time.sleep(0.1)
            for c in x:
                if c == '.':
                    send_command(self.esp_ip, "tkey")
                    time.sleep(0.1)
                elif c == '-':
                    send_command(self.esp_ip, "mkey")
                    time.sleep(0.1)
                else:
                    send_command(self.esp_ip, "key" + c)
                    time.sleep(0.1)
            send_command(self.esp_ip, "insertkey")

        if z:
            send_command(self.esp_ip, "zykey")  # Z
            time.sleep(0.1)
            for c in z:
                if c == '.':
                    send_command(self.esp_ip, "tkey")
                    time.sleep(0.1)
                elif c == '-':
                    send_command(self.esp_ip, "mkey")
                    time.sleep(0.1)
                else:
                    send_command(self.esp_ip, "key" + c)
                    time.sleep(0.1)
            send_command(self.esp_ip, "insertkey")

    def send_gcode_commands(self, line):
        if line:
            for char in line:
                if char.isdigit():
                    send_command(self.esp_ip, "key" + char)
                elif char == '.':
                    send_command(self.esp_ip, "tkey")
                elif char == '-':
                    send_command(self.esp_ip, "mkey")
                elif char == ' ':
                    send_command(self.esp_ip, "insertkey")
                elif char == 'A':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "key7")
                elif char == 'B':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "key8")
                elif char == 'C':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "xckey")
                elif char == 'D':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "key9")
                elif char == 'E':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "eobkey")
                elif char == 'F':
                    send_command(self.esp_ip, "flkey")
                elif char == 'G':
                    send_command(self.esp_ip, "grkey")
                elif char == 'H':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "uhkey")
                elif char == 'I':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "mikey")
                elif char == 'J':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "tjkey")
                elif char == 'K':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "skkey")
                elif char == 'L':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "flkey")
                elif char == 'M':
                    send_command(self.esp_ip, "mikey")
                elif char == 'N':
                    send_command(self.esp_ip, "nqkey")
                elif char == 'O':
                    send_command(self.esp_ip, "opkey")
                elif char == 'P':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "opkey")
                elif char == 'Q':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "nqkey")
                elif char == 'R':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "grkey")
                elif char == 'S':
                    send_command(self.esp_ip, "skkey")
                elif char == 'T':
                    send_command(self.esp_ip, "tjkey")
                elif char == 'U':
                    send_command(self.esp_ip, "uhkey")
                elif char == 'V':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "wvkey")
                elif char == 'W':
                    send_command(self.esp_ip, "wvkey")
                elif char == 'X':
                    send_command(self.esp_ip, "xckey")
                elif char == 'Y':
                    send_command(self.esp_ip, "shiftkey")
                    time.sleep(0.1)
                    send_command(self.esp_ip, "zykey")
                elif char == 'Z':
                    send_command(self.esp_ip, "zykey")
                else:
                    send_command(self.esp_ip, char.lower() + "key") # Các lệnh ký tự chữ
                time.sleep(0.1)

            send_command(self.esp_ip, "eobkey")  # Kết thúc dòng
            time.sleep(0.1)
            send_command(self.esp_ip, "insertkey")  # Reset sau khi xong dòng
            time.sleep(0.1)
        else:
            print("Không có code để gửi.")

    
