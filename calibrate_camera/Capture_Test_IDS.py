import cv2
import os
import time
from pypylon import pylon

# Khởi tạo camera Basler
camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
camera.StartGrabbing(pylon.GrabStrategy_LatestImageOnly)

# Cài đặt các thông số camera (tùy chọn)
camera.GainAuto.Value = "Off"
camera.ExposureAuto.Value = "Off"
camera.GammaSelector.Value = "User"
camera.Gamma.Value = 1.0
camera.ExposureTimeAbs.SetValue(165000)
camera.GainRaw.SetValue(0)

# Chuyển đổi định dạng hình ảnh thành BGR8packed
converter = pylon.ImageFormatConverter()
converter.OutputPixelFormat = pylon.PixelType_BGR8packed

i = 63
capture_interval = 2  # seconds
last_capture_time = time.time()

temp_dir = 'E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\checkboard_images'

while True:
    # Lấy hình ảnh từ camera Basler
    grab_result = camera.RetrieveResult(5000, pylon.TimeoutHandling_ThrowException)
    if grab_result.GrabSucceeded():
        img = converter.Convert(grab_result).GetArray()
        cv2.imshow("Webcam Image", img)

        current_time = time.time()
        if current_time - last_capture_time >= capture_interval:
            i += 1
            image_path = os.path.join(temp_dir, f'{i}.jpg')
            cv2.imwrite(image_path, img)
            print(f"Image {i} saved.")
            last_capture_time = current_time  # Reset the capture time

        # 27 là mã ASCII cho phím esc trên bàn phím.
    if cv2.waitKey(1) & 0xFF == ord('q'):
      break

# Giải phóng camera và đóng cửa sổ
camera.StopGrabbing()
camera.Close()
cv2.destroyAllWindows()