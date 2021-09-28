# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.cros import constants
from autotest_lib.server import autotest

POWER_DIR = '/var/lib/power_manager'
TMP_POWER_DIR = '/tmp/power_manager'
POWER_DEFAULTS = '/usr/share/power_manager/board_specific'

RESUME_CTRL_RETRIES = 3
RESUME_GRACE_PERIOD = 10
XMLRPC_BRINGUP_TIMEOUT_SECONDS = 60
DARK_SUSPEND_MAX_DELAY_TIMEOUT_MILLISECONDS = 60000


class DarkResumeUtils(object):
    """Class containing common functionality for tests which exercise dark
    resume pathways. We set up powerd to allow dark resume and also configure
    the suspended devices so that the backchannel can stay up. We can also
    check for the number of dark resumes that have happened in a particular
    suspend request.
    """


    def __init__(self, host, duration=0):
        """Set up powerd preferences so we will properly go into dark resume,
        and still be able to communicate with the DUT.

        @param host: the DUT to set up dark resume for

        """
        self._host = host
        logging.info('Setting up dark resume preferences')

        # Make temporary directory, which will be used to hold
        # temporary preferences. We want to avoid writing into
        # /var/lib so we don't have to save any state.
        logging.debug('Creating temporary powerd prefs at %s', TMP_POWER_DIR)
        host.run('mkdir -p %s' % TMP_POWER_DIR)

        logging.debug('Enabling dark resume')
        host.run('echo 0 > %s/disable_dark_resume' % TMP_POWER_DIR)
        logging.debug('setting max dark suspend delay timeout to %d msecs',
                  DARK_SUSPEND_MAX_DELAY_TIMEOUT_MILLISECONDS)
        host.run('echo %d > %s/max_dark_suspend_delay_timeout_ms' %
                 (DARK_SUSPEND_MAX_DELAY_TIMEOUT_MILLISECONDS, TMP_POWER_DIR))

        # bind the tmp directory to the power preference directory
        host.run('mount --bind %s %s' % (TMP_POWER_DIR, POWER_DIR))

        logging.debug('Restarting powerd with new settings')
        host.run('stop powerd; start powerd')

        logging.debug('Starting XMLRPC session to watch for dark resumes')
        self._client_proxy = self._get_xmlrpc_proxy()


    def teardown(self):
        """Clean up changes made by DarkResumeUtils."""

        logging.info('Tearing down dark resume preferences')

        logging.debug('Cleaning up temporary powerd bind mounts')
        self._host.run('umount %s' % POWER_DIR)

        logging.debug('Restarting powerd to revert to old settings')
        self._host.run('stop powerd; start powerd')


    def suspend(self, suspend_secs):
        """ Suspends the device for |suspend_secs| without blocking for resume.

        @param suspend_secs : Sleep for seconds. Sets a RTC alarm to wake the
                              system.
        """
        logging.info('Suspending DUT (in background)...')
        self._client_proxy.suspend_bg(suspend_secs)


    def stop_resuspend_on_dark_resume(self, stop_resuspend=True):
        """
        If |stop_resuspend| is True, stops re-suspend on seeing a dark resume.
        """
        self._client_proxy.set_stop_resuspend(stop_resuspend)


    def count_dark_resumes(self):
        """Return the number of dark resumes since the beginning of the test.

        This method will raise an error if the DUT is not reachable.

        @return the number of dark resumes counted by this DarkResumeUtils

        """
        return self._client_proxy.get_dark_resume_count()



    def host_has_lid(self):
        """Returns True if the DUT has a lid."""
        return self._client_proxy.has_lid()


    def _get_xmlrpc_proxy(self):
        """Get a dark resume XMLRPC proxy for the host this DarkResumeUtils is
        attached to.

        The returned object has no particular type.  Instead, when you call
        a method on the object, it marshalls the objects passed as arguments
        and uses them to make RPCs on the remote server.  Thus, you should
        read dark_resume_xmlrpc_server.py to find out what methods are supported.

        @return proxy object for remote XMLRPC server.

        """
        # Make sure the client library is on the device so that the proxy
        # code is there when we try to call it.
        client_at = autotest.Autotest(self._host)
        client_at.install()
        # Start up the XMLRPC proxy on the client
        proxy = self._host.rpc_server_tracker.xmlrpc_connect(
                constants.DARK_RESUME_XMLRPC_SERVER_COMMAND,
                constants.DARK_RESUME_XMLRPC_SERVER_PORT,
                command_name=
                    constants.DARK_RESUME_XMLRPC_SERVER_CLEANUP_PATTERN,
                ready_test_name=
                    constants.DARK_RESUME_XMLRPC_SERVER_READY_METHOD,
                timeout_seconds=XMLRPC_BRINGUP_TIMEOUT_SECONDS)
        return proxy
