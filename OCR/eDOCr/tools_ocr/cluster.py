import math
import numpy as np
from shapely import affinity
from shapely.geometry import Polygon, MultiPoint

def get_scale_factors(poly,t):
    '''Calculates the scale factor needed to get a box with the size of the box plus an offset
    Args:
        poly: a Polygon instance from shapely
        t: offset
    Returns: xfact: scale factor'''
    C=list(poly.centroid.coords)[0]
    A=poly.exterior.coords[0]
    D=poly.exterior.coords[1]
    #let's do trigonometry! yei!
    a=math.sqrt((A[0]-C[0])**2+(A[1]-C[1])**2)
    if A[0]==D[0]:
        b=t
    else:
        alfa=math.atan((C[1]-A[1])/(C[0]-A[0]))
        beta=math.atan((D[1]-A[1])/(D[0]-A[0]))
        phi=math.atan((D[1]-C[1])/(D[0]-C[0]))
        if alfa+math.pi-phi>math.pi/2:
            b=t/math.cos(beta-alfa)
        else:
            b=t/math.sin(beta-alfa)
    xfact=abs((a+b)/a)
    return xfact, xfact

def merge_poly(poly1,poly2):
    '''Given two Polygons, get the minimum rotated bounding rectangle
    Args:
        poly1: Polygon
        poly2:Polygon
    returns: poly_merged: Merged Polygon'''
    a=list(poly1.exterior.coords)[0:4]
    b=list(poly2.exterior.coords)[0:4]
    pts=np.concatenate((a,b),axis=0)
    poly_merged=MultiPoint(pts).minimum_rotated_rectangle
    return poly_merged

def agglomerative_cluster(box_groups, threshold_distance=10.0):
    '''For a list containing np arrays of bounding boxes, it returns a new list with merged boundng boxes from the previous list,
    the criteria to merge polygons is the distance speficied with a threshold
    Args:
        box_groups: list of bounding boxes
        threshold_distance: Float value that stablish the offset to merge polygons
    returns: new_group_: new list of bounding boxes'''
    new_group=[]
    box_groups_=[]
    for box in box_groups:
        box_groups_.append({'poly':Polygon(box), 'c':0})
    while len(box_groups_):
        remove_list=[]
        box1= box_groups_[0]
        poly1_=box1.get('poly')
        xfact,yfact=get_scale_factors(poly1_,threshold_distance)
        poly1=affinity.scale(poly1_,xfact=xfact,yfact=yfact)
        for box2 in box_groups_[1:]:
            poly2=box2.get('poly')
            if poly1.intersects(poly2):
                poly_merged=merge_poly(poly1_,poly2)
                box_groups_.append({'poly':poly_merged,'c':box1.get('c')+box2.get('c')+1})
                remove_list.append(box1)
                remove_list.append(box2)
                break
        if box1 not in remove_list:
            new_group.append(box1)
            remove_list.append(box1)
        box_groups_=[b for b in box_groups_ if b not in remove_list]
    new_group_=[]
    for box in new_group:
        poly=box.get('poly')
        new_group_.append(poly.exterior.coords[0:4])
    return new_group_