from pypylon import pylon
import numpy as np
import cv2
import imutils

# Camera parameters (adjust these)
class basler_cam:
    focal_length = 25  # mm
    sensor_width = 13.13  # mm
    image_width = 5472  # pixels for canvas display
    distance = 163.2  # mm - Calibrate this accurately!
    focal_length_inches = focal_length / 25.4
    sensor_width_inches = sensor_width / 25.4
    distance_inches = distance / 25.4
    threshold_value = 150  # Threshold

    # Camera initialization
    camera = pylon.InstantCamera(pylon.TlFactory.GetInstance().CreateFirstDevice())
    camera.StartGrabbing(pylon.GrabStrategy_LatestImageOnly)
    camera.GainAuto.Value = "Off"
    camera.ExposureAuto.Value = "Off"
    camera.GammaSelector.Value = "User"
    camera.Gamma.Value = 1.0
    camera.ExposureTimeAbs.SetValue(220000)
    camera.GainRaw.SetValue(0)

    converter = pylon.ImageFormatConverter()
    converter.OutputPixelFormat = pylon.PixelType_BGR8packed
    mtx = None
    dist = None
    try:
        with np.load('calibration_params_5472.npz') as data:
            mtx = data['mtx']
            dist = data['dist']
            print("Calibration parameters loaded successfully.")
    except FileNotFoundError:
        print("Calibration file not found. Using default parameters.")
        mtx = None  # Or set a default camera matrix if needed
        dist = np.zeros((4, 1))  # Or set default distortion coefficients


    def read_cam(self):
        frame = self.camera.RetrieveResult(5000, pylon.TimeoutHandling_ThrowException)
        if frame.GrabSucceeded():
            img = self.converter.Convert(frame).GetArray()
            if self.mtx is not None and self.dist is not None:  # Apply undistortion if calibration parameters are loaded
                h, w = img.shape[:2]
                newcameramtx, roi = cv2.getOptimalNewCameraMatrix(self.mtx, self.dist, (w, h), 1, (w, h))
                img = cv2.undistort(img, self.mtx, self.dist, None, newcameramtx)
                x, y, w, h = roi
                img = img[y:y + h, x:x + w]  # Crop the image according to the ROI. Avoids black areas after undistortion and keeps the correct aspect ratio.
            return img, True
        else:
            return None, False


def stop(self):
    self.camera.StopGrabbing()
    self.camera.Close()