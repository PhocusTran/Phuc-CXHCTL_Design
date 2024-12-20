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

def midpoint(ptA, ptB):
	return ((ptA[0] + ptB[0]) * 0.5, (ptA[1] + ptB[1]) * 0.5)

# Camera parameters (adjust these)
focal_length = 25  # mm
sensor_width = 13.13  # mm
image_width = 5472 # pixels for canvas display
distance = 224.85 # mm - Calibrate this accurately!
focal_length_inches = focal_length / 25.4
sensor_width_inches = sensor_width / 25.4
distance_inches = distance / 25.4
threshold_value = 170  # Threshold

object_width_inches = (sensor_width_inches * distance_inches) / focal_length_inches

ppi = image_width / object_width_inches

# Camera initialization
camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
camera.StartGrabbing(pylon.GrabStrategy_LatestImageOnly)
camera.GainAuto.Value = "Off"
camera.ExposureAuto.Value = "Off"
camera.GammaSelector.Value = "User"
camera.Gamma.Value = 1.0
camera.ExposureTimeAbs.SetValue(240000)
camera.GainRaw.SetValue(0)

converter = pylon.ImageFormatConverter()
converter.OutputPixelFormat = pylon.PixelType_BGR8packed

try:
    with np.load('calibration_params_5472.npz') as data:
        mtx = data['mtx']
        dist = data['dist']
        print("Calibration parameters loaded successfully.")
except FileNotFoundError:
    print("Calibration file not found. Using default parameters.")
    mtx = None  # Or set a default camera matrix if needed
    dist = np.zeros((4, 1))  # Or set default distortion coefficients

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
slider_distance = ttk.Scale(frame_distance, from_=100, to=300, orient=tk.HORIZONTAL, command=update_distance, variable=tk.DoubleVar(value=distance))  # Initialize with current distance
slider_distance.pack(side=tk.LEFT)

# Tạo ô nhập liệu Distance
entry_distance = tk.Entry(frame_distance)
entry_distance.insert(0, distance)
entry_distance.pack(side=tk.LEFT)

# Button to update distance from entry field
update_distance_button = tk.Button(root, text="Cập nhật Distance", command=lambda: update_distance(entry_distance.get()))
update_distance_button.pack()

canvas = tk.Canvas(root, width=1080, height=718)
canvas.pack()

while True:
	frame = camera.RetrieveResult(5000, pylon.TimeoutHandling_ThrowException)
	if frame.GrabSucceeded():
		img = converter.Convert(frame).GetArray()

		if mtx is not None and dist is not None:  # Apply undistortion if calibration parameters are loaded
			h, w = img.shape[:2]
			newcameramtx, roi = cv2.getOptimalNewCameraMatrix(mtx, dist, (w, h), 1, (w, h))
			img = cv2.undistort(img, mtx, dist, None, newcameramtx)
			x, y, w, h = roi
			img = img[y:y + h, x:x + w]  # Crop the image according to the ROI. Avoids black areas after undistortion and keeps the correct aspect ratio.

		gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
		blur = cv2.GaussianBlur(gray, (9, 9), 0)
		thresh = cv2.threshold(blur, threshold_value, 255, cv2.THRESH_BINARY)[1]  # Sử dụng ngưỡng mới
		kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
		opening = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)
		edged = cv2.Canny(opening, 50, int(threshold_value))
		cv2.imshow("Edged", edged)
		cnts = cv2.findContours(edged.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
		cnts = imutils.grab_contours(cnts)

		anh_copy = img.copy()

		if len(cnts) > 0:
			cnts = contours.sort_contours(cnts)[0]

		for c in cnts:
			if cv2.contourArea(c) < 100: # try using smaller filter to capture more objects
				continue

			hinh_hop = cv2.minAreaRect(c)
			hinh_hop = cv2.cv.boxPoints(hinh_hop) if imutils.is_cv2() else cv2.boxPoints(hinh_hop)
			hinh_hop = np.array(hinh_hop, dtype="int")
			hinh_hop = perspective.order_points(hinh_hop)
			cv2.drawContours(anh_copy, [hinh_hop.astype("int")], -1, (0, 255, 0), 15)

			for (x, y) in hinh_hop:
				cv2.circle(anh_copy, (int(x), int(y)), 5, (0, 0, 255), -1)

			(tl, tr, br, bl) = hinh_hop
			(tltrX, tltrY) = midpoint(tl, tr)
			(blbrX, blbrY) = midpoint(bl, br)

			(tlblX, tlblY) = midpoint(tl, bl)
			(trbrX, trbrY) = midpoint(tr, br)

			cv2.circle(anh_copy, (int(tltrX), int(tltrY)), 5, (255, 0, 0), -1)
			cv2.circle(anh_copy, (int(blbrX), int(blbrY)), 5, (255, 0, 0), -1)
			cv2.circle(anh_copy, (int(tlblX), int(tlblY)), 5, (255, 0, 0), -1)
			cv2.circle(anh_copy, (int(trbrX), int(trbrY)), 5, (255, 0, 0), -1)

			chieu_doc_pixels = distan.euclidean((tltrX, tltrY), (blbrX, blbrY))  # Correct usage
			chieu_ngang_pixels = distan.euclidean((tlblX, tlblY), (trbrX, trbrY))  # Correct usage

			if chieu_doc_pixels > chieu_ngang_pixels:
				cv2.line(anh_copy, (int(tltrX), int(tltrY)), (int(blbrX), int(blbrY)), (0, 215, 255), 15)
				cv2.line(anh_copy, (int(tlblX), int(tlblY)), (int(trbrX), int(trbrY)), (0, 0, 255), 15)

				chieu_rong_mm = (chieu_ngang_pixels * 25.4) / ppi
				chieu_dai_mm = (chieu_doc_pixels * 25.4) / ppi

				cv2.putText(anh_copy, "{:.4f}mm".format(chieu_dai_mm), (int(trbrX), int(trbrY - 150)),
							cv2.FONT_HERSHEY_SIMPLEX,
							5, (0, 215, 255), 15)
				cv2.putText(anh_copy, "{:.4f}mm".format(chieu_rong_mm), (int(tltrX - 15), int(tltrY - 10)),
							cv2.FONT_HERSHEY_SIMPLEX, 5, (0, 0, 255), 15)

			elif chieu_ngang_pixels >= chieu_doc_pixels:
				cv2.line(anh_copy, (int(tltrX), int(tltrY)), (int(blbrX), int(blbrY)), (0, 0, 255), 15)
				cv2.line(anh_copy, (int(tlblX), int(tlblY)), (int(trbrX), int(trbrY)), (0, 215, 255), 15)

				chieu_rong_mm = (chieu_doc_pixels * 25.4) / ppi
				chieu_dai_mm = (chieu_ngang_pixels * 25.4) / ppi

				cv2.putText(anh_copy, "{:.4f}mm".format(chieu_dai_mm), (int(tltrX - 150), int(tltrY - 100)),
							cv2.FONT_HERSHEY_SIMPLEX, 5, (0, 215, 255), 15)
				cv2.putText(anh_copy, "{:.4f}mm".format(chieu_rong_mm), (int(trbrX - 3300), int(trbrY)),
							cv2.FONT_HERSHEY_SIMPLEX, 5, (0, 0, 255), 15)

			print("Chiều rộng: ", chieu_rong_mm)
			print("Chiều dài: ", chieu_dai_mm)

		anh_copy = imutils.resize(anh_copy, width=720)
		photo = tk.PhotoImage(data=cv2.imencode('.ppm', anh_copy)[1].tobytes())
		canvas.create_image(0, 0, image=photo, anchor=tk.NW)
		canvas.image = photo
		root.update()

		if len(cnts) == 0:
			anh_copy = imutils.resize(anh_copy, width=720)
			photo = tk.PhotoImage(data=cv2.imencode('.ppm', anh_copy)[1].tobytes())
			canvas.create_image(0, 0, image=photo, anchor=tk.NW)
			canvas.image = photo
			root.update()

	if cv2.waitKey(1) == 27:
		break

camera.StopGrabbing()
camera.Close()
cv2.destroyAllWindows()
root.mainloop()