import numpy as np
import cv2
import os
import csv

def mask_frame(cl,mask_img,color):
    color_full = np.full_like(mask_img,color)
    blend = 0.6
    img_color = cv2.addWeighted(mask_img, blend, color_full, 1-blend, 0)
    mask = np.zeros_like(mask_img)
    x1,y1,x2,y2 = cl.x,cl.y,cl.x+cl.w,cl.y+cl.h
    mask = cv2.rectangle(mask, (x1, y1), (x2, y2), (255,255,255), -1)
    result = np.where(mask==0, img_color, mask_img)
    return result

def mask_infobox(points,text,mask_img,color,color_flag,flag):
    if flag:
        color=color_flag
    mask = np.ones_like(mask_img) * 255
    cv2.fillPoly(mask, [points], color)
    img_with_overlay = np.int64(mask_img) * mask # <- use int64 terms
    max_px_val = np.amax(img_with_overlay) # <-- Max pixel alue
    img_with_overlay = np.uint8((img_with_overlay/max_px_val) * 255) # <- normalize and convert back to uint8
    cv2.putText(img_with_overlay, text, (points[0]),cv2.FONT_HERSHEY_COMPLEX, 2, color, 1,cv2.LINE_AA)
    return img_with_overlay

def mask_the_drawing(img, infoblock_dict, gdt_dict, dimension_dict,cl_frame,color_palette):
    mask_img=img.copy()
    mask_img=mask_frame(cl_frame,mask_img,color_palette.get('frame')) #Mask the frame
    for block in infoblock_dict:
        pred=block.get('text')
        text=str(pred.get('ID'))
        cl=block.get('rect')
        box= np.array([(cl.x, cl.y), (cl.x+cl.w, cl.y), (cl.x+cl.w, cl.y+cl.h),(cl.x,cl.y+cl.h)], np.int32)
        mask_img=mask_infobox(box,text,mask_img,color_palette.get('infoblock'),color_palette.get('flag'),False)
    for gdt in gdt_dict:
        pred=gdt.get('text')
        text=str(pred.get('ID'))
        cls=gdt.get('rect_list')
        flag=pred.get('flag')
        box=np.array([(cls[0].x, cls[0].y), (cls[0].x+cls[0].w, cls[0].y), (cls[0].x+cls[0].w, cls[0].y+cls[0].h),(cls[0].x,cls[0].y+cls[0].h)], np.int32)
        mask_img=mask_infobox(box,text,mask_img,color_palette.get('gdts'),color_palette.get('flag'),flag)
        for cl in cls[1:]:
            box=np.array([(cl.x, cl.y), (cl.x+cl.w, cl.y), (cl.x+cl.w, cl.y+cl.h),(cl.x,cl.y+cl.h)], np.int32)
            mask_img=mask_infobox(box,'',mask_img,color_palette.get('gdts'),color_palette.get('flag'),flag)
    for di in dimension_dict:
        pred=di.get('pred')
        text=str(pred.get('ID'))
        box=di.get('box')
        offset=(cl_frame.x,cl_frame.y)
        box=np.array([(box[0]+offset),(box[1]+offset),(box[2]+offset),(box[3]+offset)])
        flag=pred.get('flag')
        mask_img=mask_infobox(box,text,mask_img,color_palette.get('dimensions'),color_palette.get('flag'),flag)
    return mask_img

########################################
def record_data(dest_DIR,filename,infoblock_dict, gdt_dict,dimension_dict,index=0):
    field_names= ['ID','type', 'flag', 'nominal','value','tolerance','upper_bound','lower_bound']
    with open(os.path.join(dest_DIR, filename+'_'+str(index)+'_dimensions.csv'), 'w', encoding='utf-8') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=field_names)
        writer.writeheader()
        for p in dimension_dict:
            writer.writerow(p.get('pred'))

    field_names=['ID','flag','nominal','condition','tolerance']
    with open(os.path.join(dest_DIR, filename+'_'+str(index)+'_gdts.csv'), 'w', encoding='utf-8') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=field_names)
        writer.writeheader()
        for p in gdt_dict:
            writer.writerow(p.get('text'))

    field_names=['ID','nominal']
    with open(os.path.join(dest_DIR, filename+'_'+str(index)+'_tables.csv'), 'w', encoding='utf-8') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=field_names)
        writer.writeheader()
        for p in infoblock_dict:
            writer.writerow(p.get('text'))
