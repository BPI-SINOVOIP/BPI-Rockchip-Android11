# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import numpy
import time

from autotest_lib.client.bin import fps_meter
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros import touch_playback_test_base
from telemetry.internal.actions import scroll

import py_utils

""" List of URLs that will be used to test users gestures on. """
_LIST_OF_URLS = ["https://www.youtube.com", "https://www.cnn.com",
    "https://slashdot.org/"]

""" Scroll bar's moving speed. """
_SCROLL_SPEED = 1500

""" The total distance that the scroll bar moved. """
_SCROLL_DISTANCE = 3000

""" Separator used in fps_meter for each VSync. """
_SEPARATOR = " "

class platform_ScrollTest(touch_playback_test_base.touch_playback_test_base):
    """Scroll up and down pressure test."""
    version = 1

    def run_once(self):
        """Runs the test once."""
        perf_results = {}

        def record_fps_info(fps_data, fps_info):
            ''' record the fps info from |fps_meter| '''
            frame_info, frame_times = fps_info
            frame_info_str = ''.join(frame_info)
            fps_count = sum(
                map(int, frame_info_str.replace(_SEPARATOR, "")))
            fps_data.append(fps_count)

        fps_data = []
        fps = fps_meter.FPSMeter(functools.partial(record_fps_info, fps_data))
        with chrome.Chrome(init_network_controller=True) as cr:
            for url in _LIST_OF_URLS:
                tab = cr.browser.tabs.New()
                tab.Navigate(url)
                try:
                    tab.WaitForDocumentReadyStateToBeComplete(timeout=15)
                except py_utils.TimeoutException:
                    logging.warning('Time out during loading url ' + url)

                for x in range(0, 3):
                    page_scroll = scroll.ScrollAction(
                        speed_in_pixels_per_second=_SCROLL_SPEED,
                        distance=_SCROLL_DISTANCE)
                    cr.browser.platform.SetHTTPServerDirectories(self.bindir)
                    page_scroll.WillRunAction(tab)
                    fps.start()
                    page_scroll.RunAction(tab)
                    fps.stop()
                    page_scroll = scroll.ScrollAction(
                        direction="up",
                        speed_in_pixels_per_second=_SCROLL_SPEED,
                        distance=_SCROLL_DISTANCE)
                    page_scroll.WillRunAction(tab)
                    fps.start()
                    page_scroll.RunAction(tab)
                    fps.stop()
                time.sleep(1)
        value = getattr(numpy, "mean")(fps_data)

        self.output_perf_value(description="fps average",
                                value=value,
                               units='frame per second',
                               higher_is_better=True)
