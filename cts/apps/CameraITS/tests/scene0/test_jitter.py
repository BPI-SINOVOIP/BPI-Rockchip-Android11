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

# PASS/FAIL thresholds
TEST_FPS = 30
MIN_AVG_FRAME_DELTA = 30  # at least 30ms delta between frames
MAX_VAR_FRAME_DELTA = 0.01  # variance of frame deltas
MAX_FRAME_DELTA_JITTER = 0.3  # max ms gap from the average frame delta

NAME = os.path.basename(__file__).split('.')[0]


def main():
    """Measure jitter in camera timestamps."""

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        its.caps.skip_unless(its.caps.manual_sensor(props) and
                             its.caps.sensor_fusion(props))

        req, fmt = its.objects.get_fastest_manual_capture_settings(props)
        req["android.control.aeTargetFpsRange"] = [TEST_FPS, TEST_FPS]
        caps = cam.do_capture([req]*50, [fmt])

        # Print out the millisecond delta between the start of each exposure
        tstamps = [c['metadata']['android.sensor.timestamp'] for c in caps]
        deltas = [tstamps[i]-tstamps[i-1] for i in range(1, len(tstamps))]
        deltas_ms = [d/1000000.0 for d in deltas]
        avg = sum(deltas_ms) / len(deltas_ms)
        var = sum([d*d for d in deltas_ms]) / len(deltas_ms) - avg * avg
        range0 = min(deltas_ms) - avg
        range1 = max(deltas_ms) - avg
        print 'Average:', avg
        print 'Variance:', var
        print 'Jitter range:', range0, 'to', range1

        # Draw a plot.
        pylab.plot(range(len(deltas_ms)), deltas_ms)
        pylab.title(NAME)
        pylab.xlabel('frame number')
        pylab.ylabel('jitter (ms)')
        matplotlib.pyplot.savefig('%s_deltas.png' % (NAME))

        # Test for pass/fail.
        emsg = 'avg: %.4fms, TOL: %.fms' % (avg, MIN_AVG_FRAME_DELTA)
        assert avg > MIN_AVG_FRAME_DELTA, emsg
        emsg = 'var: %.4fms, TOL: %.2fms' % (var, MAX_VAR_FRAME_DELTA)
        assert var < MAX_VAR_FRAME_DELTA, emsg
        emsg = 'range0: %.4fms, range1: %.4fms, TOL: %.2fms' % (
                range0, range1, MAX_FRAME_DELTA_JITTER)
        assert abs(range0) < MAX_FRAME_DELTA_JITTER, emsg
        assert abs(range1) < MAX_FRAME_DELTA_JITTER, emsg

if __name__ == '__main__':
    main()

