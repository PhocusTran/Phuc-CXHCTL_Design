from .recognition import Recognizer
import cv2

def recognize_gdts(gdt_boxes,recognizer):
    '''For a gdt cluster, returns the text in every box separated by a |
    Args:
        gdt_boxes: list of list of rect
        recognizer: recoginzer from keras_ocr configured for gdts
    return: string of text
    '''
    pred=''
    flag=False
        
    if -3<gdt_boxes[0].x-gdt_boxes[1].x<3:
        gdt_boxes= sorted(gdt_boxes, key=lambda x: x.y, reverse=True)
        for box in gdt_boxes:
            gray = cv2.cvtColor(box.crop_img, cv2.COLOR_BGR2GRAY) #Convert img to grayscale
            _,thresh = cv2.threshold(gray,127,255,cv2.THRESH_BINARY_INV) #Threshold to binary image
            cnts = cv2.findContours(thresh,cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[0] #Get contourns
            p=recognizer.recognize(image=cv2.rotate(box.crop_img,cv2.ROTATE_90_CLOCKWISE))
            pred+=p+'|'
            if len(p)!=len(cnts):
                flag=True
    else:
        gdt_boxes= sorted(gdt_boxes, key=lambda x: x.x, reverse=False)
        for box in gdt_boxes:
            gray = cv2.cvtColor(box.crop_img, cv2.COLOR_BGR2GRAY) #Convert img to grayscale
            _,thresh = cv2.threshold(gray,127,255,cv2.THRESH_BINARY_INV) #Threshold to binary image
            cnts = cv2.findContours(thresh,cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[0] #Get contourns
            p=recognizer.recognize(image=box.crop_img)
            pred+=p+'|'
            if len(p)!=len(cnts):
                flag=True
    pred=pred[:-1]
    return pred,flag

def recognize_gdts1(gdt_boxes,recognizer,recognizer_d):
    '''For a gdt cluster, returns the text in every box separated by a |
    Args:
        gdt_boxes: list of list of rect
        recognizer: recoginzer from keras_ocr configured for gdts
    return: string of text
    '''
    flag=False
    if -3<gdt_boxes[0].x-gdt_boxes[1].x<3:
        gdt_boxes= sorted(gdt_boxes, key=lambda x: x.y, reverse=True)
        pred=recognizer.recognize(image=cv2.rotate(gdt_boxes[0].crop_img,cv2.ROTATE_90_CLOCKWISE))+'|'
        for box in gdt_boxes[1:]:
            p=recognizer_d.recognize(image=cv2.rotate(box.crop_img,cv2.ROTATE_90_CLOCKWISE))
            pred+=p+'|'
    else:
        gdt_boxes= sorted(gdt_boxes, key=lambda x: x.x, reverse=False)
        pred=recognizer.recognize(image=gdt_boxes[0].crop_img)+'|'
        for box in gdt_boxes[1:]:
            p=recognizer_d.recognize(image=box.crop_img)
            pred+=p+'|'
    pred=pred[:-1]
    return pred,flag


def postprocess_gdt(pred,flag):
    pred=pred.replace(',','.')
    split_pred=pred.split('|')
    text={'flag':flag,'nominal':pred, 'condition': split_pred[0], 'tolerance':split_pred[1]}
    return text

def read_gdtbox(gdt_boxes,alphabet=None,weight_path=None):
    '''gets information from gdt boxes
    Args:
        gdt_boxes: list of list of rect
        alphabet: string of characters
        weight_path: path to recognizer weights for gdt
    return: list of dictionaries with Id, cluster of rect and its predicted text
    '''
    if alphabet and weight_path: 
        recognizer =Recognizer(alphabet=alphabet)
        recognizer.model.load_weights(weight_path)
    else:
        recognizer =Recognizer()
    predictions=[]
    i=0
    for box in gdt_boxes:
        pred,flag=recognize_gdts(box,recognizer)
        text=postprocess_gdt(pred,flag)
        i+=1
        pred_id={'ID':i}
        pred_id.update(text)
        predictions.append({'rect_list': box, 'text':pred_id})
    return predictions

def read_gdtbox1(gdt_boxes,alphabet=None,weight_path=None,alphabet_d=None,weight_path_d=None):
    '''gets information from gdt boxes
    Args:
        gdt_boxes: list of list of rect
        alphabet: string of characters
        weight_path: path to recognizer weights for gdt
    return: list of dictionaries with Id, cluster of rect and its predicted text
    '''
    if alphabet and weight_path: 
        recognizer =Recognizer(alphabet=alphabet)
        recognizer.model.load_weights(weight_path)
    else:
        recognizer =Recognizer()

    if alphabet_d and weight_path_d: 
        recognizer_d =Recognizer(alphabet=alphabet_d)
        recognizer_d.model.load_weights(weight_path_d)
    else:
        recognizer_d =Recognizer()

    predictions=[]
    i=0
    for box in gdt_boxes:
        pred,flag=recognize_gdts1(box,recognizer,recognizer_d)
        if any(char.isdigit() for char in pred):
            text=postprocess_gdt(pred,flag)
            i+=1
            pred_id={'ID':i}
            pred_id.update(text)
            predictions.append({'rect_list': box, 'text':pred_id})
    return predictions
