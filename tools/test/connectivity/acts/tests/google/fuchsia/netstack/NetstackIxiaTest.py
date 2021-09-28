#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
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

from acts import asserts
from acts.controllers.ap_lib import hostapd_ap_preset
from acts.controllers.ap_lib import hostapd_bss_settings
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib import hostapd_security

from acts.test_utils.net.NetstackBaseTest import NetstackBaseTest

from acts.utils import rand_ascii_str


class NetstackIxiaTest(NetstackBaseTest):
    def __init__(self, controllers):
        NetstackBaseTest.__init__(self, controllers)

    def setup_class(self):
        self.log.info('Setup {cls}'.format(cls=type(self)))

        if not self.fuchsia_devices:
            self.log.error(
                "NetstackFuchsiaTest Init: Not enough fuchsia devices.")
        self.log.info("Running testbed setup with one fuchsia devices")
        self.fuchsia_dev = self.fuchsia_devices[0]

        # We want to bring up several 2GHz and 5GHz BSSes.
        wifi_bands = ['2g', '5g']

        # Currently AP_DEFAULT_CHANNEL_2G is 6
        # and AP_DEFAULT_CHANNEL_5G is 36.
        wifi_channels = [
            hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            hostapd_constants.AP_DEFAULT_CHANNEL_5G
        ]

        # Each band will start up an Open BSS (security_mode=None)
        # and a WPA2 BSS (security_mode=hostapd_constants.WPA2_STRING)
        security_modes = [None, hostapd_constants.WPA2_STRING]

        # All secure BSSes will use the same password.
        wifi_password = rand_ascii_str(10)
        self.log.info('Wi-Fi password for this test: {wifi_password}'.format(
            wifi_password=wifi_password))
        hostapd_configs = []
        wifi_interfaces = {}
        bss_settings = {}

        # Build a configuration for each sub-BSSID
        for band_index, wifi_band in enumerate(wifi_bands):
            ssid_name = 'Ixia_{wifi_band}_#{bss_number}_{security_mode}'
            bss_settings[wifi_band] = []

            # Prepare the extra SSIDs.
            for mode_index, security_mode in enumerate(security_modes):

                # Skip the first SSID because we configure that separately.
                # due to the way the APIs work.  This loop is only concerned
                # with the sub-BSSIDs.
                if mode_index == 0:
                    continue

                bss_name = ssid_name.format(wifi_band=wifi_band,
                                            security_mode=security_mode,
                                            bss_number=mode_index + 1)

                bss_setting = hostapd_bss_settings.BssSettings(
                    name=bss_name,
                    ssid=bss_name,
                    security=hostapd_security.Security(
                        security_mode=security_mode, password=wifi_password))
                bss_settings[wifi_band].append(bss_setting)

            # This is the configuration for the first SSID.
            ssid_name = ssid_name.format(wifi_band=wifi_band,
                                         security_mode=security_modes[0],
                                         bss_number=1)

            hostapd_configs.append(
                hostapd_ap_preset.create_ap_preset(
                    profile_name='whirlwind',
                    iface_wlan_2g='wlan0',
                    iface_wlan_5g='wlan1',
                    ssid=ssid_name,
                    channel=wifi_channels[band_index],
                    security=hostapd_security.Security(
                        security_mode=security_modes[0],
                        password=wifi_password),
                    bss_settings=bss_settings[wifi_band]))

            access_point = self.access_points[band_index]

            # Now bring up the AP and track the interfaces we're using for
            # each BSSID.  All BSSIDs are now beaconing.
            wifi_interfaces[wifi_band] = access_point.start_ap(
                hostapd_configs[band_index])

            # Disable DHCP on this Wi-Fi band.
            # Note: This also disables DHCP on each sub-BSSID due to how
            # the APIs are built.
            #
            # We need to do this in order to enable IxANVL testing across
            # Wi-Fi, which needs to configure the IP addresses per-interface
            # on the client device.
            access_point.stop_dhcp()

            # Disable NAT.
            # NAT config in access_point.py is global at the moment, but
            # calling it twice (once per band) won't hurt anything.  This is
            # easier than trying to conditionalize per band.
            #
            # Note that we could make this per-band, but it would require
            # refactoring the access_point.py code that turns on NAT, however
            # if that ever does happen then this code will work as expected
            # without modification.
            #
            # This is also required for IxANVL testing.  NAT would interfere
            # with IxANVL because IxANVL needs to see the raw frames
            # sourcing/sinking from/to the DUT for protocols such as ARP and
            # DHCP, but it also needs the MAC/IP of the source and destination
            # frames and packets to be from the DUT, so we want the AP to act
            # like a bridge for these tests.
            access_point.stop_nat()

        # eth1 is the LAN port, which will always be a part of the bridge.
        bridge_interfaces = ['eth1']

        # This adds each bssid interface to the bridge.
        for wifi_band in wifi_bands:
            for wifi_interface in wifi_interfaces[wifi_band]:
                bridge_interfaces.append(wifi_interface)

        # Each interface can only be a member of 1 bridge, so we're going to use
        # the last access_point object to set the bridge up for all interfaces.
        access_point.create_bridge(bridge_name='ixia_bridge0',
                                   interfaces=bridge_interfaces)

    def setup_test(self):
        pass

    def teardown_test(self):
        pass

    def teardown_class(self):
        self.log.info('Teardown {cls}'.format(cls=type(self)))

        import pdb
        pdb.set_trace()

        for access_point in self.access_points:
            access_point.remove_bridge(bridge_name='ixia_bridge0')

    """Tests"""
    def test_do_nothing(self):
        return True
