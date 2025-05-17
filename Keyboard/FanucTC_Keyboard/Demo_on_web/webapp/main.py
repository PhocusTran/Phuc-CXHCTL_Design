from flask import Flask, request, jsonify, render_template, Response, send_from_directory
from flask_socketio import SocketIO
import base64
import cv2
import numpy as np
import os

class TableGenerator:
    @staticmethod
    def add_measure_2_table(pdf_table, cam_table, checkedIDS=[]):
        change_table = []
        print(checkedIDS)

        pdf_table_number = [float(num[1]) for num in pdf_table]
        cam_table_number = [float(num) for num in cam_table]
        cam_table_number = list(reversed(cam_table_number))

        # print(f"pdf ${pdf_table_number}")
        # print(f"cam ${cam_table_number}")

        for i in range(len(checkedIDS)):
            try:
                if i > len(cam_table):
                    print('bruh')
                    break

                # if 2 values was too difference, continue checking
                print("i = ", i)
                j = 0
                print(cam_table_number[j] - pdf_table_number[checkedIDS[i]])
                while cam_table_number[j] - pdf_table_number[checkedIDS[i]] > 1.5 or cam_table_number[j] - pdf_table_number[checkedIDS[i]] < 0:
                    print(f"i = {i} | j = {j}")
                    j += 1

                change_table.append([checkedIDS[i], cam_table_number[j]])
                cam_table_number.pop(j)
            except Exception as e:
                print(f"Sorting Table, error: {e}")

        print("changes about to apply", change_table)
        return change_table

    @staticmethod
    def sample_table(pdf_table):
        _table = pdf_table
        for row in _table:
            while len(row) < 5:
                row.append('Null')
        return _table

    @staticmethod
    def find_closest_index(numbers, target):
        # Convert string numbers to integers
        _numbers = [float(num) for num in numbers]

        target = float(target)
        
        # Calculate the absolute difference between each number and the target
        differences = [abs(num - target) for num in _numbers]
        
        # Find the index of the smallest difference
        closest_index = differences.index(min(differences))
        closest_value = _numbers[closest_index]

        numbers.pop(closest_index)
        
        return closest_value

class Event:
    def __init__(self):
        self._handlers = []

    def __iadd__(self, handler):
        """Add a handler using the += operator."""
        self._handlers.append(handler)
        return self

    def __isub__(self, handler):
        """Remove a handler using the -= operator."""
        self._handlers.remove(handler)
        return self

    def __call__(self, *args, **kwargs):
        """Invoke all handlers when the event is called."""
        for handler in self._handlers:
            handler(*args, **kwargs)


class FlaskApp:
    app = Flask(__name__, template_folder="src", static_folder="src")
    # websocket
    socketio = SocketIO(app)
    table_count = 0
    checkIDs = []

    # parent folder
    directory = os.path.dirname(os.path.relpath(__file__, '.'))

    # handle updload event
    pdf_uploaded = Event()
    image_requested = Event()
    table_requested = Event()
    table_checkbox_checked = Event()
    finetune_request = Event()
    keyboard_requested = Event()
    send_gcode = Event()

    def __init__(self, _camRef):
        self.app.config['UPLOAD_FOLDER'] = os.path.join(self.directory, 'uploads')
        self.app.config['SECRET_KEY'] = 'your_secret_key'

        self.app.config['PORT'] = 5000

        # Register routes
        self.register_routes()

        self.register_websocket_events()

        # video source ref
        self.camRef = _camRef

        # Remove all remaining files in 'uploads' from the last session
        self.cleanup_upload_folder()

    def cleanup_upload_folder(self):
        """Remove all files in the uploads folder."""
        folder_path = self.app.config['UPLOAD_FOLDER']

        # Check if the uploads folder exists
        if os.path.exists(folder_path):
            for filename in os.listdir(folder_path):
                file_path = os.path.join(folder_path, filename)

                # Only remove files, not subdirectories
                if os.path.isfile(file_path):
                    os.remove(file_path)
                    print(f"Removed: {file_path}")

    def upload_file(self):
        """Save PDF file into uploads directoty."""
        if 'pdf' not in request.files:
            return jsonify({'error': 'No file uploaded'}), 400

        file = request.files['pdf']
        if file.filename == '':
            return jsonify({'error': 'No selected file'}), 400

        if not file.filename.endswith('.pdf'):
            return jsonify({'error': 'Invalid file type'}), 400

        file_path = os.path.join(self.app.config['UPLOAD_FOLDER'], file.filename)
        
        print(file_path)

        file.save(file_path)

        # Emit upload event
        self.pdf_uploaded(file_path)

        return jsonify({'message': 'File processed', 'result': file_path})

    def serve_file(self, filename):
        file_path = os.path.join(self.app.config['UPLOAD_FOLDER'], filename)

        # Check if the file exists
        if not os.path.exists(file_path):
            return jsonify({'error': f'File "{filename}" not found.'}), 404

        # Serve the file if it exists
        return send_from_directory(self.app.config['UPLOAD_FOLDER'], filename)

    def encode_frame(self, frame: np.ndarray) -> bytes:
        _, buffer = cv2.imencode('.jpg', frame)
        return buffer.tobytes()

    def generate_frames(self):
        """Generator for video streaming"""
        while True:
            if self.camRef.image is None:
                print("No image")
                break

            # Convert the frame to bytes
            frame_bytes = self.encode_frame(self.camRef.image)

            yield (b'--frame\r\n' b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
        
    def frame_from_file(self):
        from imutils import resize
        file = "src/therealone.mp4"
        video = cv2.VideoCapture(f"{self.directory}/{file}")

        while True:
            _, frame = video.read()

            # if out of frame, reset, get new one, rense and repeat
            if frame is None:
                video = cv2.VideoCapture(f"{self.directory}/{file}")
                continue
                
            frame = resize(frame, width=400)
            frame_bytes = self.encode_frame(frame)
            yield (b'--frame\r\n' b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

            cv2.waitKey(40)


    # serve video from file
    def video_from_file(self):
        return Response(self.frame_from_file(), mimetype='multipart/x-mixed-replace; boundary=frame')

    # serve video from basler cam
    def video_feed(self):
        return Response(self.generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')


    def trigger_button(self):
        return jsonify({"status": "success"})

    def register_websocket_events(self):
        """Register WebSocket events."""
        @self.socketio.on('request_image')
        def handle_image_request(data):
            self.image_requested(data)

        @self.socketio.on('table/random_row')
        def generate_random_row():
            # Randomly generate data for 6 columns, keeping ID unique
            import random
            from time import sleep
            self.socketio.emit('table/control', {'isEnable': '0'})
            for i in range(100):
                unique_id = str(self.table_count)
                columns = [f"{random.randint(1, 100)}" for i in range(1, 6)]
                self.add_row({'id': unique_id, 'columns': columns})
                self.table_count += 1
                sleep(0.2)

            self.table_requested()
            self.socketio.emit('table/control', {'isEnable': '1'})

        @self.socketio.on('table/table_requested')
        def generate_row():
            """ Data being store on the caller, so use event callback """
            self.socketio.emit('table/control', {'isEnable': '0'})
            self.table_requested()
            self.socketio.emit('table/control', {'isEnable': '1'})

        @self.socketio.on('table/check')
        def checkbox_checked(data):
            """ Whenever a checkbox on the front clicked, data will be sending here """
            self.checkIDs = [int(entry) for entry in data['checkedIds']]
            print('On socket', self.checkIDs)
            self.table_checkbox_checked()

        @self.socketio.on('finetune')
        def finetune(data):
            self.finetune_request(data)

        @self.socketio.on('keyboard')
        def keyboard(data):
            self.keyboard_requested(data)

        @self.socketio.on('gcode')
        def send_gcode(data):
            self.send_gcode(data)

    def modify_fifth_cell(self, row_id, new_value):
        self.socketio.emit('table/modify_fifth_cell', 
                            { 'id': row_id,
                            'new_value': new_value })

    def add_row(self, row):
        self.socketio.emit('table/add_row', row)
 

    def send_image(self, image, id):
        image_path = os.path.join(self.directory, image)
        with open(image_path, 'rb') as img_file:
            img_data = img_file.read()
            base64_image = base64.b64encode(img_data).decode('utf-8')

        # Sending the image along with the ID in the message
        response = {'id': id, 'image': base64_image}
        self.socketio.emit('image_data', response)

    def send_matlike(self, image:cv2.typing.MatLike, id:str):
        # Encode the Mat-like image to JPEG
        success, buffer = cv2.imencode('.jpg', image)
        if not success:
            raise ValueError("Failed to encode image.")

        # Convert the encoded image to base64
        base64_image = base64.b64encode(buffer).decode('utf-8')

        # Prepare the response
        response = {'id': id, 'image': base64_image}

        # Emit the data through the socket
        self.socketio.emit('image_data', response)

    def finetune_viewer(self):
        return render_template('finetune.html')

    def pdf_viewer(self):
        return render_template('pdf.html')

    def keyboard(self):
        return render_template('manual.html')

    def table(self):
        return render_template('table.html')

    def index(self):
        return render_template('index.html')

    def register_routes(self):
        """Main routes""" 
        self.app.route('/')(self.pdf_viewer)
        self.app.route('/index')(self.index)
        self.app.route('/table')(self.table)
        self.app.route('/keyboard')(self.keyboard)
        self.app.route('/ft')(self.finetune_viewer)
        self.app.route('/video_feed')(self.video_feed)
        self.app.route('/video_from_file')(self.video_from_file)
        self.app.route('/uploads/<filename>')(self.serve_file)
        self.app.route('/upload', methods=['POST'])(self.upload_file)
        self.app.route('/trigger_button', methods=['POST'])(self.trigger_button)

    def run(self, debug=False):
        self.socketio.run(self.app, host='0.0.0.0', port=self.app.config['PORT'], debug=debug)

if __name__ == '__main__':
    flask_app = FlaskApp()

    # Clear the 'uploads' directory upon exit.
    import atexit
    atexit.register(flask_app.cleanup_upload_folder)

    # Start the server
    flask_app.run(debug=True)

