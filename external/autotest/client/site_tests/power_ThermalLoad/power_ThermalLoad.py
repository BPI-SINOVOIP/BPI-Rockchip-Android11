# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import time

from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.input_playback import keyboard
from autotest_lib.client.cros.power import power_dashboard
from autotest_lib.client.cros.power import power_status
from autotest_lib.client.cros.power import power_test


class power_ThermalLoad(power_test.power_Test):
    """class for power_ThermalLoad test.
    """
    version = 2

    FISHTANK_URL = 'http://storage.googleapis.com/chrome-power/aquarium/aquarium/aquarium.html'
    HOUR = 60 * 60

    def run_once(self, test_url=FISHTANK_URL, duration=2.5*HOUR, numFish=3000):
        """run_once method.

        @param test_url: url of webgl heavy page.
        @param duration: time in seconds to display url and measure power.
        @param numFish: number of fish to pass to WebGL Aquarium.
        """
        with chrome.Chrome(init_network_controller=True) as self.cr:
            tab = self.cr.browser.tabs.New()
            tab.Activate()

            # Just measure power in full-screen.
            fullscreen = tab.EvaluateJavaScript('document.webkitIsFullScreen')
            if not fullscreen:
                with keyboard.Keyboard() as keys:
                    keys.press_key('f4')

            self.backlight.set_percent(100)

            url = test_url + "?numFish=" + str(numFish)
            logging.info('Navigating to url: %s', url)
            tab.Navigate(url)
            tab.WaitForDocumentReadyStateToBeComplete()

            self._flog = FishTankFpsLogger(tab,
                    seconds_period=self._seconds_period,
                    checkpoint_logger=self._checkpoint_logger)
            self._meas_logs.append(self._flog)

            self.start_measurements()
            while time.time() - self._start_time < duration:
                time.sleep(60)
                self.status.refresh()
                if self.status.is_low_battery():
                    logging.info(
                        'Low battery, stop test early after %.0f minutes',
                        (time.time() - self._start_time) / 60)
                    return

    def publish_dashboard(self):
        """Report results power dashboard."""
        super(power_ThermalLoad, self).publish_dashboard()

        vdash = power_dashboard.VideoFpsLoggerDashboard(
            self._flog, self.tagged_testname, self.resultsdir,
            note=self._pdash_note)
        vdash.upload()


class FishTankFpsLogger(power_status.MeasurementLogger):
    """Class to measure Video WebGL Aquarium fps & fish per sec."""

    def __init__(self, tab, seconds_period=20.0, checkpoint_logger=None):
        """Initialize a FishTankFpsLogger.

        Args:
            tab: Chrome tab object
        """
        super(FishTankFpsLogger, self).__init__([], seconds_period,
                                                    checkpoint_logger)
        self._tab = tab
        self._lastFrameCount = 0
        fishCount = self._tab.EvaluateJavaScript('fishCount')
        self.domains = ['avg_fps_%04d_fishes' % fishCount]
        self.refresh()

    def refresh(self):
        frameCount = self._tab.EvaluateJavaScript('frameCount')
        fps = (frameCount - self._lastFrameCount) / self.seconds_period
        self._lastFrameCount = frameCount
        return [fps]

    def save_results(self, resultsdir, fname_prefix=None):
        if not fname_prefix:
            fname_prefix = '%s_results_%.0f' % (self.domains[0], time.time())
        super(FishTankFpsLogger, self).save_results(resultsdir, fname_prefix)

    def calc(self, mtype='fps'):
        return super(FishTankFpsLogger, self).calc(mtype)
