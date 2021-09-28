#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import time
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_2G
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_5G
from acts.test_utils.wifi import wifi_test_utils as wutils
import acts.test_utils.power.cellular.cellular_traffic_power_test as ctpt


class PowerTelHotspotTest(ctpt.PowerTelTrafficTest):
    """ Cellular traffic over WiFi tethering power test.

    Treated as a different case of data traffic. Inherits from
    PowerTelTrafficTest and only needs to make a change in the measurement step.
    """

    # Class config parameters
    CONFIG_KEY_WIFI = 'hotspot_network'

    # Test name configuration keywords
    PARAM_WIFI_BAND = "wifiband"
    PARAM_2G_BAND = "2g"
    PARAM_5G_BAND = "5g"

    def __init__(self, controllers):
        """ Class initialization

        Set attributes to default values.
        """

        super().__init__(controllers)

        # Initialize values
        self.wifi_band = None
        self.iperf_results = None

    def setup_class(self):
        """ Executed before any test case is started.

        Set country code for client and host devices.

        """

        if not super().setup_class():
            return False

        # If an SSID and password are indicated in the configuration parameters,
        # use those. If not, use default parameters and warn the user.

        if hasattr(self, self.CONFIG_KEY_WIFI):

            self.network = getattr(self, self.CONFIG_KEY_WIFI)

            if not (wutils.WifiEnums.SSID_KEY in self.network
                    and wutils.WifiEnums.PWD_KEY in self.network):
                raise RuntimeError(
                    "The '{}' key in the configuration file needs"
                    " to contain the '{}' and '{}' fields.".format(
                        self.CONFIG_KEY_WIFI, wutils.WifiEnums.SSID_KEY,
                        wutils.WifiEnums.PWD_KEY))
        else:

            self.log.warning("The configuration file doesn't indicate an SSID "
                             "password for the hotspot. Using default values. "
                             "To configured the SSID and pwd include a the key"
                             " {} containing the '{}' and '{}' fields.".format(
                                 self.CONFIG_KEY_WIFI,
                                 wutils.WifiEnums.SSID_KEY,
                                 wutils.WifiEnums.PWD_KEY))

            self.network = {
                wutils.WifiEnums.SSID_KEY: "Pixel_1030",
                wutils.WifiEnums.PWD_KEY: "1234567890"
            }

    def power_tel_tethering_test(self):
        """ Measure power and throughput during data transmission.

        Starts WiFi tethering in the DUT and connects a second device. Then
        the iPerf client is hosted in the second android device.

        """
        # Country Code set to 00 after toggling airplane mode.
        # We need to set this right before we setup a hotspot
        # Set country codes on both devices to US to connect to 5GHz
        country_code = "US"
        hotspot_dut = self.dut
        slave_dut = self.android_devices[1]
        for dut in [hotspot_dut, slave_dut]:
            self.log.info("Setting Country Code to %s for SN:%s" %
                          (country_code, dut.serial))
            wutils.set_wifi_country_code(dut, country_code)

        # Setup tethering
        wutils.start_wifi_tethering(self.dut,
                                    self.network[wutils.WifiEnums.SSID_KEY],
                                    self.network[wutils.WifiEnums.PWD_KEY],
                                    self.wifi_band)

        wutils.wifi_connect(self.android_devices[1],
                            self.network,
                            check_connectivity=False)

        # Start data traffic
        iperf_helpers = self.start_tel_traffic(self.android_devices[1])

        # Measure power
        result = self.collect_power_data()

        # Wait for iPerf to finish
        time.sleep(self.IPERF_MARGIN + 2)

        # Collect throughput measurement
        self.iperf_results = self.get_iperf_results(self.android_devices[1],
                                                    iperf_helpers)

        # Checks if power is below the required threshold.
        self.pass_fail_check(result.average_current)

    def setup_test(self):
        """ Executed before every test case.

        Parses test configuration from the test name and prepares
        the simulation for measurement.
        """

        # Call parent method first to setup simulation
        if not super().setup_test():
            return False

        try:
            values = self.consume_parameter(self.PARAM_WIFI_BAND, 1)

            if values[1] == self.PARAM_2G_BAND:
                self.wifi_band = WIFI_CONFIG_APBAND_2G
            elif values[1] == self.PARAM_5G_BAND:
                self.wifi_band = WIFI_CONFIG_APBAND_5G
            else:
                raise ValueError()
        except:
            self.log.error(
                "The test name has to include parameter {} followed by "
                "either {} or {}.".format(self.PARAM_WIFI_BAND,
                                          self.PARAM_2G_BAND,
                                          self.PARAM_5G_BAND))
            return False

        return True
