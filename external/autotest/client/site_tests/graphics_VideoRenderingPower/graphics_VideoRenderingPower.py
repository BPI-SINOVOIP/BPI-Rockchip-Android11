# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import service_stopper
from autotest_lib.client.cros.graphics import graphics_utils
from autotest_lib.client.cros.power import power_rapl
from autotest_lib.client.cros.power import power_status
from autotest_lib.client.cros.power import power_utils

TEST_NAME_AND_FLAGS = [
    ['hw_overlays_hw_decode', ['']],
    ['no_overlays_hw_decode', ['--enable-hardware-overlays=']],
    ['hw_overlays_sw_decode', ['--disable-accelerated-video-decode']],
    [
        'no_overlays_sw_decode',
        ['--disable-accelerated-video-decode', '--enable-hardware-overlays=']
    ]
]
# Amount of time to wait for the URL to load and the video to start playing.
PREAMBLE_DURATION_SECONDS = 8
# Amount of time to let the video play while measuring power consumption.
MEASUREMENT_DURATION_SECONDS = 12

# Time in seconds to wait for cpu idle until giveup.
IDLE_CPU_WAIT_TIMEOUT_SECONDS = 60.0
# Maximum percent of cpu usage considered as idle.
IDLE_CPU_LOAD_PERCENTAGE = 0.2

GRAPH_NAME = 'power_consumption'


class graphics_VideoRenderingPower(graphics_utils.GraphicsTest):
    """This test renders on screen for a short while a video from a given

    (controlled) URL while measuring the power consumption of the different SoC
    domains.
    """
    version = 1
    _backlight = None
    _service_stopper = None
    _power_status = None

    def initialize(self):
        super(graphics_VideoRenderingPower, self).initialize()

        self._backlight = power_utils.Backlight()
        self._backlight.set_default()

        self._service_stopper = service_stopper.ServiceStopper(
            service_stopper.ServiceStopper.POWER_DRAW_SERVICES)
        self._service_stopper.stop_services()

        self._power_status = power_status.get_status()

    def cleanup(self):
        if self._backlight:
            self._backlight.restore()
        if self._service_stopper:
            self._service_stopper.restore_services()
        super(graphics_VideoRenderingPower, self).cleanup()

    @graphics_utils.GraphicsTest.failure_report_decorator(
        'graphics_VideoRenderingPower')
    def run_once(self, video_url, video_short_name):
        """Runs the graphics_VideoRenderingPower test.

        @param video_url: URL with autoplay video inside. It's assumed that
                 there's just one <video> in the HTML, and that it fits in the
                 viewport.
        @param video_short_name: short string describing the video; itt will be
                 presented as part of the dashboard entry name.
        """

        # TODO(mcasas): Extend this test to non-Intel platforms.
        if not power_utils.has_rapl_support():
            logging.warning('This board has no RAPL power measurement support, '
                            'skipping test.')
            return

        rapl = []
        if power_utils.has_battery():
            # Sometimes, the DUT is supposed to have a battery but we may not
            # detect one. This is a symptom of a bad battery (b/145144707).
            if self._power_status.battery_path is None:
                raise error.TestFail('No battery found in this DUT (this is a '
                                     'symptom of a bad battery).')
            rapl.append(
                power_status.SystemPower(self._power_status.battery_path))
        else:
            logging.warning('This board has no battery.')
        rapl += power_rapl.create_rapl()

        for test_name_and_flags in TEST_NAME_AND_FLAGS:
            logging.info('Test case: %s', test_name_and_flags[0])
            # Launch Chrome with the appropriate flag combination.
            with chrome.Chrome(
                    extra_browser_args=test_name_and_flags[1],
                    init_network_controller=True) as cr:

                if not utils.wait_for_idle_cpu(IDLE_CPU_WAIT_TIMEOUT_SECONDS,
                                               IDLE_CPU_LOAD_PERCENTAGE):
                    raise error.TestFail('Failed: Could not get idle CPU.')
                if not utils.wait_for_cool_machine():
                    raise error.TestFail('Failed: Could not get cold machine.')

                tab = cr.browser.tabs[0]
                tab.Navigate(video_url)
                tab.WaitForDocumentReadyStateToBeComplete()
                tab.EvaluateJavaScript(
                    'document.'
                    'getElementsByTagName(\'video\')[0].scrollIntoView(true)')

                # Disabling hardware overlays is difficult because the flag is
                # already in the browser. Instead, scroll a bit down to make the
                # video bleed out of the viewport.
                if '--enable-hardware-overlays=' in test_name_and_flags[1]:
                    tab.EvaluateJavaScript('window.scrollBy(0, 1)')

                power_logger = power_status.PowerLogger(rapl)
                power_logger.start()
                time.sleep(PREAMBLE_DURATION_SECONDS)

                start_time = time.time()
                time.sleep(MEASUREMENT_DURATION_SECONDS)
                power_logger.checkpoint('result', start_time)

                measurements = power_logger.calc()
                logging.debug(measurements)

                for category in sorted(measurements):
                    if category.endswith('_pwr_avg'):
                        description = '%s_%s_%s' % (
                            video_short_name, test_name_and_flags[0], category)
                        self.output_perf_value(
                            description=description,
                            value=measurements[category],
                            units='W',
                            higher_is_better=False,
                            graph=GRAPH_NAME)

                    if category.endswith('_pwr_avg'):
                        # write_perf_keyval() wants units (W) first in lowercase.
                        description = '%s_%s_%s' % (
                            video_short_name, test_name_and_flags[0], category)
                        self.write_perf_keyval({
                            'w_' + description: measurements[category]
                        })
