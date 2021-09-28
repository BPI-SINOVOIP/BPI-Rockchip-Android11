# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import time

from autotest_lib.client.common_lib.cros.network import interface
from autotest_lib.client.cros.networking.chrome_testing \
        import chrome_networking_test_api as cnta
from autotest_lib.client.cros.networking.chrome_testing \
        import chrome_networking_test_context as cntc
from autotest_lib.client.cros.power import power_test

class power_WifiIdle(power_test.power_Test):
    """Class for power_WifiIdle test."""
    version = 1


    def initialize(self, pdash_note='', force_discharge=False):
        """Perform necessary initialization prior to test run."""
        super(power_WifiIdle, self).initialize(seconds_period=10.,
                                               pdash_note=pdash_note,
                                               force_discharge=force_discharge)

    def _is_wifi_on(self):
        """Return whether wifi is enabled."""
        enabled_devices = self.chrome_net.get_enabled_devices()
        return self.chrome_net.WIFI_DEVICE in enabled_devices

    def _verify_connected_to_network(self):
        """Raise error if not connected to network, else do nothing."""
        networks = self.chrome_net.get_wifi_networks()
        logging.info('Networks found: %s', networks)

        for network in networks:
            if network['ConnectionState'] == 'Connected':
                logging.info('Connected to network: %s', network)
                return

        logging.info('Not connected to network.')
        raise error.TestError('Not connected to network.')

    def run_once(self, idle_time=120):
        """Collect power stats when wifi is on or off.

        Args:
            idle_time: time in seconds to stay idle and measure power
        """

        # Find all wired ethernet interfaces.
        # TODO(seankao): This block to check whether ethernet interface
        # is active appears in several tests. Pull this into power_utils.
        ifaces = [iface for iface in interface.get_interfaces()
                if (not iface.is_wifi_device() and
                iface.name.startswith('eth'))]
        logging.debug('Ethernet interfaces include: ' +
                str([iface.name for iface in ifaces]))
        for iface in ifaces:
            if iface.is_lower_up:
                raise error.TestError('Ethernet interface is active. '
                                      'Please remove Ethernet cable.')

        with cntc.ChromeNetworkingTestContext() as testing_context:
            self.chrome_net = cnta.ChromeNetworkProvider(testing_context)
            # Ensure wifi is enabled.
            if not self._is_wifi_on():
                self.chrome_net.enable_network_device(
                        self.chrome_net.WIFI_DEVICE)
            if not self._is_wifi_on():
                raise error.TestError('Failed to enable wifi.')

            self.chrome_net.scan_for_networks()
            self._verify_connected_to_network()

            # Test system idle with wifi turned on.
            self.start_measurements()
            time.sleep(idle_time)
            self.checkpoint_measurements('wifi_on')

            # Disable wifi.
            self.chrome_net.disable_network_device(self.chrome_net.WIFI_DEVICE)
            if self._is_wifi_on():
                raise error.TestError('Failed to disable wifi.')

            # Test system idle with wifi turned off.
            start_time = time.time()
            time.sleep(idle_time)
            self.checkpoint_measurements('wifi_off', start_time)

            # Turn on wifi before leaving the test.
            self.chrome_net.enable_network_device(self.chrome_net.WIFI_DEVICE)
