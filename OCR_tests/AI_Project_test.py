import os
import sys
import cv2
import string
import numpy as np
from skimage import io
from pdf2image import convert_from_path

from PySide6.QtCore import QCoreApplication, QDate, QDateTime, QLocale, QMetaObject, QObject, QPoint, QRect, QSize, QTime, Qt
from PySide6.QtGui import QBrush, QColor, QConicalGradient, QCursor, QFont, QFontDatabase, QGradient, QIcon, QImage, QKeySequence, QLinearGradient, QPainter, QPalette, QPixmap, QRadialGradient, QTransform
from PySide6.QtWidgets import (QApplication, QDialog, QPushButton, QLabel, QVBoxLayout, QFileDialog, QProgressBar, QTextEdit, QFrame) # Import QFileDialog
from PySide6.QtCore import (QCoreApplication, Qt, QThread, Signal)

from material.plugins.search.config import pipeline
from eDOCr import tools_ocr

img_dest_DIR = 'test_samples'
dest_DIR = 'test_Results'

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


class OCRThread(QThread):
    finished = Signal(str, str)
    progress = Signal(int)

    def __init__(self, jpg_path, filename, parent=None):
        super().__init__(parent)
        self.jpg_path = jpg_path
        self.filename = filename

    def run(self):
        try:
            total_steps = 7
            current_step = 0

            img = cv2.imread(self.jpg_path)
            current_step += 1; self.progress.emit(int((current_step / total_steps) * 100))

            class_list, img_boxes = tools_ocr.box_tree.findrect(img)
            current_step += 1; self.progress.emit(int((current_step / total_steps) * 100))

            boxes_infoblock, gdt_boxes, cl_frame, process_img = tools_ocr.img_process.process_rect(class_list, img)
            processed_img_path = os.path.join(dest_DIR, self.filename + '_process.jpg')
            io.imsave(processed_img_path, process_img)
            current_step += 1; self.progress.emit(int((current_step / total_steps) * 100))

            infoblock_dict = tools_ocr.pipeline_infoblock.read_infoblocks(boxes_infoblock, img, alphabet_infoblock, model_infoblock)
            current_step += 1; self.progress.emit(int((current_step / total_steps) * 100))

            gdt_dict = tools_ocr.pipeline_gdts.read_gdtbox1(gdt_boxes, alphabet_gdts, model_gdts, alphabet_dimensions, model_dimensions)
            current_step += 1; self.progress.emit(int((current_step / total_steps) * 100))

            dimension_dict = tools_ocr.pipeline_dimensions.read_dimensions(processed_img_path, alphabet_dimensions, model_dimensions, cluster_t)
            current_step += 1; self.progress.emit(int((current_step / total_steps) * 100))

            mask_img = tools_ocr.output.mask_the_drawing(img, infoblock_dict, gdt_dict, dimension_dict, cl_frame, color_palette)
            current_step += 1; self.progress.emit(int((current_step / total_steps) * 100))

            io.imsave(os.path.join(dest_DIR, self.filename + '_boxes.jpg'), img_boxes)
            mask_img_path = os.path.join(dest_DIR, self.filename + '_mask.jpg')
            io.imsave(mask_img_path, mask_img)

            tools_ocr.output.record_data(dest_DIR, self.filename, infoblock_dict, gdt_dict, dimension_dict)

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

                    # if id_val == 1:
                    #    value_num = float(value_val)
                    #    if value_num == 50.5 and tolerance_val == "±0.1":
                    #        print("ID 1, OK")

                    results += f"ID: {id_val}, Nominal: {nominal_val}, Value: {value_val}, Tolerance: {tolerance_val}\n"
            mask_img_path = os.path.join(dest_DIR, self.filename + '_mask.jpg')
            self.finished.emit(results, mask_img_path)


        except Exception as e:
            self.finished.emit(f"Error: {e}", "")


class Ui_Dialog(object):
    def setupUi(self, Dialog):
        if not Dialog.objectName():
            Dialog.setObjectName(u"Dialog")
        Dialog.resize(800, 600) # Adjust size as needed

        self.label_image_original = QLabel(Dialog)  # QLabel for original image
        self.label_image_original.setObjectName(u"label_image_original")
        self.label_image_masked = QLabel(Dialog)  # QLabel for masked image
        self.label_image_masked.setObjectName(u"label_image_masked")

        layout = QVBoxLayout(Dialog)
        layout.addWidget(self.pushButton)

        self.pushButton.setObjectName(u"pushButton")
        self.pushButton.setText("Import PDF")

        self.label_image = QLabel(Dialog)
        self.label_image.setObjectName(u"label_image")

        self.textEdit_results = QTextEdit(Dialog)
        self.textEdit_results.setObjectName(u"textEdit_results")


        self.progressBar = QProgressBar(Dialog)
        self.progressBar.setObjectName(u"progressBar")
        self.progressBar.setValue(0)

        # Create a layout (QVBoxLayout is usually a good choice)
        layout = QVBoxLayout(Dialog)  # Set Dialog as parent for the layout
        layout.addWidget(self.pushButton)
        layout.addWidget(self.label_image)
        layout.addWidget(self.textEdit_results)
        layout.addWidget(self.progressBar)

        self.retranslateUi(Dialog)
        QMetaObject.connectSlotsByName(Dialog)

    # setupUi

    def retranslateUi(self, Dialog):
        Dialog.setWindowTitle(QCoreApplication.translate("Dialog", u"Dialog", None))
        self.pushButton.setText(QCoreApplication.translate("Dialog", u"Import PDF", None))
    # retranslateUi



class MyDialog(QDialog):
    def __init__(self):
        super().__init__()
        self.ui = Ui_Dialog()
        self.ui.setupUi(self)
        self.ui.pushButton.clicked.connect(self.import_pdf)

        self.ocr_thread = None


    def import_pdf(self):
        file_path, _ = QFileDialog.getOpenFileName(self, "Select PDF", "", "PDF Files (*.pdf)")
        if file_path:
            self.process_pdf(file_path)


    def process_pdf(self, file_path):
        jpg_path, filename = process_pdf_inner(file_path)
        if jpg_path and filename:
            self.display_image(jpg_path)  # Display initial image
            self.start_ocr_thread(jpg_path, filename)



    def start_ocr_thread(self, jpg_path, filename):
        if self.ocr_thread is not None and self.ocr_thread.isRunning():
            self.ocr_thread.terminate()  # Stop the previous thread if it's still running
            self.ocr_thread.wait()       # Wait for the thread to finish

        self.ocr_thread = OCRThread(jpg_path, filename)
        self.ocr_thread.finished.connect(self.ocr_finished)
        self.ocr_thread.progress.connect(self.ui.progressBar.setValue) # Directly connect to progress bar
        self.ocr_thread.start()



    def ocr_finished(self, results, mask_img_path):
        self.ui.textEdit_results.setPlainText(results)
        if mask_img_path:
            self.display_image(mask_img_path)


    def display_image(self, image_path):
        pixmap = QPixmap(image_path)
        scaled_pixmap = pixmap.scaled(self.ui.label_image.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation)
        self.ui.label_image.setPixmap(scaled_pixmap)


def process_pdf_inner(pdf_path):
    filename = os.path.splitext(os.path.basename(pdf_path))[0]
    try:
        img = convert_from_path(pdf_path, poppler_path='E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\Release-24.07.0-0\\poppler-24.07.0\\Library\\bin')  # Replace with your poppler path
        img = np.array(img[0])
        jpg_path = os.path.join(img_dest_DIR, filename + '.jpg')
        io.imsave(jpg_path, img)
        return jpg_path, filename
    except Exception as e:
        print(f"Error converting PDF: {e}")
        return None, None

if __name__ == "__main__":
    app = QApplication(sys.argv)
    dialog = MyDialog()
    dialog.show()
    sys.exit(app.exec())