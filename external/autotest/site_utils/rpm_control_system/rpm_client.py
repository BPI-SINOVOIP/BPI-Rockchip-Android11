#!/usr/bin/python2
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import sys
import xmlrpclib

import common

from config import rpm_config
from autotest_lib.client.common_lib import global_config
from autotest_lib.client.common_lib.cros import retry

RPM_FRONTEND_URI = global_config.global_config.get_config_value('CROS',
        'rpm_frontend_uri', type=str, default='')
RPM_CALL_TIMEOUT_MINS = rpm_config.getint('RPM_INFRASTRUCTURE',
                                          'call_timeout_mins')

POWERUNIT_HOSTNAME_KEY = 'powerunit_hostname'
POWERUNIT_OUTLET_KEY = 'powerunit_outlet'
HYDRA_HOSTNAME_KEY = 'hydra_hostname'


class RemotePowerException(Exception):
    """This is raised when we fail to set the state of the device's outlet."""
    pass


def set_power(host, new_state, timeout_mins=RPM_CALL_TIMEOUT_MINS):
    """Sends the power state change request to the RPM Infrastructure.

    @param host: A CrosHost or ServoHost instance.
    @param new_state: State we want to set the power outlet to.
    """
    # servo V3 is handled differently from the rest.
    # The best heuristic we have to determine servo V3 is the hostname.
    if host.hostname.endswith('servo'):
        args_tuple = (host.hostname, new_state)
    else:
        info = host.host_info_store.get()
        try:
            args_tuple = (host.hostname,
                          info.attributes[POWERUNIT_HOSTNAME_KEY],
                          info.attributes[POWERUNIT_OUTLET_KEY],
                          info.attributes.get(HYDRA_HOSTNAME_KEY),
                          new_state)
        except KeyError as e:
            logging.warning('Powerunit information not found. Missing:'
                            ' %s in host_info_store.', e)
            raise RemotePowerException('Remote power control is not applicable'
                                       ' for %s, it could be either RPM is not'
                                       ' supported on the rack or powerunit'
                                       ' attributes is not configured in'
                                       ' inventory.' % host.hostname)
    _set_power(args_tuple, timeout_mins)


def _set_power(args_tuple, timeout_mins=RPM_CALL_TIMEOUT_MINS):
    """Sends the power state change request to the RPM Infrastructure.

    @param args_tuple: A args tuple for rpc call. See example below:
        (hostname, powerunit_hostname, outlet, hydra_hostname, new_state)
    """
    client = xmlrpclib.ServerProxy(RPM_FRONTEND_URI,
                                   verbose=False,
                                   allow_none=True)
    timeout = None
    result = None
    endpoint = (client.set_power_via_poe if len(args_tuple) == 2
                else client.set_power_via_rpm)
    try:
        timeout, result = retry.timeout(endpoint,
                                        args=args_tuple,
                                        timeout_sec=timeout_mins * 60,
                                        default_result=False)
    except Exception as e:
        logging.exception(e)
        raise RemotePowerException(
                'Client call exception: ' + str(e))
    if timeout:
        raise RemotePowerException(
                'Call to RPM Infrastructure timed out.')
    if not result:
        error_msg = ('Failed to change outlet status for host: %s to '
                     'state: %s.' % (args_tuple[0], args_tuple[-1]))
        logging.error(error_msg)
        raise RemotePowerException(error_msg)


def parse_options():
    """Parse the user supplied options."""
    parser = argparse.ArgumentParser()
    parser.add_argument('-m', '--machine', dest='machine', required=True,
                        help='Machine hostname to change outlet state.')
    parser.add_argument('-s', '--state', dest='state', required=True,
                        choices=['ON', 'OFF', 'CYCLE'],
                        help='Power state to set outlet: ON, OFF, CYCLE')
    parser.add_argument('-p', '--powerunit_hostname', dest='p_hostname',
                        help='Powerunit hostname of the host.')
    parser.add_argument('-o', '--outlet', dest='outlet',
                        help='Outlet of the host.')
    parser.add_argument('-y', '--hydra_hostname', dest='hydra', default='',
                        help='Hydra hostname of the host.')
    parser.add_argument('-d', '--disable_emails', dest='disable_emails',
                        help='Hours to suspend RPM email notifications.')
    parser.add_argument('-e', '--enable_emails', dest='enable_emails',
                        action='store_true',
                        help='Resume RPM email notifications.')
    return parser.parse_args()


def main():
    """Entry point for rpm_client script."""
    options = parse_options()
    if options.machine.endswith('servo'):
        args_tuple = (options.machine, options.state)
    elif not options.p_hostname or not options.outlet:
        print "Powerunit hostname and outlet info are required for DUT."
        return
    else:
        args_tuple = (options.machine, options.p_hostname, options.outlet,
                      options.hydra, options.state)
    _set_power(args_tuple)

    if options.disable_emails is not None:
        client = xmlrpclib.ServerProxy(RPM_FRONTEND_URI, verbose=False)
        client.suspend_emails(options.disable_emails)
    if options.enable_emails:
        client = xmlrpclib.ServerProxy(RPM_FRONTEND_URI, verbose=False)
        client.resume_emails()


if __name__ == "__main__":
    sys.exit(main())
