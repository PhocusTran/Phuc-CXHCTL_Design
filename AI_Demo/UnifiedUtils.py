import os
import string
from skimage.io import imsave
from pdf2image import convert_from_path
import numpy as np
from threading import Thread
import socket
import requests
import time

class Event:
    def __init__(self):
        self._handlers = []

    def __iadd__(self, handler):
        """Add a handler using the += operator."""
        self._handlers.append(handler)
        return self

    def __isub__(self, handler):
        """Remove a handler using the -= operator."""
        self._handlers.remove(handler)
        return self

    def __call__(self, *args, **kwargs):
        """Invoke all handlers when the event is called."""
        for handler in self._handlers:
            handler(*args, **kwargs)

class process:
    def __init__(self):
        self.update = Event()
        self.value = 0
    def add(self):
        self.value += 1
        self.update(self.value)


wanted_key = ['nominal', 'value', 'tolerance']

# --- Phần ESP32 Utility (Được chuyển vào trong main) ---
def get_esp_ip():
    """Tìm địa chỉ IP của ESP32 trên mạng LAN."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("", 8888))  # Cổng UDP phải khớp với cổng trong code Arduino

    data, _ = s.recvfrom(1024)
    esp_ip = data.decode()
    return esp_ip

def send_command(esp_ip:str, button_name:str):
    """Gửi lệnh đến ESP32 theo tên nút."""
    if len(esp_ip) == 0:
        print("Lỗi: esp_ip là None. Không thể gửi lệnh.")
        return  # Dừng nếu esp_ip là None
    print(f"Sending command: {button_name}")
    try:
        url = f"http://{esp_ip}/button?name={button_name}"
        response = requests.get(url, timeout=2)  # timeout để tránh treo chương trình
        response.raise_for_status()  # Kiểm tra lỗi HTTP
        # print(f"Response from ESP32: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"Error sending command to ESP32: {e}")


def pressit(esp_ip, x, z):
    """Nhập giá trị X và Z từ ô nhập liệu."""
    if esp_ip is None:
        print("ESP32 IP is None. Please check the connection")
        return
    if not x and not z:
        raise ValueError('Both inputs are empty.')
    Thread(target=_pressit_thread, args=(esp_ip, x, z)).start()

def is_valid_number(s):
    """Kiểm tra xem chuỗi có phải là số (có thể âm và thập phân) hay không."""
    if not s:
        return False
    try:
        float(s)
        return True
    except ValueError:
        return False


def _pressit_thread(esp_ip, x, z):
    send_command(esp_ip, "menukey")  # Vào menu offset
    time.sleep(0.2)

    if x and is_valid_number(x):
        send_command(esp_ip, "key4")  # X
        time.sleep(0.2)
        for c in x:
            if c == '.':
                send_command(esp_ip, "tkey")
                time.sleep(0.2)
            elif c == '-':
                send_command(esp_ip, "mkey")
                time.sleep(0.2)
            else:
                send_command(esp_ip, "key" + c)
                time.sleep(0.2)
        send_command(esp_ip, "inputkey")

    if z and is_valid_number(z):
        send_command(esp_ip, "key5")  # Z
        time.sleep(0.2)
        for c in z:
            if c == '.':
                send_command(esp_ip, "tkey")
                time.sleep(0.2)
            elif c == '-':
                send_command(esp_ip, "mkey")
                time.sleep(0.2)
            else:
                send_command(esp_ip, "key" + c)
                time.sleep(0.2)
        send_command(esp_ip, "inputkey")


# --- Phần Giao diện Chính ---

img_dest_DIR = 'test_samples'
dest_DIR = 'test_results'
esp_ip = None # initialize esp_ip, it will be updated later

def process_PDF(pdf_path):
    import tensorflow as tf
    # Enable memory growth for GPU if available
    physical_devices = tf.config.list_physical_devices('GPU')
    if len(physical_devices) > 0:
        print("GPU is available, running with GPU")
        tf.config.experimental.set_memory_growth(physical_devices[0], True)
    else:
        print("No GPU detected, running with CPU")

    filename = os.path.splitext(os.path.basename(pdf_path))[0]
    try:
        img = convert_from_path(pdf_path,
                                poppler_path='C:\\Users\\the_pc\\scoop\\apps\\poppler\\current\\bin')
        img = np.array(img[0])
        jpg_path = os.path.join(img_dest_DIR, filename + '.jpg')
        imsave(jpg_path, img)
        return img, filename
    except Exception as e:
        print(f"Error converting PDF: {e}")
        return None, None

ocr_data = []  # To store OCR Data
mask_img = None  # To store the processed mask image
infoblock_dict = None  # To store infoblock data
gdt_dict = None  # To store gdt data
dimension_dict = None  # To store dimension data
cl_frame = None

def ocr_process(img, filename, process:process):
    from OCR_Test.Programs.eDOCr_master.tests.eDOCr import tools_ocr
    global loading_bar, loading_label, image_label, ocr_data, mask_img, infoblock_dict, gdt_dict, dimension_dict, cl_frame

    #update process
    process.add()

    GDT_symbols = '⏤⏥○⌭⌒⌓⏊∠⫽⌯⌖◎↗⌰'
    FCF_symbols = 'ⒺⒻⓁⓂⓅⓈⓉⓊ'
    Extra = '(),.+-±:/°"⌀'

    alphabet_dimensions = string.digits + 'AaBCDRGHhMmnx' + Extra
    model_dimensions = 'OCR_Test\\Programs\\eDOCr_master\\tests\\eDOCr\\keras_ocr_models\\models\\recognizer_dimensions.h5'
    alphabet_infoblock = string.digits + string.ascii_letters + ',.:-/'
    model_infoblock = 'OCR_Test\\Programs\\eDOCr_master\\tests\\eDOCr\\keras_ocr_models\\models\\recognizer_infoblock.h5'
    alphabet_gdts = string.digits + ',.⌀ABCD' + GDT_symbols
    model_gdts = 'OCR_Test\\Programs\\eDOCr_master\\tests\\eDOCr\\keras_ocr_models\\models\\recognizer_gdts.h5'

    color_palette = {'infoblock': (180, 220, 250), 'gdts': (94, 204, 243), 'dimensions': (93, 206, 175),
                     'frame': (167, 234, 82), 'flag': (241, 65, 36)}
    cluster_t = 20

    class_list, img_boxes = tools_ocr.box_tree.findrect(img)

    boxes_infoblock, gdt_boxes, cl_frame, process_img = tools_ocr.img_process.process_rect(class_list, img)
    imsave(os.path.join(dest_DIR, filename + '_process.jpg'), process_img)

    #update process
    process.add()


    infoblock_dict = tools_ocr.pipeline_infoblock.read_infoblocks(boxes_infoblock, img, alphabet_infoblock,
                                                                  model_infoblock)

    #update process
    process.add()

    gdt_dict = tools_ocr.pipeline_gdts.read_gdtbox1(gdt_boxes, alphabet_gdts, model_gdts, alphabet_dimensions,
                                                    model_dimensions)

    #update process
    process.add()

    process_img = os.path.join(dest_DIR, filename + '_process.jpg')
    dimension_dict = tools_ocr.pipeline_dimensions.read_dimensions(process_img, alphabet_dimensions, model_dimensions, cluster_t)

    #update process
    process.add()
    returnList = []

    for i in range(len(dimension_dict)):
        tmp1 = []
        for j in range(len(wanted_key)):
            tmp1.append(dimension_dict[i]['pred'][wanted_key[j]])
        returnList.append(tmp1)

    #update process
    process.add()
    mask_img = tools_ocr.output.mask_the_drawing(img, infoblock_dict, gdt_dict, dimension_dict, cl_frame, color_palette)

    #update process
    process.add()

    imsave(os.path.join(dest_DIR, filename + '_boxes.jpg'), img_boxes)

    #update process
    process.add()
    imsave(os.path.join(dest_DIR, filename + '_mask.jpg'), mask_img)

    #update process
    process.add()
    tools_ocr.output.record_data(dest_DIR, filename, infoblock_dict, gdt_dict, dimension_dict)

    #update process
    process.add()

    return returnList

# Function to get all line values from camera by corresponding ID
def get_live_measurements(selected_rows):
    live_line_data = []

    # Get the values and maintain order from live data from camera
    live_values = {i: line["length_mm"] for i, line in enumerate(live_line_data)}

    # Determine scan direction (we will be sorting ids of selected_rows with these below ids)
    # scan_from_left = True if int(ocr_data[int(selected_rows[0])]["ID"]) < int(
    #   ocr_data[int(selected_rows[-1])]["ID"]) else False

    # Sort the list of IDs based on user select and direction scan (from OCR'd IDs).
    # selected_ids = [ocr_data[int(row_id)]["ID"] for row_id in selected_rows]
    # if scan_from_left:
        # sorted_ids = sorted(selected_ids, key=lambda id: int(id))
    # else:
        # sorted_ids = sorted(selected_ids, key=lambda id: int(id), reverse=True)

    live_index = 0
    for _, row_id in enumerate(selected_rows):  # Loop through the indexes of list based on the selectedRows order
        item = ocr_data[int(row_id)]  # convert to int so we index to right location in list ocr_data
        value = float(item["Value"])  # Take nominal from the right location to verify and correct the values.

        # check each index if found from all value within data
        for index, live_value in enumerate(live_values.values()):  # now looping though values in original sequence
            if (
                    value - 0.5 <= live_value <= value + 0.5):  # Found the matching ID so break and start next search from previous match index +1
                item["Measuring"] = f"{live_value:.4f}"  # We found the match, assign and continue next iteration
                live_index = live_index + index + 1  # Increase the index position from data list for next row to keep sync the measurement (and avoid match to same value)

                del live_values[list(live_values.keys())[index]]  # remove this matched data, to reduce search range.

                break  # break this current index loop.

        else:  # this runs when the inner loop had a loop through but can't match it, therefore assigning `N/A` value.
            item["Measuring"] = "N/A"  # when value not found then assigning N/A


def send_gcode_commands(esp_ip, line:str):
    """Gửi các lệnh G-code từ danh sách các dòng đến ESP32."""
    #send_command("progkey")  # Nhấn nút program
    #time.sleep(0.5)
    if line:
        for char in line:
            if char.isdigit():
                send_command(esp_ip, "key" + char)
            elif char == '.':
                send_command(esp_ip, "tkey")
            elif char == '-':
                send_command(esp_ip, "mkey")
            elif char == ' ':
                send_command(esp_ip, "insertkey")

            elif char == 'A':
                for _ in range(2):  # Nhấn 2 lần - CAB
                    send_command(esp_ip, "backey")
                    time.sleep(0.1)
            elif char == 'B':
                for _ in range(3):  # Nhấn 3 lần - CAB
                    send_command(esp_ip, "backey")
                    time.sleep(0.1)
            elif char == 'C': # Nhấn 1 lần
                send_command(esp_ip, "backey")

            elif char == 'K':
                for _ in range(4):  # Nhấn 2 lần - CAB
                    send_command(esp_ip, "kihkey")
                    time.sleep(0.1)
            elif char == 'I':
                for _ in range(3):  # Nhấn 3 lần - CAB
                    send_command(esp_ip, "kihkey")
                    time.sleep(0.1)
            elif char == 'H': # Nhấn 1 lần
                for _ in range(3):  # Nhấn 3 lần - CAB
                    send_command(esp_ip, "kihkey")
                    time.sleep(0.1)
            elif char == 'Y': # Nhấn 1 lần
                send_command(esp_ip, "kihkey")

            elif char == 'V':
                for _ in range(4):  # Nhấn 2 lần - CAB
                    send_command(esp_ip, "jqpkey")
                    time.sleep(0.1)
            elif char == 'J':
                for _ in range(3):  # Nhấn 3 lần - CAB
                    send_command(esp_ip, "jqpkey")
                    time.sleep(0.1)
            elif char == 'Q': # Nhấn 1 lần
                for _ in range(3):  # Nhấn 3 lần - CAB
                    send_command(esp_ip, "jqpkey")
                    time.sleep(0.1)
            elif char == 'P': # Nhấn 1 lần
                send_command(esp_ip, "jqpkey")

            elif char == 'S':
                send_command(esp_ip, "key0")
            elif char == 'U':
                send_command(esp_ip, "key1")
            elif char == 'W':
                send_command(esp_ip, "key2")
            elif char == 'R':
                send_command(esp_ip, "key3")
            elif char == 'X':
                send_command(esp_ip, "key4")
            elif char == 'Z':
                send_command(esp_ip, "key5")
            elif char == 'F':
                send_command(esp_ip, "key6")
            elif char == 'O':
                send_command(esp_ip, "key7")
            elif char == 'N':
                send_command(esp_ip, "key8")
            elif char == 'G':
                send_command(esp_ip, "key9")
            elif char == 'T':
                 send_command(esp_ip, "tkey")
            elif char == 'M':
                send_command(esp_ip, "mkey")

            else:
                send_command(esp_ip, char.lower() + "key") # Các lệnh ký tự chữ
            time.sleep(0.1)

        send_command(esp_ip, "eobkey")  # Kết thúc dòng
        time.sleep(0.1)
        send_command(esp_ip, "insertkey")  # Reset sau khi xong dòng
        time.sleep(0.1)
    else:
        print("Không có code để gửi.")


def send_offset(esp_ip, value):
    """Xử lý đầu vào từ người dùng để nhập offset."""
    if value is not None:
        try:
            pressit(esp_ip, value, None)  # Chỉ nhận X
        except Exception as error:
            print(repr(error))
    else:
        print("No value entered")


print("UnifiedUtils.py Imported")
