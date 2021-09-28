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

import copy
import math
import os
import os.path
import re
import subprocess
import sys
import tempfile
import threading
import time

import its.caps
import its.cv2image
import its.device
from its.device import ItsSession
import its.image
import rotation_rig as rot

# For checking the installed APK's target SDK version
MIN_SUPPORTED_SDK_VERSION = 28  # P

CHART_DELAY = 1  # seconds
CHART_LEVEL = 96
NOT_YET_MANDATED_ALL = 100
NUM_TRYS = 2
PROC_TIMEOUT_CODE = -101  # terminated process return -process_id
PROC_TIMEOUT_TIME = 900  # timeout in seconds for a process (15 minutes)
SCENE3_FILE = os.path.join(os.environ['CAMERA_ITS_TOP'], 'pymodules', 'its',
                           'test_images', 'ISO12233.png')
SKIP_RET_CODE = 101  # note this must be same as tests/scene*/test_*
VGA_HEIGHT = 480
VGA_WIDTH = 640

# All possible scenes
# Notes on scene names:
#   scene*_1/2/... are same scene split to load balance run times for scenes
#   scene*_a/b/... are similar scenes that share one or more tests
ALL_SCENES = ['scene0', 'scene1_1', 'scene1_2', 'scene2_a', 'scene2_b',
              'scene2_c', 'scene2_d', 'scene2_e', 'scene3', 'scene4',
              'scene5', 'scene6', 'sensor_fusion', 'scene_change']

# Scenes that are logically grouped and can be called as group
GROUPED_SCENES = {
        'scene1': ['scene1_1', 'scene1_2'],
        'scene2': ['scene2_a', 'scene2_b', 'scene2_c', 'scene2_d', 'scene2_e']
}

# Scenes that can be automated through tablet display
AUTO_SCENES = ['scene0', 'scene1_1', 'scene1_2', 'scene2_a', 'scene2_b',
               'scene2_c', 'scene2_d', 'scene2_e', 'scene3', 'scene4',
               'scene6', 'scene_change']

SCENE_REQ = {
        'scene0': None,
        'scene1_1': 'A grey card covering at least the middle 30% of the scene',
        'scene1_2': 'A grey card covering at least the middle 30% of the scene',
        'scene2_a': 'The picture in tests/scene2_a.pdf with 3 faces',
        'scene2_b': 'The picture in tests/scene2_b.pdf with 3 faces',
        'scene2_c': 'The picture in tests/scene2_c.pdf with 3 faces',
        'scene2_d': 'The picture in tests/scene2_d.pdf with 3 faces',
        'scene2_e': 'The picture in tests/scene2_e.pdf with 3 faces',
        'scene3': 'The ISO 12233 chart',
        'scene4': 'A specific test page of a circle covering at least the '
                  'middle 50% of the scene. See CameraITS.pdf section 2.3.4 '
                  'for more details',
        'scene5': 'Capture images with a diffuser attached to the camera. See '
                  'CameraITS.pdf section 2.3.4 for more details',
        'scene6': 'A specific test page of a grid of 9x5 circles circle '
                  'middle 50% of the scene.',
        'sensor_fusion': 'Rotating checkboard pattern. See '
                         'sensor_fusion/SensorFusion.pdf for detailed '
                         'instructions.\nNote that this test will be skipped '
                         'on devices not supporting REALTIME camera timestamp.',
        'scene_change': 'The picture in tests/scene_change.pdf with faces'
}

SCENE_EXTRA_ARGS = {
        'scene5': ['doAF=False']
}

# Not yet mandated tests ['test', first_api_level mandatory]
# ie. ['test_test_patterns', 30] is MANDATED for first_api_level >= 30
NOT_YET_MANDATED = {
        'scene0': [
                ['test_test_patterns', 30],
                ['test_tonemap_curve', 30]
        ],
        'scene1_1': [
                ['test_ae_precapture_trigger', 28],
                ['test_channel_saturation', 29]
        ],
        'scene1_2': [],
        'scene2_a': [
                ['test_jpeg_quality', 30]
        ],
        'scene2_b': [
                ['test_auto_per_frame_control', NOT_YET_MANDATED_ALL]
        ],
        'scene2_c': [],
        'scene2_d': [
                ['test_num_faces', 30]
        ],
        'scene2_e': [
                ['test_num_faces', 30],
                ['test_continuous_picture', 30]
        ],
        'scene3': [],
        'scene4': [],
        'scene5': [],
        'scene6': [
                ['test_zoom', 30]
        ],
        'sensor_fusion': [],
        'scene_change': [
                ['test_scene_change', 31]
        ]
}

# Must match mHiddenPhysicalCameraSceneIds in ItsTestActivity.java
HIDDEN_PHYSICAL_CAMERA_TESTS = {
        'scene0': [
                'test_burst_capture',
                'test_metadata',
                'test_read_write',
                'test_sensor_events',
                'test_unified_timestamps'
        ],
        'scene1_1': [
                'test_exposure',
                'test_dng_noise_model',
                'test_linearity',
        ],
        'scene1_2': [
                'test_raw_exposure',
                'test_raw_sensitivity'
        ],
        'scene2_a': [
                'test_faces',
                'test_num_faces'
        ],
        'scene2_b': [],
        'scene2_c': [],
        'scene2_d': [],
        'scene2_e': [],
        'scene3': [],
        'scene4': [
                'test_aspect_ratio_and_crop'
        ],
        'scene5': [],
        'scene6': [],
        'sensor_fusion': [
                'test_sensor_fusion'
        ],
        'scene_change': []
}

# Tests run in more than 1 scene.
# List is created of type ['scene_source', 'test_to_be_repeated']
# for the test run in current scene.
REPEATED_TESTS = {
        'scene0': [],
        'scene1_1': [],
        'scene1_2': [],
        'scene2_a': [],
        'scene2_b': [
                ['scene2_a', 'test_num_faces']
        ],
        'scene2_c': [
                ['scene2_a', 'test_num_faces']
        ],
        'scene2_d': [
                ['scene2_a', 'test_num_faces']
        ],
        'scene2_e': [
                ['scene2_a', 'test_num_faces']
        ],
        'scene3': [],
        'scene4': [],
        'scene5': [],
        'scene6': [],
        'sensor_fusion': [],
        'scene_change': []
}


def determine_not_yet_mandated_tests(device_id):
    """Determine from NEW_YET_MANDATED & phone info not_yet_mandated tests.

    Args:
        device_id:      string of device id number

    Returns:
        dict of not yet mandated tests
    """
    # initialize not_yet_mandated
    not_yet_mandated = {}
    for scene in ALL_SCENES:
        not_yet_mandated[scene] = []

    # Determine first API level for device
    first_api_level = its.device.get_first_api_level(device_id)

    # Determine which scenes are not yet mandated for first api level
    for scene, tests in NOT_YET_MANDATED.items():
        for test in tests:
            if test[1] >= first_api_level:
                not_yet_mandated[scene].append(test[0])
    return not_yet_mandated


def expand_scene(scene, scenes):
    """Expand a grouped scene and append its sub_scenes to scenes.

    Args:
        scene:      scene in GROUPED_SCENES dict
        scenes:     list of scenes to append to

    Returns:
        updated scenes
    """
    print 'Expanding %s to %s.' % (scene, str(GROUPED_SCENES[scene]))
    for sub_scene in GROUPED_SCENES[scene]:
        scenes.append(sub_scene)


def run_subprocess_with_timeout(cmd, fout, ferr, outdir):
    """Run subprocess with a timeout.

    Args:
        cmd:    list containing python command
        fout:   stdout file for the test
        ferr:   stderr file for the test
        outdir: dir location for fout/ferr

    Returns:
        process status or PROC_TIMEOUT_CODE if timer maxes
    """

    proc = subprocess.Popen(
            cmd, stdout=fout, stderr=ferr, cwd=outdir)
    timer = threading.Timer(PROC_TIMEOUT_TIME, proc.kill)

    try:
        timer.start()
        proc.communicate()
        test_code = proc.returncode
    finally:
        timer.cancel()

    if test_code < 0:
        return PROC_TIMEOUT_CODE
    else:
        return test_code


def calc_camera_fov(camera_id, hidden_physical_id):
    """Determine the camera field of view from internal params."""
    with ItsSession(camera_id, hidden_physical_id) as cam:
        props = cam.get_camera_properties()
        props = cam.override_with_hidden_physical_camera_props(props)
        focal_ls = props['android.lens.info.availableFocalLengths']
        if len(focal_ls) > 1:
            print 'Doing capture to determine logical camera focal length'
            cap = cam.do_capture(its.objects.auto_capture_request())
            focal_l = cap['metadata']['android.lens.focalLength']
        else:
            focal_l = focal_ls[0]
    sensor_size = props['android.sensor.info.physicalSize']
    diag = math.sqrt(sensor_size['height'] ** 2 +
                     sensor_size['width'] ** 2)
    try:
        fov = str(round(2 * math.degrees(math.atan(diag / (2 * focal_l))), 2))
    except ValueError:
        fov = str(0)
    print 'Calculated FoV: %s' % fov
    return fov


def evaluate_socket_failure(err_file_path):
    """Determine if test fails due to socket FAIL."""
    socket_fail = False
    with open(err_file_path, 'r') as ferr:
        for line in ferr:
            if (line.find('socket.error') != -1 or
                line.find('socket.timeout') != -1 or
                line.find('Problem with socket') != -1):
                socket_fail = True
    return socket_fail


def run_rotations(camera_id, test_name):
    """Determine if camera rotation is run for this test."""
    with ItsSession(camera_id) as cam:
        props = cam.get_camera_properties()
        method = {'test_sensor_fusion': {
                          'flag': its.caps.sensor_fusion_test_capable(props, cam),
                          'runs': 10},
                  'test_multi_camera_frame_sync': {
                          'flag': its.caps.multi_camera_frame_sync_capable(props),
                          'runs': 5}
                 }
        return method[test_name]


def main():
    """Run all the automated tests, saving intermediate files, and producing
    a summary/report of the results.

    Script should be run from the top-level CameraITS directory.

    Command line arguments:
        camera:  the camera(s) to be tested. Use comma to separate multiple
                 camera Ids. Ex: "camera=0,1" or "camera=1"
        device:  device id for adb
        scenes:  the test scene(s) to be executed. Use comma to separate
                 multiple scenes. Ex: "scenes=scene0,scene1_1" or
                 "scenes=0,1_1,sensor_fusion" (sceneX can be abbreviated by X
                 where X is scene name minus 'scene')
        chart:   another android device served as test chart display.
                 When this argument presents, change of test scene
                 will be handled automatically. Note that this argument
                 requires special physical/hardware setup to work and may not
                 work on all android devices.
        result:  Device ID to forward results to (in addition to the device
                 that the tests are running on).
        rot_rig: ID of the rotation rig being used (formatted as
                 "<vendor ID>:<product ID>:<channel #>" or "default" for
                 Canakit-based rotators or "arduino:<channel #>" for
                 Arduino-based rotators)
        tmp_dir: location of temp directory for output files
        skip_scene_validation: force skip scene validation. Used when test scene
                 is setup up front and don't require tester validation.
        dist:    chart distance in cm.
    """

    camera_id_combos = []
    scenes = []
    chart_host_id = None
    result_device_id = None
    rot_rig_id = None
    tmp_dir = None
    skip_scene_validation = False
    chart_distance = its.cv2image.CHART_DISTANCE_RFOV
    chart_level = CHART_LEVEL
    one_camera_argv = sys.argv[1:]

    for s in list(sys.argv[1:]):
        if s[:7] == 'camera=' and len(s) > 7:
            camera_ids = s[7:].split(',')
            camera_id_combos = its.device.parse_camera_ids(camera_ids)
            one_camera_argv.remove(s)
        elif s[:7] == 'scenes=' and len(s) > 7:
            scenes = s[7:].split(',')
        elif s[:6] == 'chart=' and len(s) > 6:
            chart_host_id = s[6:]
        elif s[:7] == 'result=' and len(s) > 7:
            result_device_id = s[7:]
        elif s[:8] == 'rot_rig=' and len(s) > 8:
            rot_rig_id = s[8:]  # valid values: 'default', '$VID:$PID:$CH',
            # or 'arduino:$CH'. The default '$VID:$PID:$CH' is '04d8:fc73:1'
        elif s[:8] == 'tmp_dir=' and len(s) > 8:
            tmp_dir = s[8:]
        elif s == 'skip_scene_validation':
            skip_scene_validation = True
        elif s[:5] == 'dist=' and len(s) > 5:
            chart_distance = float(re.sub('cm', '', s[5:]))
        elif s[:11] == 'brightness=' and len(s) > 11:
            chart_level = s[11:]

    chart_dist_arg = 'dist= ' + str(chart_distance)
    chart_level_arg = 'brightness=' + str(chart_level)
    auto_scene_switch = chart_host_id is not None
    merge_result_switch = result_device_id is not None

    # Run through all scenes if user does not supply one
    possible_scenes = AUTO_SCENES if auto_scene_switch else ALL_SCENES
    if not scenes:
        scenes = possible_scenes
    else:
        # Validate user input scene names
        valid_scenes = True
        temp_scenes = []
        for s in scenes:
            if s in possible_scenes:
                temp_scenes.append(s)
            elif GROUPED_SCENES.has_key(s):
                expand_scene(s, temp_scenes)
            else:
                try:
                    # Try replace "X" to "sceneX"
                    scene_str = "scene" + s
                    if scene_str in possible_scenes:
                        temp_scenes.append(scene_str)
                    elif GROUPED_SCENES.has_key(scene_str):
                        expand_scene(scene_str, temp_scenes)
                    else:
                        valid_scenes = False
                        break
                except ValueError:
                    valid_scenes = False
                    break

        if not valid_scenes:
            print 'Unknown scene specified:', s
            assert False
        # assign temp_scenes back to scenes and remove duplicates
        scenes = sorted(set(temp_scenes), key=temp_scenes.index)

    # Make output directories to hold the generated files.
    topdir = tempfile.mkdtemp(dir=tmp_dir)
    subprocess.call(['chmod', 'g+rx', topdir])
    print "Saving output files to:", topdir, "\n"

    device_id = its.device.get_device_id()
    device_id_arg = "device=" + device_id
    print "Testing device " + device_id

    # Check CtsVerifier SDK level
    # Here we only do warning as there is no guarantee on pm dump output formt not changed
    # Also sometimes it's intentional to run mismatched versions
    cmd = "adb -s %s shell pm dump com.android.cts.verifier" % (device_id)
    dump_path = os.path.join(topdir, 'CtsVerifier.txt')
    with open(dump_path, 'w') as fout:
        fout.write('ITS minimum supported SDK version is %d\n--\n' % (MIN_SUPPORTED_SDK_VERSION))
        fout.flush()
        ret_code = subprocess.call(cmd.split(), stdout=fout)

    if ret_code != 0:
        print "Warning: cannot get CtsVerifier SDK version. Is CtsVerifier installed?"

    ctsv_version = None
    ctsv_version_name = None
    with open(dump_path, 'r') as f:
        target_sdk_found = False
        version_name_found = False
        for line in f:
            match = re.search('targetSdk=([0-9]+)', line)
            if match:
                ctsv_version = int(match.group(1))
                target_sdk_found = True
            match = re.search('versionName=([\S]+)$', line)
            if match:
                ctsv_version_name = match.group(1)
                version_name_found = True
            if target_sdk_found and version_name_found:
                break

    if ctsv_version is None:
        print "Warning: cannot get CtsVerifier SDK version. Is CtsVerifier installed?"
    elif ctsv_version < MIN_SUPPORTED_SDK_VERSION:
        print "Warning: CtsVerifier version (%d) < ITS version (%d), is this intentional?" % (
                ctsv_version, MIN_SUPPORTED_SDK_VERSION)
    else:
        print "CtsVerifier targetSdk is", ctsv_version
        if ctsv_version_name:
            print "CtsVerifier version name is", ctsv_version_name

    # Hard check on ItsService/host script version that should catch incompatible APK/script
    with ItsSession() as cam:
        cam.check_its_version_compatible()

    # Correctness check for devices
    device_bfp = its.device.get_device_fingerprint(device_id)
    assert device_bfp is not None

    if auto_scene_switch:
        chart_host_bfp = its.device.get_device_fingerprint(chart_host_id)
        assert chart_host_bfp is not None

    if merge_result_switch:
        result_device_bfp = its.device.get_device_fingerprint(result_device_id)
        assert_err_msg = ('Cannot merge result to a different build, from '
                          '%s to %s' % (device_bfp, result_device_bfp))
        assert device_bfp == result_device_bfp, assert_err_msg

    # user doesn't specify camera id, run through all cameras
    if not camera_id_combos:
        with its.device.ItsSession() as cam:
            camera_ids = cam.get_camera_ids()
            camera_id_combos = its.device.parse_camera_ids(camera_ids)

    print "Running ITS on camera: %s, scene %s" % (camera_id_combos, scenes)

    if auto_scene_switch:
        # merge_result only supports run_parallel_tests
        if merge_result_switch and camera_ids[0] == "1":
            print "Skip chart screen"
            time.sleep(1)
        else:
            print "Waking up chart screen: ", chart_host_id
            screen_id_arg = ("screen=%s" % chart_host_id)
            cmd = ["python", os.path.join(os.environ["CAMERA_ITS_TOP"], "tools",
                                          "wake_up_screen.py"), screen_id_arg,
                   chart_level_arg]
            wake_code = subprocess.call(cmd)
            assert wake_code == 0

    for id_combo in camera_id_combos:
        # Initialize test results
        results = {}
        result_key = ItsSession.RESULT_KEY
        for s in ALL_SCENES:
            results[s] = {result_key: ItsSession.RESULT_NOT_EXECUTED}

        camera_fov = calc_camera_fov(id_combo.id, id_combo.sub_id)
        id_combo_string = id_combo.id
        has_hidden_sub_camera = id_combo.sub_id is not None
        if has_hidden_sub_camera:
            id_combo_string += ItsSession.CAMERA_ID_TOKENIZER + id_combo.sub_id
            scenes = [scene for scene in scenes if HIDDEN_PHYSICAL_CAMERA_TESTS[scene]]
        # Loop capturing images until user confirm test scene is correct
        camera_id_arg = "camera=" + id_combo.id
        print "Preparing to run ITS on camera", id_combo_string, "for scenes ", scenes

        os.mkdir(os.path.join(topdir, id_combo_string))
        for d in scenes:
            os.mkdir(os.path.join(topdir, id_combo_string, d))

        tot_tests = []
        tot_pass = 0
        not_yet_mandated = determine_not_yet_mandated_tests(device_id)
        for scene in scenes:
            # unit is millisecond for execution time record in CtsVerifier
            scene_start_time = int(round(time.time() * 1000))
            skip_code = None
            tests = [(s[:-3], os.path.join('tests', scene, s))
                     for s in os.listdir(os.path.join('tests', scene))
                     if s[-3:] == '.py' and s[:4] == 'test']
            if REPEATED_TESTS[scene]:
                for t in REPEATED_TESTS[scene]:
                    tests.append((t[1], os.path.join('tests', t[0], t[1]+'.py')))
            tests.sort()
            tot_tests.extend(tests)

            summary = 'Cam' + id_combo_string + ' ' + scene + '\n'
            numpass = 0
            numskip = 0
            num_not_mandated_fail = 0
            numfail = 0
            validate_switch = True
            if SCENE_REQ[scene] is not None:
                out_path = os.path.join(topdir, id_combo_string, scene+'.jpg')
                out_arg = 'out=' + out_path
                if ((scene == 'sensor_fusion' and rot_rig_id) or
                            skip_scene_validation):
                    validate_switch = False
                cmd = None
                if auto_scene_switch:
                    if (not merge_result_switch or
                            (merge_result_switch and id_combo_string == '0')):
                        scene_arg = 'scene=' + scene
                        fov_arg = 'fov=' + camera_fov
                        cmd = ['python',
                               os.path.join(os.getcwd(), 'tools/load_scene.py'),
                               scene_arg, chart_dist_arg, fov_arg, screen_id_arg]
                    else:
                        time.sleep(CHART_DELAY)
                else:
                    # Skip scene validation under certain conditions
                    if validate_switch and not merge_result_switch:
                        scene_arg = 'scene=' + SCENE_REQ[scene]
                        extra_args = SCENE_EXTRA_ARGS.get(scene, [])
                        cmd = ['python',
                               os.path.join(os.getcwd(),
                                            'tools/validate_scene.py'),
                               camera_id_arg, out_arg,
                               scene_arg, device_id_arg] + extra_args
                if cmd is not None:
                    valid_scene_code = subprocess.call(cmd, cwd=topdir)
                    assert valid_scene_code == 0
            print 'Start running ITS on camera %s, %s' % (
                    id_combo_string, scene)
            # Extract chart from scene for scene3 once up front
            chart_loc_arg = ''
            chart_height = its.cv2image.CHART_HEIGHT
            if scene == 'scene3':
                chart_height *= its.cv2image.calc_chart_scaling(
                        chart_distance, camera_fov)
                chart = its.cv2image.Chart(SCENE3_FILE, chart_height,
                                           chart_distance,
                                           its.cv2image.CHART_SCALE_START,
                                           its.cv2image.CHART_SCALE_STOP,
                                           its.cv2image.CHART_SCALE_STEP,
                                           id_combo.id)
                chart_loc_arg = 'chart_loc=%.2f,%.2f,%.2f,%.2f,%.3f' % (
                        chart.xnorm, chart.ynorm, chart.wnorm, chart.hnorm,
                        chart.scale)
            if scene == 'scene_change' and not auto_scene_switch:
                print '\nWave hand over camera to create scene change'
            # Run each test, capturing stdout and stderr.
            for (testname, testpath) in tests:
                # Only pick predefined tests for hidden physical camera
                if has_hidden_sub_camera and \
                        testname not in HIDDEN_PHYSICAL_CAMERA_TESTS[scene]:
                    numskip += 1
                    continue
                if auto_scene_switch:
                    if merge_result_switch and id_combo_string == '0':
                        # Send an input event to keep the screen not dimmed.
                        # Since we are not using camera of chart screen, FOCUS event
                        # should do nothing but keep the screen from dimming.
                        # The "sleep after x minutes of inactivity" display setting
                        # determines how long this command can keep screen bright.
                        # Setting it to something like 30 minutes should be enough.
                        cmd = ('adb -s %s shell input keyevent FOCUS'
                               % chart_host_id)
                        subprocess.call(cmd.split())
                t0 = time.time()
                t_rotate = 0.0  # time in seconds
                for num_try in range(NUM_TRYS):
                    outdir = os.path.join(topdir, id_combo_string, scene)
                    outpath = os.path.join(outdir, testname+'_stdout.txt')
                    errpath = os.path.join(outdir, testname+'_stderr.txt')
                    if scene == 'sensor_fusion':
                        # determine if you need to rotate for specific test
                        rotation_props = run_rotations(id_combo.id, testname)
                        if rotation_props['flag']:
                            if rot_rig_id:
                                print 'Rotating phone w/ rig %s' % rot_rig_id
                                rig = 'python tools/rotation_rig.py rotator=%s num_rotations=%s' % (
                                        rot_rig_id, rotation_props['runs'])
                                subprocess.Popen(rig.split())
                                t_rotate = (rotation_props['runs'] *
                                            len(rot.ARDUINO_ANGLES) *
                                            rot.ARDUINO_MOVE_TIME) + 2  # 2s slop
                            else:
                                print 'Rotate phone 15s as shown in SensorFusion.pdf'
                        else:
                            test_code = skip_code
                    if skip_code is not SKIP_RET_CODE:
                        cmd = ['python', os.path.join(os.getcwd(), testpath)]
                        cmd += one_camera_argv + ["camera="+id_combo_string] + [chart_loc_arg]
                        cmd += [chart_dist_arg]
                        with open(outpath, 'w') as fout, open(errpath, 'w') as ferr:
                            test_code = run_subprocess_with_timeout(
                                cmd, fout, ferr, outdir)
                    if test_code == 0 or test_code == SKIP_RET_CODE:
                        break
                    else:
                        socket_fail = evaluate_socket_failure(errpath)
                        if socket_fail or test_code == PROC_TIMEOUT_CODE:
                            if num_try != NUM_TRYS-1:
                                print ' Retry %s/%s' % (scene, testname)
                            else:
                                break
                        else:
                            break
                t_test = time.time() - t0

                # define rotator_type
                rotator_type = 'canakit'
                if rot_rig_id and 'arduino' in rot_rig_id.split(':'):
                    rotator_type = 'arduino'
                    # if arduino, wait for rotations to stop
                    if t_rotate > t_test:
                        time.sleep(t_rotate - t_test)

                test_failed = False
                if test_code == 0:
                    retstr = "PASS "
                    numpass += 1
                elif test_code == SKIP_RET_CODE:
                    retstr = "SKIP "
                    numskip += 1
                elif test_code != 0 and testname in not_yet_mandated[scene]:
                    retstr = "FAIL*"
                    num_not_mandated_fail += 1
                else:
                    retstr = "FAIL "
                    numfail += 1
                    test_failed = True

                msg = '%s %s/%s' % (retstr, scene, testname)
                if rotator_type == 'arduino':
                    msg += ' [%.1fs]' % t_rotate
                else:
                    msg += ' [%.1fs]' % t_test
                print msg
                its.device.adb_log(device_id, msg)
                msg_short = '%s %s [%.1fs]' % (retstr, testname, t_test)
                if test_failed:
                    summary += msg_short + "\n"

            # unit is millisecond for execution time record in CtsVerifier
            scene_end_time = int(round(time.time() * 1000))

            if numskip > 0:
                skipstr = ", %d test%s skipped" % (
                    numskip, "s" if numskip > 1 else "")
            else:
                skipstr = ""

            test_result = "\n%d / %d tests passed (%.1f%%)%s" % (
                numpass + num_not_mandated_fail, len(tests) - numskip,
                100.0 * float(numpass + num_not_mandated_fail) /
                (len(tests) - numskip)
                if len(tests) != numskip else 100.0, skipstr)
            print test_result

            if num_not_mandated_fail > 0:
                msg = "(*) tests are not yet mandated"
                print msg

            tot_pass += numpass
            print "%s compatibility score: %.f/100\n" % (
                    scene, 100.0 * numpass / len(tests))

            summary_path = os.path.join(topdir, id_combo_string, scene, "summary.txt")
            with open(summary_path, "w") as f:
                f.write(summary)

            passed = numfail == 0
            results[scene][result_key] = (ItsSession.RESULT_PASS if passed
                                          else ItsSession.RESULT_FAIL)
            results[scene][ItsSession.SUMMARY_KEY] = summary_path
            results[scene][ItsSession.START_TIME_KEY] = scene_start_time
            results[scene][ItsSession.END_TIME_KEY] = scene_end_time

        if tot_tests:
            print "Compatibility Score: %.f/100" % (100.0 * tot_pass / len(tot_tests))
        else:
            print "Compatibility Score: 0/100"

        msg = "Reporting ITS result to CtsVerifier"
        print msg
        its.device.adb_log(device_id, msg)
        if merge_result_switch:
            # results are modified by report_result
            results_backup = copy.deepcopy(results)
            its.device.report_result(result_device_id, id_combo_string, results_backup)

        # Report hidden_physical_id results as well.
        its.device.report_result(device_id, id_combo_string, results)

    if auto_scene_switch:
        if merge_result_switch:
            print 'Skip shutting down chart screen'
        else:
            print 'Shutting down chart screen: ', chart_host_id
            screen_id_arg = ('screen=%s' % chart_host_id)
            cmd = ['python', os.path.join(os.environ['CAMERA_ITS_TOP'], 'tools',
                                          'toggle_screen.py'), screen_id_arg,
                                          'state=OFF']
            screen_off_code = subprocess.call(cmd)
            assert screen_off_code == 0

            print 'Shutting down DUT screen: ', device_id
            screen_id_arg = ('screen=%s' % device_id)
            cmd = ['python', os.path.join(os.environ['CAMERA_ITS_TOP'], 'tools',
                                          'toggle_screen.py'), screen_id_arg,
                                          'state=OFF']
            screen_off_code = subprocess.call(cmd)
            assert screen_off_code == 0

    print "ITS tests finished. Please go back to CtsVerifier and proceed"

if __name__ == '__main__':
    main()
