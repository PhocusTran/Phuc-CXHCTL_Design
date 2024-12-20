from pdf2image import convert_from_path
import os, numpy as np
from skimage import io


dest_DIR='test_samples'
file_path='E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\eDOCr-master\\eDOCr-master\\tests\\test_samples\\kaifa.pdf'
filename=os.path.splitext(os.path.basename(file_path))[0]
img = convert_from_path(file_path, poppler_path='E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\Release-24.07.0-0\\poppler-24.07.0\\Library\\bin')
img = np.array(img[0])

io.imsave(os.path.join(dest_DIR, filename+'.jpg'),img)