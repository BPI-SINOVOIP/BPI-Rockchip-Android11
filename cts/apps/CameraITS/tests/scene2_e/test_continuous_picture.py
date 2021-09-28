# Copyright 2020 The Android Open Source Project
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

CONTINUOUS_PICTURE_MODE = 4
CONVERGED_3A = [2, 2, 2]  # [AE, AF, AWB]
# AE_STATES: {0: INACTIVE, 1: SEARCHING, 2: CONVERGED, 3: LOCKED,
#             4: FLASH_REQ, 5: PRECAPTURE}
# AF_STATES: {0: INACTIVE, 1: PASSIVE_SCAN, 2: PASSIVE_FOCUSED,
#             3: ACTIVE_SCAN, 4: FOCUS_LOCKED, 5: NOT_FOCUSED_LOCKED,
#             6: PASSIVE_UNFOCUSED}
# AWB_STATES: {0: INACTIVE, 1: SEARCHING, 2: CONVERGED, 3: LOCKED}
NAME = os.path.basename(__file__).split('.')[0]
NUM_FRAMES = 50
W, H = 640, 480


def capture_frames(cam, debug):
    """Capture frames."""
    cap_data_list = []
    req = its.objects.auto_capture_request()
    req['android.control.afMode'] = CONTINUOUS_PICTURE_MODE
    fmt = {'format': 'yuv', 'width': W, 'height': H}
    caps = cam.do_capture([req]*NUM_FRAMES, fmt)

    # extract frame metadata and frame
    for i, cap in enumerate(caps):
        cap_data = {}
        md = cap['metadata']
        exp = md['android.sensor.exposureTime']
        iso = md['android.sensor.sensitivity']
        fd = md['android.lens.focalLength']
        ae_state = md['android.control.aeState']
        af_state = md['android.control.afState']
        awb_state = md['android.control.awbState']
        fd_str = 'infinity'
        if fd != 0.0:
            fd_str = str(round(1.0E2/fd, 2)) + 'cm'
        img = its.image.convert_capture_to_rgb_image(cap)
        tile = its.image.get_image_patch(img, 0.45, 0.45, 0.1, 0.1)
        g = its.image.compute_image_means(tile)[1]
        print '%d, iso: %d, exp: %.2fms, fd: %s, avg: %.3f' % (
                i, iso, exp*1E-6, fd_str, g),
        print '[ae,af,awb]: [%d,%d,%d]' % (ae_state, af_state, awb_state)
        cap_data['exp'] = exp
        cap_data['iso'] = iso
        cap_data['fd'] = fd
        cap_data['3a_state'] = [ae_state, af_state, awb_state]
        cap_data['avg'] = g
        cap_data_list.append(cap_data)
        if debug:
            its.image.write_image(img, '%s_%d.jpg' % (NAME, i))
    return cap_data_list


def main():
    """Test 3A converges in CONTINUOUS_PICTURE mode.

    Set camera into CONTINUOUS_PICTURE mode and do NUM_FRAMES capture.
    By the end of NUM_FRAMES capture, 3A should be in converged state.
    """

    # check for skip conditions and do 3a up front
    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        props = cam.override_with_hidden_physical_camera_props(props)
        its.caps.skip_unless(its.caps.continuous_picture(props) and
                             its.caps.read_3a(props))
        debug = its.caps.debug_mode()
        cam.do_3a()

        # ensure 3a settles in CONTINUOUS_PICTURE mode with no scene change
        cap_data = capture_frames(cam, debug)
        final_3a = cap_data[NUM_FRAMES-1]['3a_state']
        msg = '\n Last frame [ae, af, awb] state: [%d, %d, %d]' % (
                final_3a[0], final_3a[1], final_3a[2])
        msg += '\n Converged states:' + str(CONVERGED_3A)
        assert final_3a == CONVERGED_3A, msg


if __name__ == '__main__':
    main()
