import threading
import webview
from sd_server_http import app, DATA, start_server  # Import app và DATA từ sd_server_http.py
import logging
try:
    if __name__ == '__main__':
        # Start Flask server trong một thread riêng
        flask_thread = threading.Thread(target=start_server)
        flask_thread.daemon = True
        flask_thread.start()

        # Start cửa sổ webview
        webview.create_window('USB Drive Management', 'http://127.0.0.1:5000/')
        webview.start()
except Exception as e:
    logging.exception("Đã xảy ra lỗi:")  # Ghi chi tiết lỗi, bao gồm cả traceback
    raise  # Re-raise exception để Flask có thể xử lý
