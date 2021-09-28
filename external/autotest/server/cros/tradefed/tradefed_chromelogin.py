# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server import autotest
from autotest_lib.server.cros.tradefed import tradefed_constants as constants


class ChromeLogin(object):
    """Context manager to handle Chrome login state."""

    def need_reboot(self, hard_reboot=False):
        """Marks state as "dirty" - reboot needed during/after test."""
        logging.info('Will reboot DUT when Chrome stops.')
        self._need_reboot = True
        if hard_reboot and self._host.servo:
            self._hard_reboot_on_failure = True

    def __init__(self, host, board=None, dont_override_profile=False,
                 enable_default_apps=False):
        """Initializes the ChromeLogin object.

        @param board: optional parameter to extend timeout for login for slow
                      DUTs. Used in particular for virtual machines.
        @param dont_override_profile: reuses the existing test profile if any
        @param enable_default_apps: enables default apps (like Files app)
        """
        self._host = host
        self._timeout = constants.LOGIN_BOARD_TIMEOUT.get(
            board, constants.LOGIN_DEFAULT_TIMEOUT)
        self._dont_override_profile = dont_override_profile
        self._enable_default_apps = enable_default_apps
        self._need_reboot = False
        self._hard_reboot_on_failure = False

    def _cmd_builder(self, verbose=False):
        """Gets remote command to start browser with ARC enabled."""
        # If autotest is not installed on the host, as with moblab at times,
        # getting the autodir will raise an exception.
        cmd = autotest.Autotest.get_installed_autodir(self._host)
        cmd += '/bin/autologin.py --arc'
        # We want to suppress the Google doodle as it is not part of the image
        # and can be different content every day interacting with testing.
        cmd += ' --no-startup-window'
        # Disable several forms of auto-installation to stablize the tests.
        cmd += ' --no-arc-syncs'
        if self._dont_override_profile:
            logging.info('Starting Chrome with a possibly reused profile.')
            cmd += ' --dont_override_profile'
        else:
            logging.info('Starting Chrome with a fresh profile.')
        if self._enable_default_apps:
            logging.info('Using --enable_default_apps to start Chrome.')
            cmd += ' --enable_default_apps'
        if not verbose:
            cmd += ' > /dev/null 2>&1'
        return cmd

    def _login_by_script(self, timeout, verbose):
        """Runs the autologin.py script on the DUT to log in."""
        self._host.run(
            self._cmd_builder(verbose=verbose),
            ignore_status=False,
            verbose=verbose,
            timeout=timeout)

    def _login(self, timeout, verbose=False, install_autotest=False):
        """Logs into Chrome. Raises an exception on failure."""
        if not install_autotest:
            try:
                # Assume autotest to be already installed.
                self._login_by_script(timeout=timeout, verbose=verbose)
            except autotest.AutodirNotFoundError:
                logging.warning('Autotest not installed, forcing install...')
                install_autotest = True

        if install_autotest:
            # Installs the autotest client to the DUT by running a dummy test.
            autotest.Autotest(self._host).run_timed_test(
                'dummy_Pass', timeout=2 * timeout, check_client_result=True)
            # The (re)run the login script.
            self._login_by_script(timeout=timeout, verbose=verbose)

        # Sanity check if Android has really started. When autotest client
        # installed on the DUT was partially broken, the script may succeed
        # without actually logging into Chrome/Android. See b/129382439.
        self._host.run(
            'android-sh -c "ls /data/misc/adb"', ignore_status=False, timeout=9)

    def enter(self):
        """Logs into Chrome with retry."""
        timeout = self._timeout
        try:
            logging.info('Ensure Android is running (timeout=%d)...', timeout)
            self._login(timeout=timeout)
        except Exception as e:
            logging.error('Login failed.', exc_info=e)
            # Retry with more time, with refreshed client autotest installation,
            # and the DUT cleanup by rebooting. This can hide some failures.
            self._reboot()
            timeout *= 2
            logging.info('Retrying failed login (timeout=%d)...', timeout)
            try:
                self._login(timeout=timeout,
                            verbose=True,
                            install_autotest=True)
            except Exception as e2:
                logging.error('Failed to login to Chrome', exc_info=e2)
                raise error.TestError('Failed to login to Chrome')

    def exit(self):
        """On exit restart the browser or reboot the machine."""
        if not self._need_reboot:
            try:
                self._restart()
            except:
                logging.error('Restarting browser has failed.')
                self.need_reboot()
        if self._need_reboot:
            self._reboot()

    def _restart(self):
        """Restart Chrome browser."""
        # We clean up /tmp (which is memory backed) from crashes and
        # other files. A reboot would have cleaned /tmp as well.
        # TODO(ihf): Remove "start ui" which is a nicety to non-ARC tests (i.e.
        # now we wait on login screen, but login() above will 'stop ui' again
        # before launching Chrome with ARC enabled).
        logging.info('Skipping reboot, restarting browser.')
        script = 'stop ui'
        script += '&& find /tmp/ -mindepth 1 -delete '
        script += '&& start ui'
        self._host.run(script, ignore_status=False, verbose=False, timeout=120)

    def _reboot(self):
        """Reboot the machine."""
        if self._hard_reboot_on_failure:
            logging.info('Powering OFF the DUT: %s', self._host)
            self._host.servo.get_power_state_controller().power_off()
            logging.info('Powering ON the DUT: %s', self._host)
            self._host.servo.get_power_state_controller().power_on()
        else:
            logging.info('Rebooting...')
            self._host.reboot()


@contextlib.contextmanager
def login_chrome(hosts, **kwargs):
    """Returns Chrome log-in context manager for multiple hosts. """
    # TODO(pwang): Chromelogin takes 10+ seconds for it to successfully
    #              enter. Parallelize if this becomes a bottleneck.
    instances = [ChromeLogin(host, **kwargs) for host in hosts]
    for instance in instances:
        instance.enter()
    yield instances
    for instance in instances:
        instance.exit()
