#!/usr/bin/env python3

import numpy as np
import cv2
from sensor_msgs.msg import Image

img=np.ones((512,512,3))

def image_callback(self, data):
        try:
            #converting data to a cv2 standard image
            img = bridge.imgmsg_to_cv2(data, "bgr8")
            img_v = cv2.flip(img, 0)

            #img_v is the mirror image


rospy.Subscriber("/drone/camera/image_raw", Image,image_callback)
