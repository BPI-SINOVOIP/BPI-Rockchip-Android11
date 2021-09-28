# Copyright 2014 The Android Open Source Project
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
import its.target

from matplotlib import pylab
import matplotlib.pyplot
import numpy

API_LEVEL_30 = 30
BURST_LEN = 50
BURSTS = 5
COLORS = ["R", "G", "B"]
FRAMES = BURST_LEN * BURSTS
NAME = os.path.basename(__file__).split(".")[0]
SPREAD_THRESH = 0.03
SPREAD_THRESH_API_LEVEL_30 = 0.02


def main():
    """Take long bursts of images and check that they're all identical.

    Assumes a static scene. Can be used to idenfity if there are sporadic
    frames that are processed differently or have artifacts. Uses manual
    capture settings.
    """

    with its.device.ItsSession() as cam:

        # Capture at the smallest resolution.
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.compute_target_exposure(props) and
                             its.caps.per_frame_control(props))
        debug = its.caps.debug_mode()

        _, fmt = its.objects.get_fastest_manual_capture_settings(props)
        e, s = its.target.get_target_exposure_combos(cam)["minSensitivity"]
        req = its.objects.manual_capture_request(s, e)
        w, h = fmt["width"], fmt["height"]

        # Capture bursts of YUV shots.
        # Get the mean values of a center patch for each.
        # Also build a 4D array, which is an array of all RGB images.
        r_means = []
        g_means = []
        b_means = []
        imgs = numpy.empty([FRAMES, h, w, 3])
        for j in range(BURSTS):
            caps = cam.do_capture([req]*BURST_LEN, [fmt])
            for i, cap in enumerate(caps):
                n = j*BURST_LEN + i
                imgs[n] = its.image.convert_capture_to_rgb_image(cap)
                tile = its.image.get_image_patch(imgs[n], 0.45, 0.45, 0.1, 0.1)
                means = its.image.compute_image_means(tile)
                r_means.append(means[0])
                g_means.append(means[1])
                b_means.append(means[2])

        # Dump all images if debug
        if debug:
            print "Dumping images"
            for i in range(FRAMES):
                its.image.write_image(imgs[i], "%s_frame%03d.jpg"%(NAME, i))

        # The mean image.
        img_mean = imgs.mean(0)
        its.image.write_image(img_mean, "%s_mean.jpg"%(NAME))

        # Plot means vs frames
        frames = range(FRAMES)
        pylab.title(NAME)
        pylab.plot(frames, r_means, "-ro")
        pylab.plot(frames, g_means, "-go")
        pylab.plot(frames, b_means, "-bo")
        pylab.ylim([0, 1])
        pylab.xlabel("frame number")
        pylab.ylabel("RGB avg [0, 1]")
        matplotlib.pyplot.savefig("%s_plot_means.png" % (NAME))

        # determine spread_thresh
        spread_thresh = SPREAD_THRESH
        if its.device.get_first_api_level() >= API_LEVEL_30:
            spread_thresh = SPREAD_THRESH_API_LEVEL_30

        # PASS/FAIL based on center patch similarity.
        for plane, means in enumerate([r_means, g_means, b_means]):
            spread = max(means) - min(means)
            msg = "%s spread: %.5f, spread_thresh: %.3f" % (
                    COLORS[plane], spread, spread_thresh)
            print msg
            assert spread < spread_thresh, msg

if __name__ == "__main__":
    main()

