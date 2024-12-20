import cv2
import pytesseract
import re

# Cấu hình Tesseract (cần cài đặt Tesseract OCR)
pytesseract.pytesseract.tesseract_cmd = 'E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\Tesseract-OCR\\tesseract.exe'  # Thay đổi đường dẫn nếu cần

def extract_dimensions(image_path):
    """Trích xuất kích thước, dung sai và ký tự từ ảnh."""

    # 1. Đọc ảnh
    img = cv2.imread(image_path)
    if img is None:
        print(f"Error: Không thể đọc ảnh từ {image_path}")
        return None

    # 2. Chuyển ảnh sang thang xám
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # 3. Tiền xử lý ảnh (có thể điều chỉnh để tăng độ chính xác OCR)
    thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)[1]
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3,3))
    thresh = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel, iterations=1)
    # thresh = cv2.GaussianBlur(thresh, (5, 5), 0)


    # 4. Sử dụng Tesseract OCR để nhận diện chữ
    text = pytesseract.image_to_string(thresh, lang='eng', config='--psm 6') # Thử các psm khác nhau xem

    # 5. Phân tích cú pháp
    extracted_data = {
        'dimensions': [],
        'tolerances': [],
        'symbols': [],
        'other_text': []
    }
    # Regular expressions for numbers, tolerances, symbols
    dimension_pattern = re.compile(r'(\d+\.?\d*)(?:\s*±\s*(\d+\.?\d*))?(?:\s*[+-]\s*(\d+\.?\d*)\/(\d+\.?\d*))?')
    tolerance_pattern = re.compile(r'([ØR]\d+\.?\d*\s*[K])|([ØR]\d+\.?\d*\s*[F])|(\d+\.?\d*\s*[±]\s*\d+\.?\d*\s*[K|F]?)')
    symbol_pattern = re.compile(r'Rz\d+\.?\d*|Ø|\+|\-|°|\/\/|±')

    for line in text.split('\n'):
        line = line.strip()
        if not line:
            continue

        # Extract dimensions
        for match in dimension_pattern.finditer(line):
          dimension_value = match.group(1)
          upper_tolerance = match.group(2)
          pos_tolerance = match.group(3)
          neg_tolerance = match.group(4)

          if upper_tolerance:
            extracted_data['tolerances'].append(f"±{upper_tolerance}")
          elif pos_tolerance and neg_tolerance:
             extracted_data['tolerances'].append(f"+{pos_tolerance}/{neg_tolerance}")


          extracted_data['dimensions'].append(dimension_value)

        # Extract tolerances
        for match in tolerance_pattern.finditer(line):
            tolerance = match.group(0)
            extracted_data['tolerances'].append(tolerance)

        # Extract symbols
        for match in symbol_pattern.finditer(line):
            symbol = match.group(0)
            extracted_data['symbols'].append(symbol)

        # Extract other text
        if not any(match in line for match in extracted_data['dimensions'] + extracted_data['tolerances'] + extracted_data['symbols']):
            extracted_data['other_text'].append(line)

    return extracted_data


if __name__ == "__main__":
    image_path = 'E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\eDOCr-master\\eDOCr-master\\tests\\test_samples\\kaifa.jpg'  # Thay đổi đường dẫn ảnh của bạn
    extracted_info = extract_dimensions(image_path)

    if extracted_info:
        print("Dimensions:", extracted_info['dimensions'])
        print("Tolerances:", extracted_info['tolerances'])
        print("Symbols:", extracted_info['symbols'])
        print("Other text:", extracted_info['other_text'])