import cv2
import numpy as np
import glob

square_size = 10
# termination criteria
criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

# prepare object points, like (0,0,0), (1,0,0), (2,0,0) ....,(6,5,0)
objp = np.zeros((10*7,3), np.float32)
objp[:,:2] = np.mgrid[0:10,0:7].T.reshape(-1,2) * square_size

# Arrays to store object points and image points from all the images.
objpoints = [] # 3d point in real world space
imgpoints = [] # 2d points in image plane.

images = glob.glob('E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\checkboard_images\\*.jpg') # Path to your calibration images
print(images)

for fname in images:
    img = cv2.imread(fname)
    gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)

    ret, corners = cv2.findChessboardCorners(gray, (10, 7), None)  # Kiểm tra lại kích thước bảng
    print(f"ret = {ret} cho hình ảnh: {fname}")  # In ra giá trị ret

    if ret == True:
        objpoints.append(objp)
        corners2 = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
        imgpoints.append(corners2)

        # Draw and display the corners
        cv2.drawChessboardCorners(img, (10, 7), corners2, ret)  # Kiểm tra lại kích thước bảng
        cv2.imshow('img', img)
        cv2.waitKey(500)  # Thời gian hiển thị hình ảnh (milliseconds)


    else:
        print(f"Không tìm thấy góc trong hình ảnh: {fname}")
        cv2.imshow('img', gray)  # Hiển thị ảnh GRAYSCALE để debug
        cv2.waitKey(0)

cv2.destroyAllWindows()

ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1],None,None)

# Save the camera matrix and distortion coefficients
np.savez("calibration_params_5472.npz", mtx=mtx, dist=dist, rvecs=rvecs, tvecs=tvecs)