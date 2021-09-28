# Copyright 2015 The Android Open Source Project
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

import os.path

import its.caps
import its.cv2image
import its.device
import its.image
import its.objects
import its.target

import numpy

NAME = os.path.basename(__file__).split(".")[0]
NUM_SAMPLES = 4
THRESH_REL_SHARPNESS_DIFF = 0.1


def test_edge_mode(cam, edge_mode, sensitivity, exp, fd, out_surface, chart):
    """Return sharpness of the output image and the capture result metadata.

       Processes a capture request with a given edge mode, sensitivity, exposure
       time, focus distance, output surface parameter.

    Args:
        cam: An open device session.
        edge_mode: Edge mode for the request as defined in android.edge.mode
        sensitivity: Sensitivity for the request as defined in
            android.sensor.sensitivity
        exp: Exposure time for the request as defined in
            android.sensor.exposureTime.
        fd: Focus distance for the request as defined in
            android.lens.focusDistance
        out_surface: Specifications of the output image format and size.
        chart: object that contains chart information

    Returns:
        Object containing reported edge mode and the sharpness of the output
        image, keyed by the following strings:
            "edge_mode"
            "sharpness"
    """

    req = its.objects.manual_capture_request(sensitivity, exp)
    req["android.lens.focusDistance"] = fd
    req["android.edge.mode"] = edge_mode

    sharpness_list = []
    for n in range(NUM_SAMPLES):
        cap = cam.do_capture(req, out_surface, repeat_request=req)
        y, _, _ = its.image.convert_capture_to_planes(cap)
        chart.img = its.image.normalize_img(its.image.get_image_patch(
                y, chart.xnorm, chart.ynorm, chart.wnorm, chart.hnorm))
        if n == 0:
            its.image.write_image(
                    chart.img, "%s_edge=%d.jpg" % (NAME, edge_mode))
            res_edge_mode = cap["metadata"]["android.edge.mode"]
        sharpness_list.append(its.image.compute_image_sharpness(chart.img))

    ret = {}
    ret["edge_mode"] = res_edge_mode
    ret["sharpness"] = numpy.mean(sharpness_list)

    return ret


def main():
    """Test that the android.edge.mode param is applied correctly.

    Capture non-reprocess images for each edge mode and calculate their
    sharpness as a baseline.
    """

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()

        its.caps.skip_unless(its.caps.read_3a(props) and
                             its.caps.per_frame_control(props) and
                             its.caps.edge_mode(props, 0))

    # initialize chart class and locate chart in scene
    chart = its.cv2image.Chart()

    with its.device.ItsSession() as cam:
        mono_camera = its.caps.mono_camera(props)
        test_fmt = "yuv"
        size = its.objects.get_available_output_sizes(test_fmt, props)[0]
        out_surface = {"width": size[0], "height": size[1], "format": test_fmt}

        # Get proper sensitivity, exposure time, and focus distance.
        s, e, _, _, fd = cam.do_3a(get_results=True, mono_camera=mono_camera)

        # Get the sharpness for each edge mode for regular requests
        sharpness_regular = []
        edge_mode_reported_regular = []
        for edge_mode in range(4):
            # Skip unavailable modes
            if not its.caps.edge_mode(props, edge_mode):
                edge_mode_reported_regular.append(edge_mode)
                sharpness_regular.append(0)
                continue
            ret = test_edge_mode(cam, edge_mode, s, e, fd, out_surface, chart)
            edge_mode_reported_regular.append(ret["edge_mode"])
            sharpness_regular.append(ret["sharpness"])

        print "Reported edge modes:", edge_mode_reported_regular
        print "Sharpness with EE mode [0,1,2,3]:", sharpness_regular

        print "Verify HQ(2) is sharper than OFF(0)"
        assert sharpness_regular[2] > sharpness_regular[0]

        print "Verify OFF(0) is not sharper than FAST(1)"
        msg = "FAST: %.3f, OFF: %.3f, TOL: %.2f" % (
                sharpness_regular[1], sharpness_regular[0],
                THRESH_REL_SHARPNESS_DIFF)
        assert (sharpness_regular[1] >
                sharpness_regular[0] * (1.0 - THRESH_REL_SHARPNESS_DIFF)), msg

        # Verify FAST(1) is not sharper than HQ(2)
        msg = "HQ: %.3f, FAST: %.3f, TOL: %.2f" % (
                sharpness_regular[2], sharpness_regular[1],
                THRESH_REL_SHARPNESS_DIFF)
        assert (sharpness_regular[2] >
                sharpness_regular[1] * (1.0 - THRESH_REL_SHARPNESS_DIFF)), msg

if __name__ == "__main__":
    main()
