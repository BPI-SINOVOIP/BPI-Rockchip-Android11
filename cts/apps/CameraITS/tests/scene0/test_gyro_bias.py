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

import os
import time

import its.caps
import its.device
import its.image
import its.objects
import its.target
import matplotlib
from matplotlib import pylab
import numpy

NAME = os.path.basename(__file__).split('.')[0]
N = 20  # Number of samples averaged together, in the plot.
MEAN_THRESH = 0.01  # PASS/FAIL threshold for gyro mean drift
VAR_THRESH = 0.001  # PASS/FAIL threshold for gyro variance drift


def main():
    """Test if the gyro has stable output when device is stationary.
    """
    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        # Only run test if the appropriate caps are claimed.
        its.caps.skip_unless(its.caps.sensor_fusion(props) and
            cam.get_sensors().get("gyro"))

        print 'Collecting gyro events'
        cam.start_sensor_events()
        time.sleep(5)
        gyro_events = cam.get_sensor_events()['gyro']

    nevents = (len(gyro_events) / N) * N
    gyro_events = gyro_events[:nevents]
    times = numpy.array([(e['time'] - gyro_events[0]['time'])/1000000000.0
                         for e in gyro_events])
    xs = numpy.array([e['x'] for e in gyro_events])
    ys = numpy.array([e['y'] for e in gyro_events])
    zs = numpy.array([e['z'] for e in gyro_events])

    # Group samples into size-N groups and average each together, to get rid
    # of individual random spikes in the data.
    times = times[N/2::N]
    xs = xs.reshape(nevents/N, N).mean(1)
    ys = ys.reshape(nevents/N, N).mean(1)
    zs = zs.reshape(nevents/N, N).mean(1)

    pylab.plot(times, xs, 'r', label='x')
    pylab.plot(times, ys, 'g', label='y')
    pylab.plot(times, zs, 'b', label='z')
    pylab.xlabel('Time (seconds)')
    pylab.ylabel('Gyro readings (mean of %d samples)'%(N))
    pylab.legend()
    matplotlib.pyplot.savefig('%s_plot.png' % (NAME))

    for samples in [xs, ys, zs]:
        mean = samples.mean()
        var = numpy.var(samples)
        assert mean < MEAN_THRESH, 'mean: %.3f, TOL=%.2f' % (mean, MEAN_THRESH)
        assert var < VAR_THRESH, 'var: %.4f, TOL=%.3f' % (var, VAR_THRESH)

if __name__ == '__main__':
    main()

