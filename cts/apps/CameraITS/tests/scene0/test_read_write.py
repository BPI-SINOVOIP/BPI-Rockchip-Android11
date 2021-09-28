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

NAME = os.path.basename(__file__).split('.')[0]
RTOL_EXP_GAIN = 0.97
TEST_EXP_RANGE = [6E6, 1E9]  # ns [6ms, 1s]


def main():
    """Test that the device will write/read correct exp/gain values."""

    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        props = cam.override_with_hidden_physical_camera_props(props)
        its.caps.skip_unless(its.caps.manual_sensor(props) and
                             its.caps.per_frame_control(props))

        valid_formats = ['yuv', 'jpg']
        if its.caps.raw16(props):
            valid_formats.insert(0, 'raw')
        # grab exp/gain ranges from camera
        sensor_exp_range = props['android.sensor.info.exposureTimeRange']
        sens_range = props['android.sensor.info.sensitivityRange']
        print 'sensor e range:', sensor_exp_range
        print 'sensor s range:', sens_range

        # determine if exposure test range is within sensor reported range
        assert sensor_exp_range[0] != 0
        exp_range = []
        if sensor_exp_range[0] < TEST_EXP_RANGE[0]:
            exp_range.append(TEST_EXP_RANGE[0])
        else:
            exp_range.append(sensor_exp_range[0])
        if sensor_exp_range[1] > TEST_EXP_RANGE[1]:
            exp_range.append(TEST_EXP_RANGE[1])
        else:
            exp_range.append(sensor_exp_range[1])

        data = {}
        # build requests
        for fmt in valid_formats:
            print 'format: %s' % fmt
            size = its.objects.get_available_output_sizes(fmt, props)[-1]
            out_surface = {'width': size[0], 'height': size[1], 'format': fmt}
            if cam._hidden_physical_id:
                out_surface['physicalCamera'] = cam._hidden_physical_id
            reqs = []
            index_list = []
            for exp in exp_range:
                for sens in sens_range:
                    reqs.append(its.objects.manual_capture_request(sens, exp))
                    index_list.append((fmt, exp, sens))
                    print 'exp_write: %d, sens_write: %d' % (exp, sens)

            # take shots
            caps = cam.do_capture(reqs, out_surface)

            # extract exp/sensitivity data
            for i, cap in enumerate(caps):
                e_read = cap['metadata']['android.sensor.exposureTime']
                s_read = cap['metadata']['android.sensor.sensitivity']
                data[index_list[i]] = (fmt, e_read, s_read)

        # check read/write match across all shots
        e_failed = []
        s_failed = []
        for fmt_write in valid_formats:
            for e_write in exp_range:
                for s_write in sens_range:
                    fmt_read, e_read, s_read = data[(
                            fmt_write, e_write, s_write)]
                    if (e_write < e_read or
                                e_read/float(e_write) <= RTOL_EXP_GAIN):
                        e_failed.append({'format': fmt_read,
                                         'e_write': e_write,
                                         'e_read': e_read,
                                         's_write': s_write,
                                         's_read': s_read})
                    if (s_write < s_read or
                                s_read/float(s_write) <= RTOL_EXP_GAIN):
                        s_failed.append({'format': fmt_read,
                                         'e_write': e_write,
                                         'e_read': e_read,
                                         's_write': s_write,
                                         's_read': s_read})

        # print results
        if e_failed:
            print '\nFAILs for exposure time'
            for fail in e_failed:
                print ' format: %s, e_write: %d, e_read: %d, RTOL: %.2f, ' % (
                        fail['format'], fail['e_write'], fail['e_read'],
                        RTOL_EXP_GAIN),
                print 's_write: %d, s_read: %d, RTOL: %.2f' % (
                        fail['s_write'], fail['s_read'], RTOL_EXP_GAIN)
        if s_failed:
            print 'FAILs for sensitivity(ISO)'
            for fail in s_failed:
                print ' format: %s, s_write: %d, s_read: %d, RTOL: %.2f, ' % (
                        fail['format'], fail['s_write'], fail['s_read'],
                        RTOL_EXP_GAIN),
                print 'e_write: %d, e_read: %d, RTOL: %.2f' % (
                        fail['e_write'], fail['e_read'], RTOL_EXP_GAIN)

        # assert PASS/FAIL
        assert not e_failed+s_failed


if __name__ == '__main__':
    main()
