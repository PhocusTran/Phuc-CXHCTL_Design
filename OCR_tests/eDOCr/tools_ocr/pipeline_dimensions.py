import cv2
import numpy as np
import math
import re
from .cluster import agglomerative_cluster
from .tolerances import check_tolerances
from OCR_Test.Programs.eDOCr_master.tests.eDOCr.keras_ocr import tools,detection,recognition
from PIL import Image


class Pipeline:
    """A wrapper for a combination of detector and recognizer.
    Args:
        detector: The detector to use
        recognizer: The recognizer to use
        scale: The scale factor to apply to input images
        max_size: The maximum single-side dimension of images for
            inference.
    """
    def __init__(self, detector=None, recognizer=None, scale=2, max_size=2048):
        if detector is None:
            detector = detection.Detector()
        if recognizer is None:
            recognizer = recognition.Recognizer()
        self.scale = scale
        self.detector = detector
        self.recognizer = recognizer
        self.max_size = max_size

    def detect(self, images, detection_kwargs=None):
        """Run the pipeline on one or multiples images.
        Args:
            images: The images to parse (can be a list of actual images or a list of filepaths)
            detection_kwargs: Arguments to pass to the detector call
            recognition_kwargs: Arguments to pass to the recognizer call
        Returns:
            A list of lists of (text, box) tuples.
        """
        if not isinstance(images, np.ndarray):
            images = [tools.read(image) for image in images]

        images = [
            tools.resize_image(image, max_scale=self.scale, max_size=self.max_size)
            for image in images
        ]
        max_height, max_width = np.array(
            [image.shape[:2] for image, scale in images]
        ).max(axis=0)
        scales = [scale for _, scale in images]
        images = np.array(
            [
                tools.pad(image, width=max_width, height=max_height)
                for image, _ in images
            ]
        )
        if detection_kwargs is None:
            detection_kwargs = {}

        box_groups = self.detector.detect(images=images, **detection_kwargs)
        box_groups = [
            tools.adjust_boxes(boxes=boxes, boxes_format="boxes", scale=1 / scale)
            if scale != 1
            else boxes
            for boxes, scale in zip(box_groups, scales)
        ]
        return box_groups

    def recognize_dimensions(self, box_groups, img):
        predictions = []
        i = 0
        recognition_kwargs = {}
        # Sắp xếp các box theo thứ tự từ trên xuống dưới, trái sang phải
        box_groups = sorted(box_groups, key=lambda box: (np.mean(box[:, 1]), np.mean(box[:, 0])))
        for box in box_groups:
            rect = cv2.minAreaRect(box)
            alfa = get_alfa(box)
            if -5 < alfa < 85:
                angle = -round(alfa / 5) * 5
            elif 85 < alfa < 95:
                angle = round(alfa / 5) * 5 - 180
            elif 95 < alfa < 185:
                angle = 180 - round(alfa / 5) * 5
            else:
                angle = alfa

            # Rotate the image before cropping
            if angle != 0:
                M = cv2.getRotationMatrix2D(rect[0], angle, 1.0)
                rotated_img = cv2.warpAffine(img, M, (img.shape[1], img.shape[0]), flags=cv2.INTER_CUBIC)
            else:
                rotated_img = img

            w = int(max(rect[1]) + 5)
            h = int(min(rect[1]) + 2)

            img_croped = subimage(rotated_img, rect[0], 0, w, h)  # Angle should be 0
            img_croped, thresh = clean_h_lines(img_croped)
            cnts = cv2.findContours(thresh,cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[0] #Get contourns

            if len(cnts)==1:
                #pred=self.recognizer.recognize(image=cv2.rotate(img_croped,cv2.ROTATE_90_COUNTERCLOCKWISE))
                img_croped=cv2.rotate(img_croped,cv2.ROTATE_90_COUNTERCLOCKWISE)
                box_groups=[np.array([[[0,0],[h,0],[h,w],[0,w]]])]
                pred=self.recognizer.recognize_from_boxes(images=[img_croped],box_groups=box_groups,**recognition_kwargs)[0][0]
                pred,add=analyse_pred(pred,cnts)
            elif 1<len(cnts)<15:
                arr=check_tolerances(img_croped)
                pred=''
                for img_ in arr:
                    h,w,_=img_.shape
                    box_groups=[np.array([[[0,0],[w,0],[w,h],[0,h]]])]
                    pred_ = self.recognizer.recognize_from_boxes(images=[img_],box_groups=box_groups,**recognition_kwargs)[0][0]
                    if pred_=='':
                        pred=self.recognizer.recognize(image=img_croped)+' '
                        break
                    else:
                        pred+=pred_+' '
                pred=pred[:-1]
                pred,add=analyse_pred(pred,cnts)
            else:
                add=False

            if add:
                i+=1
                pred_id={'ID':i}
                pred_id.update(pred)
                predictions.append({'pred':pred_id,'box':box})

                # In ra ID và các giá trị khác
                print(f"ID: {pred_id['ID']}, Nominal: {pred_id.get('nominal', '')}, Value: {pred_id.get('value', '')}, Tolerance: {pred_id.get('tolerance', '')}")
        return predictions

############################################################################

def get_alfa(box):
    exp_box=np.vstack((box[3], box, box[0]))
    i=np.argmax(box[:,1])
    B=box[i]
    A=exp_box[i]
    C=exp_box[i+2]
    AB_=math.sqrt((A[0]-B[0])**2+(A[1]-B[1])**2)
    BC_=math.sqrt((C[0]-B[0])**2+(C[1]-B[1])**2)
    m=np.array([(A,AB_),(C,BC_)],dtype=object)
    j=np.argmax(m[:,1])
    O=m[j,0]
    if B[0]==O[0]:
        alfa=math.pi/2
    else:
        alfa=math.atan((O[1]-B[1])/(O[0]-B[0]))
    if alfa==0:
        return alfa/math.pi*180
    elif B[0]<O[0]:
        return -alfa/math.pi*180
    else:
        return (math.pi-alfa)/math.pi*180

def subimage(image, center, theta, width, height):
   '''
   Rotates OpenCV image around center with angle theta (in deg)
   then crops the image according to width and height.
   '''
   shape = ( image.shape[1], image.shape[0] ) # cv2.warpAffine expects shape in (length, height)
   matrix = cv2.getRotationMatrix2D( center=center, angle=theta, scale=1 )
   image = cv2.warpAffine( src=image, M=matrix, dsize=shape )
   x,y = (int( center[0] - width/2  ),int( center[1] - height/2 ))
   image = image[ y:y+height, x:x+width ]
   return image

def clean_h_lines(img_croped):
    gray = cv2.cvtColor(img_croped, cv2.COLOR_BGR2GRAY) #Convert img to grayscale
    _,thresh = cv2.threshold(gray,127,255,cv2.THRESH_BINARY_INV) #Threshold to binary image
    horizontal_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (int(img_croped.shape[1]),1))
    detect_horizontal = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, horizontal_kernel, iterations=2)
    cnts = cv2.findContours(detect_horizontal, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    cnts = cnts[0] if len(cnts) == 2 else cnts[1]
    for c in cnts:
        cv2.drawContours(img_croped, [c], -1, (255,255,255), 2)
    return img_croped, thresh


def analyse_pred(pred,cnts):
    pred_dict={'type':'','flag':True}
    c=len(cnts)
    add=True
    if any(char.isdigit() for char in pred):
        if pred == '0':
            add=False
        else:
            add=True
        pred=pred.replace(',','.')
        if len(pred.replace(' ',''))==c:
            pred_dict['flag']=False
        else:
            pred_dict['flag']=True
        if 'Ra' in pred:
            pred_dict['type']='Roughness'
            pred_dict.update(nominal=pred, value=pred.replace('Ra',''))
        elif 'G' in pred[0] or 'M' in pred[0]:
            pred_dict['type']='Thread'
            pred_dict.update(nominal=pred, value=re.sub("[\(].*?[\)]", "", pred))
        elif ':' in pred:
            add=False
        elif '°' in pred and len(pred)<5:
            pred_dict['type']='Angle'
            pred_dict.update(nominal=pred, value=pred.replace('°',''))
        else:
            pred_dict['type']='Length'
            if ' ' in pred:
                split_pred=re.sub("[\(].*?[\)]", "", pred).split()
                if len(split_pred)==3:
                    pred_dict.update(nominal=pred, value=split_pred[0], upper_bound=split_pred[1], lower_bound=split_pred[2])
                else:
                    pred_dict.update(nominal=pred, value=re.sub("[\(].*?[\)]", "", pred),tolerance='general')
            elif 'h' in pred:
                split_pred=re.sub("[\(].*?[\)]", "", pred).split('h')
                pred_dict.update(nominal=pred, value=split_pred[0], tolerance='h'+''.join(split_pred[1]))
            elif 'H' in pred:
                split_pred=re.sub("[\(].*?[\)]", "", pred).split('H')
                pred_dict.update(nominal=pred, value=split_pred[0], tolerance='H'+''.join(split_pred[1]))

            elif '±' in pred:
                split_pred=re.sub("[\(].*?[\)]", "", pred).split('±')
                pred_dict.update(nominal=pred, value=split_pred[0], tolerance="±"+split_pred[1], upper_bound='+'+''.join(split_pred[1:]),lower_bound='-'+''.join(split_pred[1]))
                #print("Nominal =", pred, "Value =", split_pred[0], "Tolerance =", split_pred[1], "Upper bound =", '+'+''.join(split_pred[1:]), "Lower bound =", '-'+''.join(split_pred[1]))
            else:
                pred_dict.update(nominal=pred, value=re.sub("[\(].*?[\)]", "", pred), tolerance='general')
    else:
        add=False
    return pred_dict, add
######################################################################
def detect_the_patches(img,pipeline,patches_x=5,patches_y=4,ol=0.05,cluster_t=20):
    a_x=(1-ol)/(patches_x)
    b_x=a_x+ol
    a_y=(1-ol)/(patches_y)
    b_y=a_y+ol
    box_groups=[]
    for i in range(0, patches_x):
        for j in range(0, patches_y):
            offset=np.array([int(a_x*i*img.size[0]),int(a_y*j*img.size[1])])
            img1=img.copy()
            img_=img1.crop((offset[0],offset[1] ,int((i*a_x+b_x)*img.size[0]),int((j*a_y+b_y)*img.size[1])))
            img_=np.array(img_)
            box_group=pipeline.detect([img_])
            for b in box_group[0]:
                pts=[]
                for xy in b:
                    xy=xy+offset
                    pts.append(xy)
                box_groups.append(pts)
    box_groups=agglomerative_cluster(box_groups, threshold_distance=cluster_t)
    new_group=[box for box in box_groups]
    snippets=pipeline.recognize_dimensions(np.int32(new_group),np.array(img))
    return snippets

def read_dimensions(img,alphabet=None,weight_path=None,cluster_t=20):
    if alphabet and weight_path:
        recognizer =recognition.Recognizer(alphabet=alphabet)
        recognizer.model.load_weights(weight_path)
    else:
        recognizer =recognition.Recognizer()
    pipeline=Pipeline(recognizer=recognizer)
    img=Image.open(img)
    snippets=detect_the_patches(img,pipeline,cluster_t=cluster_t)
    return snippets