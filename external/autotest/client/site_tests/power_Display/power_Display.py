# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.input_playback import keyboard
from autotest_lib.client.cros.power import power_test

class power_Display(power_test.power_Test):
    """class for power_Display test.
    """
    version = 1

    # TODO(tbroch) find more patterns that typical display vendors use to show
    # average and worstcase display power.
    PAGES = ['checker1', 'black', 'white', 'red', 'green', 'blue']
    def run_once(self, pages=None, secs_per_page=60, brightness=''):
        """run_once method.

        @param pages: list of pages names that must be in
            <testdir>/html/<name>.html
        @param secs_per_page: time in seconds to display page and measure power.
        @param brightness: flag for brightness setting to use for testing.
                           possible value are 'max' (100%) and 'all' (all manual
                           brightness steps in Chrome OS)
        """
        if pages is None:
            pages = self.PAGES

        with chrome.Chrome(init_network_controller=True) as self.cr:
            http_path = os.path.join(self.job.testdir, 'power_Display', 'html')
            self.cr.browser.platform.SetHTTPServerDirectories(http_path)
            tab = self.cr.browser.tabs.New()
            tab.Activate()

            # Just measure power in full-screen.
            fullscreen = tab.EvaluateJavaScript('document.webkitIsFullScreen')
            if not fullscreen:
                with keyboard.Keyboard() as keys:
                    keys.press_key('f4')

            if brightness not in ['', 'all', 'max']:
                raise error.TestFail(
                        'Invalid brightness flag: %s' % (brightness))

            if brightness == 'max':
                self.backlight.set_percent(100)

            brightnesses = []
            if brightness == 'all':
                self.backlight.set_percent(100)
                for step in range(16, 0, -1):
                    nonlinear = step * 6.25
                    linear = self.backlight.nonlinear_to_linear(nonlinear)
                    brightnesses.append((nonlinear, linear))
            else:
                linear = self.backlight.get_percent()
                nonlinear = self.backlight.linear_to_nonlinear(linear)
                brightnesses.append((nonlinear, linear))

            self.start_measurements()

            loop = 0
            for name in pages:
                url = os.path.join(http_path, name + '.html')
                logging.info('Navigating to url: %s', url)
                tab.Navigate(self.cr.browser.platform.http_server.UrlOf(url))
                tab.WaitForDocumentReadyStateToBeComplete()

                for nonlinear, linear in brightnesses:
                    self.backlight.set_percent(linear)
                    tagname = '%s_%s' % (self.tagged_testname, name)
                    if len(brightnesses) > 1:
                        tagname += '_%.2f' % (nonlinear)
                    loop_start = time.time()
                    self.loop_sleep(loop, secs_per_page)
                    self.checkpoint_measurements(tagname, loop_start)
                    loop += 1
