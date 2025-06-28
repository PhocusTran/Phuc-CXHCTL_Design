from webapp.main import FlaskApp, TableGenerator
from UnifiedUtils import process_PDF, ocr_process, process
from virtual_keyboard import virtual_keyboard_FC_O_t, send_command, get_esp_ip
import threading
import cv2
import signal
import sys

# all the neccesary stuff from basler CAM will be stored here
class CAM_data:
    def __init__(self):
        self.image = None  # Initialize image to None
        self.list = []

    def load_image(self, img):
        # Method to load an image into the class
        self.image = img

    def simplifyList(self):
        # Only keep length value
        if len(self.list) > 0:
            self.simplified_data = ["{:.2f}".format(entry['length_mm'])
                                    for entry in self.list]

class PDF_data:
    list = None
    name = None

    def __init__(self) -> None:
        self.list = []
        self.name = ''


class ESP_Handler:
    def __init__(self, stop_event: threading.Event, cnc_controller):
        self.esp_ip = ''
        self.cnc_controller = cnc_controller

        t2 = threading.Thread(target=self.get_ip, args=(stop_event,))
        t2.daemon = True
        t2.start()

    def get_ip(self, stop_event):  # Accept stop_event as a parameter
        while not stop_event.is_set() and self.esp_ip == '':
            self.esp_ip = get_esp_ip()
        exit()

    def finetune(self, data):
        self.cnc_controller.send_offset(value=data[0][5])

    def keyboard(self, data):
        send_command(self.esp_ip, data)

    def send_gcode(self, data):
        lines = str.split(data, '\n')
        for line in lines:
            if len(line) < 1:
                continue
            self.cnc_controller.send_gcode_commands(line)
        return


class Cam_Handler:
    def __init__(self, stop_event: threading.Event):
        self.cam_data = CAM_data()
        self.stop_event = stop_event
        t1 = threading.Thread(target=self.get_cam, args=())
        t1.daemon = True
        t1.start()

    def get_cam(self):  # Accept stop_event as a parameter
        import meaureUtils
        import imutils

        while not self.stop_event.is_set():  # Loop until the stop event is set
            img, isSuccess = meaureUtils.camera_process()
            if isSuccess:
                img, self.cam_data.list = meaureUtils.process_camera_feed(
                    img, meaureUtils.threshold_value, meaureUtils.ppi, live_line=[])
                self.cam_data.image = imutils.resize(img, width=480)
                cv2.imshow('hello', self.cam_data.image)
                key = cv2.waitKey(1)
                if key == ord('q'):
                    self.stop_event.set()  # Break the loop when 'q' is pressed
                    break
        exit(0)


class CNC_Auto_Finetune_App:
    def __init__(self):
        self.pdf_data = PDF_data()

        self.stop_event = threading.Event()
        self.cam_handler = Cam_Handler(self.stop_event)

        self.app = FlaskApp(self.cam_handler.cam_data)
        self.esp = ESP_Handler(self.stop_event, virtual_keyboard_FC_O_t())
        self.Web_Event_Handling_Register()

    def run(self):
        # thread interrupt
        def signal_handler(signal, frame):
            _ = signal
            _ = frame
            print('Interrupt received, shutting down...')
            self.stop_event.set()
            sys.exit(0)

        signal.signal(signal.SIGINT, signal_handler)

        # Handle keyboard interrupt
        self.app.run(debug=False)

    def Web_Event_Handling_Register(self):

        def update_processbar(value):
            print(value)
            self.app.socketio.emit('processbar/value', value)
            pass

        def PDF_uploaded(data):
            """ Fired after user confirm the pdf file"""

            # Start process bar
            self.app.socketio.emit('processbar/enable', 'it turn me on')

            # Start analyze pdf file
            ocr_after, self.pdf_data.name = process_PDF(data)
            print("name", self.pdf_data.name)

            pro = process()
            pro.update += update_processbar
            self.pdf_data.list = ocr_process(
                ocr_after, self.pdf_data.name, pro)
            print("list", self.pdf_data.list)

            # Analyzed result will be saved into 'uploads' folder
            image = cv2.imread(f"test_results//{self.pdf_data.name}_mask.jpg")

            # display result
            self.app.send_matlike(image, 'result1')

        def return_ocr_result(data):
            _ = data
            image = cv2.imread(f"test_results//{self.pdf_data.name}_mask.jpg")
            self.app.send_matlike(image, 'result1')

        def return_table():
            i = 0
            _table = TableGenerator.sample_table(pdf_table=self.pdf_data.list)
            print(_table)
            for row in _table:
                self.app.add_row({'id': i, 'columns': row})
                i += 1

        def apply_measure():
            self.cam_handler.cam_data.simplifyList()
            changes = TableGenerator.add_measure_2_table(pdf_table=self.pdf_data.list,
                                                         cam_table=self.cam_handler.cam_data.simplified_data,
                                                         checkedIDS=self.app.checkIDs)

            for change in changes:
                self.app.modify_fifth_cell(
                    row_id=change[0], new_value=change[1])

        self.app.pdf_uploaded += PDF_uploaded
        self.app.image_requested += return_ocr_result
        self.app.table_requested += return_table
        self.app.table_checkbox_checked += apply_measure
        self.app.finetune_request += self.esp.finetune
        self.app.keyboard_requested += self.esp.keyboard
        self.app.send_gcode += self.esp.send_gcode


if __name__ == "__main__":
    cnc_app = CNC_Auto_Finetune_App()
    cnc_app.run()
