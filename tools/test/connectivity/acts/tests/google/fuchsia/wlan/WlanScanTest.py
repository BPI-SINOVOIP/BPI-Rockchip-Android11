#!/usr/bin/env python3.4
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
"""
This test exercises basic scanning functionality to confirm expected behavior
related to wlan scanning
"""

from datetime import datetime

import pprint
import time

import acts.base_test
import acts.test_utils.wifi.wifi_test_utils as wutils

from acts import signals
from acts.controllers.ap_lib import hostapd_ap_preset
from acts.controllers.ap_lib import hostapd_bss_settings
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib import hostapd_security
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest


class WlanScanTest(WifiBaseTest):
    """WLAN scan test class.

    Test Bed Requirement:
    * One or more Fuchsia devices
    * Several Wi-Fi networks visible to the device, including an open Wi-Fi
      network or a onHub/GoogleWifi
    """
    def setup_class(self):
        super().setup_class()

        self.start_access_point = False
        if "AccessPoint" in self.user_params:
            # This section sets up the config that could be sent to the AP if
            # the AP is needed. The reasoning is since ACTS already connects
            # to the AP if it is in the config, generating the config in memory
            # has no over head is used if need by the test if one of the ssids
            # needed for the test is not included in the config.  The logic
            # here creates 2 ssids on each radio, 5ghz and 2.4ghz, with an
            # open, no security network and one that is wpa2, for a total of 4
            # networks.  However, if all of the ssids are specified in the
            # the config will never be written to the AP and the AP will not be
            # brought up.  For more information about how to configure the
            # hostapd config info, see the hostapd libraries, which have more
            # documentation.
            bss_settings_2g = []
            bss_settings_5g = []
            open_network = self.get_open_network(False, [])
            self.open_network_2g = open_network['2g']
            self.open_network_5g = open_network['5g']
            wpa2_settings = self.get_psk_network(False, [])
            self.wpa2_network_2g = wpa2_settings['2g']
            self.wpa2_network_5g = wpa2_settings['5g']
            bss_settings_2g.append(
                hostapd_bss_settings.BssSettings(
                    name=self.wpa2_network_2g['SSID'],
                    ssid=self.wpa2_network_2g['SSID'],
                    security=hostapd_security.Security(
                        security_mode=self.wpa2_network_2g["security"],
                        password=self.wpa2_network_2g["password"])))
            bss_settings_5g.append(
                hostapd_bss_settings.BssSettings(
                    name=self.wpa2_network_5g['SSID'],
                    ssid=self.wpa2_network_5g['SSID'],
                    security=hostapd_security.Security(
                        security_mode=self.wpa2_network_5g["security"],
                        password=self.wpa2_network_5g["password"])))
            self.ap_2g = hostapd_ap_preset.create_ap_preset(
                iface_wlan_2g=self.access_points[0].wlan_2g,
                iface_wlan_5g=self.access_points[0].wlan_5g,
                channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
                ssid=self.open_network_2g['SSID'],
                bss_settings=bss_settings_2g)
            self.ap_5g = hostapd_ap_preset.create_ap_preset(
                iface_wlan_2g=self.access_points[0].wlan_2g,
                iface_wlan_5g=self.access_points[0].wlan_5g,
                channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
                ssid=self.open_network_5g['SSID'],
                bss_settings=bss_settings_5g)

        if "wlan_open_network_2g" in self.user_params:
            self.open_network_2g = self.user_params.get("wlan_open_network_2g")
        elif "AccessPoint" in self.user_params:
            self.start_access_point_2g = True
        else:
            raise Exception('Missing parameter in config '
                            '(wlan_open_network_2g)')

        if "wlan_open_network_5g" in self.user_params:
            self.open_network_5g = self.user_params.get("wlan_open_network_5g")
        elif "AccessPoint" in self.user_params:
            self.start_access_point_5g = True
        else:
            raise Exception('Missing parameter in config '
                            '(wlan_open_network_5g)')

        if "wlan_wpa2_network_2g" in self.user_params:
            self.wpa2_network_2g = self.user_params.get("wlan_wpa2_network_2g")
        elif "AccessPoint" in self.user_params:
            self.start_access_point_2g = True
        else:
            raise Exception('Missing parameter in config '
                            '(wlan_wpa2_network_2g)')

        if "wlan_wpa2_network_5g" in self.user_params:
            self.wpa2_network_5g = self.user_params.get("wlan_wpa2_network_5g")
        elif "AccessPoint" in self.user_params:
            self.start_access_point_5g = True
        else:
            raise Exception('Missing parameter in config '
                            '(wlan_wpa2_network_5g)')

        # Only bring up the APs that are needed for the test.  Each ssid is
        # randomly generated so there is no chance of re associating to a
        # previously saved ssid on the device.
        if self.start_access_point_2g:
            self.start_access_point = True
            self.access_points[0].start_ap(hostapd_config=self.ap_2g)
        if self.start_access_point_5g:
            self.start_access_point = True
            self.access_points[0].start_ap(hostapd_config=self.ap_5g)

    def setup_test(self):
        for fd in self.fuchsia_devices:
            # stub for setting up all the fuchsia devices in the testbed.
            pass

    def teardown_test(self):
        for fd in self.fuchsia_devices:
            fd.wlan_lib.wlanDisconnect()

    def teardown_class(self):
        if self.start_access_point:
            self.access_points[0].stop_all_aps()

    """Helper Functions"""

    def check_connect_response(self, connection_response):
        """ Checks the result of connecting to a wlan.
            Args:
                connection_response: The response from SL4F after attempting
                    to connect to a wlan.
        """
        if connection_response.get("error") is None:
            # the command did not get an error response - go ahead and
            # check the result
            connection_result = connection_response.get("result")
            if connection_result:
                self.log.info("connection to network successful")
            else:
                # ideally, we would have the actual error...  but logging
                # here to cover that error case
                raise signals.TestFailure("Connect call failed, aborting test")
        else:
            # the response indicates an error - log and raise failure
            raise signals.TestFailure("Aborting test - Connect call failed "
                                      "with error: %s"
                                      % connection_response.get("error"))

    def scan_while_connected(self, wlan_network_params, fd):
        """ Connects to as specified network and initiates a scan
                Args:
                    wlan_network_params: A dictionary containing wlan
                        infomation.
                    fd: The fuchsia device to connect to the wlan.
        """
        target_ssid = wlan_network_params['SSID']
        self.log.info("got the ssid! %s", target_ssid)
        target_pwd = None
        if 'password' in wlan_network_params:
            target_pwd = wlan_network_params['password']

        connection_response = fd.wlan_lib.wlanConnectToNetwork(
            target_ssid,
            target_pwd)
        self.check_connect_response(connection_response)
        self.basic_scan_request(fd)

    def basic_scan_request(self, fd):
        """ Initiates a basic scan on a Fuchsia device
            Args:
                fd: A fuchsia device
        """
        start_time = datetime.now()

        scan_response = fd.wlan_lib.wlanStartScan()

        # first check if we received an error
        if scan_response.get("error") is None:
            # the scan command did not get an error response - go ahead
            # and check for scan results
            scan_results = scan_response["result"]
        else:
            # the response indicates an error - log and raise failure
            raise signals.TestFailure("Aborting test - scan failed with "
                                      "error: %s"
                                      % scan_response.get("error"))

        self.log.info("scan contained %d results", len(scan_results))

        total_time_ms = (datetime.now() - start_time).total_seconds() * 1000
        self.log.info("scan time: %d ms", total_time_ms)

        if len(scan_results) > 0:
            raise signals.TestPass(details="",
                                   extras={"Scan time":"%d" % total_time_ms})
        else:
            raise signals.TestFailure("Scan failed or did not "
                                      "find any networks")


    """Tests"""
    def test_basic_scan_request(self):
        """Verify a general scan trigger returns at least one result"""
        for fd in self.fuchsia_devices:
            self.basic_scan_request(fd)

    def test_scan_while_connected_open_network_2g(self):
        for fd in self.fuchsia_devices:
            self.scan_while_connected(self.open_network_2g, fd)

    def test_scan_while_connected_wpa2_network_2g(self):
        for fd in self.fuchsia_devices:
            self.scan_while_connected(self.wpa2_network_2g, fd)

    def test_scan_while_connected_open_network_5g(self):
        for fd in self.fuchsia_devices:
            self.scan_while_connected(self.open_network_5g, fd)

    def test_scan_while_connected_wpa2_network_5g(self):
        for fd in self.fuchsia_devices:
            self.scan_while_connected(self.wpa2_network_5g, fd)

