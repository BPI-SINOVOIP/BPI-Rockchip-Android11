# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from autotest_lib.server import autotest, test
from autotest_lib.server.cros.multimedia import remote_facade_factory


class camera_HAL3Server(test.test):
    """
    Server side camera_HAL3 test for configure dummy image on chart tablet and
    run test on DUT.
    """
    version = 1
    DISPLAY_LEVEL = 96.0
    SCENE_NAME = 'scene.pdf'
    BRIGHTNESS_CMD = 'backlight_tool --get_brightness_percent'
    SET_BRIGHTNESS_CMD = 'backlight_tool --set_brightness_percent=%s'

    def setup(self, chart_host):
        # prepare chart device
        self.chart_dir = chart_host.get_tmp_dir()
        logging.debug('chart_dir=%s', self.chart_dir)
        self.display_facade = remote_facade_factory.RemoteFacadeFactory(
                chart_host).create_display_facade()

        # set chart display brightness
        self.init_display_level = chart_host.run(
                self.BRIGHTNESS_CMD).stdout.rstrip()
        chart_host.run(self.SET_BRIGHTNESS_CMD % self.DISPLAY_LEVEL)

        # keep display always on
        chart_host.run('stop powerd', ignore_status=True)

        # scp scene to chart_host
        chart_host.send_file(
                os.path.join(self.bindir, 'files', self.SCENE_NAME),
                self.chart_dir)
        chart_host.run('chmod', args=('-R', '755', self.chart_dir))

        # display scene
        self.display_facade.load_url(
                'file://' + os.path.join(self.chart_dir, self.SCENE_NAME))
        self.display_facade.set_fullscreen(True)

    def run_once(self, host, chart_host, **kwargs):
        autotest.Autotest(host).run_test('camera_HAL3', **kwargs)

    def cleanup(self, chart_host):
        # restore display default behavior
        chart_host.run('start powerd', ignore_status=True)
        chart_host.run(self.SET_BRIGHTNESS_CMD % self.init_display_level)
