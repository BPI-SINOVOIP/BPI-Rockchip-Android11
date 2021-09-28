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
import its.target

import matplotlib
from matplotlib import pylab

NAME = os.path.basename(__file__).split('.')[0]


def main():
    """Test that the android.sensor.exposureTime parameter is applied."""

    exp_times = []
    r_means = []
    g_means = []
    b_means = []

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.compute_target_exposure(props))
        sync_latency = its.caps.sync_latency(props)

        debug = its.caps.debug_mode()
        largest_yuv = its.objects.get_largest_yuv_format(props)
        if debug:
            fmt = largest_yuv
        else:
            match_ar = (largest_yuv['width'], largest_yuv['height'])
            fmt = its.objects.get_smallest_yuv_format(props, match_ar=match_ar)

        e, s = its.target.get_target_exposure_combos(cam)['midExposureTime']
        for i, e_mult in enumerate([0.8, 0.9, 1.0, 1.1, 1.2]):
            req = its.objects.manual_capture_request(
                    s, e * e_mult, 0.0, True, props)
            cap = its.device.do_capture_with_latency(
                    cam, req, sync_latency, fmt)
            img = its.image.convert_capture_to_rgb_image(cap)
            its.image.write_image(
                    img, '%s_frame%d.jpg' % (NAME, i))
            tile = its.image.get_image_patch(img, 0.45, 0.45, 0.1, 0.1)
            rgb_means = its.image.compute_image_means(tile)
            exp_times.append(e * e_mult)
            r_means.append(rgb_means[0])
            g_means.append(rgb_means[1])
            b_means.append(rgb_means[2])

    # Draw a plot.
    pylab.plot(exp_times, r_means, '-ro')
    pylab.plot(exp_times, g_means, '-go')
    pylab.plot(exp_times, b_means, '-bo')
    pylab.ylim([0, 1])
    pylab.title(NAME)
    pylab.xlabel('Exposure times (ns)')
    pylab.ylabel('RGB means')
    plot_name = '%s_plot_means.png' % NAME
    matplotlib.pyplot.savefig(plot_name)

    # Test for pass/fail: check that each shot is brighter than the previous.
    for means in [r_means, g_means, b_means]:
        for i in range(len(means)-1):
            assert means[i+1] > means[i], 'See %s' % plot_name

if __name__ == '__main__':
    main()

