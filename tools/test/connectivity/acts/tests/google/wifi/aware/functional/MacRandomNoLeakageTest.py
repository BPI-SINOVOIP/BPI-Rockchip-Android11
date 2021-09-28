#!/usr/bin/python3.4
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

from acts import asserts
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import connectivity_const as cconsts
from acts.test_utils.wifi.aware import aware_const as aconsts
from acts.controllers.ap_lib.hostapd_constants import BAND_2G
from acts.controllers.ap_lib.hostapd_constants import BAND_5G
from acts.test_utils.wifi.aware import aware_test_utils as autils
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi.aware.AwareBaseTest import AwareBaseTest
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from scapy.all import *


class MacRandomNoLeakageTest(AwareBaseTest, WifiBaseTest):
    """Set of tests for Wi-Fi Aware MAC address randomization of NMI (NAN
    management interface) and NDI (NAN data interface)."""

    SERVICE_NAME = "GoogleTestServiceXYZ"
    ping_msg = 'PING'

    AWARE_DEFAULT_CHANNEL_5_BAND = 149
    AWARE_DEFAULT_CHANNEL_24_BAND = 6

    ENCR_TYPE_OPEN = 0
    ENCR_TYPE_PASSPHRASE = 1
    ENCR_TYPE_PMK = 2

    PASSPHRASE = "This is some random passphrase - very very secure!!"
    PMK = "ODU0YjE3YzdmNDJiNWI4NTQ2NDJjNDI3M2VkZTQyZGU="

    def setup_class(self):
        super().setup_class()

        asserts.assert_true(hasattr(self, 'packet_capture'),
                            "Needs packet_capture attribute to support sniffing.")
        self.configure_packet_capture(channel_5g=self.AWARE_DEFAULT_CHANNEL_5_BAND,
                                      channel_2g=self.AWARE_DEFAULT_CHANNEL_24_BAND)

    def setup_test(self):
        WifiBaseTest.setup_test(self)
        AwareBaseTest.setup_test(self)

    def teardown_test(self):
        WifiBaseTest.teardown_test(self)
        AwareBaseTest.teardown_test(self)

    def verify_mac_no_leakage(self, pcap_procs, factory_mac_addresses, mac_addresses):
        # Get 2G and 5G pcaps
        pcap_fname = '%s_%s.pcap' % (pcap_procs[BAND_5G][1], BAND_5G.upper())
        pcap_5g = rdpcap(pcap_fname)

        pcap_fname = '%s_%s.pcap' % (pcap_procs[BAND_2G][1], BAND_2G.upper())
        pcap_2g = rdpcap(pcap_fname)
        pcaps = pcap_5g + pcap_2g

        # Verify factory MAC is not leaked in both 2G and 5G pcaps
        ads = [self.android_devices[0], self.android_devices[1]]
        for i, mac in enumerate(factory_mac_addresses):
            wutils.verify_mac_not_found_in_pcap(ads[i], mac, pcaps)

        # Verify random MACs are being used and in pcaps
        for i, mac in enumerate(mac_addresses):
            wutils.verify_mac_is_found_in_pcap(ads[i], mac, pcaps)

    def transfer_mac_format(self, mac):
        """add ':' to mac String, and transfer to lower case

    Args:
        mac: String of mac without ':'
        @return: Lower case String of mac like "xx:xx:xx:xx:xx:xx"
    """
        return re.sub(r"(?<=\w)(?=(?:\w\w)+$)", ":", mac.lower())

    def start_aware(self, dut, is_publish):
        """Start Aware attach, then start Publish/Subscribe based on role

     Args:
         dut: Aware device
         is_publish: True for Publisher, False for subscriber
         @:return: dict with Aware discovery session info
    """
        aware_id = dut.droid.wifiAwareAttach(True)
        autils.wait_for_event(dut, aconsts.EVENT_CB_ON_ATTACHED)
        event = autils.wait_for_event(dut, aconsts.EVENT_CB_ON_IDENTITY_CHANGED)
        mac = self.transfer_mac_format(event["data"]["mac"])
        dut.log.info("NMI=%s", mac)

        if is_publish:
            config = autils.create_discovery_config(self.SERVICE_NAME,
                                                    aconsts.PUBLISH_TYPE_UNSOLICITED)
            disc_id = dut.droid.wifiAwarePublish(aware_id, config)
            autils.wait_for_event(dut, aconsts.SESSION_CB_ON_PUBLISH_STARTED)
        else:
            config = autils.create_discovery_config(self.SERVICE_NAME,
                                                    aconsts.SUBSCRIBE_TYPE_PASSIVE)
            disc_id = dut.droid.wifiAwareSubscribe(aware_id, config)
            autils.wait_for_event(dut, aconsts.SESSION_CB_ON_SUBSCRIBE_STARTED)
        aware_session = {"awareId": aware_id, "discId": disc_id, "mac": mac}
        return aware_session

    def create_date_path(self, p_dut, pub_session, s_dut, sub_session, sec_type):
        """Create NDP based on the security type(open, PMK, PASSPHRASE), run socket connect

    Args:
        p_dut: Publish device
        p_disc_id: Publisher discovery id
        peer_id_on_pub: peer id on publisher
        s_dut: Subscribe device
        s_disc_id: Subscriber discovery id
        peer_id_on_sub: peer id on subscriber
        sec_type: NDP security type(open, PMK or PASSPHRASE)
        @:return: dict with NDP info
    """

        passphrase = None
        pmk = None

        if sec_type == self.ENCR_TYPE_PASSPHRASE:
            passphrase = self.PASSPHRASE
        if sec_type == self.ENCR_TYPE_PMK:
            pmk = self.PMK

        p_req_key = autils.request_network(
            p_dut,
            p_dut.droid.wifiAwareCreateNetworkSpecifier(pub_session["discId"],
                                                        None,
                                                        passphrase, pmk))
        s_req_key = autils.request_network(
            s_dut,
            s_dut.droid.wifiAwareCreateNetworkSpecifier(sub_session["discId"],
                                                        sub_session["peerId"],
                                                        passphrase, pmk))

        p_net_event_nc = autils.wait_for_event_with_keys(
            p_dut, cconsts.EVENT_NETWORK_CALLBACK, autils.EVENT_NDP_TIMEOUT,
            (cconsts.NETWORK_CB_KEY_EVENT,
             cconsts.NETWORK_CB_CAPABILITIES_CHANGED),
            (cconsts.NETWORK_CB_KEY_ID, p_req_key))
        s_net_event_nc = autils.wait_for_event_with_keys(
            s_dut, cconsts.EVENT_NETWORK_CALLBACK, autils.EVENT_NDP_TIMEOUT,
            (cconsts.NETWORK_CB_KEY_EVENT,
             cconsts.NETWORK_CB_CAPABILITIES_CHANGED),
            (cconsts.NETWORK_CB_KEY_ID, s_req_key))
        p_net_event_lp = autils.wait_for_event_with_keys(
            p_dut, cconsts.EVENT_NETWORK_CALLBACK, autils.EVENT_NDP_TIMEOUT,
            (cconsts.NETWORK_CB_KEY_EVENT,
             cconsts.NETWORK_CB_LINK_PROPERTIES_CHANGED),
            (cconsts.NETWORK_CB_KEY_ID, p_req_key))
        s_net_event_lp = autils.wait_for_event_with_keys(
            s_dut, cconsts.EVENT_NETWORK_CALLBACK, autils.EVENT_NDP_TIMEOUT,
            (cconsts.NETWORK_CB_KEY_EVENT,
             cconsts.NETWORK_CB_LINK_PROPERTIES_CHANGED),
            (cconsts.NETWORK_CB_KEY_ID, s_req_key))

        p_aware_if = p_net_event_lp["data"][cconsts.NETWORK_CB_KEY_INTERFACE_NAME]
        s_aware_if = s_net_event_lp["data"][cconsts.NETWORK_CB_KEY_INTERFACE_NAME]
        p_if_mac = self.transfer_mac_format(autils.get_mac_addr(p_dut, p_aware_if))
        p_dut.log.info("NDI %s=%s", p_aware_if, p_if_mac)
        s_if_mac = self.transfer_mac_format(autils.get_mac_addr(s_dut, s_aware_if))
        s_dut.log.info("NDI %s=%s", s_aware_if, s_if_mac)

        s_ipv6 = p_net_event_nc["data"][aconsts.NET_CAP_IPV6]
        p_ipv6 = s_net_event_nc["data"][aconsts.NET_CAP_IPV6]
        asserts.assert_true(
            autils.verify_socket_connect(p_dut, s_dut, p_ipv6, s_ipv6, 0),
            "Failed socket link with Pub as Server")
        asserts.assert_true(
            autils.verify_socket_connect(s_dut, p_dut, s_ipv6, p_ipv6, 0),
            "Failed socket link with Sub as Server")

        ndp_info = {"pubReqKey": p_req_key, "pubIfMac": p_if_mac,
                    "subReqKey": s_req_key, "subIfMac": s_if_mac}
        return ndp_info

    @test_tracker_info(uuid="c9c66873-a8e0-4830-8baa-ada03223bcef")
    def test_ib_multi_data_path_mac_random_test(self):
        """Verify there is no factory MAC Address leakage during the Aware discovery, NDP creation,
        socket setup and IP service connection."""

        p_dut = self.android_devices[0]
        s_dut = self.android_devices[1]
        mac_addresses = []
        factory_mac_addresses = []
        sec_types = [self.ENCR_TYPE_PMK, self.ENCR_TYPE_PASSPHRASE]

        self.log.info("Starting packet capture")
        pcap_procs = wutils.start_pcap(
            self.packet_capture, 'dual', self.test_name)

        factory_mac_1 = p_dut.droid.wifigetFactorymacAddresses()[0]
        p_dut.log.info("Factory Address: %s", factory_mac_1)
        factory_mac_2 = s_dut.droid.wifigetFactorymacAddresses()[0]
        s_dut.log.info("Factory Address: %s", factory_mac_2)
        factory_mac_addresses.append(factory_mac_1)
        factory_mac_addresses.append(factory_mac_2)

        # Start Aware and exchange messages
        publish_session = self.start_aware(p_dut, True)
        subscribe_session = self.start_aware(s_dut, False)
        mac_addresses.append(publish_session["mac"])
        mac_addresses.append(subscribe_session["mac"])
        discovery_event = autils.wait_for_event(s_dut, aconsts.SESSION_CB_ON_SERVICE_DISCOVERED)
        subscribe_session["peerId"] = discovery_event['data'][aconsts.SESSION_CB_KEY_PEER_ID]

        msg_id = self.get_next_msg_id()
        s_dut.droid.wifiAwareSendMessage(subscribe_session["discId"], subscribe_session["peerId"],
                                         msg_id, self.ping_msg, aconsts.MAX_TX_RETRIES)
        autils.wait_for_event(s_dut, aconsts.SESSION_CB_ON_MESSAGE_SENT)
        pub_rx_msg_event = autils.wait_for_event(p_dut, aconsts.SESSION_CB_ON_MESSAGE_RECEIVED)
        publish_session["peerId"] = pub_rx_msg_event['data'][aconsts.SESSION_CB_KEY_PEER_ID]

        msg_id = self.get_next_msg_id()
        p_dut.droid.wifiAwareSendMessage(publish_session["discId"], publish_session["peerId"],
                                         msg_id, self.ping_msg, aconsts.MAX_TX_RETRIES)
        autils.wait_for_event(p_dut, aconsts.SESSION_CB_ON_MESSAGE_SENT)
        autils.wait_for_event(s_dut, aconsts.SESSION_CB_ON_MESSAGE_RECEIVED)

        # Create Aware NDP
        p_req_keys = []
        s_req_keys = []
        for sec in sec_types:
            ndp_info = self.create_date_path(p_dut, publish_session, s_dut, subscribe_session, sec)
            p_req_keys.append(ndp_info["pubReqKey"])
            s_req_keys.append(ndp_info["subReqKey"])
            mac_addresses.append(ndp_info["pubIfMac"])
            mac_addresses.append(ndp_info["subIfMac"])

        # clean-up
        for p_req_key in p_req_keys:
            p_dut.droid.connectivityUnregisterNetworkCallback(p_req_key)
        for s_req_key in s_req_keys:
            s_dut.droid.connectivityUnregisterNetworkCallback(s_req_key)
        p_dut.droid.wifiAwareDestroyAll()
        s_dut.droid.wifiAwareDestroyAll()

        self.log.info("Stopping packet capture")
        wutils.stop_pcap(self.packet_capture, pcap_procs, False)

        self.verify_mac_no_leakage(pcap_procs, factory_mac_addresses, mac_addresses)
