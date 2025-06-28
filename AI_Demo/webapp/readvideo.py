import cv2

video = cv2.VideoCapture("webapp/src/howcatwork.mp4")

while True:
    _, frame = video.read()
    cv2.imshow("cunt", frame)
    key = cv2.waitKey(2)
    if key == ord('q'):
        break