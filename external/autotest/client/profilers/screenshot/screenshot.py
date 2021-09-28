# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This profiler will take a screenshot at the specified interval.
"""

import os, subprocess
import threading, time
import logging
from autotest_lib.client.bin import profiler


class screenshot(profiler.profiler):
    """ Profiler for running screenshot """

    version = 1

    def initialize(self, interval=300):
        """Initializes the screenshot profiler.

        Args:
            interval (int): How often to take a screenshot in seconds
        """
        self.interval = interval

    def start(self, test):
        self.thread = ScreenshotThread(interval=self.interval, test=test)

        self.thread.start()

    def stop(self, test):
        self.thread.stop()


class ScreenshotThread(threading.Thread):
    """ Thread that runs screenshot at the specified interval """

    def __init__(self, interval, test):
        threading.Thread.__init__(self)
        self.stopped = threading.Event()
        self.interval = interval
        self.test = test

    def run(self):
        logging.info("screenshot thread starting")

        sleep_time = 0

        while not self.stopped.wait(sleep_time):
            start_time = time.time()

            path = os.path.join(self.test.profdir,
                                "screenshot-%d.png" % (int(start_time)))

            # Don't use graphics_utils because we can't control the suffix
            cmd = ['screenshot', path]

            logging.debug("Taking screenshot")

            process = subprocess.Popen(
                    cmd, stderr=subprocess.PIPE, close_fds=True)

            _, stderr = process.communicate()

            if process.returncode:
                # If the screen is turned off, screenshot will fail
                logging.info('screenshot failed. code: %d, error: %s ',
                             process.returncode, stderr)

            end_time = time.time()

            sleep_time = self.interval - (end_time - start_time)

            if sleep_time < 0:
                sleep_time = 0

    def stop(self):
        """ Stops the thread """
        logging.info("Stopping screenshot thread")

        self.stopped.set()

        # Only block for five seconds to not hold up the test shutdown.
        # It's very unlikely that the screenshot command will take more
        # than a second.
        self.join(5)

        logging.info("screenshot thread stopped")
