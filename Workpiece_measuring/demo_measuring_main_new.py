from scipy.spatial import distance as distan
from imutils import perspective
from imutils import contours
import numpy as np
import imutils
import cv2
import time
from pypylon import pylon
import tkinter as tk
from tkinter import ttk
from shapely.geometry import LineString, Polygon
from shapely.ops import transform

def midpoint(ptA, ptB):
    return ((ptA[0] + ptB[0]) * 0.5, (ptA[1] + ptB[1]) * 0.5)

# Camera parameters (global variables)
focal_length = 25  # mm
sensor_width = 13.13  # mm
image_width = 5472  # pixels (default resolution width)
image_height = 3648  # pixels (default resolution height, assuming 3:2 aspect ratio)
distance = 120  # mm (initial value)
threshold_value = 200  # Threshold
calibration_distances = [110, 120, 130]  # Các khoảng cách hiệu chuẩn
ppm_values = [] # Danh sách để lưu trữ giá trị ppm tương ứng với khoảng cách

# Calibration parameters
mtx = None
dist = None
try:
    with np.load('calibration_params_5472.npz') as data:
        mtx = data['mtx']
        dist = data['dist']
        print("Calibration parameters loaded successfully.")
except FileNotFoundError:
    print("Calibration file not found. Using default parameters.")
    mtx = None
    dist = np.zeros((4, 1))

# Function to calculate ppm (Pixel Per Millimeter)
def calculate_ppm(wd, focal_length, sensor_width, image_width):
    """
    Calculates the pixels per millimeter (ppm) based on the working distance (wd),
    focal length, sensor width, and image width.

    Args:
        wd (float): The working distance in millimeters.
        focal_length (float): The focal length of the lens in millimeters.
        sensor_width (float): The width of the camera sensor in millimeters.
        image_width (int): The width of the image in pixels.

    Returns:
        float: The pixels per millimeter (ppm).
    """
    object_width_mm = (sensor_width * wd) / focal_length
    return image_width / object_width_mm

# Function to calculate object size
def calculate_real_size(pixel_size, ppm):
    """
    Calculates the real size of an object in millimeters based on its size in pixels
    and the pixels per millimeter (ppm).

    Args:
        pixel_size (float): The size of the object in pixels.
        ppm (float): The pixels per millimeter (ppm).

    Returns:
        float: The real size of the object in millimeters.
    """
    return pixel_size / ppm

def update_threshold(new_value):
    global threshold_value
    threshold_value = float(new_value)
    entry_threshold.delete(0, tk.END)
    entry_threshold.insert(0, threshold_value)

def update_distance(new_distance):
    global distance
    distance = float(new_distance)
    entry_distance.delete(0, tk.END)
    entry_distance.insert(0, distance)

def draw_parallel_lines(image, x1, y1, x2, y2, fractions=[0.1, 0.4, 0.7], color=(255, 100, 0), thickness=10, contour=None):
    if contour is None:
        raise ValueError("Contour must be provided")

    dx = x2 - x1
    dy = y2 - y1
    length = np.sqrt(dx**2 + dy**2)
    parallel_lines_lengths_mm = []

    # Tạo đa giác từ contour
    contour_polygon = Polygon(contour.reshape(-1, 2))

    for fraction in fractions:
        px = int(x1 + fraction * dx)
        py = int(y1 + fraction * dy)

        # Tăng phạm vi để đảm bảo giao điểm
        extend_factor = 1500
        normal_dx = -dy / length if length > 0 else 0
        normal_dy = dx / length if length > 0 else 0
        perp_x1 = int(px + extend_factor * normal_dx)
        perp_y1 = int(py + extend_factor * normal_dy)
        perp_x2 = int(px - extend_factor * normal_dx)
        perp_y2 = int(py - extend_factor * normal_dy)

        perp_line = LineString([(perp_x1, perp_y1), (perp_x2, perp_y2)])
        buffered_line = perp_line.buffer(0.1)  # Thêm buffer để tránh sai số

        # Tính giao điểm với contour_polygon
        intersection = buffered_line.intersection(contour_polygon)

        if not intersection.is_empty:
            if intersection.geom_type == 'MultiLineString':
                linestrings = list(intersection.geoms)
                if len(linestrings) >= 2:  # Lấy 2 đoạn giao gần nhất
                    coords1 = list(linestrings[0].coords)
                    coords2 = list(linestrings[1].coords)
                    if len(coords1) >= 1 and len(coords2) >= 1:
                        pt1 = (int(coords1[0][0]), int(coords1[0][1]))
                        pt2 = (int(coords2[0][0]), int(coords2[0][1]))
                        line_length_px = distan.euclidean(pt1, pt2)
                        # Tính toán ppm dựa trên khoảng cách hiện tại
                        current_ppm = calculate_ppm(distance, focal_length, sensor_width, image_width)
                        line_length_mm = calculate_real_size(line_length_px, current_ppm)
                        parallel_lines_lengths_mm.append(line_length_mm)
                        cv2.line(image, pt1, pt2, color, thickness)
                        mid_x = int((pt1[0] + pt2[0]) / 2)
                        mid_y = int((pt1[1] + pt2[1]) / 2)
                        cv2.putText(image, f"{line_length_mm:.4f}mm", (mid_x + 10, mid_y), cv2.FONT_HERSHEY_SIMPLEX, 3, color, 5)
            elif intersection.geom_type == 'LineString':
                coords = list(intersection.coords)
                if len(coords) >= 2:
                    # Lấy hai điểm đầu và cuối
                    pt1 = (int(coords[0][0]), int(coords[-1][0]))
                    pt2 = (int(coords[0][1]), int(coords[-1][1]))
                    line_length_px = distan.euclidean(pt1, pt2)
                    # Tính toán ppm dựa trên khoảng cách hiện tại
                    current_ppm = calculate_ppm(distance, focal_length, sensor_width, image_width)
                    line_length_mm = calculate_real_size(line_length_px, current_ppm)
                    parallel_lines_lengths_mm.append(line_length_mm)
                    cv2.line(image, pt1, pt2, color, thickness)
                    mid_x = int((pt1[0] + pt2[0]) / 2)
                    mid_y = int((pt1[1] + pt2[1]) / 2)
                    cv2.putText(image, f"{line_length_mm:.4f}mm", (mid_x + 10, mid_y), cv2.FONT_HERSHEY_SIMPLEX, 3, color, 5)
        else:
            print(f"Không có giao điểm tại y={py}. Đoạn thẳng không cắt qua contour.")

    return parallel_lines_lengths_mm

# Tạo cửa sổ tkinter
root = tk.Tk()
root.title("Camera App")

frame = tk.Frame(root)
frame.pack()

frame_threshold = tk.Frame(frame)
frame_threshold.pack(side=tk.LEFT)

label_threshold = ttk.Label(frame_threshold, text="Threshold")
label_threshold.pack(side=tk.LEFT)

slider_threshold = ttk.Scale(frame_threshold, from_=0, to=255, orient=tk.HORIZONTAL, command=update_threshold)
slider_threshold.pack(side=tk.LEFT)

entry_threshold = tk.Entry(frame_threshold)
entry_threshold.insert(0, threshold_value)
entry_threshold.pack(side=tk.LEFT)

button_frame = ttk.Frame(frame)
button_frame.pack(side=tk.LEFT)

frame_distance = tk.Frame(frame)
frame_distance.pack(side=tk.LEFT)

label_distance = ttk.Label(frame_distance, text="Distance (mm)")
label_distance.pack(side=tk.LEFT)

slider_distance = ttk.Scale(frame_distance, from_=100, to=300, orient=tk.HORIZONTAL, command=update_distance, variable=tk.DoubleVar(value=distance))
slider_distance.pack(side=tk.LEFT)

entry_distance = tk.Entry(frame_distance)
entry_distance.insert(0, distance)
entry_distance.pack(side=tk.LEFT)

update_distance_button = tk.Button(root, text="Cập nhật Distance", command=lambda: update_distance(entry_distance.get()))
update_distance_button.pack()

canvas = tk.Canvas(root, width=1080, height=718)  # Display resolution
canvas.pack()

# Camera initialization
camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
camera.StartGrabbing(pylon.GrabStrategy_LatestImageOnly)
camera.GainAuto.Value = "Off"
camera.ExposureAuto.Value = "Off"
camera.GammaSelector.Value = "User"
camera.Gamma.Value = 1.0
camera.ExposureTimeAbs.SetValue(100000)
camera.GainRaw.SetValue(0)

converter = pylon.ImageFormatConverter()
converter.OutputPixelFormat = pylon.PixelType_BGR8packed

while True:
    frame = camera.RetrieveResult(5000, pylon.TimeoutHandling_ThrowException)
    if frame.GrabSucceeded():
        img = converter.Convert(frame).GetArray()  # Get full resolution image

        if mtx is not None and dist is not None:
            h, w = img.shape[:2]
            newcameramtx, roi = cv2.getOptimalNewCameraMatrix(mtx, dist, (w, h), 1, (w, h))
            img = cv2.undistort(img, mtx, dist, None, newcameramtx)
            x, y, w, h = roi
            img = img[y:y + h, x:x + w]
            image_width = w # Cập nhật lại image_width sau khi undistort và crop

        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        blur = cv2.GaussianBlur(gray, (9, 9), 0)
        thresh = cv2.threshold(blur, threshold_value, 255, cv2.THRESH_BINARY)[1]
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
        opening = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)
        edged = cv2.Canny(opening, 50, int(threshold_value)) # Thử nghiệm với các ngưỡng khác nhau

        cnts = cv2.findContours(edged.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        cnts = imutils.grab_contours(cnts)

        anh_copy = img.copy()

        if len(cnts) == 0:
            cv2.imshow("Camera", anh_copy)
        else:
            # Lấy contour lớn nhất
            c = max(cnts, key=cv2.contourArea)
            cv2.drawContours(anh_copy, [c], -1, (0, 255, 0), 10)

            if cv2.contourArea(c) < 10:  # Lọc contour nhỏ
                continue

            # Tính toán hình chữ nhật bao quanh nhỏ nhất
            hinh_hop = cv2.minAreaRect(c)
            hinh_hop = cv2.cv.boxPoints(hinh_hop) if imutils.is_cv2() else cv2.boxPoints(hinh_hop)
            hinh_hop = np.array(hinh_hop, dtype="int")
            hinh_hop = perspective.order_points(hinh_hop)

            (tl, tr, br, bl) = hinh_hop
            (tltrX, tltrY) = midpoint(tl, tr)
            (blbrX, blbrY) = midpoint(bl, br)
            (tlblX, tlblY) = midpoint(tl, bl)
            (trbrX, trbrY) = midpoint(tr, br)

            chieu_doc_pixels = distan.euclidean((tltrX, tltrY), (blbrX, blbrY))
            chieu_ngang_pixels = distan.euclidean((tlblX, tlblY), (trbrX, trbrY))

            # Tính toán ppm dựa trên khoảng cách hiện tại
            current_ppm = calculate_ppm(distance, focal_length, sensor_width, image_width)
            chieu_doc_mm = calculate_real_size(chieu_doc_pixels, current_ppm)
            chieu_ngang_mm = calculate_real_size(chieu_ngang_pixels, current_ppm)

            if chieu_doc_pixels > chieu_ngang_pixels:
                lengths = draw_parallel_lines(anh_copy, int(tltrX), int(tltrY), int(blbrX), int(blbrY),
                                              fractions=[0.2, 0.55, 0.85], color=(255, 100, 0), contour=c)
            else:
                lengths = draw_parallel_lines(anh_copy, int(tlblX), int(tlblY), int(trbrX), int(trbrY),
                                              fractions=[0.2, 0.5, 0.8], color=(255, 100, 0), contour=c)

        # Resize image for display only after processing
        display_img = cv2.resize(anh_copy, (1080, 718))  # Adjust height to fit canvas
        photo = tk.PhotoImage(data=cv2.imencode('.ppm', display_img)[1].tobytes())
        canvas.create_image(0, 0, image=photo, anchor=tk.NW)
        canvas.image = photo
        root.update()

    if cv2.waitKey(1) == 27:
        break

camera.StopGrabbing()
camera.Close()
cv2.destroyAllWindows()
root.mainloop()
