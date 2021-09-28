# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Expects to be run in an environment with sudo and no interactive password
# prompt, such as within the Chromium OS development chroot.


"""This file provides core logic for pdtester verify/repair process."""

import logging

from autotest_lib.server.hosts import servo_host


# Names of the host attributes in the database that represent the values for
# the pdtester_host and pdtester_port for a PD tester connected to the DUT.
PDTESTER_HOST_ATTR = 'pdtester_host'
PDTESTER_PORT_ATTR = 'pdtester_port'


def make_pdtester_hostname(dut_hostname):
    """Given a DUT's hostname, return the hostname of its PD tester.

    @param dut_hostname: hostname of a DUT.

    @return hostname of the DUT's PD tester.

    """
    host_parts = dut_hostname.split('.')
    host_parts[0] = host_parts[0] + '-pdtester'
    return '.'.join(host_parts)


class PDTesterHost(servo_host.ServoHost):
    """Host class for a host that controls a PDTester object."""


    def _initialize(self, pdtester_host='localhost', pdtester_port=9999,
                    *args, **dargs):
        """Initialize a PDTesterHost instance.

        A PDTesterHost instance represents a host that controls a PD tester.

        @param pdtester_host: Name of the host where the servod process
                              is running.
        @param pdtester_port: Port the servod process is listening on.

        """
        super(PDTesterHost, self)._initialize(pdtester_host, pdtester_port,
                                              *args, **dargs)
        self.connect_servo()


def create_pdtester_host(pdtester_args, servo_host):
    """Create a PDTesterHost object used to access pdtester servo

    The `pdtester_args` parameter is a dictionary specifying optional
    PDTester client parameter overrides (i.e. a specific host or port).
    When specified, the caller requires that an exception be raised
    unless both the PDTesterHost and the PDTester are successfully
    created.

    @param pdtester_args: A dictionary that contains args for creating
                          a PDTesterHost object,
                          e.g. {'pdtester_host': '172.11.11.111',
                                'pdtester_port': 9999}.
    @param servo_host: If PDTester and Servo are the same, this
                       servo_host object will be returned.
    @returns: A PDTesterHost object or None.

    """
    # None means PDTester is not required to run a test.
    if pdtester_args is None:
        return None

    # If an user doesn't pass the PDTester info, fall back to use the servo
    # info. Usually we use Servo v4 as PDTester, so make it default.
    if PDTESTER_HOST_ATTR not in pdtester_args:
        logging.debug('%s not specified, reuse the same hostname as servo: %s',
                      PDTESTER_HOST_ATTR, servo_host.hostname)
        pdtester_args[PDTESTER_HOST_ATTR] = servo_host.hostname

    if PDTESTER_PORT_ATTR not in pdtester_args:
        logging.debug('%s not specified, reuse the same port as servo: %s',
                      PDTESTER_PORT_ATTR, servo_host.servo_port)
        pdtester_args[PDTESTER_PORT_ATTR] = servo_host.servo_port

    # Just return the servo_host object, if the hostname and the port are the
    # same as servo_host.
    if (pdtester_args[PDTESTER_HOST_ATTR] == servo_host.hostname and
        pdtester_args[PDTESTER_PORT_ATTR] == servo_host.servo_port):
        logging.debug('Return the servo_host directly as PDTester and Servo '
                      'are the same.')
        return servo_host

    return PDTesterHost(**pdtester_args)
