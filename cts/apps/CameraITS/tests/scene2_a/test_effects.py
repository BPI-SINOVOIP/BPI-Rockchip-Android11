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
import numpy as np

# android.control.availableEffects
EFFECTS = {0: 'OFF',
           1: 'MONO',
           2: 'NEGATIVE',
           3: 'SOLARIZE',
           4: 'SEPIA',
           5: 'POSTERIZE',
           6: 'WHITEBOARD',
           7: 'BLACKBOARD',
           8: 'AQUA'}
MONO_UV_SPREAD_MAX = 2  # max spread for U & V channels [0:255] for mono image
NAME = os.path.basename(__file__).split('.')[0]
W, H = 640, 480
YUV_MAX = 255.0  # normalization number for YUV images [0:1] --> [0:255]
YUV_UV_SPREAD_MIN = 10  # min spread for U & V channels [0:255] for color image
YUV_Y_SPREAD_MIN = 50  # min spread for Y channel [0:255] for color image


def main():
    """Test effects.

    Test: capture frame for supported camera effects and check if generated
    correctly. Note we only check effects OFF and MONO currently, but save
    images for all supported effects.
    """

    print '\nStarting %s' % NAME
    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        mono_camera = its.caps.mono_camera(props)
        effects = props['android.control.availableEffects']
        its.caps.skip_unless(effects != [0])
        cam.do_3a(mono_camera=mono_camera)
        print 'Supported effects:', effects
        failed = []
        for effect in effects:
            req = its.objects.auto_capture_request()
            req['android.control.effectMode'] = effect
            fmt = {'format': 'yuv', 'width': W, 'height': H}
            cap = cam.do_capture(req, fmt)

            # Save image
            img = its.image.convert_capture_to_rgb_image(cap, props=props)
            its.image.write_image(img, '%s_%s.jpg' % (NAME, EFFECTS[effect]))

            # Simple checks
            if effect is 0:
                print 'Checking effects OFF...'
                y, u, v = its.image.convert_capture_to_planes(cap, props)
                y_min, y_max = np.amin(y)*YUV_MAX, np.amax(y)*YUV_MAX
                msg = 'Y_range:%.f,%.f THRESH:%d, ' % (
                        y_min, y_max, YUV_Y_SPREAD_MIN)
                if (y_max-y_min) < YUV_Y_SPREAD_MIN:
                    failed.append({'effect': EFFECTS[effect], 'error': msg})
                if not mono_camera:
                    u_min, u_max = np.amin(u)*YUV_MAX, np.amax(u)*YUV_MAX
                    v_min, v_max = np.amin(v)*YUV_MAX, np.amax(v)*YUV_MAX
                    msg += 'U_range:%.f,%.f THRESH:%d, ' % (
                            u_min, u_max, YUV_UV_SPREAD_MIN)
                    msg += 'V_range:%.f,%.f THRESH:%d' % (
                            v_min, v_max, YUV_UV_SPREAD_MIN)
                    if ((u_max-u_min) < YUV_UV_SPREAD_MIN or
                                (v_max-v_min) < YUV_UV_SPREAD_MIN):
                        failed.append({'effect': EFFECTS[effect], 'error': msg})
            if effect is 1:
                print 'Checking MONO effect...'
                _, u, v = its.image.convert_capture_to_planes(cap, props)
                u_min, u_max = np.amin(u)*YUV_MAX, np.amax(u)*YUV_MAX
                v_min, v_max = np.amin(v)*YUV_MAX, np.amax(v)*YUV_MAX
                msg = 'U_range:%.f,%.f, ' % (u_min, u_max)
                msg += 'V_range:%.f,%.f, TOL:%d' % (
                        v_min, v_max, MONO_UV_SPREAD_MAX)
                if ((u_max-u_min) > MONO_UV_SPREAD_MAX or
                            (v_max-v_min) > MONO_UV_SPREAD_MAX):
                    failed.append({'effect': EFFECTS[effect], 'error': msg})
        if failed:
            print 'Failed effects:'
            for fail in failed:
                print ' %s: %s' % (fail['effect'], fail['error'])
        assert not failed


if __name__ == '__main__':
    main()
