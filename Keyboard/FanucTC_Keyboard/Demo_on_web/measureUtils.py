import cv2
import numpy as np
from scipy.spatial import distance as distan
from imutils import perspective, contours
import imutils
from shapely.geometry import LineString, Polygon, Point
from camera import basler_cam
import queue

# Create camera helper object
cam = basler_cam()

object_width_inches = (cam.sensor_width_inches * cam.distance_inches) / cam.focal_length_inches
ppi = cam.image_width / object_width_inches
threshold_value = cam.threshold_value
distance = cam.distance

# Queue for transferring images between threads
image_queue = queue.Queue(maxsize=10)


def midpoint(ptA, ptB):
    return ((ptA[0] + ptB[0]) * 0.5, (ptA[1] + ptB[1]) * 0.5)


def find_closest_point_on_contour(x, y, contour):
    min_distance = float('inf')
    closest_point = None
    for point in contour.reshape(-1, 2):
        distance = distan.euclidean((x, y), point)
        if distance < min_distance:
            min_distance = distance
            closest_point = point
    if closest_point is not None:
        return tuple(closest_point)
    else:
        return None


# why needed live_data
def draw_parallel_lines(image, x1, y1, x2, y2, fractions, ppi, color=(255, 0, 0), thickness=15,
                        contour=None, live_data=None):
    if contour is None:
        raise ValueError("Contour must be provided")

    dx = x2 - x1
    dy = y2 - y1

    normal_dx = -dy
    normal_dy = dx

    length = np.sqrt(dx ** 2 + dy ** 2)
    parallel_lines_lengths_mm = []
    line_data = []  # Move line_data here to be local to the function

    for fraction_idx, fraction in enumerate(fractions):
        px = int(x1 + fraction * (x2 - x1))
        py = int(y1 + fraction * (y2 - y1))

        # Extend the perpendicular line to both sides to ensure intersection
        perp_x1 = int(px + 2000 * normal_dx / length)
        perp_y1 = int(py + 2000 * normal_dy / length)
        perp_x2 = int(px - 2000 * normal_dx / length)
        perp_y2 = int(py - 2000 * normal_dy / length)

        perp_line = LineString([(perp_x1, perp_y1), (perp_x2, perp_y2)])
        contour_polygon = Polygon(contour.reshape(-1, 2))

        intersection_points = perp_line.intersection(contour_polygon)

        coords = []
        if intersection_points.geom_type == 'MultiLineString':
            linestrings = sorted(intersection_points.geoms, key=lambda ls: ls.centroid.distance(Point(px, py)))
            if linestrings:
                coords = list(linestrings[0].coords)
        elif intersection_points.geom_type == 'LineString':
            coords = list(intersection_points.coords)
        elif intersection_points.geom_type == 'MultiPoint':
            points = list(intersection_points.geoms)
            if len(points) >= 2:
                points = sorted(points, key=lambda p: p.distance(Point(px, py)))
                coords = [points[0].coords[0], points[1].coords[0]]
        elif intersection_points.geom_type == 'Point':
            closest_point = find_closest_point_on_contour(px, py, contour)
            if closest_point:
                coords = [intersection_points.coords[0], closest_point]

        if len(coords) == 2:
            line_length_px = distan.euclidean(coords[0], coords[-1])
            line_length_mm = (line_length_px * 25.4) / ppi

            parallel_lines_lengths_mm.append(line_length_mm)

            line_id = f"{fraction_idx}"

            mid_x = int((coords[0][0] + coords[-1][0]) / 2)
            mid_y = int((coords[0][1] + coords[-1][1]) / 2)

            if live_data is not None:
                live_data.append({
                    "id": line_id,
                    "length_mm": line_length_mm
                })

            line_data.append({
                "id": line_id,
                "start": coords[0],
                "end": coords[1],
                "length_mm": line_length_mm,
                "midpoint": (mid_x, mid_y)
            })

            line_color = color
            line_thickness = thickness
            # if line_id == selected_line_id:
            #     line_color = (0, 255, 255)  # Highlight color
            #     line_thickness = thickness + 5

            cv2.line(image, (int(coords[0][0]), int(coords[0][1])), (int(coords[-1][0]), int(coords[-1][1])),
                     line_color,
                     line_thickness)

            cv2.putText(image, f"{line_id}: {line_length_mm:.4f}mm", (mid_x - 370, mid_y + 150),
                        cv2.FONT_HERSHEY_SIMPLEX, 5, (0, 0, 255), 20)  # Red color for ID
    return line_data


def process_camera_feed(img, threshold_value, ppi, live_line):
    # Process and display image
    anh_copy = img.copy()
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (3, 3), 0)
    thresh = cv2.threshold(blur, threshold_value, 255, cv2.THRESH_BINARY)[1]  # Sử dụng ngưỡng mới
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
    opening = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)
    edged = cv2.Canny(opening, 50, int(threshold_value))

    cnts = cv2.findContours(edged.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    cnts = imutils.grab_contours(cnts)
    live_line_data = []
    line_data = []

    if len(cnts) > 0:
        cnt = max(cnts, key=cv2.contourArea)
        cnts = contours.sort_contours(cnts)[0]
        cv2.drawContours(anh_copy, cnts, -1, (0, 255, 0), 20)

        if cv2.contourArea(cnt) < 17:
            return anh_copy, line_data

        hinh_hop = cv2.minAreaRect(cnt)
        hinh_hop = cv2.boxPoints(hinh_hop) if imutils.is_cv2() else cv2.boxPoints(hinh_hop)
        hinh_hop = np.array(hinh_hop, dtype="int")
        hinh_hop = perspective.order_points(hinh_hop)

        cv2.drawContours(anh_copy, [hinh_hop.astype("int")], -1, (0, 255, 0), 2)

        for (x, y) in hinh_hop:
            cv2.circle(anh_copy, (int(x), int(y)), 5, (0, 0, 255), -1)

        (tl, tr, br, bl) = hinh_hop
        (tltrX, tltrY) = midpoint(tl, tr)
        (blbrX, blbrY) = midpoint(bl, br)

        (tlblX, tlblY) = midpoint(tl, bl)
        (trbrX, trbrY) = midpoint(tr, br)

        cv2.circle(anh_copy, (int(tltrX), int(tltrY)), 5, (255, 0, 0), -1)
        cv2.circle(anh_copy, (int(blbrX), int(blbrY)), 5, (255, 0, 0), -1)
        cv2.circle(anh_copy, (int(tlblX), int(tlblY)), 5, (255, 0, 0), -1)
        cv2.circle(anh_copy, (int(trbrX), int(trbrY)), 5, (255, 0, 0), -1)

        height_pixels = distan.euclidean((tltrX, tltrY), (blbrX, blbrY))  # Correct usage
        width_pixels = distan.euclidean((tlblX, tlblY), (trbrX, trbrY))  # Correct usage

        if height_pixels > width_pixels:
            line_data = draw_parallel_lines(anh_copy, int(tltrX), int(tltrY), int(blbrX), int(blbrY),
                                            fractions=[0.01, 0.2, 0.5, 0.8], color=(255, 100, 0),
                                            contour=cnt, ppi=ppi, live_data=live_line_data)
        elif width_pixels >= height_pixels:
            line_data = draw_parallel_lines(anh_copy, int(tltrX), int(tltrY), int(blbrX), int(blbrY),
                                            fractions=[0.03, 0.2, 0.5, 0.8], color=(255, 100, 0),
                                            contour=cnt, ppi=ppi, live_data=live_line_data)

    # Shift the image to the right after rotation
    shift_x = 70  # Adjust this value to control how much the object moves to the right. Positive values move right
    shift_y = -25  # Adjust this value to control how much the object moves up. Negative values move up

    # Rotate the image by 90 degrees counter-clockwise

    height, width = anh_copy.shape[:2]
    blank_image = np.zeros((height, width, 3), dtype=np.uint8)  # black image
    blank_image[:, :] = (255, 255, 255)  # convert to white background

    anh_copy = cv2.rotate(anh_copy, cv2.ROTATE_90_COUNTERCLOCKWISE)

    M = np.float32([[1, 0, shift_x], [0, 1, shift_y]])  # Translation matrix
    shifted_image = cv2.warpAffine(anh_copy, M, (width, height))  # shift image

    # Paste the shifted image onto the blank image
    blank_image[0:height, 0:width] = shifted_image
    anh_copy = blank_image

    return anh_copy, line_data

def camera_process():
    return cam.read_cam()
