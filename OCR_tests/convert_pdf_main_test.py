import os
from eDOCr import tools_ocr
import cv2
import string
from skimage import io
from pdf2image import convert_from_path
import numpy as np
import tkinter as tk
from tkinter import filedialog, ttk, messagebox
from PIL import Image, ImageTk
import time

img_dest_DIR = 'test_samples'
dest_DIR = 'test_Results'
jpg_path = ""

def process_pdf(pdf_path):
    filename = os.path.splitext(os.path.basename(pdf_path))[0]

    try:
        img = convert_from_path(pdf_path, poppler_path='E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\Release-24.07.0-0\\poppler-24.07.0\\Library\\bin')
        img = np.array(img[0])
        jpg_path = os.path.join(img_dest_DIR, filename + '.jpg')
        io.imsave(jpg_path, img)
        return jpg_path, filename
    except Exception as e:
        print(f"Error converting PDF: {e}")
        return None, None

global roi_image_path  # Biến toàn cục lưu đường dẫn ảnh ROI
def roi_selection(image_path):
    global roi_image_path
    try:
        img = cv2.imread(image_path)
        img = cv2.resize(img, (1920, 1080))
        roi = cv2.selectROI(windowName="Select ROI", img=img, showCrosshair=True, fromCenter=False)
        x, y, w, h = roi
        if w > 0 and h > 0:  # Kiểm tra xem đã chọn vùng ROI hay chưa
            roi_img = img[y:y+h, x:x+w]
            roi_image_path = os.path.join(dest_DIR, "roi_image.jpg")  # Lưu ảnh ROI
            cv2.imwrite(roi_image_path, roi_img)
            display_image_and_results(roi_image_path) #hiển thị ảnh ROI sau khi khoanh vùng
            messagebox.showinfo("ROI Selected", "ROI image saved successfully!")

            # Kích hoạt nút "Read" sau khi chọn ROI
            read_button.config(state=tk.NORMAL)
        cv2.destroyAllWindows() #đóng cửa sổ chọn ROI

    except Exception as e:
        print(f"Error selecting ROI: {e}")
        messagebox.showerror("Error", f"Error selecting ROI: {e}")

def ocr_process_roi():
    global roi_image_path  # Sử dụng biến toàn cục
    if roi_image_path:
      ocr_process(roi_image_path, "roi_image")  # Gọi ocr_process với ảnh ROI

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
    display_image_and_results(mask_img_path) # Bỏ tham số results
    results_text.delete("1.0", tk.END)  # Xóa nội dung cũ
    results_text.insert(tk.END, results) # Chỉ chèn results

def update_loading_bar(percent):
    global loading_bar, loading_label
    loading_bar["value"] = percent
    loading_label.config(text=f"{percent}%")
    root.update_idletasks()

def ocr_process(jpg_path, filename):
    global loading_bar, loading_label
    total_steps = 7
    current_step = 0

    # Xóa nội dung của results_text
    results_text.delete("1.0", tk.END)
    display_image_and_results(jpg_path)

    # Hiển thị loading_bar và loading_label
    loading_bar.grid()
    loading_label.grid()

    loading_label.config(text="0%")
    update_loading_bar(0)

    img = cv2.imread(jpg_path)
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    GDT_symbols = '⏤⏥○⌭⌒⌓⏊∠⫽⌯⌖◎↗⌰'
    FCF_symbols = 'ⒺⒻⓁⓂⓅⓈⓉⓊ'
    Extra = '(),.+-±:/°"⌀'

    alphabet_dimensions = string.digits + 'AaBCDRGHhMmnx' + Extra
    model_dimensions = 'eDOCr/keras_ocr_models/models/recognizer_dimensions.h5'
    alphabet_infoblock = string.digits + string.ascii_letters + ',.:-/'
    model_infoblock = 'eDOCr/keras_ocr_models/models/recognizer_infoblock.h5'
    alphabet_gdts = string.digits + ',.⌀ABCD' + GDT_symbols
    model_gdts = 'eDOCr/keras_ocr_models/models/recognizer_gdts.h5'

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

    infoblock_dict = tools_ocr.pipeline_infoblock.read_infoblocks(boxes_infoblock, img, alphabet_infoblock, model_infoblock)
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    gdt_dict = tools_ocr.pipeline_gdts.read_gdtbox1(gdt_boxes, alphabet_gdts, model_gdts, alphabet_dimensions, model_dimensions)
    current_step += 1
    update_loading_bar(int((current_step / total_steps) * 100))

    process_img = os.path.join(dest_DIR, filename + '_process.jpg')
    dimension_dict = tools_ocr.pipeline_dimensions.read_dimensions(process_img, alphabet_dimensions, model_dimensions, cluster_t)
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

            #if id_val == 1:
            #    value_num = float(value_val)
            #    if value_num == 50.5 and tolerance_val == "±0.1":
            #        print("ID 1, OK")

            results += f"ID: {id_val}, Nominal: {nominal_val}, Value: {value_val}, Tolerance: {tolerance_val}\n"

    mask_img_path = os.path.join(dest_DIR, filename + '_mask.jpg')
    display_results(results, mask_img_path)

    current_step += 1
    update_loading_bar(100)

    # Ẩn loading_bar và loading_label sau khi hoàn thành
    loading_bar.grid_remove()
    loading_label.grid_remove()

def browse_pdf():
    global jpg_path  # Sử dụng biến toàn cục jpg_path

    file_path = filedialog.askopenfilename(filetypes=[("PDF Files", "*.pdf")])
    if file_path:
        jpg_path, filename = process_pdf(file_path)
        if jpg_path and filename:
            # display_image_and_results(jpg_path)  # Hiển thị ảnh gốc trước
            roi_button.config(command=lambda path=jpg_path: roi_selection(path), state=tk.NORMAL)  # Kích hoạt nút ROI

# Tạo giao diện
root = tk.Tk()
root.title("PDF OCR")

browse_button = tk.Button(root, text="Import PDF", command=browse_pdf)
browse_button.grid(row=0, column=0, pady=(10, 0))  # Đặt button ở hàng 0, cột 0, padding top 10

roi_button = tk.Button(root, text="ROI Image", command=lambda path=jpg_path: roi_selection(path), state=tk.DISABLED)
roi_button.grid(row=0, column=1, pady=(10, 0))

read_button = tk.Button(root, text="Read", command=ocr_process_roi, state=tk.DISABLED) # Thêm nút Read, ban đầu bị vô hiệu hóa
read_button.grid(row=0, column=2, pady=(10, 0))

image_label = tk.Label(root)
image_label.grid(row=1, column=0, sticky="nsew") # Đặt image_label ở hàng 1, cột 0, sticky để co giãn theo cell

results_text = tk.Text(root)
results_text.grid(row=2, column=0, sticky="nsew") # Đặt result_text ở hàng 2, cột 0, sticky để co giãn

loading_bar = ttk.Progressbar(root, mode='determinate', length=200, maximum=100)
loading_bar.grid(row=3, column=0, pady=(0, 10))
loading_bar.grid_remove()  # Ẩn loading_bar ban đầu

loading_label = tk.Label(root, text="")
loading_label.grid(row=4, column=0)
loading_label.grid_remove() # Ẩn loading_label ban đầu

root.mainloop()