from main import FlaskApp
import cv2

app = FlaskApp()

def theFunction(data):
    image = cv2.imread("src/pics/loyd2.jpg")
    app.send_matlike(image, 'result1')
    print(f"upload {data}")


app.pdf_uploaded += theFunction
app.run()

