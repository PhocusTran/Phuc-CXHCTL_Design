import os
import string
from eDOCr import keras_ocr
from eDOCr.keras_ocr_models import train_recognizer


DIR=os.getcwd()
recognizer_basepath = os.path.join(DIR,'eDOCr/Keras_OCR_models/models')
data_dir='./tests'
GDT_symbols='⏤⏥○⌭⌒⌓⏊∠⫽⌯⌖◎↗⌰'
FCF_symbols='ⒺⒻⓁⓂⓅⓈⓉⓊ'
Extra='().,+-±:/°"⌀'

alphabet=string.digits + 'AaBCDRGHhMmnx'+ Extra
#alphabet=string.digits+string.ascii_letters+',.:-/'
#alphabet=string.digits+',⌀ABCD'+GDT_symbols #+FCF_symbols


backgrounds=[]
samples=10000
for i in range(0,samples):
    backgrounds.append(os.path.join('./eDOCr/Keras_OCR_models/backgrounds/0.jpg'))

#backgrounds = keras_ocr.data_generation.get_backgrounds(cache_dir=data_dir)
#fonts = keras_ocr.data_generation.get_fonts(alphabet=alphabet,cache_dir=data_dir,exclude_smallcaps=True)
fonts=[]
for i in os.listdir(os.path.join(DIR,'eDOCr/Keras_OCR_models/fonts')):
    fonts.append(os.path.join('./eDOCr/Keras_OCR_models/fonts',i))

pretrained_model=os.path.join(recognizer_basepath,'E:\\CXHCTL_Phuc\\Programs\\PythonCode\\pythonProject\\OCR_Test\\Programs\\eDOCr-master\\eDOCr-master\\tests\\eDOCr\\keras_ocr_models\models\\recognizer_dimensions.h5')
#pretrained_model=None

train_recognizer.generate_n_train(alphabet,backgrounds,fonts,recognizer_basepath=recognizer_basepath, pretrained_model=pretrained_model)