from pypylon import pylon
import cv2
import tkinter as tk
from tkinter import ttk
import modbus_tcp
import math
import os
from panorama_and_stitching.cam_homo.panorama import Stitcher
import sys
from contextlib import contextmanager
from keras.models import load_model
import numpy as np

# Training phôi
np.set_printoptions(suppress=True)
model_tm = load_model(
    "E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\Teachable_machine\\11_10_2024\\converted_keras\\keras_model.h5",
    compile=False)
class_names = open(
    "E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\Teachable_machine\\11_10_2024\\converted_keras\\labels.txt",
    "r").readlines()

# Khởi tạo instant của camera
camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
camera.StartGrabbing(pylon.GrabStrategy_LatestImageOnly)
camera.GainAuto.Value = "Off"
camera.ExposureAuto.Value = "Off"
# Set the gamma type to User
camera.GammaSelector.Value = "User"
camera.Gamma.Value = 1.0
camera.ExposureTimeAbs.SetValue(100000)
camera.GainRaw.SetValue(51)

converter = pylon.ImageFormatConverter()
converter.OutputPixelFormat = pylon.PixelType_BGR8packed

# Thông số camera
focal_length = 25  # Tiêu cự của camera (mm)
sensor_width = 13.13  # Chiều rộng cảm biến (mm)
image_width = 1080  # Chiều rộng ảnh (pixel)
#distance = 199.640919
distance = 191.044
distance_line = 200.508

# Khảng cách từ camera đến vật

focal_length_inches = focal_length / 25.4
sensor_width_inches = sensor_width / 25.4
distance_inches = distance / 25.4
line_distance_inches = distance_line / 25.4

object_width_inches = (sensor_width_inches * distance_inches) / focal_length_inches
object_line_width_inches = (sensor_width_inches * line_distance_inches) / focal_length_inches

ppi = image_width / object_width_inches
ppi_line = image_width / object_line_width_inches
threshold_value = 45  # Threshold

# Biến toàn cục để lưu trữ tọa độ của đường thẳng vuông góc
perpendicular_line_coords = None
# Cờ để đánh dấu đã vẽ đường thẳng màu hồng hay chưa
is_perpendicular_line_drawn = False
# Cờ để đánh dấu đã phát hiện phôi lần đầu tiên
first_detection = False
# Cờ để đánh dấu đã tìm thấy gốc tọa độ O lần đầu tiên
first_origin_detection = False
# Tạo label báo lỗi
alarm_label = None

@contextmanager
def suppress_stdout():  # Tắt thông báo của Teachable Machine
    # Lưu lại stdout hiện tại
    current_stdout = sys.stdout
    # Tạo một file tạm thời để chuyển hướng stdout
    with open(os.devnull, 'w') as devnull:
        # Chuyển hướng stdout sang file tạm thời
        sys.stdout = devnull
        try:
            # Thực hiện các công việc bên trong context
            yield
        finally:
            # Trả lại stdout ban đầu
            sys.stdout = current_stdout

# Hàm cập nhật giá trị ngưỡng
def update_threshold(new_value):
    global threshold_value
    threshold_value = float(new_value)
    # Cập nhật giá trị trong ô nhập liệu
    entry_threshold.delete(0, tk.END)
    entry_threshold.insert(0, threshold_value)

# Hàm cập nhật giá trị distance
def update_distance(new_distance):
    global distance, ppi  # Access global variables
    distance = float(new_distance)
    # Update calculations that depend on distance
    distance_inches = distance / 25.4
    object_width_inches = (sensor_width_inches * distance_inches) / focal_length_inches
    ppi = image_width / object_width_inches
    # Update the entry field
    entry_distance.delete(0, tk.END)
    entry_distance.insert(0, distance)

stitcher = Stitcher()

# motion = BasicMotionDetector(minArea=500)
# Hàm ghép ảnh và lưu kết quả
def stitch_and_save_images():
    image1_path = "E:/CXHCTL_Phuc/Programs/PythonCode/pythonProject/stitching_images/stitching_image_1.jpg"
    image2_path = "E:/CXHCTL_Phuc/Programs/PythonCode/pythonProject/stitching_images/stitching_image_2.jpg"

    if os.path.exists(image1_path) and os.path.exists(image2_path):
        imageA = cv2.imread(image1_path)
        imageB = cv2.imread(image2_path)

        # Resize ảnh để đảm bảo chiều rộng tương tự (nếu cần)
        # imageA = imutils.resize(imageA, width=400)
        # imageB = imutils.resize(imageB, width=400)

        # Tạo ma trận biến đổi cho ảnh thứ hai
        # Hàm findHomography() sẽ trả về ma trận biến đổi
        # Sử dụng thuật toán SIFT để tìm các điểm tương đồng
        sift = cv2.SIFT_create()
        kp1, des1 = sift.detectAndCompute(imageA, None)
        kp2, des2 = sift.detectAndCompute(imageB, None)

        # Sử dụng thuật toán FLANN để tìm các điểm tương đồng tốt nhất giữa hai tập mô tả
        FLANN_INDEX_KDTREE = 1
        index_params = dict(algorithm=FLANN_INDEX_KDTREE, trees=5)
        search_params = dict(checks=50)
        flann = cv2.FlannBasedMatcher(index_params, search_params)
        matches = flann.knnMatch(des1, des2, k=2)

        good = []
        for m, n in matches:
            if m.distance < 0.7 * n.distance:
                good.append(m)

        #  Kiểm tra số lượng điểm tương đồng tìm được
        if len(good) > 4:
            src_pts = np.float32([kp1[m.queryIdx].pt for m in good]).reshape(-1, 1, 2)
            dst_pts = np.float32([kp2[m.trainIdx].pt for m in good]).reshape(-1, 1, 2)

            # Tìm ma trận biến đổi
            M, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC, 5.0)

            # Ghép ảnh sử dụng Stitcher
            result = stitcher.stitch([imageA, imageB], M, mask)

            if result is not None:
                output_path = "E:/CXHCTL_Phuc/Programs/PythonCode/pythonProject/stitching_images/stitched_image.jpg"
                cv2.imwrite(output_path, result)
                print("Ảnh đã được ghép và lưu tại:", output_path)
            else:
                print("Lỗi: Không thể ghép ảnh.")
        else:
            print("Lỗi: Không đủ điểm tương đồng để ghép ảnh.")
    else:
        print("Lỗi: Không tìm thấy ảnh để ghép.")


def handleButtonClick(button_name):
    global image_counter
    ################ CHẾ ĐỘ AUTO NGOẠI QUAN #################
    if button_name == "start_ht":
        modbus_tcp.client.write_coil(8192, 1)  # M0
        modbus_tcp.client.write_coil(8192, 0)
    elif button_name == "stop_ht":
        modbus_tcp.client.write_coil(8193, 1)  # M1
        modbus_tcp.client.write_coil(8193, 0)
    elif button_name == "home":
        read_m19 = modbus_tcp.client.read_coils(8211, 1)  # M19 - Xử lý lỗi khi đã chụp 1 ảnh mà muốn về home
        m19_status = read_m19.bits[0]
        print(m19_status)
        if m19_status:
            modbus_tcp.client.write_coil(8212, 1)  # M20
            image_counter = 0
        else:
            modbus_tcp.client.write_coil(8199, 1)  # M7
            modbus_tcp.client.write_coil(8199, 0)
    elif button_name == "run":
        modbus_tcp.client.write_coil(8208, 1)  # M16
        modbus_tcp.client.write_coil(8208, 0)

center1 = None
center2 = None

# Hàm xử lý ảnh
def process_image(img):
    global center1, center2
        ################## XỬ LÝ ẢNH ĐỂ ĐO KÍCH THƯỚC #################
    # Xử lý ảnh bằng OpenCV:
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (3, 3), 0)
    thresh = cv2.threshold(blur, threshold_value, 255, cv2.THRESH_BINARY)[1]  # Sử dụng ngưỡng mới
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
    opening = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)

    cnts = cv2.findContours(opening, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    cnts = cnts[0] if len(cnts) == 2 else cnts[1]

    filtered_cnts = []
    circle_data = {}
    circle_count = 0  # Biến đếm số hình tròn đã phát hiện
    for i, c in enumerate(cnts):
        if cv2.contourArea(c) < 1500:
            continue

        # Find perimeter of contour
        perimeter = cv2.arcLength(c, True)
        # Perform contour approximation
        approx = cv2.approxPolyDP(c, 0.04 * perimeter, True)

        # We assume that if the contour has more than a certain
        # number of verticies, we can make the assumption
        # that the contour shape is a circle
        if len(approx) > 6:
            # Obtain bounding rectangle to get measurements
            x, y, w, h = cv2.boundingRect(c)

            # Find measurements
            diameter = w  # Đường kính của hình tròn trong ảnh (pixel)
            # Tìm tâm
            M = cv2.moments(c)
            cX = int(M["m10"] / M["m00"])
            cY = int(M["m01"] / M["m00"])
            position = (cX, cY)
            # Tính chiều cao thực tế của vật thể
            diameter_mm = diameter * 25.4 / ppi

            # Lưu trữ giá trị của hình tròn
            if circle_count == 0:
                x1, y1, w1, h1 = x, y, w, h
                center1 = (cX, cY)
                circle_count += 1

            elif circle_count == 1:
                x2, y2, w2, h2 = x, y, w, h
                center2 = (cX, cY)
                circle_count += 1

            # Draw the contour and center of the shape on the image
            cv2.rectangle(img, (x, y), (x + w, y + h), (0, 255, 0), 1)
            cv2.drawContours(img, [c], 0, (36, 255, 12), 1)
            cv2.circle(img, (cX, cY), 5, (320, 159, 22), -1)

            # Draw line and diameter information
            cv2.line(img, (x, y + int(h / 2)), (x + w, y + int(h / 2)), (156, 188, 24), 1)
            cv2.putText(img, "Diameter: {}mm".format(round(diameter_mm, 3)), (cX - 60, cY - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)

            # Vẽ nhãn lên ảnh
            cv2.putText(img, f"({cX}, {cY})", (cX - 43, cY + 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)

            # Lưu đường kính vào dictionary
            circle_data[position] = diameter_mm

            # Thêm contour vào filtered_cnts
            filtered_cnts.append(c)

            if center1 is not None and center2 is not None:
                # Tính phương trình đường thẳng
                x1, y1 = center1
                x2, y2 = center2

                # Chọn hai điểm trên đường thẳng vuông góc
                point1_c1 = center1  # Điểm đầu tiên của line 1 trùng với tâm của hình tròn thứ nhất
                point2_c1 = (x1 - (x1 - x2), y1)

                point1_c2 = center2  # Điểm đầu tiên của line 2 trùng với tâm của hình tròn thứ hai
                point2_c2 = (x2, int(y2 + (y1 - y2)))

                mid_c1_x = (point1_c1[0] + point2_c1[0]) // 2
                mid_c1_y = (point1_c1[1] + point2_c1[1]) // 2
                mid_c2_x = (point1_c2[0] + point2_c2[0]) // 2
                mid_c2_y = (point1_c2[1] + point2_c2[1]) // 2

                # Vẽ đường thẳng
                cv2.line(img, point1_c1, point2_c1, (0, 255, 0), 1)  # Vẽ đường thẳng màu xanh lá cây, dày 2 pixel
                cv2.line(img, point1_c2, point2_c2, (0, 0, 255), 1)  # Vẽ đường thẳng màu đỏ, dày 2 pixel
                cv2.line(img, point1_c1, point1_c2, (0, 0, 255), 1)  # Vẽ đường thẳng màu đỏ, dày 2 pixel

                # Tính độ dài của hai đường thẳng
                length_huyen = math.sqrt((x1 - x2) ** 2 + (y1 - y2) ** 2)
                length_line1 = math.sqrt(
                    length_huyen ** 2 - (math.sqrt((point2_c2[0] - x2) ** 2 + (point2_c2[1] - y2) ** 2) ** 2))
                length_line2 = math.sqrt(
                    length_huyen ** 2 - (math.sqrt((point2_c1[0] - x1) ** 2 + (point2_c1[1] - y1) ** 2) ** 2))

                length_line1_mm = length_line1 * 25.4 / ppi_line
                length_line2_mm = length_line2 * 25.4 / ppi_line
                length_huyen_mm = length_huyen * 25.4 / ppi_line

                cv2.putText(img, "{}mm".format(round(length_line1_mm, 3)), (mid_c1_x - 30, mid_c1_y + 20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (156, 188, 24), 1)
                cv2.putText(img, "{}mm".format(round(length_line2_mm, 3)), (mid_c2_x + 10, mid_c2_y),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (156, 188, 24), 1)
                cv2.putText(img, "{}mm".format(round(length_huyen_mm, 3)), (mid_c2_x + 30, mid_c2_y - 20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (156, 188, 24), 1)

    cv2.imshow("Opening", opening)
    return img

# Tạo cửa sổ tkinter
root = tk.Tk()
root.title("Camera App")

# Tạo khung cho slider và ô nhập liệu
frame = tk.Frame(root)
frame.pack()

# Tạo khung cho slider Threshold
frame_threshold = tk.Frame(frame)
frame_threshold.pack(side=tk.LEFT)

# Tạo nhãn cho slider Threshold
label_threshold = ttk.Label(frame_threshold, text="Threshold")
label_threshold.pack(side=tk.LEFT)

# Tạo slider Threshold
slider_threshold = ttk.Scale(frame_threshold, from_=0, to=255, orient=tk.HORIZONTAL, command=update_threshold)
slider_threshold.pack(side=tk.LEFT)

# Tạo ô nhập liệu Threshold
entry_threshold = tk.Entry(frame_threshold)
entry_threshold.insert(0, threshold_value)
entry_threshold.pack(side=tk.LEFT)

button_frame = ttk.Frame(frame)
button_frame.pack(side=tk.LEFT)

# Tạo khung cho slider Distance
frame_distance = tk.Frame(frame)  # Use the same frame as threshold
frame_distance.pack(side=tk.LEFT)

# Tạo nhãn cho slider Distance
label_distance = ttk.Label(frame_distance, text="Distance (mm)")
label_distance.pack(side=tk.LEFT)

# Tạo slider Distance (adjust range as needed)
slider_distance = ttk.Scale(frame_distance, from_=100, to=300, orient=tk.HORIZONTAL,
                           command=update_distance, variable=tk.DoubleVar(value=distance))  # Initialize with current distance
slider_distance.pack(side=tk.LEFT)


# Tạo ô nhập liệu Distance
entry_distance = tk.Entry(frame_distance)
entry_distance.insert(0, distance)
entry_distance.pack(side=tk.LEFT)

# Button to update distance from entry field
update_distance_button = tk.Button(root, text="Cập nhật Distance",
                                   command=lambda: update_distance(entry_distance.get()))
update_distance_button.pack()

start_ht = tk.Button(
    button_frame,
    text="Start",
    fg="#333",
    font=("Arial", 12, "bold"),
    width=5,
    height=1,
    borderwidth=1,
    relief=tk.SOLID,
    command=lambda button_name="start_ht": handleButtonClick(button_name)
)
start_ht.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

stop_ht = tk.Button(
    button_frame,
    text="Stop",
    fg="#333",
    font=("Arial", 12, "bold"),
    width=5,
    height=1,
    borderwidth=1,
    relief=tk.SOLID,
    command=lambda button_name="stop_ht": handleButtonClick(button_name)
)
stop_ht.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")

home = tk.Button(
    button_frame,
    text="Home",
    fg="#333",
    font=("Arial", 12, "bold"),
    width=5,
    height=1,
    borderwidth=1,
    relief=tk.SOLID,
    command=lambda button_name="home": handleButtonClick(button_name)
)
home.grid(row=1, column=0, padx=10, pady=10, sticky="nsew")

run = tk.Button(
    button_frame,
    text="Run",
    fg="#333",
    font=("Arial", 12, "bold"),
    width=5,
    height=1,
    borderwidth=1,
    relief=tk.SOLID,
    command=lambda button_name="run": handleButtonClick(button_name)
)
run.grid(row=1, column=1, padx=10, pady=10, sticky="nsew")

update_pixel_button = tk.Button(root, text="Cập nhật threshold",
                                command=lambda: update_threshold(entry_threshold.get()))
update_pixel_button.pack()

# Tạo cửa sổ hiển thị ảnh (thay đổi kích thước)
canvas = tk.Canvas(root, width=908, height=718, bg="black")  # Canvas frame = [933, 718]
canvas.pack()

image_counter = 0

while True:
    ################## ĐỌC GIÁ TRỊ COIL TRONG PLC ĐỂ XÉT ĐIỀU KIỆN ###################
    read_m9 = modbus_tcp.client.read_coils(8201,
                                           1)  # M9 - Biến tạm của Home: Khi đang trong trạng thái Home, không chụp ảnh
    m9_status = read_m9.bits[0]

    read_m13 = modbus_tcp.client.read_coils(8205, 1)  # M13 - Trạng thái chuẩn bị chụp cho Capture 2/3
    m13_status = read_m13.bits[0]

    read_m15 = modbus_tcp.client.read_coils(8207, 1)  # M15 - Trạng thái chuẩn bị chụp cho Capture 3/3
    m15_status = read_m15.bits[0]

    read_m18 = modbus_tcp.client.read_coils(8210, 1)  # M18 - Biến tạm của chế độ ghép ảnh tự động
    m18_status = read_m18.bits[0]

    read_m20 = modbus_tcp.client.read_coils(8212, 1)  # M20 - Biến tạm kích M8 để về Home sau khi chụp 3 ảnh
    m20_status = read_m20.bits[0]
    ##################################################################################

    ####################### ######################
    grab_result = camera.RetrieveResult(5000, pylon.TimeoutHandling_ThrowException)
    if grab_result.GrabSucceeded():
        img = converter.Convert(grab_result).GetArray()

        ########################### RESIZE & CREATE ROI ##########################
        img = cv2.resize(img, (1080, 720))
        img = img[0:720, 170:1080]
        #########################################################################

        # Xác định có phôi
        input_image = cv2.resize(img, (224, 224), interpolation=cv2.INTER_AREA)
        input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2RGB)
        input_image = np.asarray(input_image, dtype=np.float32).reshape(1, 224, 224, 3)
        input_image = (input_image / 127.5) - 1

        with suppress_stdout():
            prediction = model_tm.predict(input_image)
        index = np.argmax(prediction)
        confidence_score = round(prediction[0][index] * 100)
        print(index)
        print(confidence_score)

        # **Phát hiện phôi lần đầu tiên**
        if index == 1 and confidence_score >= 50 and not first_detection:
            alarm_label = tk.Label(root, text="Vui lòng lấy phôi ra để xác định gốc tọa độ O", fg="red", font=("Arial", 12, "bold"))
            alarm_label.pack()
            first_detection = True  # Đánh dấu đã phát hiện phôi lần đầu tiên
        else:
            ########################### Tìm kiếm gốc tọa độ O ############################
            if index == 0 and confidence_score >= 50:
                first_detection = True # Phát hiện gốc tọa độ O khi vừa chạy chương trình
                ########################## XÁC ĐỊNH GỐC TỌA ĐỘ O ########################
                OAxis_gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
                _, OAxis_thresh = cv2.threshold(OAxis_gray, 185, 255, cv2.THRESH_BINARY_INV)

                # Phát hiện biên
                OAxis_edges = cv2.Canny(OAxis_thresh, 50, 150)
                # Tìm hình chữ nhật bao quanh biên
                OAxis_cnts = cv2.findContours(OAxis_edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
                OAxis_cnts = OAxis_cnts[0] if len(OAxis_cnts) == 2 else OAxis_cnts[1]

                # Lưu trữ tọa độ của hai hình chữ nhật
                rect_coords = []
                for i, c in enumerate(OAxis_cnts):
                    if cv2.contourArea(c) < 1:
                        continue
                    # Tìm hình chữ nhật bao quanh contour
                    x, y, w, h = cv2.boundingRect(c)

                    # Kiểm tra xem chiều cao có lớn hơn chiều rộng hay không
                    if h > w:  # Nếu chiều cao lớn hơn chiều rộng (nằm dọc)
                        # Vẽ hình chữ nhật
                        #cv2.rectangle(img, (x, y), (x + w, y + h), (0, 0, 255), 2)

                        # Lưu trữ tọa độ của hình chữ nhật
                        rect_coords.append((x, y + h, x + w, y + h))  # Lưu trữ (x, y, x + w, y)

                # Vẽ đoạn thẳng nối giữa hai hình chữ nhật
                if len(rect_coords) >= 2:
                    OAxis_point1 = rect_coords[0]  # Lấy hình chữ nhật dọc đầu tiên
                    OAxis_point2 = rect_coords[1]  # Lấy hình chữ nhật dọc thứ hai
                    #cv2.line(img, (OAxis_point1[0], OAxis_point1[1]), (OAxis_point2[2], OAxis_point2[1]), (0, 255, 0), 2)  # Vẽ đoạn thẳng màu xanh lá cây

                    # Tính toán trung điểm của đoạn thẳng
                    OAxis_mid_x = (OAxis_point1[0] + OAxis_point2[2]) // 2  # Sử dụng x của point1 và x + w của point2
                    OAxis_mid_y = OAxis_point1[1]  # Sử dụng y của cả hai điểm (vì chúng nằm trên cùng một hàng)
                    OAxis_midpoint1 = (OAxis_mid_x, OAxis_mid_y)
                    OAxis_midpoint2 = (OAxis_mid_x, int(OAxis_mid_y - 1000))
                    # cv2.circle(img, OAxis_midpoint1, 5, (255, 0, 0), -1)  # Vẽ điểm trung điểm
                    # cv2.line(img, OAxis_midpoint1, OAxis_midpoint2, (0, 0, 255), 2)

                    # Cập nhật tọa độ của đường thẳng vuông góc CHỈ KHI CHƯƠNG TRÌNH CHẠY LẦN ĐẦU TIÊN
                    if perpendicular_line_coords is None and not is_perpendicular_line_drawn:
                        perpendicular_line_coords = [(OAxis_mid_x, OAxis_mid_y), (OAxis_mid_x, int(OAxis_mid_y - 1000))]
                        is_perpendicular_line_drawn = True
                        first_origin_detection = True  # Đánh dấu đã tìm thấy gốc tọa độ O lần đầu tiên
                # VẼ ĐOẠN THẲNG MÀU HỒNG CHỈ KHI NÓ ĐÃ ĐƯỢC LƯU TRỮ
                if perpendicular_line_coords is not None:
                    OAxis_point1, OAxis_point2 = perpendicular_line_coords
                    cv2.line(img, OAxis_point1, OAxis_point2, (255, 0, 255), 2)  # Đường thẳng màu hồng
                # Xóa thông báo lỗi khi đã tìm thấy gốc tọa độ O
                if alarm_label is not None:
                    alarm_label.destroy()
                    alarm_label = None
                #########################################################################

            # **Đo kích thước phôi**
            if index == 1 and confidence_score >= 80:
                if first_origin_detection:  # Chỉ đo kích thước phôi khi đã tìm thấy gốc tọa độ O
                    if image_counter > 2:
                        image_counter = 2
                    if m20_status == 0 and m18_status and image_counter == 0:
                        output_dir = f'E:\CXHCTL_Phuc\Programs\PythonCode\pythonProject\stitching_images'
                        if not os.path.exists(output_dir):
                            os.makedirs(output_dir)

                        image_path = os.path.join(output_dir, f'stitching_image_1.jpg')
                        cv2.imwrite(image_path, img)
                        print("Image 1 saved:", image_path)
                        modbus_tcp.client.write_coil(8211, 1)  # M19 - Biến tạm của Capture 1/2
                        image_counter += 1

                    if m20_status == 0 and m13_status and image_counter == 1:
                        output_dir = f'E:\CXHCTL_Phuc\Programs\PythonCode\pythonProject\stitching_images'
                        if not os.path.exists(output_dir):
                            os.makedirs(output_dir)

                        image_path = os.path.join(output_dir, f'stitching_image_2.jpg')
                        cv2.imwrite(image_path, img)
                        print("Image 2 saved:", image_path)
                        modbus_tcp.client.write_coil(8206, 1)  # M14 - Biến tạm của Capture 2/3
                        image_counter += 1

                    if m20_status == 0 and m15_status and image_counter == 2:
                        output_dir = f'E:\CXHCTL_Phuc\Programs\PythonCode\pythonProject\stitching_images'
                        if not os.path.exists(output_dir):
                            os.makedirs(output_dir)

                        image_path = os.path.join(output_dir, f'stitching_image_3.jpg')
                        cv2.imwrite(image_path, img)
                        print("Image 3 saved:", image_path)
                        modbus_tcp.client.write_coil(8212, 1)  # M20 - Biến tạm của Capture 3/3
                        # stitch_and_save_images() # Gọi hàm stitch 3 ảnh vừa chụp lại với nhau
                        image_counter = 0

                    ############################ ĐO KÍCH THƯỚC PHÔI ##########################
                    img = process_image(img)
                    if perpendicular_line_coords is not None:
                        OAxis_point1, OAxis_point2 = perpendicular_line_coords
                        cv2.line(img, OAxis_point1, OAxis_point2, (255, 0, 255), 2)
                    ##########################################################################

        ##################### HIỂN THỊ HÌNH ẢNH LÊN CANVAS #######################
        photo = tk.PhotoImage(data=cv2.imencode('.ppm', img)[1].tobytes())
        canvas.create_image(0, 0, image=photo, anchor=tk.NW)
        canvas.image = photo

        # Cập nhật giao diện
        root.update()
        ##########################################################################

    ###############################################################################

    if cv2.waitKey(1) == 27:
        break

# Ngắt kết nối camera
camera.StopGrabbing()
camera.Close()
cv2.destroyAllWindows()
root.mainloop()