import tkinter as tk
import socket
import requests

def get_esp_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("", 8888))  # Cổng UDP phải khớp với cổng trong code Arduino
    return "192.168.4.2"

    data, addr = s.recvfrom(1024)
    esp_ip = data.decode()
    return esp_ip


def send_command(button_name):
    print(button_name)
    try:
        url = f"http://{esp_ip}/button?name={button_name}"
        # timeout tránh treo chương trình
        response = requests.get(url, timeout=2)
        response.raise_for_status()
        print(f"Response from ESP32: {response.text}")

    except requests.exceptions.RequestException as e:
        print(f"Error: {e}")


# --- Giao diện Tkinter ---
window = tk.Tk()
window.title("CNC Keyboard Control")

# Lấy địa chỉ IP
esp_ip = get_esp_ip()
if esp_ip is None:
    print("Không tìm thấy ESP32. Vui lòng kiểm tra lại kết nối.")
    exit()  # Thoát chương trình nếu không tìm thấy ESP32
else:
    print(f"ESP32 IP Address: http://{esp_ip}/")

# Tạo các nút
button_names = ["resetkey", "cursorupkey", "cursordownkey", "pageupkey", "pagedownkey", "alterkey", "insertkey",
                "deletekey", "eobkey", "cankey",
                "inputkey", "outputkey", "poskey", "progkey", "menukey", "paramkey", "alarmkey", "auxkey", "mkey",
                "tkey", "key0", "key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8", "key9",
                "backey", "kihkey", "jqpkey"]

# Text hiển thị cho các nút (khớp với thứ tự trong button_names)
button_labels = ["RESET", "CURSOR UP", "CURSOR DOWN", "PAGE UP", "PAGE DOWN", "ALTER", "INSERT", "DELETE", "/.# EOB",
                 "CAN",
                 "INPUT", "OUTPUT START", "POS", "PROGRAM", "MENU OFFSET", "DGNOS PARAM", "OPR ALARM", "AUX GRAPH",
                 "- M",
                 "/ T", "0 S", "1 U", "2 Down/W", "3 R", "4 Left/X", "5 Sin/Z", "6 Right/F", "7 O", "8 Up/N", "9 G",
                 "BAC Back", "KIHY Next", "JQPV No."]

buttons = {}
row, col = 0, 0
i = 0
for i, name in enumerate(button_names):
    label = button_labels[i]

    # Đường dẫn tương ứng trên ESP32
    command = f"/{name.replace(' ', '').replace('/', '').replace('.', '').lower()}"
    button = tk.Button(window, text=label, width=10, command=lambda name=name: send_command(
        name.replace(' ', '').replace('.', '').replace('/', '').lower()))  # Sửa command
    button.grid(row=row, column=col, padx=5, pady=5)
    buttons[name] = button
    col += 1
    if col > 6:  # 7 cột
        col = 0
        row += 1


def pressit(x, z):
    if not x and not z:
        raise ValueError('both inputs are empty.')
        return

    send_command("menukey")  # enter menu

    isLegit = x.replace('.', '', 1).isdigit()
    if isLegit:
        send_command("key4")  # x
        for c in x:
            if c == '.':
                send_command("tkey")
                continue
            send_command("key" + c)
        send_command("insertkey")

    isLegit = z.replace('.', '', 1).isdigit()
    if isLegit:
        send_command("key5")  # z
        for c in z:
            if c == '.':
                send_command("tkey")
                continue
            send_command("key" + c)
        send_command("insertkey")


# Ví dụ để kích biến nhấn nút từ xa #
def handle_input():
    x = input_entry.get()  # Lấy giá trị từ ô nhập liệu
    try:
        pressit(x, None);
        # if x == 1: # Thêm điều kiện ở đây
        #    send_command("key1")
        # elif x == 2: # và ở đây
        #    send_command("key2")
    except Exception as error:
        print(repr(error))


# Tạo ô nhập liệu
input_entry = tk.Entry(window)

input_entry.grid()

# Tạo nút nhấn để gửi
input_button = tk.Button(window, text="Nhập", command=handle_input)
input_button.grid()
window.mainloop()
