#!/usr/bin/env python3.4
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
from acts.test_decorators import test_tracker_info
from acts.test_utils.power import PowerWiFiBaseTest as PWBT
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_2G
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_5G
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi import wifi_power_test_utils as wputils
from acts.test_utils.power.IperfHelper import IperfHelper


class PowerWiFiHotspotTest(PWBT.PowerWiFiBaseTest):
    """ WiFi Tethering HotSpot Power test.

     Treated as a different case of data traffic. Traffic flows between DUT
     (android_device[0]) and client device (android_device[1]).
     The iperf server is always the client device and the iperf client is
     always the DUT (hotspot)

     """

    # Class config parameters
    CONFIG_KEY_WIFI = 'hotspot_network'

    # Test name configuration keywords
    PARAM_WIFI_BAND = 'wifiband'
    PARAM_2G_BAND = '2g'
    PARAM_5G_BAND = '5g'

    # Iperf waiting time (margin)
    IPERF_MARGIN = 10

    def __init__(self, controllers):

        super().__init__(controllers)

        # Initialize values
        self.client_iperf_helper = None
        self.iperf_server_address = None
        self.network = None

    def setup_class(self):
        """ Executed before the test class is started.

        Configures the Hotspot SSID
        """
        super().setup_class()

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
                wutils.WifiEnums.SSID_KEY: 'Pixel_1030',
                wutils.WifiEnums.PWD_KEY: '1234567890'
            }

        # Both devices need to have a country code in order
        # to use the 5 GHz band.
        wutils.set_wifi_country_code(self.android_devices[0], 'US')
        wutils.set_wifi_country_code(self.android_devices[1], 'US')

    def setup_test(self):
        """Set up test specific parameters or configs.

        """
        super().setup_test()
        wutils.reset_wifi(self.android_devices[1])

    def setup_hotspot(self, connect_client=False):
        """Configure Hotspot and connects client device.

        Args:
            connect_client: Connects (or not) the client device to the Hotspot
        """
        try:
            if self.test_configs.band == self.PARAM_2G_BAND:
                wifi_band_id = WIFI_CONFIG_APBAND_2G
            elif self.test_configs.band == self.PARAM_5G_BAND:
                wifi_band_id = WIFI_CONFIG_APBAND_5G
            else:
                raise ValueError()
        except ValueError:
            self.log.error(
                "The test name has to include parameter {} followed by "
                "either {} or {}.".format(self.PARAM_WIFI_BAND,
                                          self.PARAM_2G_BAND,
                                          self.PARAM_5G_BAND))
            return False

        # Turn WiFi ON for DUT (hotspot) and connect to AP (WiFiSharing)
        # Hotspot needs airplane mode OFF
        self.dut.droid.connectivityToggleAirplaneMode(False)
        time.sleep(2)
        if self.test_configs.wifi_sharing == 'OFF':
            wutils.wifi_toggle_state(self.dut, True)
            time.sleep(2)
        else:
            self.setup_ap_connection(
                self.main_network[self.test_configs.wifi_sharing])

        # Setup tethering on dut
        wutils.start_wifi_tethering(
            self.dut, self.network[wutils.WifiEnums.SSID_KEY],
            self.network[wutils.WifiEnums.PWD_KEY], wifi_band_id)

        # Connect client device to Hotspot
        if connect_client:
            wutils.wifi_connect(
                self.android_devices[1],
                self.network,
                check_connectivity=False)

    def measure_power_and_validate(self, traffic=False):
        """ Measure power and validate.

        Args:
            traffic: Flag indicating whether or not iperf traffic is running
        """
        # Set Screen Status
        if self.test_configs.screen_status == 'OFF':
            self.dut.droid.goToSleepNow()
            self.dut.log.info('Screen is OFF')
        time.sleep(2)

        # Measure power
        result = self.collect_power_data()

        if traffic:
            # Wait for iperf to finish
            time.sleep(self.IPERF_MARGIN + 2)

            # Process iperf results
            self.client_iperf_helper.process_iperf_results(
                self.dut, self.log, self.iperf_servers, self.test_name)

        self.pass_fail_check(result.average_current)

    def power_idle_tethering_test(self):
        """ Start power test when Hotspot is idle

        Starts WiFi tethering. Hotspot is idle and has (or not) a client
        connected. Hotspot device can also share WiFi internet
        """
        attrs = ['screen_status', 'band', 'client_connect', 'wifi_sharing']
        indices = [2, 4, 7, 9]
        self.decode_test_configs(attrs, indices)

        client_connect = self.test_configs.client_connect == 'true'

        # Setup Hotspot with desired config and connect (or not) client
        self.setup_hotspot(client_connect)

        # Measure power and validate
        self.measure_power_and_validate(False)

    def power_traffic_tethering_test(self):
        """ Measure power and throughput during data transmission.

        Starts WiFi tethering and connects the client device.
        The iperf server is always the client device and the iperf client is
        always the DUT (hotspot)

        """
        attrs = [
            'screen_status', 'band', 'traffic_direction', 'traffic_type',
            'wifi_sharing'
        ]
        indices = [2, 4, 5, 6, 8]
        self.decode_test_configs(attrs, indices)

        # Setup Hotspot and connect client device
        self.setup_hotspot(True)

        # Create the iperf config
        iperf_config = {
            'traffic_type': self.test_configs.traffic_type,
            'duration':
            self.mon_duration + self.mon_offset + self.IPERF_MARGIN,
            'server_idx': 0,
            'traffic_direction': self.test_configs.traffic_direction,
            'port': self.iperf_servers[0].port,
            'start_meas_time': 4,
        }

        # Start iperf traffic (dut is the client)
        self.client_iperf_helper = IperfHelper(iperf_config)
        self.iperf_servers[0].start()
        time.sleep(1)
        self.iperf_server_address = wputils.get_phone_ip(
            self.android_devices[1])
        wputils.run_iperf_client_nonblocking(
            self.dut, self.iperf_server_address,
            self.client_iperf_helper.iperf_args)

        # Measure power
        self.measure_power_and_validate(True)

    # Test cases - Idle
    @test_tracker_info(uuid='95438fcb-4a0d-4528-ad70-d723db8e841c')
    def test_screen_OFF_wifiband_2g_standby_client_false_wifiSharing_OFF(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='a2b5ce92-f22f-4442-99be-ff6fa89b1f55')
    def test_screen_OFF_wifiband_2g_standby_client_true_wifiSharing_OFF(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='7c1180c0-6ab9-4890-8dbc-eec8a1de2137')
    def test_screen_OFF_wifiband_2g_standby_client_false_wifiSharing_5g(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='f9befd44-9096-44d2-a2ca-6bc09d274fc9')
    def test_screen_OFF_wifiband_2g_standby_client_true_wifiSharing_5g(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='0a6c1d8d-eb70-4b38-b9ad-511a5c9107ba')
    def test_screen_OFF_wifiband_2g_standby_client_true_wifiSharing_2g(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='e9881c0c-2464-4f0b-8602-152eae3c8206')
    def test_screen_OFF_wifiband_5g_standby_client_false_wifiSharing_OFF(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='4abb2760-ca85-4489-abb4-54d078016f59')
    def test_screen_OFF_wifiband_5g_standby_client_true_wifiSharing_OFF(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='47f99ea5-06aa-4f7a-b4c3-8f397c93be7a')
    def test_screen_OFF_wifiband_5g_standby_client_false_wifiSharing_2g(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='5caba4a7-fe7c-4071-bb5e-df7d2585ed64')
    def test_screen_OFF_wifiband_5g_standby_client_true_wifiSharing_2g(self):
        self.power_idle_tethering_test()

    @test_tracker_info(uuid='27976823-b1be-4928-9e6c-4631ecfff65c')
    def test_screen_OFF_wifiband_5g_standby_client_true_wifiSharing_5g(self):
        self.power_idle_tethering_test()

    # Test cases - Traffic
    @test_tracker_info(uuid='1b0cd04c-afa5-423a-8f8e-1169d403e93a')
    def test_screen_OFF_wifiband_5g_DL_TCP_wifiSharing_OFF(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='9d1a3be2-22f7-4002-a440-9d50fba6a8c0')
    def test_screen_OFF_wifiband_5g_UL_TCP_wifiSharing_OFF(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='3af59262-7f32-4759-82d0-30b89d448b37')
    def test_screen_OFF_wifiband_5g_DL_TCP_wifiSharing_5g(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='069ebf3c-dc8a-4a71-ad62-e10da3be53cc')
    def test_screen_OFF_wifiband_5g_UL_TCP_wifiSharing_5g(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='f9155204-6441-4a50-a77a-24b063db88f7')
    def test_screen_OFF_wifiband_5g_DL_TCP_wifiSharing_2g(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='145f838a-c07c-4102-b767-96be942f8080')
    def test_screen_OFF_wifiband_5g_UL_TCP_wifiSharing_2g(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='a166e4d1-aaec-4d91-abbe-f8ac4288acd7')
    def test_screen_OFF_wifiband_2g_DL_TCP_wifiSharing_OFF(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='2f3e0cf9-25c6-40e2-8eee-ce3af3b39c92')
    def test_screen_OFF_wifiband_2g_UL_TCP_wifiSharing_OFF(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='08470f44-d25c-432c-9133-f1fd5a30c3b0')
    def test_screen_OFF_wifiband_2g_DL_TCP_wifiSharing_2g(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='0d233720-c5a4-4cef-a47a-9fd9b524031b')
    def test_screen_OFF_wifiband_2g_UL_TCP_wifiSharing_2g(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='ec3a0982-4dcb-4175-9c27-fe1205ad4519')
    def test_screen_OFF_wifiband_2g_DL_TCP_wifiSharing_5g(self):
        self.power_traffic_tethering_test()

    @test_tracker_info(uuid='cbd83309-b4dc-44a3-8049-9e0f9275c91e')
    def test_screen_OFF_wifiband_2g_UL_TCP_wifiSharing_5g(self):
        self.power_traffic_tethering_test()
