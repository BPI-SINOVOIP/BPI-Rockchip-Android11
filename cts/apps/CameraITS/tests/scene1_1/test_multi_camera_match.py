# Copyright 2018 The Android Open Source Project
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

import numpy as np
NAME = os.path.basename(__file__).split('.')[0]
PATCH_SIZE = 0.0625  # 1/16 x 1/16 in center of image
PATCH_LOC = (1-PATCH_SIZE)/2
THRESH_DIFF = 0.06
THRESH_GAIN = 0.1
THRESH_EXP = 0.05


def main():
    """Test both cameras give similar RBG values for gray patch."""

    yuv_sizes = {}
    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.per_frame_control(props) and
                             its.caps.logical_multi_camera(props))
        ids = its.caps.logical_multi_camera_physical_ids(props)
        for i in ids:
            physical_props = cam.get_camera_properties_by_id(i)
            its.caps.skip_unless(not its.caps.mono_camera(physical_props))
            its.caps.skip_unless(its.caps.backward_compatible(physical_props))
            yuv_sizes[i] = its.objects.get_available_output_sizes(
                    'yuv', physical_props)
            if i == ids[0]:  # get_available_output_sizes returns sorted list
                yuv_match_sizes = yuv_sizes[i]
            else:
                yuv_match_sizes = list(
                        set(yuv_sizes[i]).intersection(yuv_match_sizes))

        # find matched size for captures
        yuv_match_sizes.sort()
        w = yuv_match_sizes[-1][0]
        h = yuv_match_sizes[-1][1]
        print 'Matched YUV size: (%d, %d)' % (w, h)

        # do 3a and create requests
        avail_fls = sorted(props['android.lens.info.availableFocalLengths'],
                           reverse=True)
        cam.do_3a()
        reqs = []
        for i, fl in enumerate(avail_fls):
            reqs.append(its.objects.auto_capture_request())
            reqs[i]['android.lens.focalLength'] = fl
            if i > 0:
                # Calculate the active sensor region for a non-cropped image
                zoom = avail_fls[0] / fl
                a = props['android.sensor.info.activeArraySize']
                ax, ay = a['left'], a['top']
                aw, ah = a['right'] - a['left'], a['bottom'] - a['top']

                # Calculate a center crop region.
                assert zoom >= 1
                cropw = aw / zoom
                croph = ah / zoom
                crop_region = {
                        'left': aw / 2 - cropw / 2,
                        'top': ah / 2 - croph / 2,
                        'right': aw / 2 + cropw / 2,
                        'bottom': ah / 2 + croph / 2
                }
                reqs[i]['android.scaler.cropRegion'] = crop_region

        # capture YUVs
        y_means = {}
        msg = ''
        fmt = [{'format': 'yuv', 'width': w, 'height': h}]
        caps = cam.do_capture(reqs, fmt)
        if not isinstance(caps, list):
            caps = [caps]  # handle canonical case where caps is not list

        for i, fl in enumerate(avail_fls):
            img = its.image.convert_capture_to_rgb_image(caps[i], props=props)
            its.image.write_image(img, '%s_yuv_fl=%s.jpg' % (NAME, fl))
            y, _, _ = its.image.convert_capture_to_planes(caps[i], props=props)
            y_mean = its.image.compute_image_means(
                    its.image.get_image_patch(y, PATCH_LOC, PATCH_LOC,
                                              PATCH_SIZE, PATCH_SIZE))[0]
            print 'y[%s]: %.3f' % (fl, y_mean)
            msg += 'y[%s]: %.3f, ' % (fl, y_mean)
            y_means[fl] = y_mean

        # compare YUVs
        msg += 'TOL=%.5f' % THRESH_DIFF
        assert np.isclose(max(y_means.values()), min(y_means.values()),
                          rtol=THRESH_DIFF), msg


if __name__ == '__main__':
    main()
