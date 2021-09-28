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

import multiprocessing
import os.path
import subprocess
import sys
import time
import its.caps
import its.device
import its.image
import its.objects

BRIGHT_CHANGE_TOL = 0.2
CONTINUOUS_PICTURE_MODE = 4
CONVERGED_3A = [2, 2, 2]  # [AE, AF, AWB]
# AE_STATES: {0: INACTIVE, 1: SEARCHING, 2: CONVERGED, 3: LOCKED,
#             4: FLASH_REQ, 5: PRECAPTURE}
# AF_STATES: {0: INACTIVE, 1: PASSIVE_SCAN, 2: PASSIVE_FOCUSED,
#             3: ACTIVE_SCAN, 4: FOCUS_LOCKED, 5: NOT_FOCUSED_LOCKED,
#             6: PASSIVE_UNFOCUSED}
# AWB_STATES: {0: INACTIVE, 1: SEARCHING, 2: CONVERGED, 3: LOCKED}
DELAY_CAPTURE = 1.5  # delay in first capture to sync events (sec)
DELAY_DISPLAY = 3.0  # time when display turns OFF (sec)
FPS = 30
FRAME_SHIFT = 5.0  # number of frames to shift to try and find scene change
NAME = os.path.basename(__file__).split('.')[0]
NUM_BURSTS = 6
NUM_FRAMES = 50
W, H = 640, 480


def get_cmd_line_args():
    chart_host_id = None
    for s in list(sys.argv[1:]):
        if s[:6] == 'chart=' and len(s) > 6:
            chart_host_id = s[6:]
    return chart_host_id


def mask_3a_settling_frames(cap_data):
    converged_frame = -1
    for i, cap in enumerate(cap_data):
        if cap['3a_state'] == CONVERGED_3A:
            converged_frame = i
            break
    print 'Frames index where 3A converges: %d' % converged_frame
    return converged_frame


def determine_if_scene_changed(cap_data, converged_frame):
    scene_changed = False
    bright_changed = False
    start_frame_brightness = cap_data[0]['avg']
    for i in range(converged_frame, len(cap_data)):
        if cap_data[i]['avg'] <= (
                start_frame_brightness * (1.0 - BRIGHT_CHANGE_TOL)):
            bright_changed = True
        if cap_data[i]['flag'] == 1:
            scene_changed = True
    return scene_changed, bright_changed


def toggle_screen(chart_host_id, state, delay):
    t0 = time.time()
    screen_id_arg = ('screen=%s' % chart_host_id)
    state_id_arg = 'state=%s' % state
    delay_arg = 'delay=%.3f' % delay
    cmd = ['python', os.path.join(os.environ['CAMERA_ITS_TOP'], 'tools',
                                  'toggle_screen.py'), screen_id_arg,
           state_id_arg, delay_arg]
    screen_cmd_code = subprocess.call(cmd)
    assert screen_cmd_code == 0
    t = time.time() - t0
    print 'tablet event %s: %.3f' % (state, t)


def capture_frames(cam, delay, burst):
    """Capture frames."""
    cap_data_list = []
    req = its.objects.auto_capture_request()
    req['android.control.afMode'] = CONTINUOUS_PICTURE_MODE
    fmt = {'format': 'yuv', 'width': W, 'height': H}
    t0 = time.time()
    time.sleep(delay)
    print 'cap event start:', time.time() - t0
    caps = cam.do_capture([req]*NUM_FRAMES, fmt)
    print 'cap event stop:', time.time() - t0
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
        scene_change_flag = md['android.control.afSceneChange']
        assert scene_change_flag in [0, 1], 'afSceneChange not in [0,1]'
        img = its.image.convert_capture_to_rgb_image(cap)
        its.image.write_image(img, '%s_%d_%d.jpg' % (NAME, burst, i))
        tile = its.image.get_image_patch(img, 0.45, 0.45, 0.1, 0.1)
        g = its.image.compute_image_means(tile)[1]
        print '%d, iso: %d, exp: %.2fms, fd: %s, avg: %.3f' % (
                i, iso, exp*1E-6, fd_str, g),
        print '[ae,af,awb]: [%d,%d,%d], change: %d' % (
                ae_state, af_state, awb_state, scene_change_flag)
        cap_data['exp'] = exp
        cap_data['iso'] = iso
        cap_data['fd'] = fd
        cap_data['3a_state'] = [ae_state, af_state, awb_state]
        cap_data['avg'] = g
        cap_data['flag'] = scene_change_flag
        cap_data_list.append(cap_data)
    return cap_data_list


def main():
    """Test scene change.

    Do auto capture with face scene. Power down tablet and recapture.
    Confirm android.control.afSceneChangeDetected is True.
    """
    # check for skip conditions and do 3a up front
    with its.device.ItsSession() as cam:
        props = cam.get_camera_properties()
        props = cam.override_with_hidden_physical_camera_props(props)
        its.caps.skip_unless(its.caps.continuous_picture(props) and
                             its.caps.af_scene_change(props) and
                             its.caps.read_3a(props))
        cam.do_3a()

        # do captures with scene change
        chart_host_id = get_cmd_line_args()
        scene_delay = DELAY_DISPLAY
        for burst in range(NUM_BURSTS):
            print 'burst number: %d' % burst
            # create scene change by turning off chart display & capture frames
            if chart_host_id:
                print '\nToggling tablet. Scene change at %.3fs.' % scene_delay
                multiprocessing.Process(name='p1', target=toggle_screen,
                                        args=(chart_host_id, 'OFF',
                                              scene_delay,)).start()
            else:
                print '\nWave hand in front of camera to create scene change.'
            cap_data = capture_frames(cam, DELAY_CAPTURE, burst)

            # find frame where 3A converges
            converged_frame = mask_3a_settling_frames(cap_data)

            # turn tablet back on to return to baseline scene state
            if chart_host_id:
                toggle_screen(chart_host_id, 'ON', 0)

            # determine if brightness changed and/or scene change flag asserted
            scene_changed, bright_changed = determine_if_scene_changed(
                    cap_data, converged_frame)

            # handle different capture cases
            if converged_frame > -1:  # 3A converges
                if scene_changed:
                    if bright_changed:
                        print ' scene & brightness change on burst %d.' % burst
                        sys.exit(0)
                    else:
                        msg = ' scene change, but no brightness change.'
                        assert False, msg
                else:  # shift scene change timing if no scene change
                    scene_shift = FRAME_SHIFT / FPS
                    if bright_changed:
                        print ' No scene change, but brightness change.'
                        print 'Shift %.3fs earlier' % scene_shift
                        scene_delay -= scene_shift  # tablet-off earlier
                    else:
                        scene_shift = FRAME_SHIFT / FPS * NUM_BURSTS
                        print ' No scene change, no brightness change.'
                        if cap_data[NUM_FRAMES-1]['avg'] < 0.2:
                            print ' Scene dark entire capture.',
                            print 'Shift %.3fs later.' % scene_shift
                            scene_delay += scene_shift  # tablet-off later
                        else:
                            print ' Scene light entire capture.',
                            print 'Shift %.3fs earlier.' % scene_shift
                            scene_delay -= scene_shift  # tablet-off earlier

            else:  # 3A does not converge
                if bright_changed:
                    scene_shift = FRAME_SHIFT / FPS
                    print ' 3A does not converge, but brightness change.',
                    print 'Shift %.3fs later' % scene_shift
                    scene_delay += scene_shift  # tablet-off earlier
                else:
                    msg = ' 3A does not converge with no brightness change.'
                    assert False, msg

        # fail out if too many tries
        msg = 'No scene change in %dx tries' % NUM_BURSTS
        assert False, msg


if __name__ == '__main__':
    main()
