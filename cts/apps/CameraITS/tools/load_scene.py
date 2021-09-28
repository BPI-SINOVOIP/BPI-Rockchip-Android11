# Copyright 2016 The Android Open Source Project
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
import re
import subprocess
import sys
import time

import its.cv2image
import numpy as np

LOAD_SCENE_DELAY = 2  # seconds


def main():
    """Load charts on device and display."""
    scene = None
    for s in sys.argv[1:]:
        if s[:6] == 'scene=' and len(s) > 6:
            scene = s[6:]
        elif s[:7] == 'screen=' and len(s) > 7:
            screen_id = s[7:]
        elif s[:5] == 'dist=' and len(s) > 5:
            chart_distance = float(re.sub('cm', '', s[5:]))
        elif s[:4] == 'fov=' and len(s) > 4:
            camera_fov = float(s[4:])

    cmd = ('adb -s %s shell am force-stop com.google.android.apps.docs' %
           screen_id)
    subprocess.Popen(cmd.split())

    if not scene:
        print 'Error: need to specify which scene to load'
        assert False

    if not screen_id:
        print 'Error: need to specify screen serial'
        assert False

    src_scene_path = os.path.join(os.environ['CAMERA_ITS_TOP'], 'tests', scene)
    dst_scene_file = '/sdcard/Download/%s.pdf' % scene
    chart_scaling = its.cv2image.calc_chart_scaling(chart_distance, camera_fov)
    if np.isclose(chart_scaling, its.cv2image.SCALE_TELE_IN_WFOV_BOX, atol=0.01):
        file_name = '%s_%sx_scaled.pdf' % (
                scene, str(its.cv2image.SCALE_TELE_IN_WFOV_BOX))
    elif np.isclose(chart_scaling, its.cv2image.SCALE_RFOV_IN_WFOV_BOX, atol=0.01):
        file_name = '%s_%sx_scaled.pdf' % (
                scene, str(its.cv2image.SCALE_RFOV_IN_WFOV_BOX))
    else:
        file_name = '%s.pdf' % scene
    src_scene_file = os.path.join(src_scene_path, file_name)
    print 'Loading %s on %s' % (src_scene_file, screen_id)
    cmd = 'adb -s %s push %s /mnt%s' % (screen_id, src_scene_file,
                                        dst_scene_file)
    subprocess.Popen(cmd.split())
    time.sleep(LOAD_SCENE_DELAY)  # wait-for-device doesn't always seem to work
    # The intent require PDF viewing app be installed on device.
    # Also the first time such app is opened it might request some permission,
    # so it's  better to grant those permissions before using this script
    cmd = ("adb -s %s wait-for-device shell am start -d 'file://%s'"
           " -a android.intent.action.VIEW" % (screen_id, dst_scene_file))
    subprocess.Popen(cmd.split())

if __name__ == '__main__':
    main()
