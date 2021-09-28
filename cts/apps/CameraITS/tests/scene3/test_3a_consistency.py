# Copyright 2017 The Android Open Source Project
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

import its.caps
import its.device
import its.target

import numpy as np

GGAIN_TOL = 0.1
FD_TOL = 0.1
ISO_EXP_TOL = 0.1
NUM_TEST_ITERATIONS = 3


def main():
    """Basic test for 3A consistency.

    To pass, 3A must converge for exp, gain, awb, fd within TOL.
    """

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.read_3a(props))
        mono_camera = its.caps.mono_camera(props)

        iso_exps = []
        g_gains = []
        fds = []
        for _ in range(NUM_TEST_ITERATIONS):
            try:
                s, e, gains, xform, fd = cam.do_3a(get_results=True,
                                                   mono_camera=mono_camera)
                print ' iso: %d, exposure: %d, iso*exp: %d' % (s, e, e*s)
                print ' awb_gains', gains, 'awb_transform', xform
                print ' fd', fd
                print ''
                iso_exps.append(e*s)
                g_gains.append(gains[2])
                fds.append(fd)
            except its.error.Error:
                print ' FAIL\n'
        assert len(iso_exps) == NUM_TEST_ITERATIONS
        assert np.isclose(np.amax(iso_exps), np.amin(iso_exps), ISO_EXP_TOL)
        assert np.isclose(np.amax(g_gains), np.amin(g_gains), GGAIN_TOL)
        assert np.isclose(np.amax(fds), np.amin(fds), FD_TOL)
        for g in gains:
            assert not np.isnan(g)
        for x in xform:
            assert not np.isnan(x)

if __name__ == '__main__':
    main()

