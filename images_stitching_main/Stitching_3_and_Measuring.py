from pypylon import pylon
import cv2
import tkinter as tk
from tkinter import ttk
import modbus_tcp
import math
import os
import imutils
from panorama_and_stitching.cam_homo.panorama import Stitcher
import numpy as np

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

# Biến toàn cục để lưu trữ giá trị pixel_per_inch và ngưỡng
pixel_per_inch_circle = 1337.99961013231
threshold_value = 30


# Hàm cập nhật giá trị pixel_per_inch_line
def update_pixel_per_inch_line():
    global pixel_per_inch_line
    pixel_per_inch_line = pixel_per_inch_circle / 1.054045085410249


# Hàm cập nhật giá trị pixel_per_inch
def update_pixel_per_inch_circle(new_value):
    global pixel_per_inch_circle
    pixel_per_inch_circle = float(new_value)
    # Cập nhật giá trị trong ô nhập liệu
    entry_ppi_circle.delete(0, tk.END)
    entry_ppi_circle.insert(0, pixel_per_inch_circle)


# Hàm cập nhật giá trị ngưỡng
def update_threshold(new_value):
    global threshold_value
    threshold_value = float(new_value)
    # Cập nhật giá trị trong ô nhập liệu
    entry_threshold.delete(0, tk.END)
    entry_threshold.insert(0, threshold_value)

stitcher = Stitcher()
#motion = BasicMotionDetector(minArea=500)
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
    thresh = cv2.threshold(blur, threshold_value, 150, cv2.THRESH_BINARY)[1]  # Sử dụng ngưỡng mới
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
            diameter = w
            diameter_mm = (diameter * 25.4) / pixel_per_inch_circle
            # Tìm tâm
            M = cv2.moments(c)
            cX = int(M["m10"] / M["m00"])
            cY = int(M["m01"] / M["m00"])
            position = (cX, cY)

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
            cv2.rectangle(img, (x, y), (x + w, y + h), (0, 255, 0), 4)
            cv2.drawContours(img, [c], 0, (36, 255, 12), 4)
            cv2.circle(img, (cX, cY), 15, (320, 159, 22), -1)

            # Draw line and diameter information
            cv2.line(img, (x, y + int(h / 2)), (x + w, y + int(h / 2)), (156, 188, 24), 3)
            cv2.putText(img, "Diameter: {}mm".format(round(diameter_mm, 3)), (cX - 50, cY - 50),
                        cv2.FONT_HERSHEY_SIMPLEX, 3, (156, 188, 24), 3)

            # Vẽ nhãn lên ảnh
            cv2.putText(img, f"({cX}, {cY})", (cX - 200, cY + 100), cv2.FONT_HERSHEY_SIMPLEX, 2, (0, 0, 255), 3)

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

                # Vẽ đường thẳng
                cv2.line(img, point1_c1, point2_c1, (0, 255, 0), 2)  # Vẽ đường thẳng màu xanh lá cây, dày 2 pixel
                cv2.line(img, point1_c2, point2_c2, (0, 0, 255), 2)  # Vẽ đường thẳng màu đỏ, dày 2 pixel
                cv2.line(img, point1_c1, point1_c2, (0, 0, 255), 2)  # Vẽ đường thẳng màu đỏ, dày 2 pixel

                # Tính độ dài của hai đường thẳng
                length_huyen = math.sqrt((x1 - x2) ** 2 + (y1 - y2) ** 2)
                length_line1 = math.sqrt(
                    length_huyen ** 2 - (math.sqrt((point2_c2[0] - x2) ** 2 + (point2_c2[1] - y2) ** 2) ** 2))
                length_line2 = math.sqrt(
                    length_huyen ** 2 - (math.sqrt((point2_c1[0] - x1) ** 2 + (point2_c1[1] - y1) ** 2) ** 2))

                # length_huyen_mm = length_huyen * 25.4 / pixel_per_inch_line
                length_line1_mm = length_line1 * 25.4 / pixel_per_inch_line
                length_line2_mm = length_line2 * 25.4 / pixel_per_inch_line

                # cv2.putText(img, "{}mm".format(round(length_huyen_mm, 3)), (x1 + 100, y1 + 50), cv2.FONT_HERSHEY_SIMPLEX, 3, (156, 188, 24), 3)
                cv2.putText(img, "{}mm".format(round(length_line1_mm, 3)), (x1 + 600, y1 + 120),
                            cv2.FONT_HERSHEY_SIMPLEX, 3, (156, 188, 24), 3)
                cv2.putText(img, "{}mm".format(round(length_line2_mm, 3)), (x2 + 50, y2 + 1000),
                            cv2.FONT_HERSHEY_SIMPLEX, 3, (156, 188, 24), 3)

    opening = cv2.resize(opening, (1080, 720))
    cv2.imshow("Opening", opening)
    # Resize ảnh về kích thước 1080x720
    img = cv2.resize(img, (1080, 720))
    img = img[0:720, 170:1080]  # Height, Width [720, 935]
    return img

# Tạo cửa sổ tkinter
root = tk.Tk()
root.title("Camera App")

# Tạo khung cho slider và ô nhập liệu
frame = tk.Frame(root)
frame.pack()

# Tạo nhãn cho slider Pixel per Inch
label_ppi = ttk.Label(frame, text="PPI")
label_ppi.pack(side=tk.LEFT)

# Tạo slider Pixel per Inch
slider_ppi = ttk.Scale(frame, from_=0, to=2000, orient=tk.HORIZONTAL, command=update_pixel_per_inch_circle)
slider_ppi.pack(side=tk.LEFT)

# Tạo ô nhập liệu Pixel per Inch
entry_ppi_circle = tk.Entry(frame)
entry_ppi_circle.insert(0, pixel_per_inch_circle)
entry_ppi_circle.pack(side=tk.LEFT)

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

# Tạo nút cập nhật giá trị
update_pixel_button = tk.Button(root, text="Cập nhật ppi",
                                command=lambda: update_pixel_per_inch_circle(entry_ppi_circle.get()))
update_pixel_button.pack()
update_pixel_button = tk.Button(root, text="Cập nhật threshold",
                                command=lambda: update_threshold(entry_threshold.get()))
update_pixel_button.pack()

# Tạo cửa sổ hiển thị ảnh (thay đổi kích thước)
canvas = tk.Canvas(root, width=908, height=718, bg="black")  # Canvas frame = [933, 718]
canvas.pack()

image_counter = 0
while True:
    ################## ĐỌC GIÁ TRỊ COIL TRONG PLC ĐỂ XÉT ĐIỀU KIỆN ###################
    read_m9 = modbus_tcp.client.read_coils(8201, 1)  # M9 - Biến tạm của Home: Khi đang trong trạng thái Home, không chụp ảnh
    m9_status = read_m9.bits[0]

    read_m13 = modbus_tcp.client.read_coils(8205, 1)  # M13 - Trạng thái chuẩn bị chụp cho Capture 2/3
    m13_status = read_m13.bits[0]

    read_m15 = modbus_tcp.client.read_coils(8207, 1)  # M15 - Trạng thái chuẩn bị chụp cho Capture 3/3
    m15_status = read_m15.bits[0]

    read_m18 = modbus_tcp.client.read_coils(8210, 1)  # M18 - Biến tạm của chế độ ghép ảnh tự động
    m18_status = read_m18.bits[0]

    read_m20 = modbus_tcp.client.read_coils(8212, 1)  # M20 - Biến tạm kích M8 để về Home sau khi chụp 3 ảnh
    m20_status = read_m20.bits[0]
    ################## XỬ LÝ ẢNH VÀ UPDATE GIAO DIỆN ###################
    grab_result = camera.RetrieveResult(5000, pylon.TimeoutHandling_ThrowException)
    if grab_result.GrabSucceeded():
        img = converter.Convert(grab_result).GetArray()

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
            image_counter = 0

        update_pixel_per_inch_line()
        # Xử lý ảnh
        img = process_image(img)

        # Hiển thị ảnh lên canvas
        photo = tk.PhotoImage(data=cv2.imencode('.ppm', img)[1].tobytes())
        canvas.create_image(0, 0, image=photo, anchor=tk.NW)
        canvas.image = photo

        root.update()

    if cv2.waitKey(1) == 27:
        break

# Ngắt kết nối camera
camera.StopGrabbing()
camera.Close()
cv2.destroyAllWindows()
root.mainloop()