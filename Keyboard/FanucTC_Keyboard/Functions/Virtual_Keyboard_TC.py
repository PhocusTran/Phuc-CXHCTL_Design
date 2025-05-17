import tkinter as tk
from tkinter import filedialog, scrolledtext
import socket
import requests
import time
import re
import speech_recognition as sr
import threading
from unidecode import unidecode  # Import thư viện unidecode
import os
import pyaudio
import wave
import tensorflow as tf
import numpy as np
import pyttsx3  # Import thư viện pyttsx3

def get_esp_ip():
    """Tìm địa chỉ IP của ESP32 trên mạng LAN."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("", 8888))  # Cổng UDP phải khớp với cổng trong code Arduino
    s.settimeout(5)  # Đặt timeout là 5 giây

    try:
        data, addr = s.recvfrom(1024)
        esp_ip = data.decode()
        return esp_ip
    except socket.timeout:
        print("Không nhận được phản hồi từ ESP32 sau 5 giây.")
        return None


def send_command(button_name):
    """Gửi lệnh đến ESP32 theo tên nút."""
    print(f"Sending command: {button_name}")
    try:
        url = f"http://{esp_ip}/button?name={button_name}"
        response = requests.get(url, timeout=2)  # timeout để tránh treo chương trình
        response.raise_for_status()  # Kiểm tra lỗi HTTP
        print(f"Response from ESP32: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"Error sending command to ESP32: {e}")


def remove_parentheses_content(line):
    """
    Loại bỏ tất cả nội dung bên trong dấu ngoặc đơn (...) trong một dòng.

    Args:
        line (str): Dòng văn bản cần xử lý.

    Returns:
        str: Dòng văn bản sau khi đã loại bỏ nội dung trong dấu ngoặc đơn.
    """
    return re.sub(r'\([^)]*\)', '', line)


# --- Giao diện Tkinter ---
window = tk.Tk()
window.title("CNC Keyboard Control")

# Lấy địa chỉ IP của ESP32
esp_ip = get_esp_ip()
if esp_ip is None:
    print("Vui lòng bật điểm phát sóng cá nhân lên và đợi trong giây lát.")
    exit()  # Thoát chương trình nếu không tìm thấy ESP32
else:
    print(f"ESP32 IP Address: http://{esp_ip}/")

# --- Get Temperature from ESP32 ---
# temperature_label = tk.Label(window, text="Temperature: N/A")
# temperature_label.grid(row=0, column=0, columnspan=7, pady=10)


# def get_temperature():
#     """Lấy nhiệt độ từ ESP32 và cập nhật label."""
#     try:
#         url = f"http://{esp_ip}/temperature"
#         response = requests.get(url, timeout=2)
#         response.raise_for_status()  # Kiểm tra lỗi HTTP
#         temperature = response.text
#         temperature_label.config(text=f"Temperature: {temperature} °C")
#     except requests.exceptions.RequestException as e:
#         temperature_label.config(text=f"Error getting temperature: {e}")
#     window.after(100, get_temperature)


# if esp_ip:  # Chỉ bắt đầu nếu có IP
#     get_temperature()  # Gọi lần đầu tiên để bắt đầu cập nhật

# Danh sách tên nút và nhãn hiển thị trên giao diện
button_names = ["resetkey", "helpkey", "deletekey", "inputkey", "cankey", "insertkey", "shiftkey", "alterkey",
                "offsetkey", "graphkey",
                "progkey", "messagekey", "poskey", "systemkey", "pageupkey", "pagedownkey", "upkey", "downkey",
                "leftkey", "rightkey",
                "mkey", "tkey", "key0", "key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8", "key9",
                "eobkey", "wvkey", "uhkey", "tjkey", "skkey", "mikey", "flkey", "zykey", "xckey", "grkey", "nqkey",
                "opkey",
                "f1key", "f2key", "f3key", "f4key", "f5key", "f6key", "f7key"]

# Text hiển thị cho các nút (khớp với thứ tự trong button_names)
button_labels = ["RESET", "HELP", "DELETE", "INPUT", "CAN", "INSERT", "SHIFT", "ALTER", "OFFSET", "CSTM/GR",
                 "PROG", "MESSAGE", "POS", "SYSTEM", "PAGE UP", "PAGE DOWN", "UP", "DOWN", "LEFT", "RIGHT",
                 ". /", "- +", "0 *", "1 ,", "2 #", "3 =", "4 [", "5 ]", "6 SP", "7 A", "8 B", "9 D",
                 "EOB E", "W V", "U H", "T J", "S K", "M I", "F L", "Z Y", "X C", "G R", "N Q", "O P",
                 "F1", "F2", "F3", "F4", "F5", "F6", "F7"]

buttons = {}
row, col = 1, 0  # Start from row 1 to accommodate temperature label
for i, name in enumerate(button_names):
    label = button_labels[i]
    command = name.replace(' ', '').replace('/', '').replace('.', '').lower()  # chuẩn hóa tên command
    button = tk.Button(window, text=label, width=10, command=lambda cmd=command: send_command(cmd))
    button.grid(row=row, column=col, padx=5, pady=5)
    buttons[name] = button
    col += 1
    if col > 6:
        col = 0
        row += 1


def read_gcode_file(file_path):
    """
    Đọc file G-code, loại bỏ các dòng comment,
        và nội dung bên trong dấu ngoặc đơn.

    Args:
        file_path (str): Đường dẫn đến file G-code.

    Returns:
        list: Một danh sách các dòng lệnh G-code đã được xử lý.
              Trả về None nếu file không thể mở.
    """
    try:
        with open(file_path, 'r') as file:
            gcode_lines = file.readlines()

        # Loại bỏ ký tự xuống dòng và khoảng trắng thừa
        gcode_lines = [line.strip() for line in gcode_lines]

        # Loại bỏ các dòng comment (bắt đầu bằng ; hoặc %) và dòng trống
        gcode_lines = [line for line in gcode_lines if not line.startswith(';') and not line.startswith('%') and line]

        # Loại bỏ nội dung bên trong dấu ngoặc đơn
        gcode_lines = [remove_parentheses_content(line) for line in gcode_lines]

        return gcode_lines
    except Exception as e:
        print(f"Lỗi khi mở hoặc đọc file G-code: {e}")
        return None


def send_gcode_commands(gcode_lines):
    """Gửi các lệnh G-code từ danh sách các dòng đến ESP32."""
    # send_command("progkey")  # Nhấn nút program
    # time.sleep(0.5)
    if gcode_lines:
        for line in gcode_lines:
            for char in line:
                if char.isdigit():
                    send_command("key" + char)
                elif char == '.':
                    send_command("mkey")
                elif char == '-':
                    send_command("tkey")
                # elif char == '  ':               Đối với máy TC thì bỏ insert với từng khoảng trắng đi vì máy
                #     send_command("insertkey")    có thể hiểu các kí tự viết liền và viết hết dòng mới cần insert

                elif char == 'A' or char.lower() == 'a':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("key7")
                elif char == 'B' or char.lower() == 'b':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("key8")
                elif char == 'C' or char.lower() == 'c':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("xckey")
                elif char == 'D' or char.lower() == 'd':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("key9")
                elif char == 'E' or char.lower() == 'e':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("eobkey")
                elif char == 'F' or char.lower() == 'f':
                    send_command("flkey")
                elif char == 'G' or char.lower() == 'g':
                    send_command("grkey")
                elif char == 'H' or char.lower() == 'h':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("uhkey")
                elif char == 'I' or char.lower() == 'i':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("mikey")
                elif char == 'J' or char.lower() == 'j':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("tjkey")
                elif char == 'K' or char.lower() == 'k':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("skkey")
                elif char == 'L' or char.lower() == 'l':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("flkey")
                elif char == 'M' or char.lower() == 'm':
                    send_command("mikey")
                elif char == 'N' or char.lower() == 'n':
                    send_command("nqkey")
                elif char == 'O' or char.lower() == 'o':
                    send_command("opkey")
                elif char == 'P' or char.lower() == 'p':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("opkey")
                elif char == 'Q' or char.lower() == 'q':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("nqkey")
                elif char == 'R' or char.lower() == 'r':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("grkey")
                elif char == 'S' or char.lower() == 's':
                    send_command("skkey")
                elif char == 'T' or char.lower() == 't':
                    send_command("tjkey")
                elif char == 'U' or char.lower() == 'u':
                    send_command("uhkey")
                elif char == 'V' or char.lower() == 'v':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("wvkey")
                elif char == 'W' or char.lower() == 'w':
                    send_command("wvkey")
                elif char == 'X' or char.lower() == 'x':
                    send_command("xckey")
                elif char == 'Y' or char.lower() == 'y':
                    send_command("shiftkey")
                    time.sleep(0.1)
                    send_command("zykey")
                elif char == 'Z' or char.lower() == 'z':
                    send_command("zykey")
                time.sleep(0.1)

            send_command("eobkey")  # Kết thúc dòng
            time.sleep(0.1)
            send_command("insertkey")
            time.sleep(0.1)
    else:
        print("Không có code để gửi.")


# Khai báo gcode_text ở global scope để sử dụng ở nhiều hàm
gcode_text = scrolledtext.ScrolledText(window, height=10, width=50)


def import_file():
    """Mở hộp thoại chọn file và gửi lệnh G-code."""
    file_path = filedialog.askopenfilename(filetypes=[("G-code files", "*.nc *.tap *.txt")])
    if file_path:
        gcode_lines = read_gcode_file(file_path)
        # Sau khi gửi code từ file, hiển thị nó trên gcode_text
        gcode_text.delete("1.0", tk.END)  # Xóa nội dung hiện tại
        for line in gcode_lines:
            gcode_text.insert(tk.END, line + "\n")
        send_gcode_commands(gcode_lines)


# Thêm nút Import G-code
import_button = tk.Button(window, text="Import G-code", command=import_file)
import_button.grid(row=row + 1, column=2, pady=5)


# Tạo Text Widget và Scrollbar cho nhập G-code (Được tạo MỘT LẦN)
# gcode_text = scrolledtext.ScrolledText(window, height=10, width=50) # Di chuyển lên global scope
# gcode_text.grid(row=row + 2, column=0, columnspan=7, pady=10) #Remove dòng này

def send_gcode_from_text():
    """Lấy G-code từ Text Widget và gửi đến ESP32."""
    gcode_content = gcode_text.get("1.0", tk.END).strip()
    gcode_lines = [line.strip() for line in gcode_content.splitlines() if line.strip()]

    # Loại bỏ các dòng comment (bắt đầu bằng ; hoặc %) và dòng trống
    gcode_lines = [line for line in gcode_lines if not line.startswith(';') and not line.startswith('%') and line]
    # Loại bỏ nội dung bên trong dấu ngoặc đơn
    gcode_lines = [remove_parentheses_content(line) for line in gcode_lines]
    send_gcode_commands(gcode_lines)


def clear_gcode_text():
    """Xóa toàn bộ nội dung trong ô gcode_text."""
    gcode_text.delete("1.0", tk.END)


# Nút Write để gửi code từ Text Widget
write_button = tk.Button(window, text="Write", command=send_gcode_from_text)
write_button.grid(row=row + 1, column=3, pady=5)

def pressit(x, z):
    """Xử lý đầu vào từ người dùng để nhập offset."""
    if not x and not z:
        raise ValueError('both inputs are empty.')
        return

    send_command("offsetkey")  # Vào menu offset
    time.sleep(0.1)

    if x and is_valid_number(x):
        send_command("xckey")  # X
        time.sleep(0.1)
        for c in x:
            if c == '.':
                send_command("mkey")
            elif c == '-':
                send_command("tkey")
            else:
                send_command("key" + c)
                time.sleep(0.1)
        send_command("insertkey")

    if z and is_valid_number(z):
        send_command("zykey")  # Z
        time.sleep(0.1)
        for c in z:
            if c == '.':
                send_command("mkey")
            elif c == '-':
                send_command("tkey")
            else:
                send_command("key" + c)
                time.sleep(0.1)
        send_command("insertkey")


def is_valid_number(s):
    """Kiểm tra xem chuỗi có phải là số (có thể âm và thập phân) hay không."""
    if not s:
        return False
    try:
        float(s)
        return True
    except ValueError:
        return False

# --- Nhận dạng giọng nói ---
recognizing = False  # Biến để theo dõi trạng thái nhận dạng
current_language = "vi-VN"  # Ngôn ngữ mặc định là tiếng Việt

# Thêm biến trạng thái để theo dõi chế độ "program" và "system"
program_mode = False
system_mode = False

# Các lệnh chuyển trang (để reset program_mode và system_mode)
page_change_commands = ["helpkey", "offsetkey", "messagekey", "poskey"]
# Thêm các từ đồng nghĩa
page_change_synonyms = ["nút help", "trợ giúp", "offset", "tin nhắn", "vị trí", "nút pos", "help", "message", "pos"]

# Define English voice commands
english_voice_commands = {
    "reset": "resetkey",
    "help": "helpkey",
    "delete": "deletekey",
    "input": "inputkey",
    "cancel": "cankey",
    "insert": "insertkey",
    "shift": "shiftkey",
    "alter": "alterkey",
    "offset": "offsetkey",
    "upset": "offsetkey",
    "program": "progkey",
    "message": "messagekey",
    "position": "poskey",
    "system": "systemkey",
    "page up": "pageupkey",
    "page down": "pagedownkey",
    "up side": "upkey",
    "up button": "upkey",
    "down side": "downkey",
    "down button": "downkey",
    "left side": "leftkey",
    "left button": "leftkey",
    "right side": "rightkey",
    "right button": "rightkey",
    "dot": "mkey",
    "minus": "tkey",
    "number zero": "key0",
    "number oh": "key0",
    "number one": "key1",
    "number two": "key2",
    "number three": "key3",
    "number four": "key4",
    "number five": "key5",
    "number six": "key6",
    "number seven": "key7",
    "number eight": "key8",
    "number nine": "key9",
    "eob": "eobkey",
    "new line": "newline",
    "enter": "newline",
    "back line": "oldline",
    "old line": "oldline",
    "send code": "send_gcode",
    "write code": "send_gcode",
    "refresh": "clear_text",
    "clear all": "clear_text",
    "backspace": "backspace",
    "undo": "backspace",
    "search parameter": "f2key",
    "search it": "f2key",
    "f1": "f1key",
    "f2": "f2key",
    "f3": "f3key",
    "f4": "f4key",
    "f5": "f5key",
    "f6": "f6key",
    "f7": "f7key"
}

voice_commands = {
    "reset": "resetkey",
    "nut help": "helpkey",
    "nút help": "helpkey",
    "tro giup": "helpkey",
    "trợ giúp": "helpkey",
    "offset": "offsetkey",
    "óp sét": "offsetkey",
    "ốc sên": "offsetkey",
    "ốc sét": "offsetkey",
    "delete": "deletekey",
    "nut nhap": "inputkey",
    "nút nhập": "inputkey",
    "input": "inputkey",
    "cancel": "cankey",
    "nut can": "cankey",
    "nút can": "cankey",
    "nut xoa": "deletekey",
    "nút xóa": "deletekey",
    "delete": "deletekey",
    "nut chen": "insertkey",
    "nút chèn": "insertkey",
    "chen vao": "insertkey",
    "chèn vào": "insertkey",
    "in sot": "insertkey",
    "in sợt": "insertkey",
    "insert": "insertkey",
    "shift": "shiftkey",
    "nut shift": "shiftkey",
    "nút shift": "shiftkey",
    "alter": "alterkey",
    "thay the": "alterkey",
    "thay thế": "alterkey",
    "offset": "offsetkey",
    "program": "progkey",
    "chuong trinh": "progkey",
    "chương trình": "progkey",
    "message": "messagekey",
    "tin nhan": "messagekey",
    "tin nhắn": "messagekey",
    "message": "messagekey",
    "position": "poskey",
    "vi tri": "poskey",
    "vị trí": "poskey",
    "nut pos": "poskey",
    "nút pos": "poskey",
    "pos": "poskey",
    "system": "systemkey",
    "he thong": "systemkey",
    "hệ thống": "systemkey",
    "page up": "pageupkey",
    "len trang": "pageupkey",
    "lên trang": "pageupkey",
    "page down": "pagedownkey",
    "xuong trang": "pagedownkey",
    "xuống trang": "pageupkey",
    "nut len": "upkey",
    "nút lên": "upkey",
    "up": "upkey",
    "nut xuong": "downkey",
    "nút xuống": "downkey",
    "down": "downkey",
    "nut trai": "leftkey",
    "nút trái": "leftkey",
    "left": "leftkey",
    "nut phai": "rightkey",
    "nút phải": "rightkey",
    "right": "rightkey",
    "dau cham": "mkey",
    "dấu chấm": "mkey",
    "dau gach": "tkey",
    "dấu gạch": "tkey",
    "dau tru": "tkey",
    "dấu trừ": "tkey",
    "trừ": "tkey",
    "dau cong": "dau cong",
    "dấu cộng": "dau cong",
    "so 0": "key0",
    "số 0": "key0",
    "zero": "zero",
    "so khong": "key0",
    "số không": "key0",
    "so 1": "key1",
    "số 1": "key1",
    "one": "one",
    "so mot": "key1",
    "số một": "key1",
    "so 2": "key2",
    "số 2": "key2",
    "two": "two",
    "so hai": "key2",
    "số hai": "key2",
    "so 3": "key3",
    "số 3": "key3",
    "three": "three",
    "so ba": "key3",
    "số ba": "key3",
    "so 4": "key4",
    "số 4": "key4",
    "four": "four",
    "so bon": "key4",
    "số bốn": "key4",
    "so 5": "key5",
    "số 5": "key5",
    "five": "five",
    "so nam": "key5",
    "số năm": "key5",
    "so 6": "key6",
    "số 6": "key6",
    "six": "six",
    "so sau": "key6",
    "số sáu": "key6",
    "so 7": "key7",
    "số 7": "key7",
    "seven": "seven",
    "so bay": "key7",
    "số bảy": "key7",
    "so 8": "key8",
    "số 8": "key8",
    "eight": "eight",
    "so tam": "key8",
    "số tám": "key8",
    "so 9": "key9",
    "số 9": "key9",
    "nine": "nine",
    "so chin": "key9",
    "số chín": "key9",
    "nut eob": "eobkey",
    "nút eob": "eobkey",
    "cham phay": "eobkey",
    "chấm phẩy": "eobkey",
    "e o b": "eobkey",
    "chu a": "akey",
    "chữ a": "akey",
    "chu b": "bkey",
    "chữ b": "bkey",
    "chu c": "ckey",
    "chữ c": "ckey",
    "chu d": "dkey",
    "chữ d": "dkey",
    "chu e": "ekey",
    "chữ e": "ekey",
    "chu h": "hkey",
    "chữ h": "hkey",
    "chu i": "ikey",
    "chữ i": "ikey",
    "chu i ngan": "ikey",
    "chữ i ngắn": "ikey",
    "chu j": "jkey",
    "chữ j": "jkey",
    "chu k": "kkey",
    "chữ k": "kkey",
    "chu l": "lkey",
    "chữ l": "lkey",
    "chu p": "pkey",
    "chữ p": "pkey",
    "chu q": "qkey",
    "chữ q": "qkey",
    "chu r": "rkey",
    "chữ r": "rkey",
    "chu v": "vkey",
    "chữ v": "vkey",
    "chu y dai": "ykey",
    "chữ y dài": "ykey",
    "chu f": "flkey",  #
    "chữ f": "flkey",  #
    "chu g": "gkey",  #
    "chữ g": "gkey",  #
    "chu m": "mikey",  #
    "chữ m": "mikey",  #
    "chu n": "nqkey",  #
    "chữ n": "nqkey",  #
    "chu o": "opkey",  #
    "chữ o": "opkey",  #
    "chu s": "skkey",  #
    "chữ s": "skkey",  #
    "chu t": "tjkey",  #
    "chữ t": "tjkey",  #
    "chu u": "uhkey",  #
    "chữ u": "uhkey",  #
    "chu w": "wvkey",  #
    "chữ w": "wvkey",  #
    "chu x": "xckey",  #
    "chữ x": "xckey",  #
    "chu z": "zykey",  #
    "chữ z": "zykey",  #
    "xuong dong": "newline",  # Không thực sự là lệnh, dùng để xuống dòng
    "xuống dòng": "newline",  # Không thực sự là lệnh, dùng để xuống dòng
    "xuong hang": "newline",  # Không thực sự là lệnh, dùng để xuống dòng
    "xuống hàng": "newline",  # Không thực sự là lệnh, dùng để xuống dòng
    "enter": "newline",  # Không thực sự là lệnh, dùng để xuống dòng
    "len dong": "oldline",  # Không thực sự là lệnh, dùng để xuống dòng
    "lên dòng": "oldline",  # Không thực sự là lệnh, dùng để xuống dòng
    "len hang": "oldline",  # Không thực sự là lệnh, dùng để xuống dòng
    "lên hàng": "oldline",  # Không thực sự là lệnh, dùng để xuống dòng
    "nap code": "send_gcode",  # Thêm lệnh cho "nạp code"
    "nạp code": "send_gcode",  # Thêm lệnh cho "nạp code"
    "viet code": "send_gcode",  # Thêm lệnh cho "viết code"
    "viết code": "send_gcode",  # Thêm lệnh cho "viết code"
    "nap chuong trinh": "send_gcode",  # Thêm lệnh cho "nạp code"
    "nạp chương trình": "send_gcode",  # Thêm lệnh cho "nạp code"
    "viet chuong trinh": "send_gcode",  # Thêm lệnh cho "viết code"
    "viết chương trình": "send_gcode",  # Thêm lệnh cho "viết code"
    "lam moi": "clear_text",  # Thêm lệnh cho "làm mới"
    "làm mới": "clear_text",  # Thêm lệnh cho "làm mới"
    "refresh": "clear_text",  # Thêm lệnh cho "làm mới"
    "xoa het": "clear_text",  # Thêm lệnh "xóa hết"
    "xóa hết": "clear_text",  # Thêm lệnh "xóa hết"
    "lui lai": "backspace",
    "lui lại": "backspace",
    "backspace": "backspace",
    "tim kiem": "f2key",
    "tìm kiếm": "f2key",
    "f1": "f1key",
    "f2": "f2key",
    "f3": "f3key",
    "f4": "f4key",
    "f5": "f5key",
    "f6": "f6key",
    "f7": "f7key"
}


def execute_voice_command(command):
    """Thực thi lệnh dựa trên giọng nói."""
    global program_mode, system_mode, current_language, recognizing

    command = command.lower()

    # Use the appropriate voice command dictionary based on the selected language
    if current_language == "vi-VN":
        commands = voice_commands
    elif current_language == "en-US":
        commands = english_voice_commands
    else:
        commands = voice_commands  # Default to Vietnamese if language is unknown

    if command is None:
        return

    print(f"Lệnh nhận dạng được: {command}")
    print(f"system_mode trước: {system_mode}")
    print(f"program_mode trước: {program_mode}")

    # Check for change language command FIRST
    if command in ["doi ngon ngu", "đổi ngôn ngữ", "chuyển ngôn ngữ", "chuyen ngon ngu", "change language", "change curent language"]:
        print("Đổi ngôn ngữ theo yêu cầu.")
        stop_recognize_and_execute()  # Dừng vòng lặp hiện tại
        start_recognize_and_execute()  # Khởi động lại để chọn ngôn ngữ mới
        return  # Kết thúc hàm sau khi xử lý lệnh đổi ngôn ngữ

    # 1. Tách chuỗi thành trục và phần còn lại
    match = re.match(r"^(mx|my|mz)\s*(.*)$", command, re.IGNORECASE)
    if match:
        axis = match.group(1).upper()  # MX, MY, hoặc MZ (viết hoa)
        rest = match.group(2).strip()  # Phần còn lại của lệnh

        print(f"Detected axis command: {axis}, rest: {rest}")

        # Gửi lệnh mikey (luôn luôn)
        send_command("mikey")
        time.sleep(0.1)

        # Gửi lệnh cho trục
        if axis == "MX":
            send_command("xckey")
        elif axis == "MY":
            send_command("shiftkey")
            time.sleep(0.1)  # Đợi trước khi gửi lệnh tiếp theo.
            send_command("zykey")
        elif axis == "MZ":
            send_command("zykey")
        time.sleep(0.1)

        # 2. Xử lý dấu trừ và từ "trừ"
        minus_prefix = False
        if rest.startswith("-"):
            print("Detected minus sign prefix")
            send_command("tkey")
            time.sleep(0.1)
            rest = rest[1:].strip()  # Loại bỏ dấu trừ và khoảng trắng
            minus_prefix = True
        elif "trừ" in rest:
            print("Detected 'trừ' prefix")
            send_command("tkey")
            time.sleep(0.1)
            rest = rest.replace("trừ", "").strip()  # Loại bỏ "trừ" và khoảng trắng
            minus_prefix = True
        elif "từ" in rest:  # trừ hay bị nghe nhầm thành từ
            print("Detected 'trừ' prefix")
            send_command("tkey")
            time.sleep(0.1)
            rest = rest.replace("từ", "").strip()  # Loại bỏ "từ" và khoảng trắng
            minus_prefix = True

        # 3. Xử lý phần số
        if rest:  # Đảm bảo có phần số
            for char in rest:
                if char.isdigit():
                    print(f"Sending digit: {char}")
                    send_command(f"key{char}")
                    time.sleep(0.1)
                elif char == '.':
                    print("Sending decimal point")
                    send_command("mkey")
                    time.sleep(0.1)

        return  # Kết thúc xử lý nếu là lệnh MX/MY/MZ

    # 2. Kiểm tra lệnh chuyển chế độ (System/Program)
    if command == "system" or command == "hệ thống" or command == "he thong" or command == "system":
        system_mode = True
        program_mode = False
        send_command("systemkey")
        print(f"Chuyển sang system mode")
        print(f"system_mode: {system_mode}")
        print(f"program_mode: {program_mode}")
        return

    elif command == "program" or command == "chương trình" or command == "chuong trinh" or command == "program":
        program_mode = True
        system_mode = False
        send_command("progkey")
        print(f"Chuyển sang program mode")
        print(f"system_mode: {system_mode}")
        print(f"program_mode: {program_mode}")
        return

    # 3. Kiểm tra lệnh chuyển trang (và reset chế độ)
    if command in page_change_synonyms:
        program_mode = False
        system_mode = False
        print(f"Đã reset mode do lệnh chuyển trang")
        print(f"system_mode sau reset: {system_mode}")
        print(f"program_mode sau reset: {program_mode}")

        # Find the corresponding command in voice_commands and send it
        for key, value in voice_commands.items():
            if key == command:
                send_command(value)
                return

    # 4. Xử lý các lệnh khác dựa trên chế độ hiện tại
    if system_mode:
        print("Đang ở trang parameter")
        normalized_command = unidecode(command)
        print(f"Normalized command: '{normalized_command}'")

        if normalized_command.isdigit():
            print("Detected digit command in system mode")
            for digit in normalized_command:
                print(f"Sending digit: {digit}")
                send_command(f"key{digit}")
                time.sleep(0.1)
            return
        elif normalized_command in commands:
            send_command(commands[normalized_command])
            return
    elif program_mode:
        print("Đang ở trong trang program")
        if command in commands:
            esp_command = commands[command]
            shift_keys = {
                "akey": "key7",
                "bkey": "key8",
                "ckey": "xckey",
                "dkey": "key9",
                "ekey": "eobkey",
                "hkey": "uhkey",
                "ikey": "mikey",
                "jkey": "tjkey",
                "kkey": "skkey",
                "lkey": "flkey",
                "pkey": "opkey",
                "qkey": "qkey",
                "rkey": "grkey",
                "vkey": "wvkey",
                "ykey": "zykey",
            }

            if esp_command in shift_keys:
                send_command("shiftkey")
                time.sleep(0.1)
                send_command(shift_keys[esp_command])
            elif esp_command == "newline":
                gcode_text.insert(tk.END, "\n")
            elif esp_command == "oldline":
                gcode_text.mark_set(tk.INSERT, "insert-1line")
            elif esp_command == "send_gcode":
                send_gcode_from_text()
            elif esp_command == "clear_text":
                clear_gcode_text()
            elif esp_command == "backspace":
                current_content = gcode_text.get("1.0", tk.END)
                if current_content:
                    gcode_text.delete("end-2c")
            else:
                send_command(esp_command)
        else:
            # Thêm vào gcode_text nếu không tìm thấy trong voice_commands
            gcode_text.insert(tk.END, command + " ")
        return

    # 5. Nếu không ở trang parameter, không ở trang program
    else:
        print("Không ở trang parameter hoặc program")
        if command in commands:
            esp_command = commands[command]
            shift_keys = {
                "akey": "key7",
                "bkey": "key8",
                "ckey": "xckey",
                "dkey": "key9",
                "ekey":"eobkey",
                "hkey": "uhkey",
                "ikey": "mikey",
                "jkey": "tjkey",
                "kkey": "skkey",
                "lkey": "flkey",
                "pkey": "opkey",
                "qkey": "nqkey",
                "rkey": "grkey",
                "vkey": "wvkey",
                "ykey": "zykey",
            }
            if esp_command in shift_keys:
                send_command("shiftkey")
                time.sleep(0.1)
                send_command(shift_keys[esp_command])
            else:
                send_command(esp_command)
        return

voice_control_enabled = False

# Initialize pyttsx3
engine = pyttsx3.init()

def speak(text, language="vi-VN"):
    """Nói một đoạn văn bản thành tiếng."""
    global engine
    try:
        if language == "vi-VN":
            engine.setProperty('voice', 'vietnamese')  # Cần cài đặt giọng đọc tiếng Việt phù hợp
        else:
            engine.setProperty('voice', 'english')  # Cần cài đặt giọng đọc tiếng Anh phù hợp
        engine.say(text)
        engine.runAndWait()
    except Exception as e:
        print(f"Lỗi khi nói: {e}")

def toggle_voice_control():
    """Bật/Tắt điều khiển bằng giọng nói khi nút được nhấn."""
    global recognizing, voice_control_enabled, current_language

    voice_control_enabled = not voice_control_enabled  # Đảo ngược trạng thái

    if voice_control_enabled:
        current_language = None  # Reset language choice when starting
        start_recognize_and_execute()  # Bắt đầu nhận dạng giọng nói
        voice_button.config(text="Stop voice control")
    else:
        stop_recognize_and_execute()  # Dừng nhận dạng giọng nói
        voice_button.config(text="Start voice control")

def recognize_and_execute():
    """Nhận dạng giọng nói và thực thi liên tục cho đến khi dừng."""
    global recognizing, current_language

    while recognizing:  # Keep looping until voice control is stopped
        # Ask for language if no language is currently selected
        if not current_language:
            speak("Please choose your language.", language="en-US")
            language = get_language_choice()
            if language == "vi-VN" or language == "en-US":
                current_language = language
                print(f"Language set to: {current_language}")
                speak("Language has been changed.", language="en-US")
            else:
                print("Invalid language choice. Please try again.")
                continue  # Go back to the beginning of the while loop to ask again

        if current_language:  #Sau khi đã có current_language
            command = recognize_speech()
            if command:
                execute_voice_command(command)
            time.sleep(0.1)  # Nghỉ một chút để giảm tải CPU
        else:
            print("No language chosen, please choose your language.")
            time.sleep(1) #Pause 1s

def execute_voice_command(command):
    """Thực thi lệnh dựa trên giọng nói."""
    global program_mode, system_mode, current_language, recognizing

    command = command.lower()

    # Use the appropriate voice command dictionary based on the selected language
    if current_language == "vi-VN":
        commands = voice_commands
    elif current_language == "en-US":
        commands = english_voice_commands
    else:
        commands = voice_commands  # Default to Vietnamese if language is unknown

    if command is None:
        return

    print(f"Lệnh nhận dạng được: {command}")
    print(f"system_mode trước: {system_mode}")
    print(f"program_mode trước: {program_mode}")

    # Check for change language command FIRST
    if command in ["doi ngon ngu", "đổi ngôn ngữ", "chuyển ngôn ngữ", "chuyen ngon ngu", "change language", "change curent language", "change current language"]:
        print("Đổi ngôn ngữ theo yêu cầu.")
        current_language = None  # Đặt lại ngôn ngữ để chọn lại
        return  # Kết thúc hàm sau khi xử lý lệnh đổi ngôn ngữ

    # 1. Tách chuỗi thành trục và phần còn lại
    match = re.match(r"^(mx|my|mz)\s*(.*)$", command, re.IGNORECASE)
    if match:
        axis = match.group(1).upper()  # MX, MY, hoặc MZ (viết hoa)
        rest = match.group(2).strip()  # Phần còn lại của lệnh

        print(f"Detected axis command: {axis}, rest: {rest}")

        # Gửi lệnh mikey (luôn luôn)
        send_command("mikey")
        time.sleep(0.1)

        # Gửi lệnh cho trục
        if axis == "MX":
            send_command("xckey")
        elif axis == "MY":
            send_command("shiftkey")
            time.sleep(0.1)  # Đợi trước khi gửi lệnh tiếp theo.
            send_command("zykey")
        elif axis == "MZ":
            send_command("zykey")
        time.sleep(0.1)

        # 2. Xử lý dấu trừ và từ "trừ"
        minus_prefix = False
        if rest.startswith("-"):
            print("Detected minus sign prefix")
            send_command("tkey")
            time.sleep(0.1)
            rest = rest[1:].strip()  # Loại bỏ dấu trừ và khoảng trắng
            minus_prefix = True
        elif "trừ" in rest:
            print("Detected 'trừ' prefix")
            send_command("tkey")
            time.sleep(0.1)
            rest = rest.replace("trừ", "").strip()  # Loại bỏ "trừ" và khoảng trắng
            minus_prefix = True
        elif "từ" in rest:  # trừ hay bị nghe nhầm thành từ
            print("Detected 'trừ' prefix")
            send_command("tkey")
            time.sleep(0.1)
            rest = rest.replace("từ", "").strip()  # Loại bỏ "từ" và khoảng trắng
            minus_prefix = True

        # 3. Xử lý phần số
        if rest:  # Đảm bảo có phần số
            for char in rest:
                if char.isdigit():
                    print(f"Sending digit: {char}")
                    send_command(f"key{char}")
                    time.sleep(0.1)
                elif char == '.':
                    print("Sending decimal point")
                    send_command("mkey")
                    time.sleep(0.1)

        return  # Kết thúc xử lý nếu là lệnh MX/MY/MZ

    # 2. Kiểm tra lệnh chuyển chế độ (System/Program)
    if command == "system" or command == "hệ thống" or command == "he thong" or command == "system":
        system_mode = True
        program_mode = False
        send_command("systemkey")
        print(f"Chuyển sang system mode")
        print(f"system_mode: {system_mode}")
        print(f"program_mode: {program_mode}")
        return

    elif command == "program" or command == "chương trình" or command == "chuong trinh" or command == "program":
        program_mode = True
        system_mode = False
        send_command("progkey")
        print(f"Chuyển sang program mode")
        print(f"system_mode: {system_mode}")
        print(f"program_mode: {program_mode}")
        return

    # 3. Kiểm tra lệnh chuyển trang (và reset chế độ)
    if command in page_change_synonyms:
        program_mode = False
        system_mode = False
        print(f"Đã reset mode do lệnh chuyển trang")
        print(f"system_mode sau reset: {system_mode}")
        print(f"program_mode sau reset: {program_mode}")

        # Find the corresponding command in voice_commands and send it
        for key, value in voice_commands.items():
            if key == command:
                send_command(value)
                return

    # 4. Xử lý các lệnh khác dựa trên chế độ hiện tại
    if system_mode:
        print("Đang ở trang parameter")
        normalized_command = unidecode(command)
        print(f"Normalized command: '{normalized_command}'")

        if normalized_command.isdigit():
            print("Detected digit command in system mode")
            for digit in normalized_command:
                print(f"Sending digit: {digit}")
                send_command(f"key{digit}")
                time.sleep(0.1)
            return
        elif normalized_command in commands:
            send_command(commands[normalized_command])
            return
    elif program_mode:
        print("Đang ở trong trang program")
        if command in commands:
            esp_command = commands[command]
            shift_keys = {
                "akey": "key7",
                "bkey": "key8",
                "ckey": "xckey",
                "dkey": "key9",
                "ekey": "eobkey",
                "hkey": "uhkey",
                "ikey": "mikey",
                "jkey": "tjkey",
                "kkey": "skkey",
                "lkey": "flkey",
                "pkey": "opkey",
                "qkey": "qkey",
                "rkey": "grkey",
                "vkey": "wvkey",
                "ykey": "zykey",
            }

            if esp_command in shift_keys:
                send_command("shiftkey")
                time.sleep(0.1)
                send_command(shift_keys[esp_command])
            elif esp_command == "newline":
                gcode_text.insert(tk.END, "\n")
            elif esp_command == "oldline":
                gcode_text.mark_set(tk.INSERT, "insert-1line")
            elif esp_command == "send_gcode":
                send_gcode_from_text()
            elif esp_command == "clear_text":
                clear_gcode_text()
            elif esp_command == "backspace":
                current_content = gcode_text.get("1.0", tk.END)
                if current_content:
                    gcode_text.delete("end-2c")
            else:
                send_command(esp_command)
        else:
            # Thêm vào gcode_text nếu không tìm thấy trong voice_commands
            gcode_text.insert(tk.END, command + " ")
            return

    # 5. Nếu không ở trang parameter, không ở trang program
    else:
        print("Không ở trang parameter hoặc program")
        if command in commands:
            esp_command = commands[command]
            shift_keys = {
                "akey": "key7",
                "bkey": "key8",
                "ckey": "xckey",
                "dkey": "key9",
                "ekey":"eobkey",
                "hkey": "uhkey",
                "ikey": "mikey",
                "jkey": "tjkey",
                "kkey": "skkey",
                "lkey": "flkey",
                "pkey": "opkey",
                "qkey": "qkey",
                "rkey": "grkey",
                "vkey": "wvkey",
                "ykey": "zykey",
            }
            if esp_command in shift_keys:
                send_command("shiftkey")
                time.sleep(0.1)
                send_command(shift_keys[esp_command])
            else:
                send_command(esp_command)
        return
def recognize_speech():
    """Nhận dạng giọng nói và trả về lệnh."""
    global recognizing, current_language
    r = sr.Recognizer()
    with sr.Microphone() as source:
        print("Nói lệnh...")
        try:
            audio = r.listen(source, timeout=5)  # listen for 5 seconds, then stop
        except sr.WaitTimeoutError:
            print("Không có âm thanh trong 5 giây.")
            return None

    try:
        # Sử dụng Google Web Speech API để nhận dạng giọng nói
        text = r.recognize_google(audio, language=current_language)  # Nhận dạng theo ngôn ngữ đã chọn
        print(f"Bạn đã nói: {text}")
        return text.lower()
    except sr.UnknownValueError:
        print("Không thể nhận dạng giọng nói.")
        return None
    except sr.RequestError as e:
        print(f"Không thể yêu cầu kết quả từ Google Web Speech API; {e}")
        return None
    finally:
        pass

voice_control_enabled = False
# Initialize pyttsx3
engine = pyttsx3.init()

def speak(text, language="vi-VN"):
    """Nói một đoạn văn bản thành tiếng."""
    global engine
    try:
        if language == "vi-VN":
            engine.setProperty('voice', 'vietnamese')  # Cần cài đặt giọng đọc tiếng Việt phù hợp
        else:
            engine.setProperty('voice', 'english')  # Cần cài đặt giọng đọc tiếng Anh phù hợp
        engine.say(text)
        engine.runAndWait()
    except Exception as e:
        print(f"Lỗi khi nói: {e}")

def toggle_voice_control():
    """Bật/Tắt điều khiển bằng giọng nói khi nút được nhấn."""
    global recognizing, voice_control_enabled, current_language

    voice_control_enabled = not voice_control_enabled  # Đảo ngược trạng thái

    if voice_control_enabled:
        current_language = None  # Reset language choice when starting
        start_recognize_and_execute()  # Bắt đầu nhận dạng giọng nói
        voice_button.config(text="Stop voice control")
    else:
        stop_recognize_and_execute()  # Dừng nhận dạng giọng nói
        voice_button.config(text="Start voice control")

def start_recognize_and_execute():
    """Bắt đầu nhận dạng và thực thi trong một thread riêng."""
    global recognizing, voice_control_enabled
    if not recognizing and voice_control_enabled:
        recognizing = True
        threading.Thread(target=recognize_and_execute).start()

def stop_recognize_and_execute():
    """Dừng nhận dạng giọng nói."""
    global recognizing
    recognizing = False

def recognize_and_execute():
    """Nhận dạng giọng nói và thực thi liên tục cho đến khi dừng."""
    global recognizing, current_language

    while recognizing:
        # Ask for language only if no language is currently selected
        if not current_language:
            speak("Please choose your language.", language="en-US")
            language = get_language_choice()
            if language == "vi-VN" or language == "en-US":
                current_language = language
                print(f"Language set to: {current_language}")
                speak("Language has been changed.", language="en-US")
            else:
                print("Invalid language choice. Please try again.")
                continue

        command = recognize_speech()
        if command:
            execute_voice_command(command)
        time.sleep(0.1)

def get_language_choice():
    """Nhận dạng ngôn ngữ mà người dùng chọn."""
    r = sr.Recognizer()
    with sr.Microphone() as source:
        print("Say your language choice (Tiếng Việt / English)...")
        try:
            audio = r.listen(source, timeout=5)  # listen for 5 seconds, then stop
        except sr.WaitTimeoutError:
            print("Không có âm thanh trong 5 giây.")
            return None

    try:
        # Sử dụng Google Web Speech API để nhận dạng giọng nói
        text = r.recognize_google(audio, language="vi-VN").lower()
        print(f"Bạn đã nói: {text}")

        if "tiếng việt" in text or "tieng viet" in text:
            return "vi-VN"
        elif "english" in text:
            return "en-US"
        else:
            print("Không nhận dạng được ngôn ngữ.")
            return None

    except sr.UnknownValueError:
        print("Không thể nhận dạng giọng nói.")
        return None
    except sr.RequestError as e:
        print(f"Không thể yêu cầu kết quả từ Google Web Speech API; {e}")
        return None

# Tạo Text Widget và Scrollbar cho nhập G-code (Được tạo MỘT LẦN)
gcode_text = scrolledtext.ScrolledText(window, height=24, width=60)
gcode_text.grid(row=10, column=0, columnspan=7, pady=10)

# Nút điều khiển bằng giọng nói
voice_button = tk.Button(window, text="Start voice control", command=toggle_voice_control)
voice_button.grid(row=row + 1, column=4, pady=5)

window.mainloop()