# Copyright 2013 The Android Open Source Project
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
import its.device
import its.image
import its.objects
import matplotlib
from matplotlib import pylab

NAME = os.path.basename(__file__).split(".")[0]


def main():
    """Test that the device will produce full black+white images."""

    r_means = []
    g_means = []
    b_means = []

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.manual_sensor(props))
        sync_latency = its.caps.sync_latency(props)

        debug = its.caps.debug_mode()
        largest_yuv = its.objects.get_largest_yuv_format(props)
        if debug:
            fmt = largest_yuv
        else:
            match_ar = (largest_yuv["width"], largest_yuv["height"])
            fmt = its.objects.get_smallest_yuv_format(props, match_ar=match_ar)

        expt_range = props["android.sensor.info.exposureTimeRange"]
        sens_range = props["android.sensor.info.sensitivityRange"]

        # Take a shot with very low ISO and exposure time. Expect it to
        # be black.
        req = its.objects.manual_capture_request(sens_range[0], expt_range[0])
        cap = its.device.do_capture_with_latency(cam, req, sync_latency, fmt)
        img = its.image.convert_capture_to_rgb_image(cap)
        its.image.write_image(img, "%s_black.jpg" % NAME)
        tile = its.image.get_image_patch(img, 0.45, 0.45, 0.1, 0.1)
        black_means = its.image.compute_image_means(tile)
        r_means.append(black_means[0])
        g_means.append(black_means[1])
        b_means.append(black_means[2])
        print "Dark pixel means:", black_means
        r_exp = cap["metadata"]["android.sensor.exposureTime"]
        r_iso = cap["metadata"]["android.sensor.sensitivity"]
        print "Black shot write values: sens = %d, exp time = %.4fms" % (
                sens_range[0], expt_range[0]/1000000.0)
        print "Black shot read values: sens = %d, exp time = %.4fms\n" % (
                r_iso, r_exp/1000000.0)

        # Take a shot with very high ISO and exposure time. Expect it to
        # be white.
        req = its.objects.manual_capture_request(sens_range[1], expt_range[1])
        cap = its.device.do_capture_with_latency(cam, req, sync_latency, fmt)
        img = its.image.convert_capture_to_rgb_image(cap)
        its.image.write_image(img, "%s_white.jpg" % NAME)
        tile = its.image.get_image_patch(img, 0.45, 0.45, 0.1, 0.1)
        white_means = its.image.compute_image_means(tile)
        r_means.append(white_means[0])
        g_means.append(white_means[1])
        b_means.append(white_means[2])
        print "Bright pixel means:", white_means
        r_exp = cap["metadata"]["android.sensor.exposureTime"]
        r_iso = cap["metadata"]["android.sensor.sensitivity"]
        print "White shot write values: sens = %d, exp time = %.2fms" % (
                sens_range[1], expt_range[1]/1000000.0)
        print "White shot read values: sens = %d, exp time = %.2fms\n" % (
                r_iso, r_exp/1000000.0)

        # Draw a plot.
        pylab.title("test_black_white")
        pylab.plot([0, 1], r_means, "-ro")
        pylab.plot([0, 1], g_means, "-go")
        pylab.plot([0, 1], b_means, "-bo")
        pylab.xlabel("Capture Number")
        pylab.ylabel("Output Values (Normalized)")
        pylab.ylim([0, 1])
        matplotlib.pyplot.savefig("%s_plot_means.png" % (NAME))

        for black_mean in black_means:
            assert black_mean < 0.025
        for white_mean in white_means:
            assert white_mean > 0.975

if __name__ == "__main__":
    main()

