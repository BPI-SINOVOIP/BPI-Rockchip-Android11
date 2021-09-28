# Copyright 2019 The Android Open Source Project
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
import numpy as np

NAME = os.path.basename(__file__).split('.')[0]
RGB_FULL_SCALE = 255.0
RGB_SAT_MIN = 253.0
RGB_SAT_TOL = 1.0


def main():
    """Test that channels saturate evenly."""

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.manual_sensor(props))
        sync_latency = its.caps.sync_latency(props)

        debug = its.caps.debug_mode()
        largest_yuv = its.objects.get_largest_yuv_format(props)
        if debug:
            fmt = largest_yuv
        else:
            match_ar = (largest_yuv['width'], largest_yuv['height'])
            fmt = its.objects.get_smallest_yuv_format(props, match_ar=match_ar)

        exp = props['android.sensor.info.exposureTimeRange'][1]
        iso = props['android.sensor.info.sensitivityRange'][1]

        # Take shot with very high ISO and exposure time. Expect saturation
        req = its.objects.manual_capture_request(iso, exp)
        cap = its.device.do_capture_with_latency(cam, req, sync_latency, fmt)
        img = its.image.convert_capture_to_rgb_image(cap)
        its.image.write_image(img, '%s.jpg' % NAME)
        tile = its.image.get_image_patch(img, 0.45, 0.45, 0.1, 0.1)
        white_means = its.image.compute_image_means(tile)
        r = white_means[0] * RGB_FULL_SCALE
        g = white_means[1] * RGB_FULL_SCALE
        b = white_means[2] * RGB_FULL_SCALE
        print ' Saturated pixels r, g, b:', white_means
        r_exp = cap['metadata']['android.sensor.exposureTime']
        r_iso = cap['metadata']['android.sensor.sensitivity']
        print ' Saturated shot write values: iso = %d, exp = %.2fms' % (
                iso, exp/1000000.0)
        print ' Saturated shot read values: iso = %d, exp = %.2fms\n' % (
                r_iso, r_exp/1000000.0)

        # assert saturation
        assert min(r, g, b) > RGB_SAT_MIN, (
                'r: %.1f, g: %.1f, b: %.1f, MIN: %.f' % (r, g, b, RGB_SAT_MIN))
        # assert channels saturate evenly
        assert np.isclose(min(r, g, b), max(r, g, b), atol=RGB_SAT_TOL), (
                'ch_sat not EQ!  r: %.1f, g: %.1f, b: %.1f, TOL: %.f' % (
                        r, g, b, RGB_SAT_TOL))


if __name__ == '__main__':
    main()

