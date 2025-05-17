import cv2
import numpy as np

# Đường dẫn ảnh
image_path = "C:\\Users\\the_pc\\Pictures\\23.png"

# Các biến cục bộ
min_area = 20
neighborhood_size = 20 # 20px

def tach_dao_tien(img, min_area):
    """Tách dao tiện, vẽ đường tròn và bán kính."""

    # 1. Chuyển sang ảnh xám
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    resized_gray = cv2.resize(gray, (640, 720))
    blur = cv2.GaussianBlur(resized_gray, (5, 5), 0)

    # 2. Phân đoạn (Canny)
    edges = cv2.Canny(blur, 50, 150)

    # 3. Tìm contours
    contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    # 4. Lọc contours
    filtered_contours = [cnt for cnt in contours if cv2.contourArea(cnt) > min_area]

    # 5. Chọn contour lớn nhất
    if filtered_contours:
        largest_contour = max(filtered_contours, key=cv2.contourArea)

        # 6. Tìm nửa còn lại của hình elipse/hình tròn
        center, radius, halfcircle_points = fit_half_circle(resized_gray, largest_contour, neighborhood_size) #truyền ảnh gray
        if center is not None and radius is not None and halfcircle_points is not None:
            #Đã nhận diện được kết quả
            for i in range(len(halfcircle_points)):
                cv2.circle(img, (int(halfcircle_points[i][0]), int(halfcircle_points[i][1])), 1, (0, 255, 0), -1)

            pt1 = center #tâm
            pt2 = (center[0], center[1] - int(radius))
            cv2.line(img, pt1, pt2, (255, 0, 0), 2) # draw red diameter

        return img, radius

    else:
        print("Khong tim thay contour nao!")
        return None, None

def fit_half_circle(gray_img, contour, neighborhood_size):
    """Tìm đường tròn dựa trên một contour đã cho."""

    # 1. Tìm điểm trên cùng (y nhỏ nhất)
    extTop = tuple(contour[contour[:, :, 1].argmin()][0])
    x, y = extTop

    # 2.Tìm một số lượng điểm theo tham số neighborhood size
    points_for_fitting = []
    contour_points = contour.reshape(-1, 2) # Reshape into [[[x, y]] -> [x,y]

    for point in contour_points: #iterate through the points
        dist = np.sqrt((point[0] - x) ** 2 + (point[1] - y) ** 2)
        if dist <= neighborhood_size:
            points_for_fitting.append(point)

    if len(points_for_fitting) < 5:
        print("Không đủ điểm để vẽ đường tròn.")
        return None, None, None

    # 3. Ước tính center từ điểm y nhỏ nhất
    x_coords = [point[0] for point in points_for_fitting]
    y_coords = [point[1] for point in points_for_fitting]

    # 4. Chuẩn hóa và vẽ các điểm
    if x_coords and y_coords:
        mid_x = int((max(x_coords) + min(x_coords)) / 2)
        mid_y = int((max(y_coords) + min(y_coords)) / 2)
        center = (mid_x, mid_y)
        radius = int((max(x_coords) - min(x_coords)) / 2 )

        # Tạo mới nửa vòng tròn
        halfcircle_points = []
        for i in range(30, 150): # các giá trị này có thể được thay đổi
            angle = np.radians(i) # convert angle into radius
            end_x = center[0] + radius * np.cos(angle)
            end_y = center[1] - radius * np.sin(angle)
            halfcircle_points.append([end_x, end_y])

        # 5. Trả về kết quả
        return center, radius, points_for_fitting

    else:
        return None, None, None

def update_min_area(val):
    """Hàm trackbar."""
    global min_area, resized_img
    min_area = cv2.getTrackbarPos("Min Area", "Image")
    result, radius = tach_dao_tien(resized_img.copy(), min_area) #truyền vào ảnh gốc

    if result is not None:
        cv2.imshow("Dao tien da tach", result)
        if radius is not None:
            print(f"Bán kính đường tròn bo: {radius}")
        else:
            print("Không tìm thấy đường tròn bo.")
    else:
        print("Không thể tách dao tien.")

# Đọc ảnh
img = cv2.imread(image_path, cv2.IMREAD_COLOR)
if img is None:
    print(f"Không thể đọc ảnh từ: {image_path}")
    exit()

#Resize image
resized_img = cv2.resize(img, (640, 720)) #resize

#Convert Grayscale
gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY) #gray

# Tạo cửa sổ và trackbars
cv2.namedWindow("Image")
cv2.createTrackbar("Min Area", "Image", min_area, 500, update_min_area)

# Inital show and update Image
cv2.imshow("Image", resized_img)
update_min_area(min_area) #call function and put param

cv2.waitKey(0)
cv2.destroyAllWindows()