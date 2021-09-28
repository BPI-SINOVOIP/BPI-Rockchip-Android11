# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This is a profiler class for the perf profiler in ChromeOS. It differs from
the existing perf profiler in autotset by directly substituting the options
passed to the initialize function into the "perf" command line. It allows one
to specify which perf command to run and thus what type of profile to collect
(e.g. "perf record" or "perf stat"). It also does not produce a perf report
on the client (where there are no debug symbols) but instead copies
the perf.data file back to the server for analysis.
"""

import os, signal, subprocess
import threading, time
import shutil
import logging
from autotest_lib.client.bin import profiler
from autotest_lib.client.common_lib import error

class cros_perf(profiler.profiler):
    """ Profiler for running perf """

    version = 1

    def initialize(self, interval=120, duration=10, profile_type='record'):
        """Initializes the cros perf profiler.

        Args:
            interval (int): How often to start the profiler
            duration (int): How long the profiler will run per interval. Set to
                None to run for the full duration of the test.
            profile_type (str): Profile type to run.
                Valid options: record, and stat.
        """
        self.interval = interval
        self.duration = duration
        self.profile_type = profile_type

        if self.profile_type not in ['record', 'stat']:
            raise error.AutotestError(
                    'Unknown profile type: %s' % (profile_type))

    def start(self, test):

        # KASLR makes looking up the symbols from the binary impossible, save
        # the running symbols.
        shutil.copy('/proc/kallsyms', test.profdir)

        self.thread = MonitorThread(
                interval=self.interval,
                duration=self.duration,
                profile_type=self.profile_type,
                test=test)

        self.thread.start()

    def stop(self, test):
        self.thread.stop()


class MonitorThread(threading.Thread):
    """ Thread that runs perf at the specified interval and duration """

    def __init__(self, interval, duration, profile_type, test):
        threading.Thread.__init__(self)
        self.stopped = threading.Event()
        self.interval = interval
        self.duration = duration
        self.profile_type = profile_type
        self.test = test

    def _get_command(self, timestamp):
        file_name = 'perf-%s.data' % (int(timestamp))

        path = os.path.join(self.test.profdir, file_name)

        if self.profile_type == 'record':
            return ['perf', 'record', '-e', 'cycles', '-g', '--output', path]
        elif self.profile_type == 'stat':
            return ['perf', 'stat', 'record', '-a', '--output', path]
        else:
            raise error.AutotestError(
                    'Unknown profile type: %s' % (self.profile_type))

    def run(self):
        logging.info("perf thread starting")

        sleep_time = 0

        while not self.stopped.wait(sleep_time):
            start_time = time.time()

            cmd = self._get_command(start_time)

            logging.info("Running perf: %s", cmd)
            process = subprocess.Popen(cmd, close_fds=True)

            logging.info("Sleeping for %ss", self.duration)
            self.stopped.wait(self.duration)

            ret_code = process.poll()

            if ret_code is None:
                logging.info("Stopping perf")
                process.send_signal(signal.SIGINT)
                process.wait()
            else:
                logging.info(
                        'perf terminated early with return code: %d. '
                        'Please check your logs.', ret_code)

            end_time = time.time()

            sleep_time = self.interval - (end_time - start_time)

            if sleep_time < 0:
                sleep_time = 0

    def stop(self):
        """ Stops the thread """
        logging.info("Stopping perf thread")

        self.stopped.set()

        self.join()

        logging.info("perf thread stopped")
