# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file contains things that are shared by arc.py and arc_util.py.

import logging
import subprocess
import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error


# Ask Chrome to start ARC instance and the script will block until ARC's boot
# completed event.
ARC_MODE_ENABLED = "enabled"
# Similar to "enabled", except that it will not block.
ARC_MODE_ENABLED_ASYNC = "enabled_async"
# Ask Chrome to not start ARC instance.  This is the default.
ARC_MODE_DISABLED = "disabled"
# All available ARC options.
ARC_MODES = [ARC_MODE_ENABLED, ARC_MODE_ENABLED_ASYNC, ARC_MODE_DISABLED]

_BOOT_CHECK_INTERVAL_SECONDS = 2
_WAIT_FOR_ANDROID_BOOT_SECONDS = 120

_VAR_LOGCAT_PATH = '/var/log/logcat'
_VAR_LOGCAT_BOOT_PATH = '/var/log/logcat-boot'


class Logcat(object):
    """Saves the output of logcat to a file."""

    def __init__(self, path=_VAR_LOGCAT_PATH):
        with open(path, 'w') as f:
            self._proc = subprocess.Popen(
                ['android-sh', '-c', 'logcat'],
                stdout=f,
                stderr=subprocess.STDOUT,
                close_fds=True)

    def __enter__(self):
        """Support for context manager."""
        return self

    def __exit__(self, *args):
        """Support for context manager.

        Calls close().
        """
        self.close()

    def close(self):
        """Stop the logcat process gracefully."""
        if not self._proc:
            return
        self._proc.terminate()

        class TimeoutException(Exception):
            """Termination timeout timed out."""

        try:
            utils.poll_for_condition(
                condition=lambda: self._proc.poll() is not None,
                exception=TimeoutException,
                timeout=10,
                sleep_interval=0.1,
                desc='Waiting for logcat to terminate')
        except TimeoutException:
            logging.info('Killing logcat due to timeout')
            self._proc.kill()
            self._proc.wait()
        finally:
            self._proc = None


def wait_for_android_boot(timeout=None):
    """Sleep until Android has completed booting or timeout occurs."""
    if timeout is None:
        timeout = _WAIT_FOR_ANDROID_BOOT_SECONDS

    def _is_container_started():
        return utils.system('android-sh -c true', ignore_status=True) == 0

    def _is_android_booted():
        output = utils.system_output(
            'android-sh -c "getprop sys.boot_completed"', ignore_status=True)
        return output.strip() == '1'

    logging.info('Waiting for Android to boot completely.')

    start_time = time.time()
    utils.poll_for_condition(condition=_is_container_started,
                             desc='Container has started',
                             timeout=timeout,
                             exception=error.TestFail('Android did not boot!'),
                             sleep_interval=_BOOT_CHECK_INTERVAL_SECONDS)
    with Logcat(_VAR_LOGCAT_BOOT_PATH):
        boot_timeout = timeout - (time.time() - start_time)
        utils.poll_for_condition(
            condition=_is_android_booted,
            desc='Android has booted',
            timeout=boot_timeout,
            exception=error.TestFail('Android did not boot!'),
            sleep_interval=_BOOT_CHECK_INTERVAL_SECONDS)
    logging.info('Android has booted completely.')
