import cv2
import numpy as np
from scipy.spatial import distance
import imutils
from imutils import perspective, contours
import tkinter as tk
from tkinter import ttk
from pypylon import pylon

def midpoint(ptA, ptB):
    return ((ptA[0] + ptB[0]) * 0.5, (ptA[1] + ptB[1]) * 0.5)

# Camera parameters
focal_length = 25  # mm
sensor_width = 13.13  # mm
original_image_width = 5472  # pixels
original_image_height = 5472
#ppm = 70.51282051282051
ppm = 71.1103938

# Update image dimensions after resize (300% = 3x)
resize_factor = 2
image_width = original_image_width * resize_factor  # 16416
image_height = original_image_height * resize_factor  # 10944
ppm = ppm * resize_factor  # Adjust ppm for resized image

threshold_value = 200

def update_distance_and_ppm():
    global object_width, ppm
    object_width = (sensor_width / focal_length)
    ppm = (original_image_width / object_width) * resize_factor

def find_intersections(contour, y, x_min, x_max):
    intersections = []
    contour = contour.reshape(-1, 2)  # Reshape contour to (N, 2)

    for i in range(len(contour) - 1):
        x1, y1 = contour[i]
        x2, y2 = contour[i + 1]
        if (y1 <= y <= y2) or (y2 <= y <= y1):
            if y1 != y2:  # Avoid division by zero
                x = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
                if x_min <= x <= x_max:
                    intersections.append((int(x), int(y)))

    intersections.sort(key=lambda p: p[0])

    unique_intersections = []
    seen_x = set()
    for pt in intersections:
        if pt[0] not in seen_x:
            unique_intersections.append(pt)
            seen_x.add(pt[0])

    if len(unique_intersections) < 2:
        contour_points = contour.tolist()
        leftmost = min(contour_points, key=lambda p: p[0])
        rightmost = max(contour_points, key=lambda p: p[0])
        unique_intersections = [(int(leftmost[0]), int(y)), (int(rightmost[0]), int(y))]

    return unique_intersections

def draw_parallel_lines(image, x1, y1, x2, y2, fractions=[0.2, 0.45, 0.5, 0.55, 0.8], color=(0, 255, 0), thickness=30, contour=None, image_width=image_width, opening=None):
    if contour is None or opening is None:
        raise ValueError("Contour and opening image must be provided")

    dx = x2 - x1
    dy = y2 - y1
    parallel_lines_lengths_mm = []
    pt1_points = {}  # Lưu trữ điểm pt1 cho mỗi fraction

    wide_x_min = 0
    wide_x_max = image_width

    # Chuyển ảnh opening sang ảnh màu để vẽ các điểm
    opening_colored = cv2.cvtColor(opening, cv2.COLOR_GRAY2BGR)

    for fraction in fractions:
        py = int(y1 + fraction * dy)
        intersections = find_intersections(contour, py, wide_x_min, wide_x_max)
        print("Fraction:", fraction, "Intersections:", intersections)

        if len(intersections) >= 2:
            pt1 = intersections[0]

            pt2 = intersections[-1]
            pt1_points[fraction] = pt1  # Lưu điểm pt1
            cv2.circle(image, pt1, 21, color, 30)  # Vẽ trên image để hiển thị
            cv2.circle(image, pt2, 21, color, 30)
            # Vẽ vòng tròn cho pt1 tại fraction 0.45, 0.5, 0.55 trên opening_colored
            if fraction in [0.45, 0.5, 0.55]:
                cv2.circle(opening_colored, pt1, 21, (255, 0, 0), 30)  # Vẽ vòng tròn đỏ trên opening
            line_length_px = distance.euclidean(pt1, pt2)
            line_length_mm = line_length_px / ppm
            print("Pixel size:", line_length_px, "Real size:", line_length_mm)
            parallel_lines_lengths_mm.append(line_length_mm)
            cv2.line(image, pt1, pt2, color, thickness)
            mid_x = int((pt1[0] + pt2[0]) / 2)
            mid_y = int((pt1[1] + pt2[1]) / 2)
            print("Fraction:", fraction, "PT1:", pt1, "Length:", line_length_mm)
            cv2.putText(image, f"{line_length_mm:.4f} mm", (mid_x + 15, mid_y), cv2.FONT_HERSHEY_SIMPLEX, 6, color, 30)
        else:
            print(f"Không tìm thấy đủ giao điểm tại y={py}. Chỉ tìm thấy {len(intersections)} giao điểm.")

    # Tính và vẽ khoảng cách từ pt1 tại fraction 0.2 đến pt1 tại fraction 0.8 trên image
    if 0.2 in pt1_points and 0.8 in pt1_points:
        pt1_02 = pt1_points[0.2]
        pt1_08 = pt1_points[0.8]
        distance_px = distance.euclidean(pt1_02, pt1_08)
        print("Distance_px:", distance_px)
        distance_mm = distance_px / ppm
        cv2.line(image, pt1_02, pt1_08, (0, 0, 255), thickness)
        mid_x = int((pt1_02[0] + pt1_08[0]) / 2)
        mid_y = int((pt1_02[1] + pt1_08[1]) / 2)
        cv2.putText(image, f"{distance_mm:.4f} mm", (mid_x - 1500, mid_y - 225), cv2.FONT_HERSHEY_SIMPLEX, 6, (0, 0, 255), 30)
        print(f"Khoảng cách từ pt1 (fraction 0.2) đến pt1 (fraction 0.8): {distance_px:.2f} px, {distance_mm:.4f} mm")
    else:
        print("Không thể tính khoảng cách vì thiếu điểm pt1 tại fraction 0.2 hoặc 0.8")

    # Lưu ảnh opening_colored với các vòng tròn vào final_img.jpg
    cv2.imwrite("final_img.jpg", opening_colored)

    return parallel_lines_lengths_mm

def update_threshold(new_value):
    global threshold_value
    threshold_value = float(new_value)
    entry_threshold.delete(0, tk.END)
    entry_threshold.insert(0, threshold_value)

# Khởi tạo camera
camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
camera.StartGrabbing(pylon.GrabStrategy_LatestImageOnly)
camera.GainAuto.Value = "Off"
camera.ExposureAuto.Value = "Off"
camera.GammaSelector.Value = "User"
camera.Gamma.Value = 1.0
camera.ExposureTimeAbs.SetValue(100000)
converter = pylon.ImageFormatConverter()
converter.OutputPixelFormat = pylon.PixelType_BGR8packed

# GUI setup
root = tk.Tk()
root.title("Accuracy Measurement")

frame = tk.Frame(root)
frame.pack()

label_threshold = ttk.Label(frame, text="Threshold")
label_threshold.pack(side=tk.LEFT)
slider_threshold = ttk.Scale(frame, from_=0, to=255, orient=tk.HORIZONTAL, command=update_threshold)
slider_threshold.pack(side=tk.LEFT)
entry_threshold = tk.Entry(frame)
entry_threshold.insert(0, threshold_value)
entry_threshold.pack(side=tk.LEFT)

update_distance_button = tk.Button(root, text="Update", command=lambda: update_distance_and_ppm())
update_distance_button.pack()

canvas = tk.Canvas(root, width=955, height=960, bg="black")
canvas.pack()

while True:
    grab_result = camera.RetrieveResult(5000, pylon.TimeoutHandling_ThrowException)
    if grab_result.GrabSucceeded():
        img = converter.Convert(grab_result).GetArray()
        original_img = img.copy()
        resized_img = cv2.resize(img, (image_width, image_height), interpolation=cv2.INTER_CUBIC)

        # Apply Gaussian blur on BGR image (3x3 kernel to match C# SmoothGaussian(3))
        blur = cv2.GaussianBlur(resized_img, (3, 3), 0)

        # Convert to grayscale (after Gaussian blur to match C# order)
        gray = cv2.cvtColor(blur, cv2.COLOR_BGR2GRAY)

        # Apply inverse binary thresholding (threshold=100 to match C#)
        _, thresh = cv2.threshold(gray, threshold_value, 255, cv2.THRESH_BINARY_INV)

        # Define kernel for morphological operations (3x3 rectangular kernel)
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))

        # Apply dilation followed by erosion (equivalent to C# Dilate(1).Erode(1))
        dilated = cv2.dilate(thresh, kernel, iterations=1)
        opening = cv2.erode(dilated, kernel, iterations=1)

        cnts = cv2.findContours(opening, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        cnts = cnts[0] if len(cnts) == 2 else cnts[1]

        for c in cnts:
            print("Contour area:", cv2.contourArea(c))
            if cv2.contourArea(c) > 900:
                rect = cv2.minAreaRect(c)
                box = cv2.boxPoints(rect)
                box = np.int0(box)

                width_px = min(rect[1])
                height_px = max(rect[1])
                width_mm = width_px / ppm

                min_y = min(box[:, 1])
                max_y = max(box[:, 1])
                min_x = min(box[:, 0])
                max_x = max(box[:, 0])

                draw_parallel_lines(resized_img, min_x, min_y, max_x, max_y, contour=c, opening=opening)

        display_img = cv2.resize(resized_img, None, fx=0.3333, fy=0.3333, interpolation=cv2.INTER_CUBIC)
        display_img = cv2.resize(display_img, (960, 960)) if display_img.shape[1] > 960 or display_img.shape[0] > 960 else display_img

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