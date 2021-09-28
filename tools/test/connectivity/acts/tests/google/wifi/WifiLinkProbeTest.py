#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import time

from acts import asserts
from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
import acts.test_utils.wifi.wifi_test_utils as wutils

WifiEnums = wutils.WifiEnums
NUM_LINK_PROBES = 8
PROBE_DELAY_SEC = 3
ATTENUATION = 40


class WifiLinkProbeTest(WifiBaseTest):
    """
    Tests sending 802.11 Probe Request frames to the currently connected AP to
    determine if the uplink to the AP is working.

    Test Bed Requirements:
    * One Android Device
    * One Wi-Fi network visible to the device, with an attenuator
    """

    def setup_class(self):
        super().setup_class()

        self.dut = self.android_devices[0]
        wutils.wifi_test_device_init(self.dut)
        self.unpack_userparams(req_param_names=[],
                               opt_param_names=["reference_networks"])

        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start()
        elif "OpenWrtAP" in self.user_params:
            self.configure_openwrt_ap_and_start(wpa_network=True)
        self.configure_packet_capture()

        asserts.assert_true(len(self.reference_networks) > 0,
                            "Need at least one reference network with psk.")
        self.attenuators = wutils.group_attenuators(self.attenuators)

    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()
        wutils.wifi_toggle_state(self.dut, True)
        self.attenuators[0].set_atten(0)
        self.attenuators[1].set_atten(0)
        self.pcap_procs = wutils.start_pcap(
            self.packet_capture, 'dual', self.test_name)

    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        wutils.reset_wifi(self.dut)

    def on_pass(self, test_name, begin_time):
        wutils.stop_pcap(self.packet_capture, self.pcap_procs, True)

    def on_fail(self, test_name, begin_time):
        wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        super().on_fail(test_name, begin_time)

    def teardown_class(self):
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]

    # HELPER METHODS

    def _test_link_probe_does_not_crash_device(self, network):
        """
        Connect to a network, send link probes, and verify that the device did
        not crash. Also verify that at least one link probe succeeded.

        Steps:
        1. Connect to a network.
        2. Send a few link probes.
        3. Verify that at least one link probe succeeded.
        4. Ensure that the device did not crash (by checking that it is
           connected to the expected network).
        """
        wutils.wifi_connect(self.dut, network, num_of_tries=3)

        results = wutils.send_link_probes(
            self.dut, NUM_LINK_PROBES, PROBE_DELAY_SEC)

        asserts.assert_true(any(result.is_success for result in results),
                            "Expect at least 1 probe success: " + str(results))

        wifi_info = self.dut.droid.wifiGetConnectionInfo()
        expected = network[WifiEnums.SSID_KEY]
        actual = wifi_info[WifiEnums.SSID_KEY]
        asserts.assert_equal(
            expected, actual,
            "Device did not remain connected after sending link probes!")

    def _test_link_probe_ap_attenuated(self, network):
        """
        Connect to a network, significantly attenuate the signal, and verify
        that the device did not crash.

        Steps:
        1. Connect to a network.
        2. Attenuate the signal.
        3. Send a few link probes.
        4. Stop attenuating the signal.
        5. Ensure that the device did not crash (by checking that it is
           connected to the expected network).
        """
        wutils.wifi_connect(self.dut, network, num_of_tries=3)
        self.attenuators[0].set_atten(ATTENUATION)

        wutils.send_link_probes(self.dut, NUM_LINK_PROBES, PROBE_DELAY_SEC)

        # we cannot assert for failed link probe when attenuated, this would
        # depend too much on the attenuator setup => too flaky

        self.attenuators[0].set_atten(0)
        time.sleep(PROBE_DELAY_SEC * 3)
        wifi_info = self.dut.droid.wifiGetConnectionInfo()
        expected = network[WifiEnums.SSID_KEY]
        actual = wifi_info[WifiEnums.SSID_KEY]
        asserts.assert_equal(
            expected, actual,
            "Device did not remain connected after sending link probes!")

    # TEST METHODS

    @test_tracker_info(uuid='2afd309b-6bf3-4de4-9d8a-e4d35354a2cb')
    def test_link_probe_does_not_crash_device_2g(self):
        network = self.reference_networks[0]["2g"]
        self._test_link_probe_does_not_crash_device(network)

    @test_tracker_info(uuid='69417a6d-7090-4dd0-81ad-55fa3f12b7b1')
    def test_link_probe_does_not_crash_device_5g(self):
        network = self.reference_networks[0]["5g"]
        self._test_link_probe_does_not_crash_device(network)

    @test_tracker_info(uuid='54b8ffaa-c305-4772-928d-03342c51122d')
    def test_link_probe_ap_attenuated_2g(self):
        network = self.reference_networks[0]["2g"]
        self._test_link_probe_ap_attenuated(network)

    @test_tracker_info(uuid='54e29fa4-ff44-4aad-8999-676b361cacf4')
    def test_link_probe_ap_attenuated_5g(self):
        network = self.reference_networks[0]["5g"]
        self._test_link_probe_ap_attenuated(network)

