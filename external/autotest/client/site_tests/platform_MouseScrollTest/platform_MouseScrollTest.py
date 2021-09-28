# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import numpy
import os
import time

from autotest_lib.client.bin import fps_meter
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros import touch_playback_test_base

import py_utils

""" The tracing file that contains the desired mouse scrolling events. """
_PLAYBACK_FILE = 'mouse_event'

""" Description of the fake mouse we add to the system. """
_MOUSE_DESCRIPTION = 'mouse.prop'

""" List of URLs that will be used to test users gestures on. """
_LIST_OF_URLS = ['https://www.youtube.com', 'https://www.cnn.com',
    'https://slashdot.org/']

""" Separator used in fps_meter for each VSync """
_SEPARATOR = ' '

class platform_MouseScrollTest(
    touch_playback_test_base.touch_playback_test_base):
    """Fast scroll up and down with mouse pressure test."""
    version = 1

    def _play_events(self, event_filename):
        """
        Simulate mouse events for user scrolling.

        @param event_filename string string file name containing the events
        to play pack.
        """
        file_path = os.path.join(self.bindir, event_filename)
        self._blocking_playback(str(file_path), touch_type='mouse')

    def run_once(self):
        """ Runs the test once. """
        mouse_file = os.path.join(self.bindir, _MOUSE_DESCRIPTION)
        self._emulate_mouse(property_file=mouse_file)

        def record_fps_info(fps_data, fps_info):
            """ record the fps info from |fps_meter| """
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

                tab.Activate()
                cr.browser.platform.SetHTTPServerDirectories(self.bindir)
                fps.start()
                self._play_events(_PLAYBACK_FILE)
                fps.stop()
                time.sleep(1)

            value = getattr(numpy, 'mean')(fps_data)

            self.output_perf_value(description='fps average',
                                   value=value,
                                   units='frames per second',
                                   higher_is_better=True)
