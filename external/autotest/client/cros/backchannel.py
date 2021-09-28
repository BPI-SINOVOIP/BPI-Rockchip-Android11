# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import time

from autotest_lib.client.bin import local_host
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import interface

# Flag file used to tell backchannel script it's okay to run.
BACKCHANNEL_FILE = '/mnt/stateful_partition/etc/enable_backchannel_network'
# Backchannel interface name.
BACKCHANNEL_IFACE_NAME = 'eth_test'
# Script that handles backchannel heavy lifting.
BACKCHANNEL_SCRIPT = '/usr/local/lib/flimflam/test/backchannel'


class Backchannel(object):
    """Wrap backchannel in a context manager so it can be used with with.

    Example usage:
         with backchannel.Backchannel():
                block
    The backchannel will be torn down whether or not 'block' throws.
    """

    def __init__(self, host=None, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs
        self.gateway = None
        self.interface = None
        if host is not None:
            self.host = host
        else:
            self.host = local_host.LocalHost()
        self._run = self.host.run

    def __enter__(self):
        self.setup(*self.args, **self.kwargs)
        return self

    def __exit__(self, exception, value, traceback):
        self.teardown()
        return False

    def setup(self, create_ssh_routes=True):
        """
        Enables the backchannel interface.

        @param create_ssh_routes: If True set up routes so that all existing
                SSH sessions will remain open.

        @returns True if the backchannel is already set up, or was set up by
                this call, otherwise False.

        """

        # If the backchannel interface is already up there's nothing
        # for us to do.
        if self._is_test_iface_running():
            return True

        # Retrieve the gateway for the default route.
        try:
            # Poll here until we have route information.
            # If shill was recently started, it will take some time before
            # DHCP gives us an address.
            line = utils.poll_for_condition(
                    lambda: self._get_default_route(),
                    exception=utils.TimeoutError(
                            'Timed out waiting for route information'),
                    timeout=30)
            self.gateway, self.interface = line.strip().split(' ')

            # Retrieve list of open ssh sessions so we can reopen
            # routes afterward.
            if create_ssh_routes:
                out = self._run(
                        "netstat -tanp | grep :22 | "
                        "grep ESTABLISHED | awk '{print $5}'").stdout
                # Extract IP from IP:PORT listing. Uses set to remove
                # duplicates.
                open_ssh = list(set(item.strip().split(':')[0] for item in
                                    out.split('\n') if item.strip()))

            # Build a command that will set up the test interface and add
            # ssh routes in one shot. This is necessary since we'll lose
            # connectivity to a remote host between these steps.
            cmd = '%s setup %s' % (BACKCHANNEL_SCRIPT, self.interface)
            if create_ssh_routes:
                for ip in open_ssh:
                    # Add route using the pre-backchannel gateway.
                    cmd += '&& %s reach %s %s' % (BACKCHANNEL_SCRIPT, ip,
                            self.gateway)

            self._run(cmd)

            # Make sure we have a route to the gateway before continuing.
            logging.info('Waiting for route to gateway %s', self.gateway)
            utils.poll_for_condition(
                    lambda: self._is_route_ready(),
                    exception=utils.TimeoutError('Timed out waiting for route'),
                    timeout=30)
        except Exception, e:
            logging.error(e)
            return False
        finally:
            # Remove backchannel file flag so system reverts to normal
            # on reboot.
            if os.path.isfile(BACKCHANNEL_FILE):
                os.remove(BACKCHANNEL_FILE)

        return True

    def teardown(self):
        """Tears down the backchannel."""
        if self.interface:
            self._run('%s teardown %s' % (BACKCHANNEL_SCRIPT, self.interface))

        # Hack around broken Asix network adaptors that may flake out when we
        # bring them up and down (crbug.com/349264).
        # TODO(thieule): Remove this when the adaptor/driver is fixed
        # (crbug.com/350172).
        try:
            if self.gateway:
                logging.info('Waiting for route restore to gateway %s',
                             self.gateway)
                utils.poll_for_condition(
                        lambda: self._is_route_ready(),
                        exception=utils.TimeoutError(
                                'Timed out waiting for route'),
                        timeout=30)
        except utils.TimeoutError:
            if self.host is None:
                self._reset_usb_ethernet_device()


    def is_using_ethernet(self):
        """
        Checks to see if the backchannel is using an ethernet device.

        @returns True if the backchannel is using an ethernet device.

        """
        # Check the port type reported by ethtool.
        result = self._run('ethtool %s' % BACKCHANNEL_IFACE_NAME,
                           ignore_status=True)
        if (result.exit_status == 0 and
            re.search('Port: (TP|Twisted Pair|MII|Media Independent Interface)',
                      result.stdout)):
            return True

        # ethtool doesn't report the port type for some Ethernet adapters.
        # Fall back to check against a list of known Ethernet adapters:
        #
        #   13b1:0041 - Linksys USB3GIG USB 3.0 Gigabit Ethernet Adapter
        properties = self._get_udev_properties(BACKCHANNEL_IFACE_NAME)
        # Depending on the udev version, ID_VENDOR_ID/ID_MODEL_ID may or may
        # not have the 0x prefix, so we convert them to an integer value first.
        bus = properties.get('ID_BUS', 'unknown').lower()
        vendor_id = int(properties.get('ID_VENDOR_ID', '0000'), 16)
        model_id = int(properties.get('ID_MODEL_ID', '0000'), 16)
        device_id = '%s:%04x:%04x' % (bus, vendor_id, model_id)
        if device_id in ['usb:13b1:0041']:
            return True

        return False


    def _get_udev_properties(self, iface):
        properties = {}
        result = self._run('udevadm info -q property /sys/class/net/%s' % iface,
                           ignore_status=True)
        if result.exit_status == 0:
            for line in result.stdout.splitlines():
                key, value = line.split('=', 1)
                properties[key] = value

        return properties


    def _reset_usb_ethernet_device(self):
        try:
            # Use the absolute path to the USB device instead of accessing it
            # via the path with the interface name because once we
            # deauthorize the USB device, the interface name will be gone.
            usb_authorized_path = os.path.realpath(
                    '/sys/class/net/%s/device/../authorized' % self.interface)
            logging.info('Reset ethernet device at %s', usb_authorized_path)
            utils.system('echo 0 > %s' % usb_authorized_path)
            time.sleep(10)
            utils.system('echo 1 > %s' % usb_authorized_path)
        except error.CmdError:
            pass


    def _get_default_route(self):
        """Retrieves default route information."""
        # Get default routes and parse out the gateway and interface.
        cmd = "ip -4 route show table 0 | awk '/^default via/ { print $3, $5 }'"
        return self._run(cmd).stdout.split('\n')[0]


    def _is_test_iface_running(self):
        """Checks whether the test interface is running."""
        return interface.Interface(BACKCHANNEL_IFACE_NAME).is_link_operational()


    def _is_route_ready(self):
        """Checks for a route to the specified destination."""
        dest = self.gateway
        result = self._run('ping -c 1 %s' % dest, ignore_status=True)
        if result.exit_status:
            logging.warning('Route to %s is not ready.', dest)
            return False
        logging.info('Route to %s is ready.', dest)
        return True
