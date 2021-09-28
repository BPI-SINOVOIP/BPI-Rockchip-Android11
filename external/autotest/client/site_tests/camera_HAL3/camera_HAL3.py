# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A test which verifies the camera function with HAL3 interface."""

import contextlib
import json
import logging
import os
import xml.etree.ElementTree
from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import service_stopper
from autotest_lib.client.cros.camera import camera_utils
from autotest_lib.client.cros.video import device_capability
from sets import Set


class camera_HAL3(test.test):
    """
    This test is a wrapper of the test binary cros_camera_test.
    """

    version = 1
    test_binary = 'cros_camera_test'
    dep = 'camera_hal3'
    cros_camera_service = 'cros-camera'
    media_profiles_path = os.path.join('vendor', 'etc', 'media_profiles.xml')
    tablet_board_list = ['scarlet', 'nocturne']
    test_config_path = '/var/cache/camera/test_config.json'

    def setup(self):
        """
        Run common setup steps.
        """
        self.dep_dir = os.path.join(self.autodir, 'deps', self.dep)
        self.job.setup_dep([self.dep])
        logging.debug('mydep is at %s', self.dep_dir)

    @contextlib.contextmanager
    def set_test_config(self, test_config):
        with open(self.test_config_path, 'w') as fp:
            json.dump(test_config, fp)
        yield
        os.remove(self.test_config_path)

    def get_recording_params(self):
        """
        Get recording parameters from media profiles
        """
        xml_content = utils.system_output([
            'android-sh', '-c',
            'cat "%s"' % utils.sh_escape(self.media_profiles_path)
        ])
        root = xml.etree.ElementTree.fromstring(xml_content)
        recording_params = Set()
        for camcorder_profiles in root.findall('CamcorderProfiles'):
            for encoder_profile in camcorder_profiles.findall('EncoderProfile'):
                video = encoder_profile.find('Video')
                recording_params.add('%s:%s:%s:%s' % (
                    camcorder_profiles.get('cameraId'), video.get('width'),
                    video.get('height'), video.get('frameRate')))
        return '--recording_params=' + ','.join(recording_params)

    def run_once(self,
                 cmd_timeout=600,
                 camera_hals=None,
                 options=None,
                 capability=None,
                 test_config=None):
        """
        Entry point of this test.

        @param cmd_timeout: Seconds. Timeout for running the test command.
        @param camera_hals: The camera HALs to be tested. e.g. ['usb.so']
        @param options: Option strings passed to test command. e.g. ['--v=1']
        @param capability: Capability required for executing this test.
        """
        if options is None:
            options = []

        if test_config is None:
            test_config = {}

        if capability:
            device_capability.DeviceCapability().ensure_capability(capability)

        self.job.install_pkg(self.dep, 'dep', self.dep_dir)

        camera_hal_paths = camera_utils.get_camera_hal_paths_for_test()
        if camera_hals is not None:
            name_map = dict((os.path.basename(path), path)
                            for path in camera_hal_paths)
            camera_hal_paths = []
            for name in camera_hals:
                path = name_map.get(name)
                if path is None:
                    msg = 'HAL %r is not available for test' % name
                    raise error.TestNAError(msg)
                camera_hal_paths.append(path)

        binary_path = os.path.join(self.dep_dir, 'bin', self.test_binary)

        with service_stopper.ServiceStopper([self.cros_camera_service]), \
                self.set_test_config(test_config):
            has_facing_option = False
            cmd = ['sudo', '--user=arc-camera', binary_path]
            for option in options:
                if 'gtest_filter' in option:
                    filters = option.split('=')[1]
                    if 'Camera3DeviceTest' in filters.split('-')[0]:
                        if utils.get_current_board() in self.tablet_board_list:
                            option += (':' if '-' in filters else '-')
                            option += '*SensorOrientationTest/*'
                    if any(name in filters.split('-')[0] for name in
                           ('Camera3ModuleFixture', 'Camera3RecordingFixture')):
                        cmd.append(self.get_recording_params())
                elif 'camera_facing' in option:
                    has_facing_option = True
                cmd.append(option)

            if has_facing_option:
                utils.system(cmd, timeout=cmd_timeout)
            else:
                for camera_hal_path in camera_hal_paths:
                    logging.info('Run test with %r', camera_hal_path)
                    cmd.append('--camera_hal_path=%s' % camera_hal_path)
                    utils.system(cmd, timeout=cmd_timeout)
                    cmd.pop()
