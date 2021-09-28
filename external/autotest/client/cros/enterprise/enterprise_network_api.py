# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from collections import namedtuple

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.networking.chrome_testing \
        import chrome_networking_test_context as cntc
from autotest_lib.client.cros.networking.chrome_testing \
        import chrome_networking_test_api as cnta


NETWORK_TEST_EXTENSION_PATH = cntc.NETWORK_TEST_EXTENSION_PATH


class ChromeEnterpriseNetworkContext(object):
    """
    This class contains all the Network API methods required for
    Enterprise Network WiFi tests.

    """
    SHORT_TIMEOUT = 20
    LONG_TIMEOUT = 120


    def __init__(self, browser=None):
        testing_context = cntc.ChromeNetworkingTestContext()
        testing_context.setup(browser)
        self.chrome_net_context = cnta.ChromeNetworkProvider(testing_context)
        self.enable_wifi_on_dut()


    def _extract_wifi_network_info(self, networks_found_list):
        """
        Extract the required Network params from list of Networks found.

        Filter out the required network parameters such Network Name/SSID,
        GUID, connection state and security type of each of the networks in
        WiFi range of the DUT.

        @param networks_found_list: Network information returned as a result
                of the getVisibleNetworks api.

        @returns: Formatted list of namedtuples containing the
                required network parameters.

        """
        network_info_list = []
        network_info = namedtuple(
                'NetworkInfo', 'name guid connectionState security')

        for network in networks_found_list:
            network_data = network_info(
                                name=network['Name'],
                                guid=network['GUID'],
                                connectionState=network['ConnectionState'],
                                security=network['WiFi']['Security'])
            network_info_list.append(network_data)
        return network_info_list


    def list_networks(self):
        """@returns: List of available WiFi networks."""
        return self._extract_wifi_network_info(
                self.chrome_net_context.get_wifi_networks())


    def disable_network_device(self, network):
        """
        Disable given network device.

        This will fail if called multiple times in a test. Use the version in
        'chrome_networking_test_api' if this is the case.

        @param network: string name of the network device to be disabled.
                Options include 'WiFi', 'Cellular', and 'Ethernet'.

        """
        logging.info('Disabling: %s', network)
        disable_network_result = self.chrome_net_context.\
                _chrome_testing.call_test_function_async(
                    'disableNetworkDevice',
                    '"' + network + '"')


    def _get_network_info(self, ssid):
        """
        Returns the Network parameters for a specific network.

        @param ssid: SSID of the network.

        @returns: The NetworkInfo tuple containing the network parameters.
                Returns None if network info for the SSID was not found.

        """
        networks_in_range = self._extract_wifi_network_info(
                self.chrome_net_context.get_wifi_networks())
        logging.debug('Network info of all the networks in WiFi'
                'range of DUT:%r', networks_in_range)
        for network_info in networks_in_range:
            if network_info.name == ssid:
                return network_info
        return None


    def _get_network_connection_state(self, ssid):
        """
        Returns the connection State of the network.

        @returns: Connection State for the SSID.

        """
        network_info = self._get_network_info(ssid)
        if network_info is None:
            return None
        return network_info.connectionState


    def connect_to_network(self, ssid):
        """
        Triggers a manual connect to the network using networkingPrivate API.

        @param ssid: The ssid that the connection request is initiated for.

        @raises error.TestFail: If the WiFi network is out of range or the
            DUT cannot manually connect to the network.

        """
        if not self.is_network_in_range(ssid):
            raise error.TestFail(
                    "The SSID: %r is not in WiFi range of the DUT" % ssid)

        network_to_connect = self._get_network_info(ssid)
        logging.info("Triggering a manual connect to network SSID: %r, GUID %r",
                network_to_connect.name, network_to_connect.guid)

        # TODO(krishnargv): Replace below code with the
        # self.chrome_net_context.connect_to_network(network_to_connect) method.
        new_network_connect = self.chrome_net_context._chrome_testing.\
                call_test_function(
                self.LONG_TIMEOUT,
                'connectToNetwork',
                '"%s"'% network_to_connect.guid)
        logging.debug("Manual network connection status: %r",
                new_network_connect['status'])
        if new_network_connect['status'] == 'chrome-test-call-status-failure':
            raise error.TestFail(
                    'Could not connect to %s network. Error returned by '
                    'chrome.networkingPrivate.startConnect API: %s' %
                    (network_to_connect.name, new_network_connect['error']))


    def disconnect_from_network(self, ssid):
        """
        Triggers a disconnect from the network using networkingPrivate API.

        @param ssid: The ssid that the disconnection request is initiated for.

        @raises error.TestFail: If the WiFi network is not in WiFi range of the
                DUT or if the DUT cannot manually disconnect from the SSID.

        """
        if not self.is_network_in_range(ssid):
            raise error.TestFail(
                    "The SSID: %r is not in WiFi range of the DUT" % ssid)

        network_to_disconnect = self._get_network_info(ssid)
        logging.info("Triggering a disconnect from network SSID: %r, GUID %r",
                network_to_disconnect.name, network_to_disconnect.guid)

        new_network_disconnect = self.chrome_net_context._chrome_testing.\
                call_test_function(
                self.LONG_TIMEOUT,
                'disconnectFromNetwork',
                '"%s"'% network_to_disconnect.guid)
        logging.debug("Manual network disconnection status: %r",
                new_network_disconnect['status'])
        if (new_network_disconnect['status'] ==
                'chrome-test-call-status-failure'):
            raise error.TestFail(
                    'Could not disconnect from %s network. Error returned by '
                    'chrome.networkingPrivate.startDisconnect API: %s' %
                    (network_to_disconnect.name,
                    new_network_disconnect['error']))


    def enable_wifi_on_dut(self):
        """Enable the WiFi interface on the DUT if it is disabled."""
        enabled_devices = self.chrome_net_context.get_enabled_devices()
        if self.chrome_net_context.WIFI_DEVICE not in enabled_devices:
            self.chrome_net_context.enable_network_device(
                self.chrome_net_context.WIFI_DEVICE)


    def is_network_in_range(self, ssid, wait_time=0):
        """
        Returns True if the WiFi network is within WiFi range of the DUT.

        @param ssid: The SSID of the network.
        @param wait_time: Seconds to wait for SSID to appear.

        @returns: True if the network/ssid is within WiFi range of the DUT,
                else returns False

        """
        try:
            return utils.poll_for_condition(
                    lambda: self._get_network_info(ssid) is not None,
                    timeout=wait_time)
        except utils.TimeoutError:
            return False


    def is_network_connected(self, ssid):
        """
        Return True if the DUT is connected to the Network.

        Returns True if the DUT is connected to the network. Waits for a
        a short time if the DUT is in a connecting state.

        @param ssid: The SSID of the network.

        @returns: True if the DUT is connected to the network/ssid,
                else returns False

        @raises error.TestFail: If the DUT is stuck in the connecting state.

        """
        utils.poll_for_condition(
                lambda: (self._get_network_connection_state(ssid)
                         != 'Connecting'),
                exception=error.TestFail('Device stuck in connecting state'),
                timeout=self.SHORT_TIMEOUT)
        try:
            utils.poll_for_condition(
                    lambda: (self._get_network_connection_state(ssid)
                             == 'Connected'),
                    timeout=self.SHORT_TIMEOUT)
            return True
        except utils.TimeoutError:
            logging.debug("Connection state for SSID-%r is: %r",
                          ssid, self._get_network_connection_state(ssid))
            return False
