"""
turbostat prints out CPU stats
"""

import os, subprocess, signal
import logging
from autotest_lib.client.bin import profiler, os_dep


class turbostat(profiler.profiler):
    """
    turbostat reports processor topology, frequency, idle power-state
    statistics, etc.
    """
    version = 1

    def initialize(self):
        self.bin = os_dep.command('turbostat')

    def start(self, test):
        self._output = open(os.path.join(test.profdir, "turbostat"), "wt")

        cmd = [self.bin]
        logging.debug("Starting turbostat: %s", cmd)

        # Log the start time so a complete datetime can be computed later
        subprocess.call(['date', '-Iseconds'], stdout=self._output)

        self._process = subprocess.Popen(
                cmd,
                stdout=self._output,
                stderr=subprocess.STDOUT,
                close_fds=True)

    def stop(self, test):
        logging.debug("Stopping turbostat")

        os.kill(self._process.pid, signal.SIGTERM)

        self._process.wait()

        logging.debug("Stopped turbostat")

        self._output.close()

    def report(self, test):
        pass
