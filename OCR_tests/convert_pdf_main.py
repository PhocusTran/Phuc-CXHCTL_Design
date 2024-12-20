import os
from material.plugins.search.config import pipeline
from eDOCr import tools_ocr
import cv2
import string
from skimage import io
from pdf2image import convert_from_path
import os, numpy as np
from skimage import io

img_dest_DIR='test_samples'
file_path='E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\eDOCr-master\\eDOCr-master\\tests\\test_samples\\demo_wp_2.pdf'
filename=os.path.splitext(os.path.basename(file_path))[0]
img = convert_from_path(file_path, poppler_path='E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\Release-24.07.0-0\\poppler-24.07.0\\Library\\bin')
img = np.array(img[0])

io.imsave(os.path.join(img_dest_DIR, filename+'.jpg'),img)
dest_DIR = 'E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\eDOCr-master\\eDOCr-master\\tests\\test_Results'
file_path = 'E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\eDOCr-master\\eDOCr-master\\tests\\test_samples\\demo_wp_2.jpg'
filename = os.path.splitext(os.path.basename(file_path))[0]
img = cv2.imread(file_path)
# img = convert_from_path(file_path)
# img = np.array(img[0])
GDT_symbols = '⏤⏥○⌭⌒⌓⏊∠⫽⌯⌖◎↗⌰'
FCF_symbols = 'ⒺⒻⓁⓂⓅⓈⓉⓊ'
Extra = '(),.+-±:/°"⌀'

alphabet_dimensions = string.digits + 'AaBCDRGHhMmnx' + Extra
model_dimensions = 'eDOCr/keras_ocr_models/models/recognizer_dimensions.h5'
alphabet_infoblock = string.digits + string.ascii_letters + ',.:-/'
model_infoblock = 'eDOCr/keras_ocr_models/models/recognizer_infoblock.h5'
alphabet_gdts = string.digits + ',.⌀ABCD' + GDT_symbols  # +FCF_symbols
model_gdts = 'eDOCr/keras_ocr_models/models/recognizer_gdts.h5'

color_palette = {'infoblock': (180, 220, 250), 'gdts': (94, 204, 243), 'dimensions': (93, 206, 175),
                 'frame': (167, 234, 82), 'flag': (241, 65, 36)}
cluster_t = 20

class_list, img_boxes = tools_ocr.box_tree.findrect(img)
boxes_infoblock, gdt_boxes, cl_frame, process_img = tools_ocr.img_process.process_rect(class_list, img)
io.imsave(os.path.join(dest_DIR, filename + '_process.jpg'), process_img)

infoblock_dict = tools_ocr.pipeline_infoblock.read_infoblocks(boxes_infoblock, img, alphabet_infoblock, model_infoblock)
gdt_dict = tools_ocr.pipeline_gdts.read_gdtbox1(gdt_boxes, alphabet_gdts, model_gdts, alphabet_dimensions, model_dimensions)

process_img = os.path.join(dest_DIR, filename + '_process.jpg')

dimension_dict = tools_ocr.pipeline_dimensions.read_dimensions(process_img, alphabet_dimensions, model_dimensions, cluster_t)
mask_img = tools_ocr.output.mask_the_drawing(img, infoblock_dict, gdt_dict, dimension_dict, cl_frame, color_palette)

# Record the results
io.imsave(os.path.join(dest_DIR, filename + '_boxes.jpg'), img_boxes)
io.imsave(os.path.join(dest_DIR, filename + '_mask.jpg'), mask_img)

tools_ocr.output.record_data(dest_DIR, filename, infoblock_dict, gdt_dict, dimension_dict)

dimension_dict = tools_ocr.pipeline_dimensions.read_dimensions(process_img, alphabet_dimensions, model_dimensions, cluster_t)

# Lặp qua dimension_dict
for item in dimension_dict:
    pred_data = item['pred']  # Lấy dictionary chứa ID, nominal, value, tolerance
    id_val = pred_data['ID']
    nominal_val = pred_data.get('nominal', '')
    value_val = pred_data.get('value', '')
    tolerance_val = pred_data.get('tolerance', '')

    print(f"ID: {id_val}, Nominal: {nominal_val}, Value: {value_val}, Tolerance: {tolerance_val}")

    # So sánh giá trị
    if id_val == 1:
        if nominal_val == "50±0.1" and 49.9 <= float(value_val) <= 50.1 and tolerance_val == "±0.1":
            print("ID 1, OK!")
        else:
            print("ID 1, NG!")
    elif id_val == 2:
        try:
            if float(value_val) > 10:
                print("ID 2, Value lớn hơn 10!")
        except ValueError:
            print(f"ID {id_val}: Không thể chuyển đổi value '{value_val}' thành số.")