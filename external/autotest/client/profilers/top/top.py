"""top prints out CPU stats"""

import os, subprocess, signal
import logging
from autotest_lib.client.bin import profiler


class top(profiler.profiler):
    """
    Starts top on the DUT and polls every 5 seconds. Any processes with a
    %cpu of zero will be stripped from the output.
    """

    version = 1

    SCRIPT = "top -b -c -w 200 -d 5 -o '%CPU' -H | " \
             "awk '$1 ~ /[0-9]+/ && $9 == '0.0' {next} {print}'"

    def start(self, test):
        self._output = open(os.path.join(test.profdir, "top"), "wb")

        logging.debug("Starting top")

        # Log the start time so a complete datetime can be computed later
        subprocess.call(["date", "-Iseconds"], stdout=self._output)

        self._process = subprocess.Popen(
                self.SCRIPT,
                stderr=self._output,
                stdout=self._output,
                shell=True,
                # We need to start a process group so we can kill the script's
                # children.
                preexec_fn=os.setpgrp,
                close_fds=True)

    def stop(self, test):
        logging.debug("Stopping top")

        # Kill the whole process group so top and awk die
        os.killpg(self._process.pid, signal.SIGTERM)

        self._process.wait()

        logging.debug("Stopped top")

        self._output.close()

    def report(self, test):
        pass
