# Copyright 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import math
import os.path
import cv2

import its.caps
import its.cv2image
import its.device
import its.image
import its.objects

import numpy as np

CIRCLE_COLOR = 0  # [0: black, 255: white]
CIRCLE_TOL = 0.05  # contour area vs ideal circle area pi*((w+h)/4)**2
LINE_COLOR = (255, 0, 0)  # red
LINE_THICKNESS = 5
MIN_AREA_RATIO = 0.00015  # based on 2000/(4000x3000) pixels
MIN_CIRCLE_PTS = 25
NAME = os.path.basename(__file__).split('.')[0]
NUM_STEPS = 10
OFFSET_RTOL = 0.10
RADIUS_RTOL = 0.10
ZOOM_MAX_THRESH = 10.0
ZOOM_MIN_THRESH = 2.0


def distance((x, y)):
    return math.sqrt(x**2 + y**2)


def circle_cropped(circle, size):
    """Determine if a circle is cropped by edge of img.

    Args:
        circle:         list; [x, y, radius] of circle
        size:           tuple; [x, y] size of img

    Returns:
        Boolean True if selected circle is cropped
    """

    cropped = False
    circle_x, circle_y = circle[0], circle[1]
    circle_r = circle[2]
    x_min, x_max = circle_x - circle_r, circle_x + circle_r
    y_min, y_max = circle_y - circle_r, circle_y + circle_r
    if x_min < 0 or y_min < 0 or x_max > size[0] or y_max > size[1]:
        cropped = True
    return cropped


def find_center_circle(img, name, color, min_area, debug):
    """Find the circle closest to the center of the image.

    Finds all contours in the image. Rejects those too small and not enough
    points to qualify as a circle. The remaining contours must have center
    point of color=color and are sorted based on distance from the center
    of the image. The contour closest to the center of the image is returned.

    Note: hierarchy is not used as the hierarchy for black circles changes
    as the zoom level changes.

    Args:
        img:            numpy img array with pixel values in [0,255].
        name:           str; file name
        color:          int; 0: black, 255: white
        min_area:       int; minimum area of circles to screen out
        debug:          bool; save extra data

    Returns:
        circle:         [center_x, center_y, radius]
    """

    # gray scale & otsu threshold to binarize the image
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    _, img_bw = cv2.threshold(np.uint8(gray), 0, 255,
                              cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    # use OpenCV to find contours (connected components)
    cv2_version = cv2.__version__
    if cv2_version.startswith('3.'): # OpenCV 3.x
        _, contours, _ = cv2.findContours(
                255-img_bw, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    else: # OpenCV 2.x and 4.x
        contours, _ = cv2.findContours(
                255-img_bw, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

    # check contours and find the best circle candidates
    circles = []
    img_ctr = [gray.shape[1]/2, gray.shape[0]/2]
    for contour in contours:
        area = cv2.contourArea(contour)
        if area > min_area and len(contour) >= MIN_CIRCLE_PTS:
            shape = its.cv2image.component_shape(contour)
            radius = (shape['width'] + shape['height']) / 4
            colour = img_bw[shape['cty']][shape['ctx']]
            circlish = round((math.pi * radius**2) / area, 4)
            if colour == color and (1-CIRCLE_TOL <= circlish <= 1+CIRCLE_TOL):
                circles.append([shape['ctx'], shape['cty'], radius, circlish,
                                area])

    if debug:
        circles.sort(key=lambda x: abs(x[3]-1.0))  # sort for best circles
        print 'circles [x, y, r, pi*r**2/area, area]:', circles

    # find circle closest to center
    circles.sort(key=lambda x: distance((x[0]-img_ctr[0], x[1]-img_ctr[1])))
    circle = circles[0]

    # mark image center
    size = gray.shape
    m_x, m_y = size[1]/2, size[0]/2
    marker_size = LINE_THICKNESS * 10
    if cv2_version.startswith('2.4.'):
        cv2.line(img, (m_x-marker_size/2, m_y), (m_x+marker_size/2, m_y),
                 LINE_COLOR, LINE_THICKNESS)
        cv2.line(img, (m_x, m_y-marker_size/2), (m_x, m_y+marker_size/2),
                 LINE_COLOR, LINE_THICKNESS)
    elif cv2_version.startswith('3.2.'):
        cv2.drawMarker(img, (m_x, m_y), LINE_COLOR,
                       markerType=cv2.MARKER_CROSS,
                       markerSize=marker_size,
                       thickness=LINE_THICKNESS)

    # add circle to saved image
    center_i = (int(round(circle[0], 0)), int(round(circle[1], 0)))
    radius_i = int(round(circle[2], 0))
    cv2.circle(img, center_i, radius_i, LINE_COLOR, LINE_THICKNESS)
    its.image.write_image(img/255.0, name)

    if not circles:
        print 'No circle was detected. Please take pictures according',
        print 'to instruction carefully!\n'
        assert False

    return [circle[0], circle[1], circle[2]]


def main():
    """Test the camera zoom behavior."""

    z_test_list = []
    fls = []
    circles = []
    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.zoom_ratio_range(props))

        z_range = props['android.control.zoomRatioRange']
        print 'testing zoomRatioRange:', z_range
        yuv_size = its.objects.get_largest_yuv_format(props)
        size = [yuv_size['width'], yuv_size['height']]
        debug = its.caps.debug_mode()

        z_min, z_max = float(z_range[0]), float(z_range[1])
        its.caps.skip_unless(z_max >= z_min*ZOOM_MIN_THRESH)
        z_list = np.arange(z_min, z_max, float(z_max-z_min)/(NUM_STEPS-1))
        z_list = np.append(z_list, z_max)

        # do captures over zoom range
        req = its.objects.auto_capture_request()
        for i, z in enumerate(z_list):
            print 'zoom ratio: %.2f' % z
            req['android.control.zoomRatio'] = z
            cap = cam.do_capture(req, cam.CAP_YUV)
            img = its.image.convert_capture_to_rgb_image(cap, props=props)

            # convert to [0, 255] images with unsigned integer
            img *= 255
            img = img.astype(np.uint8)

            # Find the circles in img
            circle = find_center_circle(
                    img, '%s_%s.jpg' % (NAME, round(z, 2)), CIRCLE_COLOR,
                    min_area=MIN_AREA_RATIO*size[0]*size[1]*z*z, debug=debug)
            if circle_cropped(circle, size):
                print 'zoom %.2f is too large! Skip further captures' % z
                break
            circles.append(circle)
            z_test_list.append(z)
            fls.append(cap['metadata']['android.lens.focalLength'])

    # assert some range is tested before circles get too big
    zoom_max_thresh = ZOOM_MAX_THRESH
    if z_max < ZOOM_MAX_THRESH:
        zoom_max_thresh = z_max
    msg = 'Max zoom level tested: %d, THRESH: %d' % (
            z_test_list[-1], zoom_max_thresh)
    assert z_test_list[-1] >= zoom_max_thresh, msg

    # initialize relative size w/ zoom[0] for diff zoom ratio checks
    radius_0 = float(circles[0][2])
    z_0 = float(z_test_list[0])

    for i, z in enumerate(z_test_list):
        print '\nZoom: %.2f, fl: %.2f' % (z, fls[i])
        offset_abs = ((circles[i][0] - size[0]/2), (circles[i][1] - size[1]/2))
        print 'Circle r: %.1f, center offset x, y: %d, %d' % (
                circles[i][2], offset_abs[0], offset_abs[1])
        z_ratio = z / z_0

        # check relative size against zoom[0]
        radius_ratio = circles[i][2]/radius_0
        print 'radius_ratio: %.3f' % radius_ratio
        msg = 'zoom: %.2f, radius ratio: %.2f, RTOL: %.2f' % (
                z_ratio, radius_ratio, RADIUS_RTOL)
        assert np.isclose(z_ratio, radius_ratio, rtol=RADIUS_RTOL), msg

        # check relative offset against init vals w/ no focal length change
        if i == 0 or fls[i-1] != fls[i]:  # set init values
            z_init = float(z_test_list[i])
            offset_init = (circles[i][0] - size[0] / 2,
                           circles[i][1] - size[1] / 2)
        else:  # check
            z_ratio = z / z_init
            offset_rel = (distance(offset_abs) / z_ratio /
                          distance(offset_init))
            print 'offset_rel: %.3f' % offset_rel
            msg = 'zoom: %.2f, offset(rel): %.2f, RTOL: %.2f' % (
                    z, offset_rel, OFFSET_RTOL)
            assert np.isclose(offset_rel, 1.0, rtol=OFFSET_RTOL), msg


if __name__ == '__main__':
    main()
