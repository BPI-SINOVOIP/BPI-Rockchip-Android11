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

import os

import its.caps
import its.device
import its.image
import its.objects

import matplotlib
from matplotlib import pylab
import numpy

NAME = os.path.basename(__file__).split('.')[0]
NUM_FRAMES = 4  # number of frames for temporal info to settle
NUM_SHADING_MODE_SWITCH_LOOPS = 3
SHADING_MODES = ['OFF', 'FAST', 'HQ']
THRESHOLD_DIFF_RATIO = 0.15


def main():
    """Test that the android.shading.mode param is applied.

    Switching shading modes and checks that the lens shading maps are
    modified as expected.
    """

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()

        its.caps.skip_unless(its.caps.per_frame_control(props) and
                             its.caps.lsc_map(props) and
                             its.caps.lsc_off(props))

        mono_camera = its.caps.mono_camera(props)

        # lsc_off devices should always support OFF(0), FAST(1), and HQ(2)
        assert(props.has_key('android.shading.availableModes') and
               set(props['android.shading.availableModes']) == set([0, 1, 2]))

        # Test 1: Switching shading modes several times and verify:
        #   1. Lens shading maps with mode OFF are all 1.0
        #   2. Lens shading maps with mode FAST are similar after switching
        #      shading modes.
        #   3. Lens shading maps with mode HIGH_QUALITY are similar after
        #      switching shading modes.
        cam.do_3a(mono_camera=mono_camera)

        # Use smallest yuv size matching the aspect ratio of largest yuv size to
        # reduce some USB bandwidth overhead since we are only looking at output
        # metadata in this test.
        largest_yuv_fmt = its.objects.get_largest_yuv_format(props)
        largest_yuv_size = (largest_yuv_fmt['width'], largest_yuv_fmt['height'])
        cap_fmt = its.objects.get_smallest_yuv_format(props, largest_yuv_size)

        # Get the reference lens shading maps for OFF, FAST, and HIGH_QUALITY
        # in different sessions.
        # reference_maps[mode]
        num_shading_modes = len(SHADING_MODES)
        reference_maps = [[] for mode in range(num_shading_modes)]
        num_map_gains = 0
        for mode in range(1, num_shading_modes):
            req = its.objects.auto_capture_request()
            req['android.statistics.lensShadingMapMode'] = 1
            req['android.shading.mode'] = mode
            cap_res = cam.do_capture([req]*NUM_FRAMES, cap_fmt)[NUM_FRAMES-1]['metadata']
            lsc_map = cap_res['android.statistics.lensShadingCorrectionMap']
            assert(lsc_map.has_key('width') and
                   lsc_map.has_key('height') and
                   lsc_map['width'] is not None and
                   lsc_map['height'] is not None)
            if mode == 1:
                num_map_gains = lsc_map['width'] * lsc_map['height'] * 4
                reference_maps[0] = [1.0] * num_map_gains
            reference_maps[mode] = lsc_map['map']

        # Get the lens shading maps while switching modes in one session.
        reqs = []
        for i in range(NUM_SHADING_MODE_SWITCH_LOOPS):
            for mode in range(num_shading_modes):
                for _ in range(NUM_FRAMES):
                    req = its.objects.auto_capture_request()
                    req['android.statistics.lensShadingMapMode'] = 1
                    req['android.shading.mode'] = mode
                    reqs.append(req)

        caps = cam.do_capture(reqs, cap_fmt)

        # shading_maps[mode][loop]
        shading_maps = [[[] for loop in range(NUM_SHADING_MODE_SWITCH_LOOPS)]
                        for mode in range(num_shading_modes)]

        # Get the shading maps out of capture results
        for i in range(len(caps)/NUM_FRAMES):
            shading_maps[i%num_shading_modes][i/NUM_SHADING_MODE_SWITCH_LOOPS] = \
                    caps[(i+1)*NUM_FRAMES-1]['metadata']['android.statistics.lensShadingCorrectionMap']['map']

        # Draw the maps
        for mode in range(num_shading_modes):
            for i in range(NUM_SHADING_MODE_SWITCH_LOOPS):
                pylab.clf()
                pylab.figure(figsize=(5, 5))
                pylab.subplot(2, 1, 1)
                pylab.plot(range(num_map_gains), shading_maps[mode][i], '-r.',
                           label='shading', alpha=0.7)
                pylab.plot(range(num_map_gains), reference_maps[mode], '-g.',
                           label='ref', alpha=0.7)
                pylab.xlim([0, num_map_gains])
                pylab.ylim([0.9, 4.0])
                name = '%s_ls_maps_mode_%d_loop_%d' % (NAME, mode, i)
                pylab.title(name)
                pylab.xlabel('Map gains')
                pylab.ylabel('Lens shading maps')
                pylab.legend(loc='upper center', numpoints=1, fancybox=True)

                pylab.subplot(2, 1, 2)
                shading_ref_ratio = numpy.divide(
                        shading_maps[mode][i], reference_maps[mode])
                pylab.plot(range(num_map_gains), shading_ref_ratio, '-b.',
                           clip_on=False)
                pylab.xlim([0, num_map_gains])
                pylab.ylim([1.0-THRESHOLD_DIFF_RATIO, 1.0+THRESHOLD_DIFF_RATIO])
                pylab.title('Shading/reference Maps Ratio vs Gain')
                pylab.xlabel('Map gains')
                pylab.ylabel('Shading/ref maps ratio')

                pylab.tight_layout()
                matplotlib.pyplot.savefig('%s.png' % name)

        for mode in range(num_shading_modes):
            if mode == 0:
                print 'Verifying lens shading maps with mode %s are all 1.0' % (
                        SHADING_MODES[mode])
            else:
                print 'Verifying lens shading maps with mode %s are similar' % (
                        SHADING_MODES[mode])
            for i in range(NUM_SHADING_MODE_SWITCH_LOOPS):
                e_msg = 'FAIL mode: %s, loop: %d, THRESH: %.2f' % (
                        SHADING_MODES[mode], i, THRESHOLD_DIFF_RATIO)
                assert (numpy.allclose(shading_maps[mode][i],
                                       reference_maps[mode],
                                       rtol=THRESHOLD_DIFF_RATIO)), e_msg

if __name__ == '__main__':
    main()
