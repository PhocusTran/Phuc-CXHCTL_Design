from flask import Flask, render_template, request, redirect, url_for, send_from_directory, jsonify, flash
import time
import json
import requests
import logging
import threading
from werkzeug.utils import secure_filename
from flask_cors import CORS
import webview
from flask_socketio import SocketIO, emit
from selenium import webdriver
from flask_login import LoginManager, UserMixin, login_user, logout_user, login_required, current_user
from werkzeug.security import generate_password_hash, check_password_hash
from forms import LoginForm
from zeroconf import ServiceBrowser, Zeroconf
import os
import sys

# Cấu hình logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

BASE_DIR = os.path.abspath(os.path.dirname(__file__))
TEMPLATE_DIR = os.path.join(BASE_DIR, "templates")

app = Flask(__name__, static_folder=TEMPLATE_DIR)
CORS(app, resources={r"/*": {"origins": "*"}})
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='gevent')

# *** KHÓA BÍ MẬT (QUAN TRỌNG) ***
app.config['SECRET_KEY'] = 'CXHCTL@2025'  # THAY ĐỔI CÁI NÀY!

# *** TÀI KHOẢN NGƯỜI DÙNG DUY NHẤT ***
ADMIN_USERNAME = "admin"  # Thay đổi nếu muốn
ADMIN_PASSWORD = "admin"  # Thay đổi nếu muốn

# Flask-Login setup
login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = 'login'  # Chuyển hướng người dùng chưa đăng nhập đến trang login

# Cấu hình máy chủ
app.config['UPLOAD_FOLDER'] = 'uploads'
os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)

DATA = {}
LAST_SEEN = {}
ONLINE_TIMEOUT = 10
MAX_RETRIES = 3

RPI_MAPPING = {
    "000000002fba44e1": "RPI 1",
}

data_lock = threading.Lock()

offline_rpis = set()  # Use a set for efficient membership testing

class MyListener:
    def __init__(self):
        self.rpis = {}

    def remove_service(self, zeroconf, type, name):
        """Xóa RPi khi nó không còn xuất hiện."""
        if name in self.rpis:
            rpi_id = self.rpis[name]['id']
            with data_lock:
                if rpi_id in DATA:
                    del DATA[rpi_id]
                    print(f"Remove rpi {rpi_id}")
            del self.rpis[name]
            logging.info(f"RPi {name} removed from list.")

    def add_service(self, zeroconf, type, name):
        info = zeroconf.get_service_info(type, name)
        if info:
            ip = info.parsed_addresses()[0]
            port = info.port
            print("IP Address:", ip, "Port:", port)
            # Thử kết nối 3 lần
            for attempt in range(3):
                try:
                    response = requests.get(f"http://{ip}:{port}/test_status", timeout=5)
                    if response.status_code == 200:
                        print(f"Kết nối thành công tới {ip}:{port}: {response.json()}")
                        rpi_data = response.json()
                        rpi_id = rpi_data['rpi_id']  # Lấy rpi_id từ phản hồi
                        rpi_ip = DATA[rpi_id]['os_info']['hostname']

                        with data_lock:
                            DATA[rpi_id] = rpi_data
                            self.rpis[name] = {'id': rpi_id}
                        break
                except Exception as e:
                    logging.error(e)
            else:
                print(f"Không thể kết nối tới {ip}:{port}")

    def update_service(self, zeroconf, type, name):
        """Xử lý cập nhật dịch vụ (nếu cần)."""
        # Để trống hoặc ghi log nếu không cần xử lý
        pass


# Flag to indicate if the check_inactive_rpis thread is running
check_thread_running = False


def start_discovery():
    zeroconf = Zeroconf()
    listener = MyListener()
    ServiceBrowser(zeroconf, "_http._tcp.local.", listener)
    logging.info("Started mDNS discovery service.")

# *** TẢI NGƯỜI DÙNG ***
class User(UserMixin):
    def __init__(self, id):
        self.id = id


@login_manager.user_loader
def load_user(user_id):
    return User(user_id)


# Store the last seen time for each RPi
last_seen = {}


def update_rpi_status(rpi_id, status):
    print(f"Updating rpi_status: {status} for {rpi_id}")
    socketio.emit('rpi_status', {'rpi_id': rpi_id, 'status': status})

def mark_rpi_offline(rpi_id):
    with data_lock:
        offline_rpis.add(rpi_id)

def clear_rpi_offline(rpi_id):
    with data_lock:
        if rpi_id in offline_rpis:
            offline_rpis.remove(rpi_id)


@socketio.on('connect')
def test_connect():
    print('Client connected')
    emit('my response', {'data': 'Connected!'})


@socketio.on('rpi_status')
def handle_rpi_status(json_str):
    #json_data = json.loads(json_str) if isinstance(json_str, str) else json_str
    #rpi_id = json_data['rpi_id']
    #last_seen[rpi_id] = time.time()  # Update the last seen time
    #print(f"Received RPi status update from {rpi_id}: {json_data}")
    #update_rpi_status(rpi_id, 'online')  # Now called immediately when a status is received
    pass

def fetch_rpi_status(rpi_id):
    """Fetch RPi status and details from the RPi's API."""
    try:
        with data_lock:
            if rpi_id not in DATA:
                logging.warning(f"RPI {rpi_id} not found in DATA.")
                return "offline"
            rpi_ip = DATA[rpi_id]['os_info']['hostname']
        fetch_url = f"http://{rpi_ip}:5001/test_status"
        response = requests.get(fetch_url, verify=False, timeout=5)  # Giảm timeout

        if response.status_code == 200:
            logging.info(f"Successfully called {rpi_id} and loaded info.")
            with data_lock:
                DATA[rpi_id] = response.json()
            clear_rpi_offline(rpi_id)  # Clear offline status if now online
            return "online"
        else:
            logging.warning(f"Failed to fetch {rpi_id}, status code: {response.status_code}")
            mark_rpi_offline(rpi_id)  # Mark as offline if cannot fetch
            return "offline"

    except requests.exceptions.ConnectTimeout as e:
        logging.error(f"ConnectTimeoutError: {e}")
        mark_rpi_offline(rpi_id)  # Mark as offline on timeout
        return "offline"
    except requests.exceptions.ConnectionError as e:
        logging.error(f"ConnectionError: {e}")
        mark_rpi_offline(rpi_id)  # Mark as offline on connection error
        return "offline"
    except KeyError as e:
        logging.error(f"KeyError: {e}")
        mark_rpi_offline(rpi_id)
        return "offline"
    except Exception as e:
        logging.error(f"General Exception: {e}")
        mark_rpi_offline(rpi_id)
        return "offline"


def fetch_rpi_status_periodic(rpi_id):
    """Calls fetch_rpi_status periodically."""
    while True:
        try:
            status = fetch_rpi_status(rpi_id) # Changed to fetch_rpi_status function
            # Only if the status changed we notify it
            if status == "online":  # If it can connect
                update_rpi_status(rpi_id, 'online')
            else:  # Other wise we notify that we cannot connect
                update_rpi_status(rpi_id, 'offline')  # If unable to connect
            with data_lock:
                print(f"DATA: {DATA}")
            time.sleep(5)
        except Exception as e:
            print(e)
            break  # Exit if has problems


def update_rpi_files():
    """Updates files in all RPis. To be called in interval"""
    for rpi_id, rpi_data in DATA.items():  # Add the item
        update_files(rpi_id)  # Call update files function


def update_files(rpi_id):
    try:
        with data_lock:
            rpi_ip = DATA[rpi_id]['os_info']['hostname']
        list_url = f"http://{rpi_ip}:5001/list_files"
        response = requests.get(list_url, verify=False)

        if response.status_code == 200:
            with data_lock:
                DATA[rpi_id]['files'] = json.loads(response.text)
            logging.info(f"Refreshed {rpi_id} files")

    except Exception as e:
        print(e)


def scan_offline_rpis():
    while True:
        try:
            with data_lock:
                rpi_to_check = list(offline_rpis)  # Iterate over a copy to avoid modification errors
            for rpi_id in rpi_to_check:
                logging.info(f"Attempting to re-scan offline RPi: {rpi_id}")
                status = fetch_rpi_status(rpi_id) # Use the fetch_rpi_status function

                if status == "online":
                    logging.info(f"RPi {rpi_id} is back online.")
                    update_rpi_status(rpi_id, 'online')
                else:
                    logging.info(f"RPi {rpi_id} still offline.")

            time.sleep(30)  # Scan every 30 seconds
        except Exception as e:
            logging.error(f"Error in scan_offline_rpis: {e}")
            time.sleep(60)  # Sleep a longer time on error


# *** LOGIN ROUTE ***
@app.route('/login', methods=['GET', 'POST'])
def login():
    form = LoginForm()
    if form.validate_on_submit():
        if form.username.data == ADMIN_USERNAME and form.password.data == ADMIN_PASSWORD:
            user = User(ADMIN_USERNAME)
            login_user(user)
            return redirect(url_for('index'))
        else:
            flash('Tên đăng nhập hoặc mật khẩu không đúng')
            return redirect(url_for('login'))
    return render_template('login.html', form=form)


@app.route('/logout')
@login_required
def logout():
    logout_user()
    return redirect(url_for('login'))


@app.route("/")
@login_required  # Yêu cầu đăng nhập
def index():
    rpi_statuses = {}
    # DATA["RPI_TEST"] = {'os_info': {'hostname': '192.168.63.208', 'platform': 'Linux'}}
    for rpi_id, rpi_data in DATA.items():
        # Tạo một thread cho mỗi RPi để cập nhật trạng thái định kỳ
        thread = threading.Thread(target=fetch_rpi_status_periodic, args=(rpi_id,))
        thread.daemon = True  # Thoát khi chương trình chính thoát
        thread.start()

    return render_template("index.html", rpi_data=DATA, rpi_statuses={})

@app.route("/rpi/status/<rpi_id>")
@login_required
def get_rpi_status(rpi_id):
    """API endpoint to get the online status and space info of a specific RPi."""
    with data_lock:
        data = DATA.get(rpi_id)
    if data:
        return jsonify(data)  # Trả về toàn bộ dữ liệu của RPi
    else:
        return jsonify({"error": f"Không tìm thấy dữ liệu cho RPi: {rpi_id}"}), 404


@app.route("/rpi/<rpi_id>")
@login_required
def rpi_detail(rpi_id):
    with data_lock:
        data = DATA.get(rpi_id)
    if not data:
        return f"Không tìm thấy dữ liệu cho RPi: {rpi_id}", 404

    update_files(rpi_id)
    files = data.get('files', [])
    # ADD HERE
    try:
        if data and data["space_info"]:
            used_space_mb = round(data["space_info"].get("used", 0) / (1024 * 1024), 2)
            total_space_mb = round(data["space_info"].get("total", 0) / (1024 * 1024), 2)

            used_percent = round(
                (data["space_info"].get("used", 0) / data["space_info"].get("total", 1) * 100) if data.get('space_info',
                                                                                                           {}).get(
                    'total', 1) else 0, 2)
        else:
            used_space_mb = 0
            total_space_mb = 0
            used_percent = 0
    except KeyError:
        used_space_mb = 0
        total_space_mb = 0
        used_percent = 0

    return render_template("rpi_detail.html", rpi_id=rpi_id, data=data, files=files,
                           upload_folder=app.config['UPLOAD_FOLDER'], used_space_mb=used_space_mb,
                           total_space_mb=total_space_mb, used_percent=used_percent)


@app.route("/download/<rpi_id>/<filename>")
@login_required
def download(rpi_id, filename):
    try:
        print(f"Download request received for rpi_id: {rpi_id}, filename: {filename}")
        with data_lock:
            rpi_ip = DATA[rpi_id]['os_info']['hostname']
        # download_url = app.config['RPI_DOWNLOAD_URL'].format(rpi_ip, filename)  # Sửa lại
        download_url = f'http://{rpi_ip}:5001/download/{filename}'
        print(f"Constructed download URL: {download_url}")
        webdriver.Chrome().get(download_url)
        response = requests.get(download_url, stream=True, verify=False)
        response.raise_for_status()

        headers = {
            'Content-Disposition': f'attachment; filename="{filename}"',
            'Content-Type': response.headers.get('Content-Type', 'application/octet-stream')
        }
        return response.raw, 200, headers
    except requests.exceptions.RequestException as e:
        print(f"Error downloading file: {e}")
        return redirect(url_for('rpi_detail', rpi_id=rpi_id))
    except Exception as e:
        print(e)


@app.route("/upload/<rpi_id>", methods=['POST'])
@login_required
def upload(rpi_id):
    if 'file' not in request.files:
        return "No file selected"
    file = request.files['file']
    if file.filename == '':
        return "No file selected"

    with data_lock:
        rpi_ip = DATA[rpi_id]['os_info']['hostname']
    filename = secure_filename(file.filename)
    upload_url = f"http://{rpi_ip}:5001/upload/{filename}"
    print(upload_url)

    try:
        files = {'file': (filename, file.stream, file.content_type)}
        response = requests.post(upload_url, files=files, verify=False)
        response.raise_for_status()

        print(f"File '{filename}' uploaded successfully to RPi {rpi_id}")
        return redirect(url_for('rpi_detail', rpi_id=rpi_id) + "#uploaded")

    except requests.exceptions.RequestException as e:
        print(f"Error uploading file: {e}")
        return f"Error uploading file: {e}", 500

    except Exception as e:
        print(f"General upload error: {e}")
        return f"General upload error: {e}", 500


@app.route("/upload-multiple/<rpi_id>", methods=['POST'])
@login_required
def upload_multiple(rpi_id):
    if 'files[]' not in request.files:
        return "No file part"

    files = request.files.getlist('files[]')
    if not files or files[0].filename == '':
        return "No selected file"

    with data_lock:
        rpi_ip = DATA[rpi_id]['os_info']['hostname']

    try:
        for file in files:
            filename = secure_filename(file.filename)
            upload_url = f"http://{rpi_ip}:5001/upload/{filename}"
            files_to_send = {'file': (filename, file.stream, file.content_type)}  # Wrap the file in a dictionary
            response = requests.post(upload_url, files=files_to_send, verify=False)
            response.raise_for_status()
            logging.info(f"File '{filename}' uploaded successfully to RPi {rpi_id}")

        return redirect(url_for('rpi_detail', rpi_id=rpi_id))  # Code Change Here

    except requests.exceptions.RequestException as e:
        print(f"Error uploading file: {e}")
        return f"Error uploading file: {e}", 500
    except Exception as e:
        print(f"General upload error: {e}")
        return f"General upload error: {e}", 500


@app.route("/delete/<rpi_id>/<filename>")
@login_required
def delete_file(rpi_id, filename):
    try:
        with data_lock:
            rpi_ip = DATA[rpi_id]['os_info']['hostname']
        delete_url = f"http://{rpi_ip}:5001/delete/{filename}"
        response = requests.get(delete_url, verify=False, timeout=5)
        response.raise_for_status()
        return redirect(url_for('rpi_detail', rpi_id=rpi_id))
    except requests.exceptions.RequestException as e:
        print(f"Error connecting for {filename} to RPi {rpi_id}")
        return jsonify({'message': "Error connecting to RPi"}), 500
    except Exception as e:
        print(f"It went wrong on file, likely with file paths or connection: {e}")
        return jsonify({'message': "It went wrong on file"}), 500


@app.route("/delete-multiple/<rpi_id>", methods=['POST'])
@login_required
def delete_multiple(rpi_id):
    filenames = request.get_json()  # Get list of filenames directly from request
    with data_lock:
        rpi_ip = DATA[rpi_id]['os_info']['hostname']

    try:
        for filename in filenames:
            delete_url = f"http://{rpi_ip}:5001/delete/{filename}"
            response = requests.get(delete_url, verify=False, timeout=5)
            response.raise_for_status()
            print(f"Deleted file '{filename}' on RPi {rpi_id}.")

        return jsonify({'message': 'Selected files deleted successfully.'}), 200
    except requests.exceptions.RequestException as e:
        print(f"Error connecting for {filename} to RPi {rpi_id}")
        return jsonify({'message': "Error connecting to RPi"}), 500
    except Exception as e:
        print(f"It went wrong on file, likely with file paths or connection: {e}")
        return jsonify({'message': "It went wrong on file"}), 500


@app.route("/rpi/<rpi_id>/space_info")
@login_required
def get_rpi_space_info(rpi_id):
    """API endpoint to get space info for a specific RPi."""
    with data_lock:
        data = DATA.get(rpi_id)
    if not data:
        return jsonify({"error": f"Không tìm thấy dữ liệu cho RPi: {rpi_id}"}), 404

    try:
        # Lấy IP từ DATA một cách an toàn
        with data_lock:
            rpi_ip = data['os_info']['hostname']
        status_url = f"http://{rpi_ip}:5001/test_status"
        response = requests.get(status_url, verify=False, timeout=5)
        response.raise_for_status()
        status_data = response.json()
        space_info = status_data.get('space_info', {})
        return jsonify(space_info)
    except requests.exceptions.RequestException as e:
        logging.error(f"Error fetching status from RPi {rpi_id}: {e}")
        return jsonify({"error": str(e)}), 500
    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        return jsonify({"error": "Internal Server Error"}), 500

# *** API routes for new client ***
@app.route("/api/rpi/status/<rpi_id>")
def api_get_rpi_status(rpi_id):
    """API endpoint to get the online status and space info of a specific RPi for the new client."""
    with data_lock:
        data = DATA.get(rpi_id)
    if data:
        return jsonify(data)  # Trả về toàn bộ dữ liệu của RPi
    else:
        return jsonify({"error": f"Không tìm thấy dữ liệu cho RPi: {rpi_id}"}), 404

@app.route("/api/rpi/<rpi_id>/files")
def api_get_rpi_files(rpi_id):
    """API endpoint to get the list of files for a specific RPi for the new client."""
    with data_lock:
        data = DATA.get(rpi_id)
    if data:
        return jsonify(json.loads(data.get('files', '[]')))  # Trả về danh sách tệp
    else:
        return jsonify({"error": f"Không tìm thấy dữ liệu cho RPi: {rpi_id}"}), 404

@app.route("/api/rpi/<rpi_id>/space_info")
def api_get_rpi_space_info(rpi_id):
    """API endpoint to get space info for a specific RPi for the new client."""
    with data_lock:
        data = DATA.get(rpi_id)
    if data:
        return jsonify(data.get('space_info', {}))  # Trả về thông tin dung lượng
    else:
        return jsonify({"error": f"Không tìm thấy dữ liệu cho RPi: {rpi_id}"}), 404

@app.route("/api/list_rpis")
def api_list_rpis():
    """API endpoint to list all discovered RPis for the new client."""
    with data_lock:
        rpi_list = list(DATA.keys())
    return jsonify(rpi_list)

def start_flask():
    # Important: Provide the paths to your certificate and key files.
    app.run(debug=True, host="0.0.0.0")  # Enable SSL Context


def status_update_all():
    while True:
        try:
            rpi = 'RPI_TEST'
            with data_lock:
                ip = (DATA["RPI_TEST"]["os_info"]["hostname"])
            response = requests.get(app.config['RPI_STATUS_URL'].format(ip), verify=False, timeout=5)
            response.raise_for_status()  # add raise for status for this, if you want
            data = response.json()

            used_space_mb = round(data["space_info"].get("used", 0) / (1024 * 1024), 2)
            total_space_mb = round(data["space_info"].get("total", 0) / (1024 * 1024), 2)
            used_percent = round(
                (data["space_info"].get("used", 0) / data["space_info"].get("total", 1) * 100) if data.get('space_info',
                                                                                                           {}).get(
                    'total', 1) else 0, 2)
            filelist = data["files"]
            logging.info(f"Success: Data. This was for RPi")

        except requests.exceptions.RequestException as e:  # Add this too, the request exceptions
            logging.info(f"The exception {e}")
        except Exception as a:
            logging.info(f"This failed but, {a}")

        time.sleep(5)


def start_server():
    socketio.run(app, host='127.0.0.1', port=5000, debug=True, allow_unsafe_werkzeug=True, use_reloader=False)