import cv2
import numpy as np


def check_tolerances(img):
    img_arr = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY) #Convert img to grayscale
    flag=False 
    ##Find top and bottom line
    for i in range(0, img_arr.shape[0]-1): #find top line
        for j in range(0,img_arr.shape[1]-1):
            if img_arr[i,j]<200:
                top_line=i
                flag=True
                break
        if flag==True:
            flag=False
            break
    for i in range(img_arr.shape[0]-1,top_line,-1): #find bottom line
        for j in range(0,img_arr.shape[1]-1):
            if img_arr[i,j]<200:
                bot_line=i
                flag=True
                break
        if flag==True:
            break        
    ##Measure distance from right end backwards until it finds a black pixel from top line to bottom line
    stop_at=[]
    for i in range(top_line,bot_line):
        for j in range(img_arr.shape[1]-1,0,-1):
            if img_arr[i,j]<200:
                stop_at.append(img_arr.shape[1]-j)
                break
        else:
            stop_at.append(img_arr.shape[1])
    ##Is there a normalized distance (l) relatively big with respect the others?
    for d in stop_at[int(0.3*len(stop_at)):int(0.7*len(stop_at))]:
        if d>img_arr.shape[0]*0.8:
            tole=True
            tole_h_cut=stop_at.index(d)+top_line+1
            break
        else:
            tole=False

    #If yes -> Find last character from the measurement (no tolerance)
    if tole==True:
        if d<img_arr.shape[1]: #handle error
            tole_v_cut=None
            for j in range(img_arr.shape[1]-d, img_arr.shape[1]):
                    if np.all(img_arr[int(0.3*img_arr.shape[0]):int(0.7*img_arr.shape[0]),j]>200):
                        tole_v_cut=j+2
                        break
            #-> crop images
            if tole_v_cut: #handle error
                try:
                    measu_box=img_arr[:,:tole_v_cut]
                    up_tole_box=img_arr[:tole_h_cut,tole_v_cut:]
                    bot_tole_box=img_arr[tole_h_cut:,tole_v_cut:]
                    return [cv2.cvtColor(measu_box, cv2.COLOR_GRAY2BGR),cv2.cvtColor(up_tole_box, cv2.COLOR_GRAY2BGR), cv2.cvtColor(bot_tole_box, cv2.COLOR_GRAY2BGR)]
                except:
                    return [img]  
        else:
            up_text=img_arr[:tole_h_cut,:]
            bot_text=img_arr[tole_h_cut:,:]
            return [cv2.cvtColor(up_text, cv2.COLOR_GRAY2BGR), cv2.cvtColor(bot_text, cv2.COLOR_GRAY2BGR) ] 
    return [img]
