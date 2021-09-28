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
import matplotlib
from matplotlib import pylab
import numpy as np

NAME = os.path.basename(__file__).split('.')[0]
LOCKED = 3
LUMA_LOCKED_TOL = 0.05
THRESH_CONVERGE_FOR_EV = 8  # AE must converge within this num
YUV_FULL_SCALE = 255.0
YUV_SAT_MIN = 250.0
YUV_SAT_TOL = 3.0


def main():
    """Tests that EV compensation is applied."""

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.ev_compensation(props) and
                             its.caps.ae_lock(props))

        debug = its.caps.debug_mode()
        mono_camera = its.caps.mono_camera(props)
        largest_yuv = its.objects.get_largest_yuv_format(props)
        if debug:
            fmt = largest_yuv
        else:
            match_ar = (largest_yuv['width'], largest_yuv['height'])
            fmt = its.objects.get_smallest_yuv_format(props, match_ar=match_ar)

        ev_per_step = its.objects.rational_to_float(
                props['android.control.aeCompensationStep'])
        steps_per_ev = int(1.0 / ev_per_step)
        evs = range(-2 * steps_per_ev, 2 * steps_per_ev + 1, steps_per_ev)
        lumas = []

        # Converge 3A, and lock AE once converged. skip AF trigger as
        # dark/bright scene could make AF convergence fail and this test
        # doesn't care the image sharpness.
        cam.do_3a(ev_comp=0, lock_ae=True, do_af=False, mono_camera=mono_camera)

        for ev in evs:
            # Capture a single shot with the same EV comp and locked AE.
            req = its.objects.auto_capture_request()
            req['android.control.aeExposureCompensation'] = ev
            req['android.control.aeLock'] = True
            caps = cam.do_capture([req]*THRESH_CONVERGE_FOR_EV, fmt)
            luma_locked = []
            for i, cap in enumerate(caps):
                if cap['metadata']['android.control.aeState'] == LOCKED:
                    y = its.image.convert_capture_to_planes(cap)[0]
                    tile = its.image.get_image_patch(y, 0.45, 0.45, 0.1, 0.1)
                    luma = its.image.compute_image_means(tile)[0]
                    luma_locked.append(luma)
                    if i == THRESH_CONVERGE_FOR_EV-1:
                        lumas.append(luma)
                        print 'lumas in AE locked captures: ', luma_locked
                        msg = 'AE locked lumas: %s, RTOL: %.2f' % (
                                str(luma_locked), LUMA_LOCKED_TOL)
                        assert np.isclose(min(luma_locked), max(luma_locked),
                                          rtol=LUMA_LOCKED_TOL), msg
            assert caps[THRESH_CONVERGE_FOR_EV-1]['metadata']['android.control.aeState'] == LOCKED

        pylab.plot(evs, lumas, '-ro')
        pylab.title(NAME)
        pylab.xlabel('EV Compensation')
        pylab.ylabel('Mean Luma (Normalized)')
        matplotlib.pyplot.savefig('%s_plot_means.png' % (NAME))

        # Trim extra saturated images
        while (lumas[-2] >= YUV_SAT_MIN/YUV_FULL_SCALE and
               lumas[-1] >= YUV_SAT_MIN/YUV_FULL_SCALE and
               len(lumas) > 2):
            lumas.pop(-1)
            print 'Removed saturated image.'

        # Only allow positive EVs to give saturated image
        assert len(lumas) > 2, '3 or more unsaturated images needed'
        min_luma_diffs = min(np.diff(lumas))
        print 'Min of the luma value difference between adjacent ev comp: ',
        print min_luma_diffs
        # All luma brightness should be increasing with increasing ev comp.
        assert min_luma_diffs > 0, 'Luma is not increasing!'

if __name__ == '__main__':
    main()
