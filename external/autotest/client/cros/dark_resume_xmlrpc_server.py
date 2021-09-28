#!/usr/bin/python2

# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import logging.handlers

import common
from autotest_lib.client.cros import constants
from autotest_lib.client.cros import dark_resume_listener
from autotest_lib.client.cros import xmlrpc_server
from autotest_lib.client.cros.power import sys_power
from autotest_lib.client.cros.power import power_utils


class DarkResumeXmlRpcDelegate(xmlrpc_server.XmlRpcDelegate):
    """Exposes methods called remotely during dark resume autotests.

    All instance methods of this object without a preceding '_' are exposed via
    an XMLRPC server.  This is not a stateless handler object, which means that
    if you store state inside the delegate, that state will remain around for
    future calls.

    """

    def __init__(self):
        super(DarkResumeXmlRpcDelegate, self).__init__()
        self._listener = dark_resume_listener.DarkResumeListener()

    @xmlrpc_server.dbus_safe(None)
    def suspend_bg(self, suspend_for_secs):
        """
        Suspends the system for dark resume.
        @param suspend_for_secs : Sets a rtc alarm to
            wake the system after|suspend_for_secs| secs.
        """
        sys_power.suspend_bg_for_dark_resume(suspend_for_secs)

    @xmlrpc_server.dbus_safe(None)
    def set_stop_resuspend(self, stop_resuspend):
        """
        Stops resuspend on seeing a dark resume.
        @param stop_resuspend: Stops resuspend of the device on seeing a
            a dark resume if |stop_resuspend| is true.
        """
        self._listener.stop_resuspend(stop_resuspend)

    @xmlrpc_server.dbus_safe(0)
    def get_dark_resume_count(self):
        """Gets the number of dark resumes that have occurred since
        this listener was created."""
        return self._listener.count

    @xmlrpc_server.dbus_safe(False)
    def has_lid(self):
        """
        Checks whether the DUT has lid.

        @return: Returns True if the device has a lid, False otherwise.
        """

        return power_utils.has_lid()


if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    handler = logging.handlers.SysLogHandler(address='/dev/log')
    formatter = logging.Formatter(
        'dark_resume_xmlrpc_server: [%(levelname)s] %(message)s')
    handler.setFormatter(formatter)
    logging.getLogger().addHandler(handler)
    logging.debug('dark_resume_xmlrpc_server main...')
    server = xmlrpc_server.XmlRpcServer(
        'localhost', constants.DARK_RESUME_XMLRPC_SERVER_PORT)
    server.register_delegate(DarkResumeXmlRpcDelegate())
    server.run()
