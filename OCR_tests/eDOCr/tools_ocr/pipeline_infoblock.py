from .pipeline import Pipeline
from .recognition import Recognizer
from .tools import read, drawBoxes
from skimage import io
###################---Code Structure---######################################################################
#
#Classes
# |-> row()
#
#############################################################################################################
#
#Functions          
# |-> read_infoblocks(class_list,img) -> returns a list with classes with relevant text, the text predicted in them split in rows and columns,
# |   |-> boxhastext(cl.crop_img,pipeline) -> return the predicted text (unordered) in the class box
# |   |-> order_text(pred,img) -> returns the text (ordered in rows) given unstrutured text
# |   |   |-> get_distance(predictions) -> returns position information from unstructured words
# |   |   |-> distinguish_rows(words) -> order the text in rows of consequent words, identifies if the text belongs to a sentence or not
# |   |   |-> postprocess_text(text) ->

class row():
    def __init__(self, name, y, words, thres,size):
        self.name = name
        self.y=y
        self.words = words
        self.thres = thres
        self.size=size

def get_distance(predictions):
    '''returns position information from unstructured words
    Args: predictions: np array with bounding boxes and character information
    return: detections: list of dictionaries with word information '''
    x0 = 0 
    # Generate dictionary
    detections = []
    for group in predictions:
        # Get center point of bounding box
        top_left_x, top_left_y = group[1][0]
        bottom_right_x, bottom_right_y = group[1][2]
        center_x, center_y = (top_left_x + bottom_right_x)/2, (top_left_y + bottom_right_y)/2
        size=(bottom_right_x-top_left_x,bottom_right_y-top_left_y)
        # Calculate difference between x and origin
        distance_x = center_x - x0
        # Append all results
        detections.append({'text': group[0],'baseline': bottom_right_y,'distance_x': distance_x,'size': size,'thresh': size[1]/2})
    return detections

def distinguish_rows(words):
    """order the text in rows of consequent words, identifies if the text belongs to a sentence or not
    Args: words: list of dictionaries with word information
    return: rows: list of dictionaries with sentence information """
    words =sorted(words,key=lambda x:x['baseline'])
    row0=row('row_0', words[0]['baseline'], [words[0]],words[0]['thresh'],words[0]['size'])
    words=words[1:]
    rows=[row0]
    counter_row=0
    while not len(words)==0:
        words_to_del=[]
        for w in words:
            for r in rows:
                if w['baseline']-r.thres<r.y<w['baseline']+r.thres:
                    if  w['size'][1]-r.thres<r.size[1]<w['size'][1]+r.thres:
                        r.words.append(w)
                        words_to_del.append(w)
        words=[d for d in words if d not in words_to_del]
        if len(words)>0:
            counter_row=counter_row+1
            rows.append(row('row_'+str(counter_row), words[0]['baseline'], [words[0]],words[0]['thresh'],words[0]['size']))
            words=words[1:]
    return rows

def boxhastext(img_,pipeline):
    '''return the predicted text (unordered) from an image
    Args: 
        img_: image
        pipeline: detection and recognition pipeline from keras-ocr
    return:
        pred: np array with bounding boxes and character information'''
    img1=[read(img_)]
    pred=pipeline.recognize(img1)[0]
    
    return pred

def order_text(pred):
    '''returns the text (ordered in rows) from unstructured predictions
    Args: pred: np array with bounding boxes and character information
    return: text: list of strings, each element corresponds to a row'''
    words = get_distance(pred)
    rows = distinguish_rows(words)
    rows = list(filter(lambda x:x!=[], rows))
    text = []
    for r in rows:
        row_text=[]
        w=r.words
        w = sorted(w, key=lambda x:x['distance_x'])
        for each in w:
            row_text.append(each['text'])
        text.append(' '.join(row_text))
    return text

def read_infoblocks(class_list,img,alphabet=None,weight_path=None):
    '''Load alphabet and recognizer into keras-ocr pipeline to return text position and content
    Args:
        class_list: list of rect
        img: np array of engineering drawing
        alphabet: string with characters
        weight_path: path to the recognizer model
    return:
        boxes_w_text: list of dictionaries with id, rect and list of sentences in rect'''
    if alphabet and weight_path: 
        recognizer =Recognizer(alphabet=alphabet)
        recognizer.model.load_weights(weight_path)
        pipeline=Pipeline(recognizer=recognizer)
    else:
        pipeline=Pipeline()
    boxes_w_text=[]
    i=0
    for cl in class_list:        
        if len(cl.children) == 0:
            pred=boxhastext(cl.crop_img,pipeline)
            if pred:
                i+=1
                text={'ID':i,'nominal': '; '.join(order_text(pred))}
                boxes_w_text.append({'rect': cl, 'text':text})
                #img_crop = drawBoxes(image=cl.crop_img,boxes=pred,color=(94, 204, 243), thickness=1, boxes_format="predictions")
                #io.imsave('tests/'+str(i)+'_box.jpg',img_crop)
    return boxes_w_text



    
