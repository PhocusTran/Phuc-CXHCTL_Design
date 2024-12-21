import os
from OCR_Test.Programs.eDOCr_master.tests.eDOCr import tools_ocr
import cv2
import string
from skimage import io
from pdf2image import convert_from_path
import numpy as np
import tkinter as tk
from tkinter import filedialog, ttk
from PIL import Image, ImageTk
import time
from scipy.spatial import distance as distan
from imutils import perspective, contours
import imutils
from shapely.geometry import LineString, Polygon, Point
import threading
import queue
from camera import basler_cam  # Import the camera class
import tensorflow as tf

physical_devices = tf.config.list_physical_devices('GPU')
if len(physical_devices) > 0:
    print("GPU is available, running with GPU")
    tf.config.experimental.set_memory_growth(physical_devices[0], True)
else:
    print("No GPU detected, running with CPU")

# create camera helper object
cam = basler_cam()

object_width_inches = (cam.sensor_width_inches * cam.distance_inches) / cam.focal_length_inches
ppi = cam.image_width / object_width_inches
threshold_value = cam.threshold_value
distance = cam.distance

img_dest_DIR = 'test_samples'
dest_DIR = 'test_results'

# Queue for transferring images between threads
image_queue = queue.Queue(maxsize=10)


def midpoint(ptA, ptB):
    return ((ptA[0] + ptB[0]) * 0.5, (ptA[1] + ptB[1]) * 0.5)

def process_pdf(pdf_path):
    filename = os.path.splitext(os.path.basename(pdf_path))[0]

    try:
        img = convert_from_path(pdf_path,
                                poppler_path='E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\Release-24.07.0-0\\poppler-24.07.0\\Library\\bin')
        img = np.array(img[0])
        jpg_path = os.path.join(img_dest_DIR, filename + '.jpg')
        io.imsave(jpg_path, img)
        return jpg_path, filename
    except Exception as e:
        print(f"Error converting PDF: {e}")
        return None, None


def display_image_and_results(image_path):
    global image_label, results_text
    try:
        img = Image.open(image_path)
        img.thumbnail((300, 300))
        photo = ImageTk.PhotoImage(img)
        image_label.config(image=photo, compound=tk.CENTER)
        image_label.image = photo

    except Exception as e:
        print(f"Error displaying image: {e}")


def display_results(results, mask_img_path):
    display_image_and_results(mask_img_path)
    results_text.delete("1.0", tk.END)
    results_text.insert(tk.END, results)


def update_loading_bar(percent):
    global loading_bar, loading_label
    loading_bar["value"] = percent
    loading_label.config(text=f"{percent}%")
    root.update_idletasks()


def ocr_process(image, filename):
    global loading_bar, loading_label
    total_steps = 7
    current_step = 0

    # Xóa nội dung của results_text
    results_text.delete("1.0", tk.END)
    # display_image_and_results(jpg_path)

    # Hiển thị loading_bar và loading_label
    loading_bar.grid()
    loading_label.grid()

    loading_label.config(text="0%")
    update_loading_bar(0)

    # img = cv2.imread(jpg_path)
    img = image
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

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
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    boxes_infoblock, gdt_boxes, cl_frame, process_img = tools_ocr.img_process.process_rect(class_list, img)
    io.imsave(os.path.join(dest_DIR, filename + '_process.jpg'), process_img)
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    infoblock_dict = tools_ocr.pipeline_infoblock.read_infoblocks(boxes_infoblock, img, alphabet_infoblock,
                                                                  model_infoblock)
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    gdt_dict = tools_ocr.pipeline_gdts.read_gdtbox1(gdt_boxes, alphabet_gdts, model_gdts, alphabet_dimensions,
                                                    model_dimensions)
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    process_img = os.path.join(dest_DIR, filename + '_process.jpg')
    dimension_dict = tools_ocr.pipeline_dimensions.read_dimensions(process_img, alphabet_dimensions, model_dimensions,
                                                                   cluster_t)
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    mask_img = tools_ocr.output.mask_the_drawing(img, infoblock_dict, gdt_dict, dimension_dict, cl_frame, color_palette)
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    io.imsave(os.path.join(dest_DIR, filename + '_boxes.jpg'), img_boxes)
    io.imsave(os.path.join(dest_DIR, filename + '_mask.jpg'), mask_img)
    tools_ocr.output.record_data(dest_DIR, filename, infoblock_dict, gdt_dict, dimension_dict)

    results = ""
    for item in dimension_dict:
        pred_data = item.get('pred')
        if pred_data:
            id_val = pred_data.get('ID')
            nominal_val = pred_data.get('nominal')
            value_val = pred_data.get('value')
            tolerance_val = pred_data.get('tolerance')
            upper_bound_val = pred_data.get('upper_bound')
            lower_bound_val = pred_data.get('lower_bound')

            print(f"ID: {id_val}, Nominal: {nominal_val}, Value: {value_val}, Tolerance: {tolerance_val}")

            results += f"ID: {id_val}, Nominal: {nominal_val}, Value: {value_val}, Tolerance: {tolerance_val}\n"

    mask_img_path = os.path.join(dest_DIR, filename + '_mask.jpg')
    display_results(results, mask_img_path)

    current_step += 1
    update_loading_bar(100)

    # Ẩn loading_bar và loading_label sau khi hoàn thành
    loading_bar.grid_remove()
    loading_label.grid_remove()


def browse_pdf():
    file_path = filedialog.askopenfilename(filetypes=[("PDF Files", "*.pdf")])
    if file_path:
        jpg_path, filename = process_pdf(file_path)
        if jpg_path and filename:
            # Load image using cv2 to get the correct numpy array
            img = cv2.imread(jpg_path)
            ocr_thread = threading.Thread(target=ocr_process, args=(img, filename))
            ocr_thread.start()
            # ocr_process(jpg_path, filename)
            # loading_label.grid_remove()


def save_settings():
    global threshold_value, distance
    try:
        new_threshold = float(entry_threshold.get())
        new_distance = float(entry_distance.get())

        # Update global variables only after successful conversion
        threshold_value = new_threshold
        distance = new_distance

        # Update the slider to the new value
        slider_threshold.set(new_threshold)
        slider_distance.set(new_distance)

        # Update calculations that depend on distance
        update_distance(new_distance)

        print(f"Settings saved - Threshold: {threshold_value}, Distance: {distance}")
    except ValueError:
        print("Invalid input for threshold or distance. Please enter numeric values.")


def camera_process():
    while True:
        img, isSuccess = cam.read_cam()
        if isSuccess:
            try:
                image_queue.put(img, block=False)  # Push image to queue
            except queue.Full:
                pass  # ignore if queue is full


def display_camera_feed():
    while True:
        try:
            img = image_queue.get(block=True, timeout=0.1)  # get image from queue
            # Process and display image
            anh_copy = img.copy()
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            blur = cv2.GaussianBlur(gray, (3, 3), 0)
            thresh = cv2.threshold(blur, threshold_value, 255, cv2.THRESH_BINARY)[1]  # Sử dụng ngưỡng mới
            kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
            opening = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)
            edged = cv2.Canny(opening, 50, int(threshold_value))

            cnts = cv2.findContours(edged.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            cnts = imutils.grab_contours(cnts)

            if len(cnts) > 0:
                cnt = max(cnts, key=cv2.contourArea)
                cnts = contours.sort_contours(cnts)[0]
                cv2.drawContours(anh_copy, cnts, -1, (0, 255, 0), 20)

                if cv2.contourArea(cnt) < 17:
                    continue

                hinh_hop = cv2.minAreaRect(cnt)
                hinh_hop = cv2.boxPoints(hinh_hop) if imutils.is_cv2() else cv2.boxPoints(hinh_hop)
                hinh_hop = np.array(hinh_hop, dtype="int")
                hinh_hop = perspective.order_points(hinh_hop)

                cv2.drawContours(anh_copy, [hinh_hop.astype("int")], -1, (0, 255, 0), 2)

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

                height_pixels = distan.euclidean((tltrX, tltrY), (blbrX, blbrY))  # Correct usage
                width_pixels = distan.euclidean((tlblX, tlblY), (trbrX, trbrY))  # Correct usage

                line_data = []  # clear data in every frame
                if height_pixels > width_pixels:
                    draw_parallel_lines(anh_copy, int(tltrX), int(tltrY), int(blbrX), int(blbrY),
                                        fractions=[0.03, 0.2, 0.5, 0.8], color=(255, 100, 0),
                                        contour=cnt)  # Blue parallel lines
                elif width_pixels >= height_pixels:
                    draw_parallel_lines(anh_copy, int(tltrX), int(tltrY), int(blbrX), int(blbrY),
                                        fractions=[0.03, 0.2, 0.5, 0.8], color=(255, 100, 0),
                                        contour=cnt)  # Blue parallel lines

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
        except queue.Empty:
            pass
        except Exception as e:
            print(f"Error displaying camera feed: {e}")
            break

        if cv2.waitKey(1) == 27:
            break

    cam.stop()
    cv2.destroyAllWindows()


# Tkinter setup
root = tk.Tk()
root.title("Camera App")

# Left frame for OCR and PDF import
left_frame = tk.Frame(root)
left_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

# Button for importing PDF
browse_button = tk.Button(left_frame, text="Import PDF", command=browse_pdf, width=20)
browse_button.grid(row=0, column=0, pady=(10, 0), sticky="nsew")

# Label for imported image
image_label = tk.Label(left_frame)
image_label.grid(row=1, column=0, sticky="nsew", pady=10)

# Text box for OCR results
results_text = tk.Text(left_frame, height=10)
results_text.grid(row=2, column=0, sticky="nsew", pady=10)

# Loading bar and label
loading_bar = ttk.Progressbar(left_frame, mode='determinate', length=200, maximum=100)
loading_bar.grid(row=3, column=0, pady=(0, 10), sticky="ew")
loading_bar.grid_remove()  # Hide initially

loading_label = tk.Label(left_frame, text="")
loading_label.grid(row=4, column=0, sticky="ew")
loading_label.grid_remove()  # Hide initially

# Right frame for camera controls
right_frame = tk.Frame(root)
right_frame.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")

# Create frame for slider and entry
settings_frame = tk.Frame(right_frame)
settings_frame.grid(row=0, column=0, sticky="ew")

# Create frame for threshold slider
frame_threshold = tk.Frame(settings_frame)
frame_threshold.grid(row=0, column=0, sticky="ew")

# Create label for threshold slider
label_threshold = ttk.Label(frame_threshold, text="Threshold")
label_threshold.pack(side=tk.LEFT)

# Create entry for threshold
entry_threshold = tk.Entry(frame_threshold)
entry_threshold.insert(0, threshold_value)
entry_threshold.pack(side=tk.LEFT)

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
    object_width_inches = (cam.sensor_width_inches * distance_inches) / \
                          cam.focal_length_inches

    ppi = cam.image_width / object_width_inches
    # Update the entry field
    entry_distance.delete(0, tk.END)
    entry_distance.insert(0, distance)

# Create slider for threshold
slider_threshold = ttk.Scale(frame_threshold, from_=0, to=255, orient=tk.HORIZONTAL, command=update_threshold)
slider_threshold.pack(side=tk.LEFT)
slider_threshold.set(threshold_value)

# Create frame for distance slider
frame_distance = tk.Frame(settings_frame)  # Use the same frame as threshold
frame_distance.grid(row=1, column=0, sticky="ew")

# Create label for distance slider
label_distance = ttk.Label(frame_distance, text="Distance (mm)")
label_distance.pack(side=tk.LEFT)

# Create slider for distance (adjust range as needed)
slider_distance = ttk.Scale(frame_distance, from_=100, to=300, orient=tk.HORIZONTAL, command=update_distance,
                            variable=tk.DoubleVar(value=distance))  # Initialize with current distance
slider_distance.pack(side=tk.LEFT)

# Create entry for distance
entry_distance = tk.Entry(frame_distance)
entry_distance.insert(0, distance)
entry_distance.pack(side=tk.LEFT)

# Button to update distance from entry field (Removed)
# update_distance_button = tk.Button(settings_frame, text="Cập nhật Distance",
#                                 command=lambda: update_distance(entry_distance.get()))
# update_distance_button.grid(row=2, column=0, sticky="ew", pady=10)
# Button to Save Settings
save_settings_button = tk.Button(settings_frame, text="Save settings", command=save_settings)
save_settings_button.grid(row=2, column=0, sticky="ew", pady=10)

# Entry field for Line ID input
label_line_id = ttk.Label(settings_frame, text="Nhập ID đoạn thẳng:")
label_line_id.grid(row=3, column=0, sticky="ew", pady=10)
entry_line_id = tk.Entry(settings_frame)
entry_line_id.grid(row=4, column=0, sticky="ew", pady=10)

# Label to display selected line value
label_selected_line_value = ttk.Label(settings_frame, text="Giá trị:")
label_selected_line_value.grid(row=5, column=0, sticky="ew", pady=10)
output_selected_line_value = tk.Label(settings_frame, text="")  # label to display selected line length
output_selected_line_value.grid(row=6, column=0, sticky="ew", pady=10)

# Canvas for camera display
canvas = tk.Canvas(right_frame, width=720, height=480)  # Điều chỉnh kích thước canvas
canvas.grid(row=7, column=0, sticky="nsew", pady=10)

# Adjust layout
root.grid_columnconfigure(0, weight=1)
root.grid_columnconfigure(1, weight=1)
root.grid_rowconfigure(0, weight=1)


def find_closest_point_on_contour(x, y, contour):
    min_distance = float('inf')
    closest_point = None
    for point in contour.reshape(-1, 2):
        distance = distan.euclidean((x, y), point)
        if distance < min_distance:
            min_distance = distance
            closest_point = point
    if closest_point is not None:
        return tuple(closest_point)
    else:
        return None


line_data = []  # List to store data about the lines
selected_line_id = None  # store the selected line id


def draw_parallel_lines(image, x1, y1, x2, y2, fractions=[0.05, 0.2, 0.5, 0.8], color=(255, 0, 0), thickness=15,
                        contour=None):
    if contour is None:
        raise ValueError("Contour must be provided")

    dx = x2 - x1
    dy = y2 - y1

    normal_dx = -dy
    normal_dy = dx

    length = np.sqrt(dx ** 2 + dy ** 2)
    parallel_lines_lengths_mm = []
    # print("Fractions:", fractions)  # In danh sách fractions
    for fraction_idx, fraction in enumerate(fractions):  # Iterate through fractions
        px = int(x1 + fraction * (x2 - x1))  # Calculate position based on fraction
        py = int(y1 + fraction * (y2 - y1))

        # Extend the perpendicular line to both sides to ensure intersection
        perp_x1 = int(px + 2000 * normal_dx / length)
        perp_y1 = int(py + 2000 * normal_dy / length)
        perp_x2 = int(px - 2000 * normal_dx / length)
        perp_y2 = int(py - 2000 * normal_dy / length)

        perp_line = LineString([(perp_x1, perp_y1), (perp_x2, perp_y2)])
        contour_polygon = Polygon(contour.reshape(-1, 2))

        intersection_points = perp_line.intersection(contour_polygon)
        # print(f"geom_type: {intersection_points.geom_type}")  # In kiểu hình học giao điểm

        coords = []  # Tạo danh sách để chứa tọa độ
        if intersection_points.geom_type == 'MultiLineString':
            # Sort linestrings based on their distance from (px, py)
            linestrings = sorted(intersection_points.geoms, key=lambda ls: ls.centroid.distance(Point(px, py)))
            if linestrings:
                coords = list(linestrings[0].coords)
        elif intersection_points.geom_type == 'LineString':
            coords = list(intersection_points.coords)
        elif intersection_points.geom_type == 'MultiPoint':
            points = list(intersection_points.geoms)
            if len(points) >= 2:
                points = sorted(points, key=lambda p: p.distance(Point(px, py)))
                coords = [points[0].coords[0], points[1].coords[0]]
        elif intersection_points.geom_type == 'Point':
            closest_point = find_closest_point_on_contour(px, py, contour)
            if closest_point:
                coords = [intersection_points.coords[0], closest_point]
        # print(f"Coords: {coords}")  # In tọa độ giao điểm
        if len(coords) == 2:
            line_length_px = distan.euclidean(coords[0], coords[-1])

            line_length_mm = (line_length_px * 25.4) / ppi

            parallel_lines_lengths_mm.append(line_length_mm)

            # Assign an ID to the line based on the fraction index
            line_id = f"{fraction_idx}"

            mid_x = int((coords[0][0] + coords[-1][0]) / 2)
            mid_y = int((coords[0][1] + coords[-1][1]) / 2)

            line_data.append({
                "id": line_id,
                "start": coords[0],
                "end": coords[1],
                "length_mm": line_length_mm,
                "midpoint": (mid_x, mid_y)
            })

            line_color = color
            line_thickness = thickness
            if line_id == selected_line_id:
                line_color = (0, 255, 255)  # Highlight color
                line_thickness = thickness + 5

            cv2.line(image, (int(coords[0][0]), int(coords[0][1])), (int(coords[-1][0]), int(coords[-1][1])),
                     line_color,
                     line_thickness)

            cv2.putText(image, f"{line_id}: {line_length_mm:.4f}mm", (mid_x - 370, mid_y + 150),
                        cv2.FONT_HERSHEY_SIMPLEX, 5, (0, 0, 255), 20)  # Red color for ID

    # In danh sách các đoạn thẳng sau khi vẽ
    # print("Line Data:", line_data)


def update_selected_line(line_id):
    global selected_line_id
    selected_line_id = line_id
    # find line data from line_data list
    selected_line = next((line for line in line_data if line["id"] == line_id), None)

    if selected_line:
        output_selected_line_value.config(text=f"{selected_line['length_mm']:.4f} mm")
    else:
        output_selected_line_value.config(text="Không tìm thấy")


# Bind the enter key to the update_selected_line function
entry_line_id.bind("<Return>", lambda event: update_selected_line(entry_line_id.get()))

# Start camera thread
camera_thread = threading.Thread(target=camera_process, daemon=True)
camera_thread.start()

# Start the display camera thread
display_thread = threading.Thread(target=display_camera_feed, daemon=True)
display_thread.start()

root.mainloop()