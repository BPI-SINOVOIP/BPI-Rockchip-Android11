# Copyright 2015 The Android Open Source Project (lint as: python2)
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

FOV_PERCENT_RTOL = 0.15  # Relative tolerance on circle FoV % to expected
LARGE_SIZE = 2000   # Define the size of a large image (compare against max(w,h))
NAME = os.path.basename(__file__).split(".")[0]
NUM_DISTORT_PARAMS = 5
THRESH_L_AR = 0.02  # aspect ratio test threshold of large images
THRESH_XS_AR = 0.075  # aspect ratio test threshold of mini images
THRESH_L_CP = 0.02  # Crop test threshold of large images
THRESH_XS_CP = 0.075  # Crop test threshold of mini images
THRESH_MIN_PIXEL = 4  # Crop test allowed offset
PREVIEW_SIZE = (1920, 1080)  # preview size

# Before API level 30, only resolutions with the following listed aspect ratio
# are checked. Device launched after API level 30 will need to pass the test
# for all advertised resolutions. Device launched before API level 30 just
# needs to pass the test for all resolutions within these aspect ratios.
AR_CHECKED_PRE_API_30 = ["4:3", "16:9", "18:9"]
AR_DIFF_ATOL = 0.01


def print_failed_test_results(failed_ar, failed_fov, failed_crop):
    """Print failed test results."""
    if failed_ar:
        print "\nAspect ratio test summary"
        print "Images failed in the aspect ratio test:"
        print "Aspect ratio value: width / height"
        for fa in failed_ar:
            print "%s with %s %dx%d: %.3f;" % (
                    fa["fmt_iter"], fa["fmt_cmpr"],
                    fa["w"], fa["h"], fa["ar"]),
            print "valid range: %.3f ~ %.3f" % (
                    fa["valid_range"][0], fa["valid_range"][1])

    if failed_fov:
        print "\nFoV test summary"
        print "Images failed in the FoV test:"
        for fov in failed_fov:
            print fov

    if failed_crop:
        print "\nCrop test summary"
        print "Images failed in the crop test:"
        print "Circle center position, (horizontal x vertical), listed",
        print "below is relative to the image center."
        for fc in failed_crop:
            print "%s with %s %dx%d: %.3f x %.3f;" % (
                    fc["fmt_iter"], fc["fmt_cmpr"], fc["w"], fc["h"],
                    fc["ct_hori"], fc["ct_vert"]),
            print "valid horizontal range: %.3f ~ %.3f;" % (
                    fc["valid_range_h"][0], fc["valid_range_h"][1]),
            print "valid vertical range: %.3f ~ %.3f" % (
                    fc["valid_range_v"][0], fc["valid_range_v"][1])


def is_checked_aspect_ratio(first_api_level, w, h):
    if first_api_level >= 30:
        return True

    for ar_check in AR_CHECKED_PRE_API_30:
        match_ar_list = [float(x) for x in ar_check.split(":")]
        match_ar = match_ar_list[0] / match_ar_list[1]
        if np.isclose(float(w)/h, match_ar, atol=AR_DIFF_ATOL):
            return True

    return False

def calc_expected_circle_image_ratio(ref_fov, img_w, img_h):
    """Determine the circle image area ratio in percentage for a given image size.

    Args:
        ref_fov:    dict with [fmt, % coverage, w, h, circle_w, circle_h]
        img_w:      the image width
        img_h:      the image height

    Returns:
        chk_percent: the expected circle image area ratio in percentage
    """

    ar_ref = float(ref_fov["w"]) / ref_fov["h"]
    ar_target = float(img_w) / img_h
    # The cropping will happen either horizontally or vertically.
    # In both case a crop results in the visble area reduce by a ratio r (r < 1.0)
    # and the circle will in turn occupy ref_pct / r (percent) on the target
    # image size.
    r = ar_ref / ar_target
    if r < 1.0:
        r = 1.0 / r
    return ref_fov["percent"] * r


def find_raw_fov_reference(cam, req, props, debug):
    """Determine the circle coverage of the image in RAW reference image.

    Args:
        cam:        camera object
        req:        camera request
        props:      camera properties
        debug:      perform debug dump or not

    Returns:
        ref_fov:         dict with [fmt, % coverage, w, h, circle_w, circle_h]
        cc_ct_gt:        circle center position relative to the center of image.
        aspect_ratio_gt: aspect ratio of the detected circle in float.
    """

    # Capture full-frame raw. Use its aspect ratio and circle center
    # location as ground truth for the other jpeg or yuv images.
    print "Creating references for fov_coverage from RAW"
    out_surface = {"format": "raw"}
    cap_raw = cam.do_capture(req, out_surface)
    print "Captured %s %dx%d" % ("raw", cap_raw["width"],
                                 cap_raw["height"])
    img_raw = its.image.convert_capture_to_rgb_image(cap_raw,
                                                     props=props)

    # The intrinsics and distortion coefficients are meant for full
    # size RAW, but convert_capture_to_rgb_image returns a 2x downsampled
    # version, so resize back to full size here.
    img_raw = cv2.resize(img_raw, (0, 0), fx=2.0, fy=2.0)

    # If the device supports lens distortion correction, apply the
    # coefficients on the RAW image so it can be compared to YUV/JPEG
    # outputs which are subject to the same correction via ISP.
    if its.caps.distortion_correction(props):
        # Intrinsic cal is of format: [f_x, f_y, c_x, c_y, s]
        # [f_x, f_y] is the horizontal and vertical focal lengths,
        # [c_x, c_y] is the position of the optical axis,
        # and s is skew of sensor plane vs lens plane.
        print "Applying intrinsic calibration and distortion params"
        ical = np.array(props["android.lens.intrinsicCalibration"])
        msg = "Cannot include lens distortion without intrinsic cal!"
        assert len(ical) == 5, msg
        sensor_h = props["android.sensor.info.physicalSize"]["height"]
        sensor_w = props["android.sensor.info.physicalSize"]["width"]
        pixel_h = props["android.sensor.info.pixelArraySize"]["height"]
        pixel_w = props["android.sensor.info.pixelArraySize"]["width"]
        fd = float(cap_raw["metadata"]["android.lens.focalLength"])
        fd_w_pix = pixel_w * fd / sensor_w
        fd_h_pix = pixel_h * fd / sensor_h
        # transformation matrix
        # k = [[f_x, s, c_x],
        #      [0, f_y, c_y],
        #      [0,   0,   1]]
        k = np.array([[ical[0], ical[4], ical[2]],
                      [0, ical[1], ical[3]],
                      [0, 0, 1]])
        print "k:", k
        e_msg = "fd_w(pixels): %.2f\tcal[0](pixels): %.2f\tTOL=20%%" % (
                fd_w_pix, ical[0])
        assert np.isclose(fd_w_pix, ical[0], rtol=0.20), e_msg
        e_msg = "fd_h(pixels): %.2f\tcal[1](pixels): %.2f\tTOL=20%%" % (
                fd_h_pix, ical[0])
        assert np.isclose(fd_h_pix, ical[1], rtol=0.20), e_msg

        # distortion
        rad_dist = props["android.lens.distortion"]
        print "android.lens.distortion:", rad_dist
        e_msg = "%s param(s) found. %d expected." % (len(rad_dist),
                                                     NUM_DISTORT_PARAMS)
        assert len(rad_dist) == NUM_DISTORT_PARAMS, e_msg
        opencv_dist = np.array([rad_dist[0], rad_dist[1],
                                rad_dist[3], rad_dist[4],
                                rad_dist[2]])
        print "dist:", opencv_dist
        img_raw = cv2.undistort(img_raw, k, opencv_dist)
    size_raw = img_raw.shape
    w_raw = size_raw[1]
    h_raw = size_raw[0]
    img_name = "%s_%s_w%d_h%d.png" % (NAME, "raw", w_raw, h_raw)
    its.image.write_image(img_raw, img_name, True)
    aspect_ratio_gt, cc_ct_gt, circle_size_raw = measure_aspect_ratio(
            img_raw, img_name, True, debug)
    raw_fov_percent = calc_circle_image_ratio(
            circle_size_raw[0], circle_size_raw[1], w_raw, h_raw)
    ref_fov = {}
    ref_fov["fmt"] = "RAW"
    ref_fov["percent"] = raw_fov_percent
    ref_fov["w"] = w_raw
    ref_fov["h"] = h_raw
    ref_fov["circle_w"] = circle_size_raw[0]
    ref_fov["circle_h"] = circle_size_raw[1]
    print "Using RAW reference:", ref_fov
    return ref_fov, cc_ct_gt, aspect_ratio_gt


def find_jpeg_fov_reference(cam, req, props):
    """Determine the circle coverage of the image in JPEG reference image.

    Args:
        cam:        camera object
        req:        camera request
        props:      camera properties

    Returns:
        ref_fov:    dict with [fmt, % coverage, w, h, circle_w, circle_h]
        cc_ct_gt:   circle center position relative to the center of image.
    """
    ref_fov = {}
    fmt = its.objects.get_largest_jpeg_format(props)
    # capture and determine circle area in image
    cap = cam.do_capture(req, fmt)
    w = cap["width"]
    h = cap["height"]

    img = its.image.convert_capture_to_rgb_image(cap, props=props)
    print "Captured JPEG %dx%d" % (w, h)
    img_name = "%s_jpeg_w%d_h%d.png" % (NAME, w, h)
    # Set debug to True to save the reference image
    _, cc_ct_gt, circle_size = measure_aspect_ratio(img, img_name, False, debug=True)
    fov_percent = calc_circle_image_ratio(circle_size[0], circle_size[1], w, h)
    ref_fov["fmt"] = "JPEG"
    ref_fov["percent"] = fov_percent
    ref_fov["w"] = w
    ref_fov["h"] = h
    ref_fov["circle_w"] = circle_size[0]
    ref_fov["circle_h"] = circle_size[1]
    print "Using JPEG reference:", ref_fov
    return ref_fov, cc_ct_gt


def calc_circle_image_ratio(circle_w, circle_h, image_w, image_h):
    """Calculate the percent of area the input circle covers in input image.

    Args:
        circle_w (int):      width of circle
        circle_h (int):      height of circle
        image_w (int):       width of image
        image_h (int):       height of image
    Returns:
        fov_percent (float): % of image covered by circle
    """
    circle_area = math.pi * math.pow(np.mean([circle_w, circle_h])/2.0, 2)
    image_area = image_w * image_h
    fov_percent = 100*circle_area/image_area
    return fov_percent


def measure_aspect_ratio(img, img_name, raw_avlb, debug):
    """Measure the aspect ratio of the black circle in the test image.

    Args:
        img: Numpy float image array in RGB, with pixel values in [0,1].
        img_name: string with image info of format and size.
        raw_avlb: True: raw capture is available; False: raw capture is not
             available.
        debug: boolean for whether in debug mode.
    Returns:
        aspect_ratio: aspect ratio number in float.
        cc_ct: circle center position relative to the center of image.
        (circle_w, circle_h): tuple of the circle size
    """
    size = img.shape
    img *= 255
    # Gray image
    img_gray = 0.299*img[:, :, 2] + 0.587*img[:, :, 1] + 0.114*img[:, :, 0]

    # otsu threshold to binarize the image
    _, img_bw = cv2.threshold(np.uint8(img_gray), 0, 255,
                              cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    # connected component
    cv2_version = cv2.__version__
    if cv2_version.startswith('3.'): # OpenCV 3.x
        _, contours, hierarchy = cv2.findContours(
                255-img_bw, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    else: # OpenCV 2.x and 4.x
        contours, hierarchy = cv2.findContours(
                255-img_bw, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

    # Check each component and find the black circle
    min_cmpt = size[0] * size[1] * 0.005
    max_cmpt = size[0] * size[1] * 0.35
    num_circle = 0
    aspect_ratio = 0
    for ct, hrch in zip(contours, hierarchy[0]):
        # The radius of the circle is 1/3 of the length of the square, meaning
        # around 1/3 of the area of the square
        # Parental component should exist and the area is acceptable.
        # The coutour of a circle should have at least 5 points
        child_area = cv2.contourArea(ct)
        if (hrch[3] == -1 or child_area < min_cmpt or child_area > max_cmpt
                    or len(ct) < 15):
            continue
        # Check the shapes of current component and its parent
        child_shape = its.cv2image.component_shape(ct)
        parent = hrch[3]
        prt_shape = its.cv2image.component_shape(contours[parent])
        prt_area = cv2.contourArea(contours[parent])
        dist_x = abs(child_shape["ctx"]-prt_shape["ctx"])
        dist_y = abs(child_shape["cty"]-prt_shape["cty"])
        # 1. 0.56*Parent"s width < Child"s width < 0.76*Parent"s width.
        # 2. 0.56*Parent"s height < Child"s height < 0.76*Parent"s height.
        # 3. Child"s width > 0.1*Image width
        # 4. Child"s height > 0.1*Image height
        # 5. 0.25*Parent"s area < Child"s area < 0.45*Parent"s area
        # 6. Child == 0, and Parent == 255
        # 7. Center of Child and center of parent should overlap
        if (prt_shape["width"] * 0.56 < child_shape["width"]
                    < prt_shape["width"] * 0.76
                    and prt_shape["height"] * 0.56 < child_shape["height"]
                    < prt_shape["height"] * 0.76
                    and child_shape["width"] > 0.1 * size[1]
                    and child_shape["height"] > 0.1 * size[0]
                    and 0.30 * prt_area < child_area < 0.50 * prt_area
                    and img_bw[child_shape["cty"]][child_shape["ctx"]] == 0
                    and img_bw[child_shape["top"]][child_shape["left"]] == 255
                    and dist_x < 0.1 * child_shape["width"]
                    and dist_y < 0.1 * child_shape["height"]):
            # If raw capture is not available, check the camera is placed right
            # in front of the test page:
            # 1. Distances between parent and child horizontally on both side,0
            #    dist_left and dist_right, should be close.
            # 2. Distances between parent and child vertically on both side,
            #    dist_top and dist_bottom, should be close.
            if not raw_avlb:
                dist_left = child_shape["left"] - prt_shape["left"]
                dist_right = prt_shape["right"] - child_shape["right"]
                dist_top = child_shape["top"] - prt_shape["top"]
                dist_bottom = prt_shape["bottom"] - child_shape["bottom"]
                if (abs(dist_left-dist_right) > 0.05 * child_shape["width"]
                            or abs(dist_top-dist_bottom) > 0.05 * child_shape["height"]):
                    continue
            # Calculate aspect ratio
            aspect_ratio = float(child_shape["width"]) / child_shape["height"]
            circle_ctx = child_shape["ctx"]
            circle_cty = child_shape["cty"]
            circle_w = float(child_shape["width"])
            circle_h = float(child_shape["height"])
            cc_ct = {"hori": float(child_shape["ctx"]-size[1]/2) / circle_w,
                     "vert": float(child_shape["cty"]-size[0]/2) / circle_h}
            num_circle += 1
            # If more than one circle found, break
            if num_circle == 2:
                break

    if num_circle == 0:
        its.image.write_image(img/255, img_name, True)
        print "No black circle was detected. Please take pictures according",
        print "to instruction carefully!\n"
        assert num_circle == 1

    if num_circle > 1:
        its.image.write_image(img/255, img_name, True)
        print "More than one black circle was detected. Background of scene",
        print "may be too complex.\n"
        assert num_circle == 1

    # draw circle center and image center, and save the image
    line_width = max(1, max(size)/500)
    move_text_dist = line_width * 3
    cv2.line(img, (circle_ctx, circle_cty), (size[1]/2, size[0]/2),
             (255, 0, 0), line_width)
    if circle_cty > size[0]/2:
        move_text_down_circle = 4
        move_text_down_image = -1
    else:
        move_text_down_circle = -1
        move_text_down_image = 4
    if circle_ctx > size[1]/2:
        move_text_right_circle = 2
        move_text_right_image = -1
    else:
        move_text_right_circle = -1
        move_text_right_image = 2
    # circle center
    text_circle_x = move_text_dist * move_text_right_circle + circle_ctx
    text_circle_y = move_text_dist * move_text_down_circle + circle_cty
    cv2.circle(img, (circle_ctx, circle_cty), line_width*2, (255, 0, 0), -1)
    cv2.putText(img, "circle center", (text_circle_x, text_circle_y),
                cv2.FONT_HERSHEY_SIMPLEX, line_width/2.0, (255, 0, 0),
                line_width)
    # image center
    text_imgct_x = move_text_dist * move_text_right_image + size[1]/2
    text_imgct_y = move_text_dist * move_text_down_image + size[0]/2
    cv2.circle(img, (size[1]/2, size[0]/2), line_width*2, (255, 0, 0), -1)
    cv2.putText(img, "image center", (text_imgct_x, text_imgct_y),
                cv2.FONT_HERSHEY_SIMPLEX, line_width/2.0, (255, 0, 0),
                line_width)
    if debug:
        its.image.write_image(img/255, img_name, True)

    print "Aspect ratio: %.3f" % aspect_ratio
    print "Circle center position wrt to image center:",
    print "%.3fx%.3f" % (cc_ct["vert"], cc_ct["hori"])
    return aspect_ratio, cc_ct, (circle_w, circle_h)


def main():
    """Test aspect ratio/field of view (FOV)/cropping for each tested formats combinations.

    This test checks for:
      1. Aspect ratio: images are not stretched
      2. Crop: center of images is always center of the image sensor no matter
         how the image is cropped from sensor's full FOV
      3. FOV: images are always cropped to keep the maximum possible FOV with
         only one dimension (horizontal or veritical) cropped.

    Aspect ratio and FOV test runs on level3, full and limited devices.
    Crop test only runs on full and level3 devices.

    The test chart is a black circle inside a black square. When raw capture is
    available, set the height vs. width ratio of the circle in the full-frame
    raw as ground truth. In an ideal setup such ratio should be very close to
    1.0, but here we just use the value derived from full resolution RAW as
    ground truth to account for the possiblity that the chart is not well
    positioned to be precisely parallel to image sensor plane.
    The test then compare the ground truth ratio with the same ratio measured
    on images captued using different stream combinations of varying formats
    ("jpeg" and "yuv") and resolutions.
    If raw capture is unavailable, a full resolution JPEG image is used to setup
    ground truth. In this case, the ground truth aspect ratio is defined as 1.0
    and it is the tester's responsibility to make sure the test chart is
    properly positioned so the detected circles indeed have aspect ratio close
    to 1.0 assuming no bugs causing image stretched.

    The aspect ratio test checks the aspect ratio of the detected circle and
    it will fail if the aspect ratio differs too much from the ground truth
    aspect ratio mentioned above.

    The FOV test examines the ratio between the detected circle area and the
    image size. When the aspect ratio of the test image is the same as the
    ground truth image, the ratio should be very close to the ground truth
    value. When the aspect ratio is different, the difference is factored in
    per the expectation of the Camera2 API specification, which mandates the
    FOV reduction from full sensor area must only occur in one dimension:
    horizontally or vertically, and never both. For example, let's say a sensor
    has a 16:10 full sensor FOV. For all 16:10 output images there should be no
    FOV reduction on them. For 16:9 output images the FOV should be vertically
    cropped by 9/10. For 4:3 output images the FOV should be cropped
    horizontally instead and the ratio (r) can be calculated as follows:
        (16 * r) / 10 = 4 / 3 => r = 40 / 48 = 0.8333
    Say the circle is covering x percent of the 16:10 sensor on the full 16:10
    FOV, and assume the circle in the center will never be cut in any output
    sizes (this can be achieved by picking the right size and position of the
    test circle), the from above cropping expectation we can derive on a 16:9
    output image the circle will cover (x / 0.9) percent of the 16:9 image; on
    a 4:3 output image the circle will cover (x / 0.8333) percent of the 4:3
    image.

    The crop test checks that the center of any output image remains aligned
    with center of sensor's active area, no matter what kind of cropping or
    scaling is applied. The test verified that by checking the relative vector
    from the image center to the center of detected circle remains unchanged.
    The relative part is normalized by the detected circle size to account for
    scaling effect.
    """
    aspect_ratio_gt = 1.0  # Ground truth circle width/height ratio.
                           # If full resolution RAW is available as reference
                           # then this will be updated to the value measured on
                           # the RAW image. Otherwise a full resolution JPEG
                           # will be used as reference and this value will be
                           # 1.0.
    failed_ar = []  # streams failed the aspect ratio test
    failed_crop = []  # streams failed the crop test
    failed_fov = []  # streams that fail FoV test
    format_list = []  # format list for multiple capture objects.

    # Do multi-capture of "iter" and "cmpr". Iterate through all the
    # available sizes of "iter", and only use the size specified for "cmpr"
    # The "cmpr" capture is only used so that we have multiple capture target
    # instead of just one, which should help catching more potential issues.
    # The test doesn't look into the output of "cmpr" images at all.
    # The "iter_max" or "cmpr_size" key defines the maximal size being iterated
    # or selected for the "iter" and "cmpr" stream accordingly. None means no
    # upper bound is specified.
    format_list.append({"iter": "yuv", "iter_max": None,
                        "cmpr": "yuv", "cmpr_size": PREVIEW_SIZE})
    format_list.append({"iter": "yuv", "iter_max": PREVIEW_SIZE,
                        "cmpr": "jpeg", "cmpr_size": None})
    format_list.append({"iter": "yuv", "iter_max": PREVIEW_SIZE,
                        "cmpr": "raw", "cmpr_size": None})
    format_list.append({"iter": "jpeg", "iter_max": None,
                        "cmpr": "raw", "cmpr_size": None})
    format_list.append({"iter": "jpeg", "iter_max": None,
                        "cmpr": "yuv", "cmpr_size": PREVIEW_SIZE})
    ref_fov = {}  # Reference frame's FOV related information
                  # If RAW is available a full resolution RAW frame will be used
                  # as reference frame; otherwise the highest resolution JPEG is used.
    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        fls_logical = props['android.lens.info.availableFocalLengths']
        print 'logical available focal lengths: %s', str(fls_logical)
        props = cam.override_with_hidden_physical_camera_props(props)
        fls_physical = props['android.lens.info.availableFocalLengths']
        print 'physical available focal lengths: %s', str(fls_physical)
        # determine skip conditions
        first_api = its.device.get_first_api_level(its.device.get_device_id())
        if first_api < 30:  # original constraint
            its.caps.skip_unless(its.caps.read_3a(props))
        else:  # loosen from read_3a to enable LIMITED coverage
            its.caps.skip_unless(its.caps.ae_lock(props) and
                                 its.caps.awb_lock(props))
        # determine capabilities
        full_device = its.caps.full_or_better(props)
        limited_device = its.caps.limited(props)
        its.caps.skip_unless(full_device or limited_device)
        level3_device = its.caps.level3(props)
        raw_avlb = its.caps.raw16(props)
        run_crop_test = (level3_device or full_device) and raw_avlb
        if not run_crop_test:
            print "Crop test skipped"
        debug = its.caps.debug_mode()
        # Converge 3A
        cam.do_3a()
        req = its.objects.auto_capture_request()

        # If raw is available and main camera, use it as ground truth.
        if raw_avlb and (fls_physical == fls_logical):
            ref_fov, cc_ct_gt, aspect_ratio_gt = find_raw_fov_reference(cam, req, props, debug)
        else:
            ref_fov, cc_ct_gt = find_jpeg_fov_reference(cam, req, props)

        if run_crop_test:
            # Normalize the circle size to 1/4 of the image size, so that
            # circle size won't affect the crop test result
            factor_cp_thres = ((min(ref_fov["w"], ref_fov["h"])/4.0) /
                               max(ref_fov["circle_w"], ref_fov["circle_h"]))
            thres_l_cp_test = THRESH_L_CP * factor_cp_thres
            thres_xs_cp_test = THRESH_XS_CP * factor_cp_thres

        # Take pictures of each settings with all the image sizes available.
        for fmt in format_list:
            fmt_iter = fmt["iter"]
            fmt_cmpr = fmt["cmpr"]
            dual_target = fmt_cmpr is not "none"
            # Get the size of "cmpr"
            if dual_target:
                sizes = its.objects.get_available_output_sizes(
                        fmt_cmpr, props, fmt["cmpr_size"])
                if not sizes:  # device might not support RAW
                    continue
                size_cmpr = sizes[0]
            for size_iter in its.objects.get_available_output_sizes(
                    fmt_iter, props, fmt["iter_max"]):
                w_iter = size_iter[0]
                h_iter = size_iter[1]
                # Skip testing same format/size combination
                # ITS does not handle that properly now
                if (dual_target
                            and w_iter*h_iter == size_cmpr[0]*size_cmpr[1]
                            and fmt_iter == fmt_cmpr):
                    continue
                out_surface = [{"width": w_iter,
                                "height": h_iter,
                                "format": fmt_iter}]
                if dual_target:
                    out_surface.append({"width": size_cmpr[0],
                                        "height": size_cmpr[1],
                                        "format": fmt_cmpr})
                cap = cam.do_capture(req, out_surface)
                if dual_target:
                    frm_iter = cap[0]
                else:
                    frm_iter = cap
                assert frm_iter["format"] == fmt_iter
                assert frm_iter["width"] == w_iter
                assert frm_iter["height"] == h_iter
                print "Captured %s with %s %dx%d. Compared size: %dx%d" % (
                        fmt_iter, fmt_cmpr, w_iter, h_iter, size_cmpr[0],
                        size_cmpr[1])
                img = its.image.convert_capture_to_rgb_image(frm_iter)
                img_name = "%s_%s_with_%s_w%d_h%d.png" % (NAME,
                                                          fmt_iter, fmt_cmpr,
                                                          w_iter, h_iter)
                aspect_ratio, cc_ct, (cc_w, cc_h) = measure_aspect_ratio(
                        img, img_name, raw_avlb, debug)
                # check fov coverage for all fmts in AR_CHECKED
                fov_percent = calc_circle_image_ratio(
                        cc_w, cc_h, w_iter, h_iter)
                chk_percent = calc_expected_circle_image_ratio(ref_fov, w_iter, h_iter)
                chk_enabled = is_checked_aspect_ratio(first_api, w_iter, h_iter)
                if chk_enabled and not np.isclose(fov_percent, chk_percent,
                                                  rtol=FOV_PERCENT_RTOL):
                    msg = "FoV %%: %.2f, Ref FoV %%: %.2f, " % (
                            fov_percent, chk_percent)
                    msg += "TOL=%.f%%, img: %dx%d, ref: %dx%d" % (
                            FOV_PERCENT_RTOL*100, w_iter, h_iter,
                            ref_fov["w"], ref_fov["h"])
                    failed_fov.append(msg)
                    its.image.write_image(img/255, img_name, True)

                # check pass/fail for aspect ratio
                # image size: the larger one of image width and height
                # image size >= LARGE_SIZE: use THRESH_L_AR
                # image size == 0 (extreme case): THRESH_XS_AR
                # 0 < image size < LARGE_SIZE: scale between THRESH_XS_AR
                # and THRESH_L_AR
                thres_ar_test = max(
                        THRESH_L_AR, THRESH_XS_AR + max(w_iter, h_iter) *
                        (THRESH_L_AR-THRESH_XS_AR)/LARGE_SIZE)
                thres_range_ar = (aspect_ratio_gt-thres_ar_test,
                                  aspect_ratio_gt+thres_ar_test)
                if (aspect_ratio < thres_range_ar[0] or
                            aspect_ratio > thres_range_ar[1]):
                    failed_ar.append({"fmt_iter": fmt_iter,
                                      "fmt_cmpr": fmt_cmpr,
                                      "w": w_iter, "h": h_iter,
                                      "ar": aspect_ratio,
                                      "valid_range": thres_range_ar})
                    its.image.write_image(img/255, img_name, True)

                # check pass/fail for crop
                if run_crop_test:
                    # image size >= LARGE_SIZE: use thres_l_cp_test
                    # image size == 0 (extreme case): thres_xs_cp_test
                    # 0 < image size < LARGE_SIZE: scale between
                    # thres_xs_cp_test and thres_l_cp_test
                    # Also, allow at least THRESH_MIN_PIXEL off to
                    # prevent threshold being too tight for very
                    # small circle
                    thres_hori_cp_test = max(
                            thres_l_cp_test, thres_xs_cp_test + w_iter *
                            (thres_l_cp_test-thres_xs_cp_test)/LARGE_SIZE)
                    min_threshold_h = THRESH_MIN_PIXEL / cc_w
                    thres_hori_cp_test = max(thres_hori_cp_test,
                                             min_threshold_h)
                    thres_range_h_cp = (cc_ct_gt["hori"]-thres_hori_cp_test,
                                        cc_ct_gt["hori"]+thres_hori_cp_test)
                    thres_vert_cp_test = max(
                            thres_l_cp_test, thres_xs_cp_test + h_iter *
                            (thres_l_cp_test-thres_xs_cp_test)/LARGE_SIZE)
                    min_threshold_v = THRESH_MIN_PIXEL / cc_h
                    thres_vert_cp_test = max(thres_vert_cp_test,
                                             min_threshold_v)
                    thres_range_v_cp = (cc_ct_gt["vert"]-thres_vert_cp_test,
                                        cc_ct_gt["vert"]+thres_vert_cp_test)
                    if (cc_ct["hori"] < thres_range_h_cp[0]
                                or cc_ct["hori"] > thres_range_h_cp[1]
                                or cc_ct["vert"] < thres_range_v_cp[0]
                                or cc_ct["vert"] > thres_range_v_cp[1]):
                        failed_crop.append({"fmt_iter": fmt_iter,
                                            "fmt_cmpr": fmt_cmpr,
                                            "w": w_iter, "h": h_iter,
                                            "ct_hori": cc_ct["hori"],
                                            "ct_vert": cc_ct["vert"],
                                            "valid_range_h": thres_range_h_cp,
                                            "valid_range_v": thres_range_v_cp})
                        its.image.write_image(img/255, img_name, True)

        # Print failed any test results
        print_failed_test_results(failed_ar, failed_fov, failed_crop)
        assert not failed_ar
        assert not failed_fov
        if level3_device:
            assert not failed_crop


if __name__ == "__main__":
    main()
