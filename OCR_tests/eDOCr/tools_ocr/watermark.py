def judge(x,y):
    temp = -(600.0/1575.0) * x
    if y > 1350 + temp and y < 1500 + temp:
        return True
    else:
        return False

def select_pixel(r,g,b):
    if (r == 208 and g == 208 and b == 208 ) or (r == 196 and g == 196 and b == 196) \
        or (r == 206 and g == 206 and b == 206 ):
        return True
    else:
        return False
def select_pixel2(r,g,b):
    if r > 175 and r < 250 and g > 175 and g < 250 and b > 175 and b < 250:
        return True
    else:
        return False
def handle(imgs):
    for  i in range(imgs.shape[0]):
        for j in range(imgs.shape[1]):
            if select_pixel2(imgs[i][j][0],imgs[i][j][1],imgs[i][j][2]):
                imgs[i][j][0] =  imgs[i][j][1] = imgs[i][j][2] = 255
    return imgs