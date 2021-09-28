#!/usr/bin/env python3.4
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

import itertools
import pprint
import queue
import re
import time

import acts.base_test
import acts.signals as signals
import acts.test_utils.wifi.wifi_test_utils as wutils
import acts.utils

from acts import asserts
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_2G
from acts.test_utils.tel.tel_test_utils import WIFI_CONFIG_APBAND_5G
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from scapy.all import *
from acts.controllers.ap_lib import hostapd_constants

WifiEnums = wutils.WifiEnums

# Default timeout used for reboot, toggle WiFi and Airplane mode,
# for the system to settle down after the operation.
DEFAULT_TIMEOUT = 10
SHORT_TIMEOUT = 5

# Constants for WiFi state change operations.
FORGET = 1
TOGGLE = 2
REBOOT_DUT = 3
REBOOT_AP = 4

# MAC Randomization setting constants.
RANDOMIZATION_NONE = 0
RANDOMIZATION_PERSISTENT = 1


class WifiMacRandomizationTest(WifiBaseTest):
    """Tests for APIs in Android's WifiManager class.

    Test Bed Requirement:
    * Atleast one Android device and atleast two Access Points.
    * Several Wi-Fi networks visible to the device.
    """

    def setup_class(self):
        super().setup_class()

        self.dut = self.android_devices[0]
        self.dut_client = self.android_devices[1]
        wutils.wifi_test_device_init(self.dut)
        wutils.wifi_test_device_init(self.dut_client)
        req_params = ["dbs_supported_models", "roaming_attn"]
        opt_param = [
            "open_network", "reference_networks", "wep_networks"
        ]
        self.unpack_userparams(
            req_param_names=req_params, opt_param_names=opt_param)

        if not hasattr(self, 'packet_capture'):
            raise signals.TestFailure("Needs packet_capture attribute to "
                                      "support sniffing.")
        self.configure_packet_capture()

        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start(wep_network=True,
                                               ap_count=2)
        elif "OpenWrtAP" in self.user_params:
            self.configure_openwrt_ap_and_start(open_network=True,
                                                wpa_network=True,
                                                wep_network=True)

        asserts.assert_true(
            len(self.reference_networks) > 0,
            "Need at least one reference network with psk.")

        # Reboot device to reset factory MAC of wlan1
        self.dut.reboot()
        self.dut_client.reboot()
        time.sleep(DEFAULT_TIMEOUT)
        wutils.wifi_toggle_state(self.dut, True)
        wutils.wifi_toggle_state(self.dut_client, True)
        self.soft_ap_factory_mac = self.get_soft_ap_mac_address()
        self.sta_factory_mac = self.dut.droid.wifigetFactorymacAddresses()[0]

        self.wpapsk_2g = self.reference_networks[0]["2g"]
        self.wpapsk_5g = self.reference_networks[0]["5g"]
        self.wep_2g = self.wep_networks[0]["2g"]
        self.wep_5g = self.wep_networks[0]["5g"]
        self.open_2g = self.open_network[0]["2g"]
        self.open_5g = self.open_network[0]["5g"]

    def setup_test(self):
        super().setup_test()
        for ad in self.android_devices:
            ad.droid.wakeLockAcquireBright()
            ad.droid.wakeUpNow()
            wutils.wifi_toggle_state(ad, True)

    def teardown_test(self):
        super().teardown_test()
        for ad in self.android_devices:
            ad.droid.wakeLockRelease()
            ad.droid.goToSleepNow()
        wutils.reset_wifi(self.dut)
        wutils.reset_wifi(self.dut_client)

    def teardown_class(self):
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]
            del self.user_params["open_network"]
            del self.user_params["wep_networks"]


    """Helper Functions"""


    def get_randomized_mac(self, network):
        """Get the randomized MAC address.

        Args:
            network: dict, network information.

        Returns:
            The randomized MAC address string for the network.

        """
        return self.dut.droid.wifigetRandomizedMacAddress(network)

    def connect_to_network_and_verify_mac_randomization(self, network,
            status=RANDOMIZATION_PERSISTENT):
        """Connect to the given network and verify MAC.

          Args:
              network: dict, the network information.
              status: int, MAC randomization level.

          Returns:
              The randomized MAC addresss string.

        """
        wutils.connect_to_wifi_network(self.dut, network)
        return self.verify_mac_randomization(network, status=status)

    def verify_mac_randomization_and_add_to_list(self, network, mac_list):
        """Connect to a network and populate it's MAC in a reference list,
        that will be used to verify any repeated MAC addresses.

        Args:
            network: dict, the network information.
            mac_list: list of MAC addresss strings.

        """
        rand_mac = self.connect_to_network_and_verify_mac_randomization(
                network)
        if rand_mac in mac_list:
            raise signals.TestFailure('A new Randomized MAC was not generated '
                                      ' for this network %s.' % network)
        mac_list.append(rand_mac)

    def verify_mac_randomization(self, network, status=RANDOMIZATION_PERSISTENT):
        """Get the various types of MAC addresses for the device and verify.

        Args:
            network: dict, the network information.
            status: int, MAC randomization level.

        Returns:
            The randomized MAC address string for the network.

        """
        randomized_mac = self.get_randomized_mac(network)
        default_mac = self.get_sta_mac_address()
        self.log.info("Factory MAC = %s\nRandomized MAC = %s\nDefault MAC = %s" %
              (self.sta_factory_mac, randomized_mac, default_mac))
        message = ('Randomized MAC and Factory MAC are the same. '
                   'Randomized MAC = %s, Factory MAC = %s' % (randomized_mac, self.sta_factory_mac))
        asserts.assert_true(randomized_mac != self.sta_factory_mac, message)
        if status == RANDOMIZATION_NONE:
            asserts.assert_true(default_mac == self.sta_factory_mac, "Connection is not "
                "using Factory MAC as the default MAC.")
        else:
            message = ('Connection is not using randomized MAC as the default MAC. '
                       'Randomized MAC = %s, Deafult MAC = %s' % (randomized_mac, default_mac))
            asserts.assert_true(default_mac == randomized_mac, message)
        return randomized_mac

    def check_mac_persistence(self, network, condition):
        """Check if the MAC is persistent after carrying out specific operations
        like forget WiFi, toggle WiFi, reboot device and AP.

        Args:
            network: dict, The network information.
            condition: int, value to trigger certain  operation on the device.

        Raises:
            TestFaikure is the MAC is not persistent.

        """
        rand_mac1 = self.connect_to_network_and_verify_mac_randomization(network)

        if condition == FORGET:
            wutils.wifi_forget_network(self.dut, network['SSID'])

        elif condition == TOGGLE:
            wutils.wifi_toggle_state(self.dut, False)
            wutils.wifi_toggle_state(self.dut, True)

        elif condition == REBOOT_DUT:
            self.dut.reboot()
            time.sleep(DEFAULT_TIMEOUT)

        elif condition == REBOOT_AP:
            wutils.turn_ap_off(self, 1)
            time.sleep(DEFAULT_TIMEOUT)
            wutils.turn_ap_on(self, 1)
            time.sleep(DEFAULT_TIMEOUT)

        rand_mac2 = self.connect_to_network_and_verify_mac_randomization(network)

        if rand_mac1 != rand_mac2:
            raise signals.TestFailure('Randomized MAC is not persistent after '
                                      'forgetting networ. Old MAC = %s New MAC'
                                      ' = %s' % (rand_mac1, rand_mac2))

    def verify_mac_not_found_in_pcap(self, mac, packets):
        for pkt in packets:
            self.log.debug("Packet Summary = %s" % pkt.summary())
            if mac in pkt.summary():
                raise signals.TestFailure("Caught Factory MAC in packet sniffer"
                                          "Packet = %s Device = %s"
                                           % (pkt.show(), self.dut))

    def verify_mac_is_found_in_pcap(self, mac, packets):
        for pkt in packets:
            self.log.debug("Packet Summary = %s" % pkt.summary())
            if mac in pkt.summary():
                return
        raise signals.TestFailure("Did not find MAC = %s in packet sniffer."
                                  "for device %s" % (mac, self.dut))

    def get_sta_mac_address(self):
        """Gets the current MAC address being used for client mode."""
        out = self.dut.adb.shell("ifconfig wlan0")
        res = re.match(".* HWaddr (\S+).*", out, re.S)
        return res.group(1)

    def get_soft_ap_mac_address(self):
        """Gets the current MAC address being used for SoftAp."""
        if self.dut.model in self.dbs_supported_models:
            out = self.dut.adb.shell("ifconfig wlan1")
            return re.match(".* HWaddr (\S+).*", out, re.S).group(1)
        else:
            return self.get_sta_mac_address()
    """Tests"""


    @test_tracker_info(uuid="2dd0a05e-a318-45a6-81cd-962e098fa242")
    def test_set_mac_randomization_to_none(self):
        self.pcap_procs = wutils.start_pcap(
            self.packet_capture, 'dual', self.test_name)
        network = self.wpapsk_2g
        # Set macRandomizationSetting to RANDOMIZATION_NONE.
        network["macRand"] = RANDOMIZATION_NONE
        self.connect_to_network_and_verify_mac_randomization(network,
            status=RANDOMIZATION_NONE)
        pcap_fname = '%s_%s.pcap' % \
            (self.pcap_procs[hostapd_constants.BAND_2G][1],
             hostapd_constants.BAND_2G.upper())
        time.sleep(SHORT_TIMEOUT)
        wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        packets = rdpcap(pcap_fname)
        self.verify_mac_is_found_in_pcap(self.sta_factory_mac, packets)

    @test_tracker_info(uuid="d9e64202-02d5-421a-967c-42e45f1f7f91")
    def test_mac_randomization_wpapsk(self):
        """Verify MAC randomization for a WPA network.

        Steps:
            1. Connect to WPA network.
            2. Get the Factory, Randomized and Default MACs.
            3. Verify randomized MAC is the default MAC for the device.

        """
        self.connect_to_network_and_verify_mac_randomization(self.wpapsk_2g)

    @test_tracker_info(uuid="b5be7c53-2edf-449e-ba70-a1fb7acf735e")
    def test_mac_randomization_wep(self):
        """Verify MAC randomization for a WEP network.

        Steps:
            1. Connect to WEP network.
            2. Get the Factory, Randomized and Default MACs.
            3. Verify randomized MAC is the default MAC for the device.

        """
        self.connect_to_network_and_verify_mac_randomization(self.wep_2g)

    @test_tracker_info(uuid="f5347ac0-68d5-4882-a58d-1bd0d575503c")
    def test_mac_randomization_open(self):
        """Verify MAC randomization for a open network.

        Steps:
            1. Connect to open network.
            2. Get the Factory, Randomized and Default MACs.
            3. Verify randomized MAC is the default MAC for the device.

        """
        self.connect_to_network_and_verify_mac_randomization(self.open_2g)

    @test_tracker_info(uuid="5d260421-2adf-4ace-b281-3d15aec39b2a")
    def test_persistent_mac_after_forget(self):
        """Check if MAC is persistent after forgetting/adding a network.

        Steps:
            1. Connect to WPA network and get the randomized MAC.
            2. Forget the network.
            3. Connect to the same network again.
            4. Verify randomized MAC has not changed.

        """
        self.check_mac_persistence(self.wpapsk_2g, FORGET)

    @test_tracker_info(uuid="09d40a93-ead2-45ca-9905-14b05fd79f34")
    def test_persistent_mac_after_toggle(self):
        """Check if MAC is persistent after toggling WiFi network.

        Steps:
            1. Connect to WPA network and get the randomized MAC.
            2. Turn WiFi ON/OFF.
            3. Connect to the same network again.
            4. Verify randomized MAC has not changed.

        """
        self.check_mac_persistence(self.wpapsk_2g, TOGGLE)

    @test_tracker_info(uuid="b3aa514f-8562-44e8-bfe0-4ecab9af165b")
    def test_persistent_mac_after_device_reboot(self):
        """Check if MAC is persistent after a device reboot.

        Steps:
            1. Connect to WPA network and get the randomized MAC.
            2. Reboot DUT.
            3. Connect to the same network again.
            4. Verify randomized MAC has not changed.

        """
        self.check_mac_persistence(self.wpapsk_2g, REBOOT_DUT)

    # Disable reboot test for debugging purpose.
    #@test_tracker_info(uuid="82d691a0-22e4-4a3d-9596-e150531fcd34")
    def persistent_mac_after_ap_reboot(self):
        """Check if MAC is persistent after AP reboots itself.

        Steps:
            1. Connect to WPA network and get the randomized MAC.
            2. Reboot AP(basically restart hostapd in our case).
            3. Connect to the same network again.
            4. Verify randomized MAC has not changed.

        """
        self.check_mac_persistence(self.wpapsk_2g, REBOOT_AP)

    @test_tracker_info(uuid="e1f33dbc-808c-4e61-8a4a-3a72c1f63c7e")
    def test_mac_randomization_multiple_networks(self):
        """Connect to multiple networks and verify same MAC.

        Steps:
            1. Connect to network A, get randomizd MAC.
            2. Conenct to network B, get randomized MAC.
            3. Connect back to network A and verify same MAC.
            4. Connect back to network B and verify same MAC.

        """
        mac_list = list()

        # Connect to two different networks and get randomized MAC addresses.
        self.verify_mac_randomization_and_add_to_list(self.wpapsk_2g, mac_list)
        self.verify_mac_randomization_and_add_to_list(self.open_2g, mac_list)

        # Connect to the previous network and check MAC is persistent.
        mac_wpapsk = self.connect_to_network_and_verify_mac_randomization(
                self.wpapsk_2g)
        msg = ('Randomized MAC is not persistent for this network %s. Old MAC = '
               '%s \nNew MAC = %s')
        if mac_wpapsk != mac_list[0]:
            raise signals.TestFailure(msg % (self.wpapsk_5g, mac_list[0], mac_wpapsk))
        mac_open = self.connect_to_network_and_verify_mac_randomization(
                self.open_2g)
        if mac_open != mac_list[1]:
            raise signals.TestFailure(msg %(self.open_5g, mac_list[1], mac_open))

    @test_tracker_info(uuid="edb5a0e5-7f3b-4147-b1d3-48ad7ad9799e")
    def test_mac_randomization_differnet_APs(self):
        """Verify randomization using two different APs.

        Steps:
            1. Connect to network A on AP1, get the randomized MAC.
            2. Connect to network B on AP2, get the randomized MAC.
            3. Veirfy the two MACs are different.

        """
        ap1 = self.wpapsk_2g
        ap2 = self.reference_networks[1]["5g"]
        mac_ap1 = self.connect_to_network_and_verify_mac_randomization(ap1)
        mac_ap2 = self.connect_to_network_and_verify_mac_randomization(ap2)
        if mac_ap1 == mac_ap2:
            raise signals.TestFailure("Same MAC address was generated for both "
                                      "APs: %s" % mac_ap1)

    @test_tracker_info(uuid="b815e9ce-bccd-4fc3-9774-1e1bc123a2a8")
    def test_mac_randomization_ap_sta(self):
        """Bring up STA and softAP and verify MAC randomization.

        Steps:
            1. Connect to a network and get randomized MAC.
            2. Bring up softAP on the DUT.
            3. Connect to softAP network on the client and get MAC.
            4. Verify AP and STA use different randomized MACs.
            5. Find the channel of the SoftAp network.
            6. Configure sniffer on that channel.
            7. Verify the factory MAC is not leaked.

        """
        wutils.set_wifi_country_code(self.dut, wutils.WifiEnums.CountryCode.US)
        wutils.set_wifi_country_code(self.dut_client, wutils.WifiEnums.CountryCode.US)
        mac_sta = self.connect_to_network_and_verify_mac_randomization(
                self.wpapsk_2g)
        softap = wutils.start_softap_and_verify(self, WIFI_CONFIG_APBAND_2G)
        wutils.connect_to_wifi_network(self.dut_client, softap)
        softap_info = self.dut_client.droid.wifiGetConnectionInfo()
        mac_ap = softap_info['mac_address']
        if mac_sta == mac_ap:
            raise signals.TestFailure("Same MAC address was used for both "
                                      "AP and STA: %s" % mac_sta)

        # Verify SoftAp MAC is randomized
        softap_mac = self.get_soft_ap_mac_address()
        message = ('Randomized SoftAp MAC and Factory SoftAp MAC are the same. '
                   'Randomized SoftAp MAC = %s, Factory SoftAp MAC = %s'
                   % (softap_mac, self.soft_ap_factory_mac))
        asserts.assert_true(softap_mac != self.soft_ap_factory_mac, message)

        softap_channel = hostapd_constants.CHANNEL_MAP[softap_info['frequency']]
        self.log.info("softap_channel = %s\n" % (softap_channel))
        result = self.packet_capture.configure_monitor_mode(
            hostapd_constants.BAND_2G, softap_channel)
        if not result:
            raise ValueError("Failed to configure channel for 2G band")
        self.pcap_procs = wutils.start_pcap(
            self.packet_capture, 'dual', self.test_name)
        # re-connect to the softAp network after sniffer is started
        wutils.connect_to_wifi_network(self.dut_client, self.wpapsk_2g)
        wutils.connect_to_wifi_network(self.dut_client, softap)
        time.sleep(SHORT_TIMEOUT)
        wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        pcap_fname = '%s_%s.pcap' % \
            (self.pcap_procs[hostapd_constants.BAND_2G][1],
             hostapd_constants.BAND_2G.upper())
        packets = rdpcap(pcap_fname)
        self.verify_mac_not_found_in_pcap(self.soft_ap_factory_mac, packets)
        self.verify_mac_not_found_in_pcap(self.sta_factory_mac, packets)
        self.verify_mac_is_found_in_pcap(softap_mac, packets)
        self.verify_mac_is_found_in_pcap(self.get_sta_mac_address(), packets)

    @test_tracker_info(uuid="3ca3f911-29f1-41fb-b836-4d25eac1669f")
    def test_roaming_mac_randomization(self):
        """test MAC randomization in the roaming scenario.

        Steps:
            1. Connect to network A on AP1, get randomized MAC.
            2. Set AP1 to MAX attenuation so that we roam to AP2.
            3. Wait for device to roam to AP2 and get randomized MAC.
            4. Veirfy that the device uses same AMC for both APs.

        """
        AP1_network = self.reference_networks[0]["5g"]
        AP2_network = self.reference_networks[1]["5g"]
        wutils.set_attns(self.attenuators, "AP1_on_AP2_off", self.roaming_attn)
        mac_before_roam = self.connect_to_network_and_verify_mac_randomization(
                AP1_network)
        wutils.trigger_roaming_and_validate(self.dut, self.attenuators,
                "AP1_off_AP2_on", AP2_network, self.roaming_attn)
        mac_after_roam = self.get_randomized_mac(AP2_network)
        if mac_after_roam != mac_before_roam:
            raise signals.TestFailure("Randomized MAC address changed after "
                   "roaming from AP1 to AP2.\nMAC before roam = %s\nMAC after "
                   "roam = %s" %(mac_before_roam, mac_after_roam))
        wutils.trigger_roaming_and_validate(self.dut, self.attenuators,
                "AP1_on_AP2_off", AP1_network, self.roaming_attn)
        mac_after_roam = self.get_randomized_mac(AP1_network)
        if mac_after_roam != mac_before_roam:
            raise signals.TestFailure("Randomized MAC address changed after "
                   "roaming from AP1 to AP2.\nMAC before roam = %s\nMAC after "
                   "roam = %s" %(mac_before_roam, mac_after_roam))

    @test_tracker_info(uuid="17b12f1a-7c62-4188-b5a5-52d7a0bb7849")
    def test_check_mac_sta_with_link_probe(self):
        """Test to ensure Factory MAC is not exposed, using sniffer data.

        Steps:
            1. Configure and start the sniffer on 5GHz band.
            2. Connect to 5GHz network.
            3. Send link probes.
            4. Stop the sniffer.
            5. Invoke scapy to read the .pcap file.
            6. Read each packet summary and make sure Factory MAC is not used.

        """
        self.pcap_procs = wutils.start_pcap(
            self.packet_capture, 'dual', self.test_name)
        time.sleep(SHORT_TIMEOUT)
        network = self.wpapsk_5g
        rand_mac = self.connect_to_network_and_verify_mac_randomization(network)
        wutils.send_link_probes(self.dut, 3, 3)
        pcap_fname = '%s_%s.pcap' % \
            (self.pcap_procs[hostapd_constants.BAND_5G][1],
             hostapd_constants.BAND_5G.upper())
        wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        time.sleep(SHORT_TIMEOUT)
        packets = rdpcap(pcap_fname)
        self.verify_mac_not_found_in_pcap(self.sta_factory_mac, packets)
        self.verify_mac_is_found_in_pcap(self.get_sta_mac_address(), packets)

    @test_tracker_info(uuid="1c2cc0fd-a340-40c4-b679-6acc5f526451")
    def test_check_mac_in_wifi_scan(self):
        """Test to ensure Factory MAC is not exposed, in Wi-Fi scans

        Steps:
          1. Configure and start the sniffer on both bands.
          2. Perform a full scan.
          3. Stop the sniffer.
          4. Invoke scapy to read the .pcap file.
          5. Read each packet summary and make sure Factory MAC is not used.

        """
        self.pcap_procs = wutils.start_pcap(
            self.packet_capture, 'dual', self.test_name)
        wutils.start_wifi_connection_scan(self.dut)
        time.sleep(SHORT_TIMEOUT)
        wutils.stop_pcap(self.packet_capture, self.pcap_procs, False)
        pcap_fname = '%s_%s.pcap' % \
            (self.pcap_procs[hostapd_constants.BAND_2G][1],
             hostapd_constants.BAND_2G.upper())
        packets = rdpcap(pcap_fname)
        self.verify_mac_not_found_in_pcap(self.sta_factory_mac, packets)
