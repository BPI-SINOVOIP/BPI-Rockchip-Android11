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

import matplotlib
from matplotlib import pylab
import numpy

NAME = os.path.basename(__file__).split(".")[0]
NUM_SAMPLES = 4
THRESH_REL_SHARPNESS_DIFF = 0.15


def check_edge_modes(sharpness):
    """Check that the sharpness for the different edge modes is correct."""
    print " Verify HQ(2) is sharper than OFF(0)"
    assert sharpness[2] > sharpness[0]

    print " Verify ZSL(3) is similar to OFF(0)"
    e_msg = "ZSL: %.5f, OFF: %.5f, RTOL: %.2f" % (
            sharpness[3], sharpness[0], THRESH_REL_SHARPNESS_DIFF)
    assert numpy.isclose(sharpness[3], sharpness[0],
                         THRESH_REL_SHARPNESS_DIFF), e_msg

    print " Verify OFF(0) is not sharper than FAST(1)"
    assert sharpness[1] > sharpness[0] * (1.0 - THRESH_REL_SHARPNESS_DIFF)

    print " Verify FAST(1) is not sharper than HQ(2)"
    assert sharpness[2] > sharpness[1] * (1.0 - THRESH_REL_SHARPNESS_DIFF)


def test_edge_mode(cam, edge_mode, sensitivity, exp, fd, out_surface, chart,
                   reprocess_format=None):
    """Return sharpness of the output images and the capture result metadata.

       Processes a capture request with a given edge mode, sensitivity, exposure
       time, focus distance, output surface parameter, and reprocess format
       (None for a regular request.)

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
        chart: object containing chart information
        reprocess_format: (Optional) The reprocessing format. If not None,
                reprocessing will be enabled.

    Returns:
        Object containing reported edge mode and the sharpness of the output
        image, keyed by the following strings:
            "edge_mode"
            "sharpness"
    """

    req = its.objects.manual_capture_request(sensitivity, exp)
    req["android.lens.focusDistance"] = fd
    req["android.edge.mode"] = edge_mode
    if reprocess_format:
        req["android.reprocess.effectiveExposureFactor"] = 1.0

    sharpness_list = []
    caps = cam.do_capture([req]*NUM_SAMPLES, [out_surface], reprocess_format)
    for n in range(NUM_SAMPLES):
        y, _, _ = its.image.convert_capture_to_planes(caps[n])
        chart.img = its.image.normalize_img(its.image.get_image_patch(
                y, chart.xnorm, chart.ynorm, chart.wnorm, chart.hnorm))
        if n == 0:
            its.image.write_image(chart.img, "%s_reprocess_fmt_%s_edge=%d.jpg" %
                                  (NAME, reprocess_format, edge_mode))
            res_edge_mode = caps[n]["metadata"]["android.edge.mode"]
        sharpness_list.append(its.image.compute_image_sharpness(chart.img))

    ret = {}
    ret["edge_mode"] = res_edge_mode
    ret["sharpness"] = numpy.mean(sharpness_list)

    return ret


def main():
    """Test android.edge.mode param applied when set for reprocessing requests.

    Capture non-reprocess images for each edge mode and calculate their
    sharpness as a baseline.

    Capture reprocessed images for each supported reprocess format and edge_mode
    mode. Calculate the sharpness of reprocessed images and compare them against
    the sharpess of non-reprocess images.
    """

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()

        its.caps.skip_unless(its.caps.read_3a(props) and
                             its.caps.per_frame_control(props) and
                             its.caps.edge_mode(props, 0) and
                             (its.caps.yuv_reprocess(props) or
                              its.caps.private_reprocess(props)))

    # initialize chart class and locate chart in scene
    chart = its.cv2image.Chart()

    with its.device.ItsSession() as cam:
        mono_camera = its.caps.mono_camera(props)
        # If reprocessing is supported, ZSL EE mode must be avaiable.
        assert its.caps.edge_mode(props, 3), "EE mode not available!"

        reprocess_formats = []
        if its.caps.yuv_reprocess(props):
            reprocess_formats.append("yuv")
        if its.caps.private_reprocess(props):
            reprocess_formats.append("private")

        size = its.objects.get_available_output_sizes("jpg", props)[0]
        out_surface = {"width": size[0], "height": size[1], "format": "jpg"}

        # Get proper sensitivity, exposure time, and focus distance.
        s, e, _, _, fd = cam.do_3a(get_results=True, mono_camera=mono_camera)

        # Intialize plot
        pylab.figure("reprocess_result")
        gr_color = {"yuv": "r", "private": "g", "none": "b"}

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

        pylab.plot(range(4), sharpness_regular, "-"+gr_color["none"]+"o")
        print "Reported edge modes",
        print "regular requests:", edge_mode_reported_regular
        print "Sharpness with EE mode [0,1,2,3]:", sharpness_regular
        print ""

        # Get the sharpness for each reprocess format and edge mode for
        # reprocess requests.
        sharpnesses_reprocess = []
        edge_mode_reported_reprocess = []

        for reprocess_format in reprocess_formats:
            # List of sharpness
            sharpnesses = []
            edge_mode_reported = []
            for edge_mode in range(4):
                # Skip unavailable modes
                if not its.caps.edge_mode(props, edge_mode):
                    edge_mode_reported.append(edge_mode)
                    sharpnesses.append(0)
                    continue

                ret = test_edge_mode(cam, edge_mode, s, e, fd, out_surface,
                                     chart, reprocess_format)
                edge_mode_reported.append(ret["edge_mode"])
                sharpnesses.append(ret["sharpness"])

            sharpnesses_reprocess.append(sharpnesses)
            edge_mode_reported_reprocess.append(edge_mode_reported)

            pylab.plot(range(4), sharpnesses,
                       "-"+gr_color[reprocess_format]+"o")
            print "Reported edge modes w/ request fmt %s:" % reprocess_format
            print "Sharpness with EE mode [0,1,2,3] for %s reprocess:" % (
                    reprocess_format), sharpnesses
            print ""

        # Finalize plot
        pylab.title("Red-YUV Reprocess  Green-Private Reprocess  Blue-None")
        pylab.xlabel("Edge Enhance Mode")
        pylab.ylabel("Sharpness")
        pylab.xticks(range(4))
        matplotlib.pyplot.savefig("%s_plot_EE.png" %
                                  ("test_reprocess_edge_enhancement"))
        print "regular requests:"
        check_edge_modes(sharpness_regular)

        for reprocess_format in range(len(reprocess_formats)):
            print "\nreprocess format:", reprocess_format
            check_edge_modes(sharpnesses_reprocess[reprocess_format])

            hq_div_off_reprocess = (sharpnesses_reprocess[reprocess_format][2] /
                                    sharpnesses_reprocess[reprocess_format][0])
            hq_div_off_regular = sharpness_regular[2] / sharpness_regular[0]
            e_msg = "HQ/OFF_reprocess: %.4f, HQ/OFF_reg: %.4f, RTOL: %.2f" % (
                    hq_div_off_reprocess, hq_div_off_regular,
                    THRESH_REL_SHARPNESS_DIFF)
            print " Verify reprocess HQ(2) ~= reg HQ(2) relative to OFF(0)"
            assert numpy.isclose(hq_div_off_reprocess, hq_div_off_regular,
                                 THRESH_REL_SHARPNESS_DIFF), e_msg

if __name__ == "__main__":
    main()

