# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import time

from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.common_lib.cros import power_load_util
from autotest_lib.client.cros.input_playback import keyboard
from autotest_lib.client.cros.power import power_dashboard
from autotest_lib.client.cros.power import power_status
from autotest_lib.client.cros.power import power_test


class power_VideoCall(power_test.power_Test):
    """class for power_VideoCall test."""
    version = 1

    video_url = 'http://crospower.page.link/power_VideoCall'
    doc_url = 'http://doc.new'
    extra_browser_args = ['--use-fake-ui-for-media-stream']

    def initialize(self, seconds_period=20., pdash_note='',
                   force_discharge=False):
        """initialize method."""
        super(power_VideoCall, self).initialize(seconds_period=seconds_period,
                                                pdash_note=pdash_note,
                                                force_discharge=force_discharge)
        self._username = power_load_util.get_username()
        self._password = power_load_util.get_password()

    def run_once(self, duration=7200):
        """run_once method.

        @param duration: time in seconds to display url and measure power.
        """
        with keyboard.Keyboard() as keys,\
             chrome.Chrome(init_network_controller=True,
                           gaia_login=True,
                           username=self._username,
                           password=self._password,
                           extra_browser_args=self.extra_browser_args,
                           autotest_ext=True) as cr:

            # Move existing window to left half and open video page
            tab_left = cr.browser.tabs[0]
            tab_left.Activate()
            keys.press_key('alt+[')
            logging.info('Navigating left window to %s', self.video_url)
            tab_left.Navigate(self.video_url)
            tab_left.WaitForDocumentReadyStateToBeComplete()
            time.sleep(5)

            # Open Google Doc on right half
            logging.info('Navigating right window to %s', self.doc_url)
            cmd = 'chrome.windows.create({ url : "%s" });' % self.doc_url
            cr.autotest_ext.EvaluateJavaScript(cmd)
            tab_right = cr.browser.tabs[-1]
            tab_right.Activate()
            keys.press_key('alt+]')
            tab_right.WaitForDocumentReadyStateToBeComplete()
            time.sleep(5)

            self._vlog = power_status.VideoFpsLogger(tab_left,
                seconds_period=self._seconds_period,
                checkpoint_logger=self._checkpoint_logger)
            self._meas_logs.append(self._vlog)

            # Start typing number block
            self.start_measurements()
            while time.time() - self._start_time < duration:
                keys.press_key('number_block')
                self.status.refresh()
                if self.status.is_low_battery():
                    logging.info(
                        'Low battery, stop test early after %.0f minutes',
                        (time.time() - self._start_time) / 60)
                    return

    def publish_dashboard(self):
        """Report results power dashboard."""
        super(power_VideoCall, self).publish_dashboard()

        vdash = power_dashboard.VideoFpsLoggerDashboard(
            self._vlog, self.tagged_testname, self.resultsdir,
            note=self._pdash_note)
        vdash.upload()
