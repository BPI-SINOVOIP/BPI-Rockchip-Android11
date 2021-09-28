# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import logging
from lxml import etree
import os
import StringIO

from autotest_lib.client.common_lib import utils


class ChartFixture:
    """Sets up chart tablet to display dummy scene image."""
    DISPLAY_SCRIPT = '/usr/local/autotest/bin/display_chart.py'

    def __init__(self, chart_host, scene_uri):
        self.host = chart_host
        self.scene_uri = scene_uri
        self.display_pid = None

    def initialize(self):
        """Prepare scene file and display it on chart host."""
        logging.info('Prepare scene file')
        chart_dir = self.host.get_tmp_dir()
        scene_path = os.path.join(
                chart_dir, self.scene_uri[self.scene_uri.rfind('/') + 1:])
        self.host.run('wget', args=('-O', scene_path, self.scene_uri))
        self.host.run('chmod', args=('-R', '755', chart_dir))

        logging.info('Display scene file')
        self.display_pid = self.host.run_background(
                'python %s %s' % (self.DISPLAY_SCRIPT, scene_path))
        # TODO(inker): Suppose chart should be displayed very soon. Or require
        # of waiting until chart actually displayed.

    def cleanup(self):
        """Cleanup display script."""
        if self.display_pid is not None:
            self.host.run('kill', args=('-2', str(self.display_pid)))


def get_chart_address(host_address, args):
    """Get address of chart tablet from commandline args or mapping logic in
    test lab.

    @param host_address: a list of hostname strings.
    @param args: a dict parse from commandline args.
    @return:
        A list of strings for chart tablet addresses.
    """
    address = utils.args_to_dict(args).get('chart')
    if address is not None:
        return address.split(',')
    elif utils.is_in_container():
        return [
                utils.get_lab_chart_address(host)
                for host in host_address
        ]
    else:
        return None


class DUTFixture:
    """Sets up camera filter for target camera facing on DUT."""
    TEST_CONFIG_PATH = '/var/cache/camera/test_config.json'
    CAMERA_PROFILE_PATH = ('/mnt/stateful_partition/encrypted/var/cache/camera'
                           '/media_profiles.xml')

    def __init__(self, test, host, facing):
        self.test = test
        self.host = host
        self.facing = facing

    @contextlib.contextmanager
    def _set_selinux_permissive(self):
        selinux_mode = self.host.run_output('getenforce')
        self.host.run('setenforce 0')
        yield
        self.host.run('setenforce', args=(selinux_mode, ))

    def _filter_camera_profile(self, content, facing):
        """Filter camera profile of target facing from content of camera
        profile.

        @return:
            New camera profile with only target facing, camera ids are
            renumbered from 0.
        """
        tree = etree.parse(
                StringIO.StringIO(content),
                parser=etree.XMLParser(compact=False))
        root = tree.getroot()
        profiles = root.findall('CamcorderProfiles')
        logging.debug('%d number of camera(s) found in camera profile',
                      len(profiles))
        assert 1 <= len(profiles) <= 2
        if len(profiles) == 2:
            cam_id = 0 if facing == 'back' else 1
            for p in profiles:
                if cam_id == int(p.attrib['cameraId']):
                    p.attrib['cameraId'] = '0'
                else:
                    root.remove(p)
        else:
            with self.test._login_chrome(
                    board=self.test._get_board_name(),
                    reboot=False), self._set_selinux_permissive():
                has_front_camera = (
                        'feature:android.hardware.camera.front' in self.host.
                        run_output('android-sh -c "pm list features"'))
                logging.debug('has_front_camera=%s', has_front_camera)
            if (facing == 'front') != has_front_camera:
                root.remove(profiles[0])
        return etree.tostring(
                tree, xml_declaration=True, encoding=tree.docinfo.encoding)

    def _read_file(self, filepath):
        """Read content of filepath from host."""
        tmp_path = os.path.join(self.test.tmpdir, os.path.basename(filepath))
        self.host.get_file(filepath, tmp_path, delete_dest=True)
        with open(tmp_path) as f:
            return f.read()

    def _write_file(self, filepath, content, permission=None, owner=None):
        """Write content to filepath on remote host.
        @param permission: set permission to 0xxx octal number of remote file.
        @param owner: set owner of remote file.
        """
        tmp_path = os.path.join(self.test.tmpdir, os.path.basename(filepath))
        with open(tmp_path, 'w') as f:
            f.write(content)
        if permission is not None:
            os.chmod(tmp_path, permission)
        self.host.send_file(tmp_path, filepath, delete_dest=True)
        if owner is not None:
            self.host.run('chown', args=(owner, filepath))

    def initialize(self):
        """Filter out camera other than target facing on DUT."""
        logging.info('Restart camera service with filter option')
        self._write_file(
                self.TEST_CONFIG_PATH,
                json.dumps({
                        'enable_back_camera': self.facing == 'back',
                        'enable_front_camera': self.facing == 'front',
                        'enable_external_camera': False
                }),
                owner='arc-camera')
        self.host.run('restart cros-camera')

        logging.info('Replace camera profile in ARC++ container')
        profile = self._read_file(self.CAMERA_PROFILE_PATH)
        new_profile = self._filter_camera_profile(profile, self.facing)
        self._write_file(self.CAMERA_PROFILE_PATH, new_profile)
        self.host.run('restart ui')

    def cleanup(self):
        """Cleanup camera filter."""
        logging.info('Remove filter option and restore camera service')
        self.host.run('rm', args=('-f', self.TEST_CONFIG_PATH))
        self.host.run('restart cros-camera')

        logging.info('Restore camera profile in ARC++ container')
        self.host.run('restart ui')
