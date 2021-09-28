#!/usr/bin/env python3
#
#   Copyright 2017 - Google
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
"""
    Base Class for Defining Common WiFi Test Functionality
"""

import copy
import itertools
import os
import time

import acts.controllers.access_point as ap

from acts import asserts
from acts import signals
from acts import utils
from acts.base_test import BaseTestClass
from acts.signals import TestSignal
from acts.controllers import android_device
from acts.controllers.access_point import AccessPoint
from acts.controllers.ap_lib import hostapd_ap_preset
from acts.controllers.ap_lib import hostapd_bss_settings
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib import hostapd_security
from acts.keys import Config
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.wifi import wifi_test_utils as wutils

AP_1 = 0
AP_2 = 1
MAX_AP_COUNT = 2


class WifiBaseTest(BaseTestClass):

    def __init__(self, configs):
        super().__init__(configs)
        self.enable_packet_log = False
        self.packet_log_2g = hostapd_constants.AP_DEFAULT_CHANNEL_2G
        self.packet_log_5g = hostapd_constants.AP_DEFAULT_CHANNEL_5G

    def setup_class(self):
        if hasattr(self, 'attenuators') and self.attenuators:
            for attenuator in self.attenuators:
                attenuator.set_atten(0)
        opt_param = ["pixel_models", "cnss_diag_file"]
        self.unpack_userparams(opt_param_names=opt_param)
        if hasattr(self, "cnss_diag_file"):
            if isinstance(self.cnss_diag_file, list):
                self.cnss_diag_file = self.cnss_diag_file[0]
            if not os.path.isfile(self.cnss_diag_file):
                self.cnss_diag_file = os.path.join(
                    self.user_params[Config.key_config_path.value],
                    self.cnss_diag_file)
        if self.enable_packet_log and hasattr(self, "packet_capture"):
            self.packet_logger = self.packet_capture[0]
            self.packet_logger.configure_monitor_mode("2G", self.packet_log_2g)
            self.packet_logger.configure_monitor_mode("5G", self.packet_log_5g)

    def setup_test(self):
        if (hasattr(self, "android_devices") and
                hasattr(self, "cnss_diag_file") and
                hasattr(self, "pixel_models")):
            wutils.start_cnss_diags(
                self.android_devices, self.cnss_diag_file, self.pixel_models)
        self.tcpdump_proc = []
        if hasattr(self, "android_devices"):
            for ad in self.android_devices:
                proc = nutils.start_tcpdump(ad, self.test_name)
                self.tcpdump_proc.append((ad, proc))
        if hasattr(self, "packet_logger"):
            self.packet_log_pid = wutils.start_pcap(
                    self.packet_logger, 'dual', self.test_name)

    def teardown_test(self):
        if (hasattr(self, "android_devices") and
                hasattr(self, "cnss_diag_file") and
                hasattr(self, "pixel_models")):
            wutils.stop_cnss_diags(self.android_devices, self.pixel_models)
        for proc in self.tcpdump_proc:
            nutils.stop_tcpdump(
                    proc[0], proc[1], self.test_name, pull_dump=False)
        self.tcpdump_proc = []
        if hasattr(self, "packet_logger") and self.packet_log_pid:
            wutils.stop_pcap(
                    self.packet_logger, self.packet_log_pid, test_status=True)
            self.packet_log_pid = {}

    def on_fail(self, test_name, begin_time):
        if hasattr(self, "android_devices"):
            for ad in self.android_devices:
                ad.take_bug_report(test_name, begin_time)
                ad.cat_adb_log(test_name, begin_time)
                wutils.get_ssrdumps(ad)
            if (hasattr(self, "cnss_diag_file") and
                    hasattr(self, "pixel_models")):
                wutils.stop_cnss_diags(self.android_devices, self.pixel_models)
                for ad in self.android_devices:
                    wutils.get_cnss_diag_log(ad)
        for proc in self.tcpdump_proc:
            nutils.stop_tcpdump(proc[0], proc[1], self.test_name)
        self.tcpdump_proc = []
        if hasattr(self, "packet_logger") and self.packet_log_pid:
            wutils.stop_pcap(
                    self.packet_logger, self.packet_log_pid, test_status=False)
            self.packet_log_pid = {}

    def get_psk_network(
            self,
            mirror_ap,
            reference_networks,
            hidden=False,
            same_ssid=False,
            security_mode=hostapd_constants.WPA2_STRING,
            ssid_length_2g=hostapd_constants.AP_SSID_LENGTH_2G,
            ssid_length_5g=hostapd_constants.AP_SSID_LENGTH_5G,
            passphrase_length_2g=hostapd_constants.AP_PASSPHRASE_LENGTH_2G,
            passphrase_length_5g=hostapd_constants.AP_PASSPHRASE_LENGTH_5G):
        """Generates SSID and passphrase for a WPA2 network using random
           generator.

           Args:
               mirror_ap: Boolean, determines if both APs use the same hostapd
                          config or different configs.
               reference_networks: List of PSK networks.
               same_ssid: Boolean, determines if both bands on AP use the same
                          SSID.
               ssid_length_2gecond AP Int, number of characters to use for 2G SSID.
               ssid_length_5g: Int, number of characters to use for 5G SSID.
               passphrase_length_2g: Int, length of password for 2G network.
               passphrase_length_5g: Int, length of password for 5G network.

           Returns: A dict of 2G and 5G network lists for hostapd configuration.

        """
        network_dict_2g = {}
        network_dict_5g = {}
        ref_5g_security = security_mode
        ref_2g_security = security_mode

        if same_ssid:
            ref_2g_ssid = 'xg_%s' % utils.rand_ascii_str(ssid_length_2g)
            ref_5g_ssid = ref_2g_ssid

            ref_2g_passphrase = utils.rand_ascii_str(passphrase_length_2g)
            ref_5g_passphrase = ref_2g_passphrase

        else:
            ref_2g_ssid = '2g_%s' % utils.rand_ascii_str(ssid_length_2g)
            ref_2g_passphrase = utils.rand_ascii_str(passphrase_length_2g)

            ref_5g_ssid = '5g_%s' % utils.rand_ascii_str(ssid_length_5g)
            ref_5g_passphrase = utils.rand_ascii_str(passphrase_length_5g)

        network_dict_2g = {
            "SSID": ref_2g_ssid,
            "security": ref_2g_security,
            "password": ref_2g_passphrase,
            "hiddenSSID": hidden
        }

        network_dict_5g = {
            "SSID": ref_5g_ssid,
            "security": ref_5g_security,
            "password": ref_5g_passphrase,
            "hiddenSSID": hidden
        }

        ap = 0
        for ap in range(MAX_AP_COUNT):
            reference_networks.append({
                "2g": copy.copy(network_dict_2g),
                "5g": copy.copy(network_dict_5g)
            })
            if not mirror_ap:
                break
        return {"2g": network_dict_2g, "5g": network_dict_5g}

    def get_open_network(self,
                         mirror_ap,
                         open_network,
                         hidden=False,
                         same_ssid=False,
                         ssid_length_2g=hostapd_constants.AP_SSID_LENGTH_2G,
                         ssid_length_5g=hostapd_constants.AP_SSID_LENGTH_5G,
                         security_mode='none'):
        """Generates SSIDs for a open network using a random generator.

        Args:
            mirror_ap: Boolean, determines if both APs use the same hostapd
                       config or different configs.
            open_network: List of open networks.
            same_ssid: Boolean, determines if both bands on AP use the same
                       SSID.
            ssid_length_2g: Int, number of characters to use for 2G SSID.
            ssid_length_5g: Int, number of characters to use for 5G SSID.
            security_mode: 'none' for open and 'OWE' for WPA3 OWE.

        Returns: A dict of 2G and 5G network lists for hostapd configuration.

        """
        network_dict_2g = {}
        network_dict_5g = {}

        if same_ssid:
            open_2g_ssid = 'xg_%s' % utils.rand_ascii_str(ssid_length_2g)
            open_5g_ssid = open_2g_ssid

        else:
            open_2g_ssid = '2g_%s' % utils.rand_ascii_str(ssid_length_2g)
            open_5g_ssid = '5g_%s' % utils.rand_ascii_str(ssid_length_5g)

        network_dict_2g = {
            "SSID": open_2g_ssid,
            "security": security_mode,
            "hiddenSSID": hidden
        }

        network_dict_5g = {
            "SSID": open_5g_ssid,
            "security": security_mode,
            "hiddenSSID": hidden
        }

        ap = 0
        for ap in range(MAX_AP_COUNT):
            open_network.append({
                "2g": copy.copy(network_dict_2g),
                "5g": copy.copy(network_dict_5g)
            })
            if not mirror_ap:
                break
        return {"2g": network_dict_2g, "5g": network_dict_5g}

    def get_wep_network(
            self,
            mirror_ap,
            networks,
            hidden=False,
            same_ssid=False,
            ssid_length_2g=hostapd_constants.AP_SSID_LENGTH_2G,
            ssid_length_5g=hostapd_constants.AP_SSID_LENGTH_5G,
            passphrase_length_2g=hostapd_constants.AP_PASSPHRASE_LENGTH_2G,
            passphrase_length_5g=hostapd_constants.AP_PASSPHRASE_LENGTH_5G):
        """Generates SSID and passphrase for a WEP network using random
           generator.

           Args:
               mirror_ap: Boolean, determines if both APs use the same hostapd
                          config or different configs.
               networks: List of WEP networks.
               same_ssid: Boolean, determines if both bands on AP use the same
                          SSID.
               ssid_length_2gecond AP Int, number of characters to use for 2G SSID.
               ssid_length_5g: Int, number of characters to use for 5G SSID.
               passphrase_length_2g: Int, length of password for 2G network.
               passphrase_length_5g: Int, length of password for 5G network.

           Returns: A dict of 2G and 5G network lists for hostapd configuration.

        """
        network_dict_2g = {}
        network_dict_5g = {}
        ref_5g_security = hostapd_constants.WEP_STRING
        ref_2g_security = hostapd_constants.WEP_STRING

        if same_ssid:
            ref_2g_ssid = 'xg_%s' % utils.rand_ascii_str(ssid_length_2g)
            ref_5g_ssid = ref_2g_ssid

            ref_2g_passphrase = utils.rand_hex_str(passphrase_length_2g)
            ref_5g_passphrase = ref_2g_passphrase

        else:
            ref_2g_ssid = '2g_%s' % utils.rand_ascii_str(ssid_length_2g)
            ref_2g_passphrase = utils.rand_hex_str(passphrase_length_2g)

            ref_5g_ssid = '5g_%s' % utils.rand_ascii_str(ssid_length_5g)
            ref_5g_passphrase = utils.rand_hex_str(passphrase_length_5g)

        network_dict_2g = {
            "SSID": ref_2g_ssid,
            "security": ref_2g_security,
            "wepKeys": [ref_2g_passphrase] * 4,
            "hiddenSSID": hidden
        }

        network_dict_5g = {
            "SSID": ref_5g_ssid,
            "security": ref_5g_security,
            "wepKeys": [ref_2g_passphrase] * 4,
            "hiddenSSID": hidden
        }

        ap = 0
        for ap in range(MAX_AP_COUNT):
            networks.append({
                "2g": copy.copy(network_dict_2g),
                "5g": copy.copy(network_dict_5g)
            })
            if not mirror_ap:
                break
        return {"2g": network_dict_2g, "5g": network_dict_5g}

    def update_bssid(self, ap_instance, ap, network, band):
        """Get bssid and update network dictionary.

        Args:
            ap_instance: Accesspoint index that was configured.
            ap: Accesspoint object corresponding to ap_instance.
            network: Network dictionary.
            band: Wifi networks' band.

        """
        bssid = ap.get_bssid_from_ssid(network["SSID"], band)

        if network["security"] == hostapd_constants.WPA2_STRING:
            # TODO:(bamahadev) Change all occurances of reference_networks
            # to wpa_networks.
            self.reference_networks[ap_instance][band]["bssid"] = bssid
        if network["security"] == hostapd_constants.WPA_STRING:
            self.wpa_networks[ap_instance][band]["bssid"] = bssid
        if network["security"] == hostapd_constants.WEP_STRING:
            self.wep_networks[ap_instance][band]["bssid"] = bssid
        if network["security"] == hostapd_constants.ENT_STRING:
            if "bssid" not in self.ent_networks[ap_instance][band]:
                self.ent_networks[ap_instance][band]["bssid"] = bssid
            else:
                self.ent_networks_pwd[ap_instance][band]["bssid"] = bssid
        if network["security"] == 'none':
            self.open_network[ap_instance][band]["bssid"] = bssid

    def populate_bssid(self, ap_instance, ap, networks_5g, networks_2g):
        """Get bssid for a given SSID and add it to the network dictionary.

        Args:
            ap_instance: Accesspoint index that was configured.
            ap: Accesspoint object corresponding to ap_instance.
            networks_5g: List of 5g networks configured on the APs.
            networks_2g: List of 2g networks configured on the APs.

        """

        if not (networks_5g or networks_2g):
            return

        for network in networks_5g:
            if 'channel' in network:
                continue
            self.update_bssid(ap_instance, ap, network,
                hostapd_constants.BAND_5G)

        for network in networks_2g:
            if 'channel' in network:
                continue
            self.update_bssid(ap_instance, ap, network,
                hostapd_constants.BAND_2G)

    def configure_openwrt_ap_and_start(
            self,
            channel_5g=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            channel_2g=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid_length_2g=hostapd_constants.AP_SSID_LENGTH_2G,
            passphrase_length_2g=hostapd_constants.AP_PASSPHRASE_LENGTH_2G,
            ssid_length_5g=hostapd_constants.AP_SSID_LENGTH_5G,
            passphrase_length_5g=hostapd_constants.AP_PASSPHRASE_LENGTH_5G,
            mirror_ap=False,
            hidden=False,
            same_ssid=False,
            open_network=False,
            wpa1_network=False,
            wpa_network=False,
            wep_network=False,
            ent_network=False,
            ent_network_pwd=False,
            owe_network=False,
            sae_network=False,
            radius_conf_2g=None,
            radius_conf_5g=None,
            radius_conf_pwd=None,
            ap_count=1):
        """Create, configure and start OpenWrt AP.

        Args:
            channel_5g: 5G channel to configure.
            channel_2g: 2G channel to configure.
            ssid_length_2g: Int, number of characters to use for 2G SSID.
            passphrase_length_2g: Int, length of password for 2G network.
            ssid_length_5g: Int, number of characters to use for 5G SSID.
            passphrase_length_5g: Int, length of password for 5G network.
            same_ssid: Boolean, determines if both bands on AP use the same SSID.
            open_network: Boolean, to check if open network should be configured.
            wpa_network: Boolean, to check if wpa network should be configured.
            wep_network: Boolean, to check if wep network should be configured.
            ent_network: Boolean, to check if ent network should be configured.
            ent_network_pwd: Boolean, to check if ent pwd network should be configured.
            owe_network: Boolean, to check if owe network should be configured.
            sae_network: Boolean, to check if sae network should be configured.
            radius_conf_2g: dictionary with enterprise radius server details.
            radius_conf_5g: dictionary with enterprise radius server details.
            radius_conf_pwd: dictionary with enterprise radiuse server details.
            ap_count: APs to configure.
        """
        if mirror_ap and ap_count == 1:
             raise ValueError("ap_count cannot be 1 if mirror_ap is True.")

        self.reference_networks = []
        self.wpa1_networks = []
        self.wpa_networks = []
        self.wep_networks = []
        self.ent_networks = []
        self.ent_networks_pwd = []
        self.open_network = []
        self.owe_networks = []
        self.sae_networks = []
        self.bssid_map = []
        for i in range(ap_count):
            network_list = []
            if wpa1_network:
                wpa1_dict = self.get_psk_network(mirror_ap,
                                                 self.wpa1_networks,
                                                 hidden, same_ssid,
                                                 ssid_length_2g, ssid_length_5g,
                                                 passphrase_length_2g,
                                                 passphrase_length_5g)
                wpa1_dict[hostapd_constants.BAND_2G]["security"] = "psk"
                wpa1_dict[hostapd_constants.BAND_5G]["security"] = "psk"
                self.wpa1_networks.append(wpa1_dict)
                network_list.append(wpa1_dict)
            if wpa_network:
                wpa_dict = self.get_psk_network(mirror_ap,
                                                self.reference_networks,
                                                hidden,
                                                same_ssid,
                                                ssid_length_2g,
                                                ssid_length_5g,
                                                passphrase_length_2g,
                                                passphrase_length_5g)
                wpa_dict[hostapd_constants.BAND_2G]["security"] = "psk2"
                wpa_dict[hostapd_constants.BAND_5G]["security"] = "psk2"
                self.wpa_networks.append(wpa_dict)
                network_list.append(wpa_dict)
            if wep_network:
                wep_dict = self.get_wep_network(mirror_ap,
                                                self.wep_networks,
                                                hidden,
                                                same_ssid,
                                                ssid_length_2g,
                                                ssid_length_5g)
                network_list.append(wep_dict)
            if ent_network:
                ent_dict = self.get_open_network(mirror_ap,
                                                 self.ent_networks,
                                                 hidden,
                                                 same_ssid,
                                                 ssid_length_2g,
                                                 ssid_length_5g)
                ent_dict["2g"]["security"] = "wpa2"
                ent_dict["2g"].update(radius_conf_2g)
                ent_dict["5g"]["security"] = "wpa2"
                ent_dict["5g"].update(radius_conf_5g)
                network_list.append(ent_dict)
            if ent_network_pwd:
                ent_pwd_dict = self.get_open_network(mirror_ap,
                                                     self.ent_networks_pwd,
                                                     hidden,
                                                     same_ssid,
                                                     ssid_length_2g,
                                                     ssid_length_5g)
                ent_pwd_dict["2g"]["security"] = "wpa2"
                ent_pwd_dict["2g"].update(radius_conf_pwd)
                ent_pwd_dict["5g"]["security"] = "wpa2"
                ent_pwd_dict["5g"].update(radius_conf_pwd)
                network_list.append(ent_pwd_dict)
            if open_network:
                open_dict = self.get_open_network(mirror_ap,
                                                  self.open_network,
                                                  hidden,
                                                  same_ssid,
                                                  ssid_length_2g,
                                                  ssid_length_5g)
                network_list.append(open_dict)
            if owe_network:
                owe_dict = self.get_open_network(mirror_ap,
                                                 self.owe_networks,
                                                 hidden,
                                                 same_ssid,
                                                 ssid_length_2g,
                                                 ssid_length_5g,
                                                 "OWE")
                owe_dict[hostapd_constants.BAND_2G]["security"] = "owe"
                owe_dict[hostapd_constants.BAND_5G]["security"] = "owe"
                network_list.append(owe_dict)
            if sae_network:
                sae_dict = self.get_psk_network(mirror_ap,
                                                self.sae_networks,
                                                hidden,
                                                same_ssid,
                                                hostapd_constants.WPA3_KEY_MGMT,
                                                ssid_length_2g,
                                                ssid_length_5g,
                                                passphrase_length_2g,
                                                passphrase_length_5g)
                sae_dict[hostapd_constants.BAND_2G]["security"] = "sae"
                sae_dict[hostapd_constants.BAND_5G]["security"] = "sae"
                network_list.append(sae_dict)
            self.access_points[i].configure_ap(network_list,
                                               channel_2g,
                                               channel_5g)
            self.access_points[i].start_ap()
            self.bssid_map.append(
                self.access_points[i].get_bssids_for_wifi_networks())
            if mirror_ap:
                self.access_points[i+1].configure_ap(network_list,
                                                     channel_2g,
                                                     channel_5g)
                self.access_points[i+1].start_ap()
                self.bssid_map.append(
                    self.access_points[i+1].get_bssids_for_wifi_networks())
                break

    def legacy_configure_ap_and_start(
            self,
            channel_5g=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            channel_2g=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            max_2g_networks=hostapd_constants.AP_DEFAULT_MAX_SSIDS_2G,
            max_5g_networks=hostapd_constants.AP_DEFAULT_MAX_SSIDS_5G,
            ap_ssid_length_2g=hostapd_constants.AP_SSID_LENGTH_2G,
            ap_passphrase_length_2g=hostapd_constants.AP_PASSPHRASE_LENGTH_2G,
            ap_ssid_length_5g=hostapd_constants.AP_SSID_LENGTH_5G,
            ap_passphrase_length_5g=hostapd_constants.AP_PASSPHRASE_LENGTH_5G,
            hidden=False,
            same_ssid=False,
            mirror_ap=True,
            wpa_network=False,
            wep_network=False,
            ent_network=False,
            radius_conf_2g=None,
            radius_conf_5g=None,
            ent_network_pwd=False,
            radius_conf_pwd=None,
            ap_count=1):

        config_count = 1
        count = 0

        # For example, the NetworkSelector tests use 2 APs and require that
        # both APs are not mirrored.
        if not mirror_ap and ap_count == 1:
             raise ValueError("ap_count cannot be 1 if mirror_ap is False.")

        if not mirror_ap:
            config_count = ap_count

        self.user_params["reference_networks"] = []
        self.user_params["open_network"] = []
        if wpa_network:
            self.user_params["wpa_networks"] = []
        if wep_network:
            self.user_params["wep_networks"] = []
        if ent_network:
            self.user_params["ent_networks"] = []
        if ent_network_pwd:
            self.user_params["ent_networks_pwd"] = []

        # kill hostapd & dhcpd if the cleanup was not successful
        for i in range(len(self.access_points)):
            self.log.debug("Check ap state and cleanup")
            self._cleanup_hostapd_and_dhcpd(i)

        for count in range(config_count):

            network_list_2g = []
            network_list_5g = []

            orig_network_list_2g = []
            orig_network_list_5g = []

            network_list_2g.append({"channel": channel_2g})
            network_list_5g.append({"channel": channel_5g})

            networks_dict = self.get_psk_network(
                                mirror_ap,
                                self.user_params["reference_networks"],
                                hidden=hidden,
                                same_ssid=same_ssid)
            self.reference_networks = self.user_params["reference_networks"]

            network_list_2g.append(networks_dict["2g"])
            network_list_5g.append(networks_dict["5g"])

            # When same_ssid is set, only configure one set of WPA networks.
            # We cannot have more than one set because duplicate interface names
            # are not allowed.
            # TODO(bmahadev): Provide option to select the type of network,
            # instead of defaulting to WPA.
            if not same_ssid:
                networks_dict = self.get_open_network(
                                    mirror_ap,
                                    self.user_params["open_network"],
                                    hidden=hidden,
                                    same_ssid=same_ssid)
                self.open_network = self.user_params["open_network"]

                network_list_2g.append(networks_dict["2g"])
                network_list_5g.append(networks_dict["5g"])

                if wpa_network:
                    networks_dict = self.get_psk_network(
                                        mirror_ap,
                                        self.user_params["wpa_networks"],
                                        hidden=hidden,
                                        same_ssid=same_ssid,
                                        security_mode=hostapd_constants.WPA_STRING)
                    self.wpa_networks = self.user_params["wpa_networks"]

                    network_list_2g.append(networks_dict["2g"])
                    network_list_5g.append(networks_dict["5g"])

                if wep_network:
                    networks_dict = self.get_wep_network(
                                        mirror_ap,
                                        self.user_params["wep_networks"],
                                        hidden=hidden,
                                        same_ssid=same_ssid)
                    self.wep_networks = self.user_params["wep_networks"]

                    network_list_2g.append(networks_dict["2g"])
                    network_list_5g.append(networks_dict["5g"])

                if ent_network:
                    networks_dict = self.get_open_network(
                                        mirror_ap,
                                        self.user_params["ent_networks"],
                                        hidden=hidden,
                                        same_ssid=same_ssid)
                    networks_dict["2g"]["security"] = hostapd_constants.ENT_STRING
                    networks_dict["2g"].update(radius_conf_2g)
                    networks_dict["5g"]["security"] = hostapd_constants.ENT_STRING
                    networks_dict["5g"].update(radius_conf_5g)
                    self.ent_networks = self.user_params["ent_networks"]

                    network_list_2g.append(networks_dict["2g"])
                    network_list_5g.append(networks_dict["5g"])

                if ent_network_pwd:
                    networks_dict = self.get_open_network(
                                        mirror_ap,
                                        self.user_params["ent_networks_pwd"],
                                        hidden=hidden,
                                        same_ssid=same_ssid)
                    networks_dict["2g"]["security"] = hostapd_constants.ENT_STRING
                    networks_dict["2g"].update(radius_conf_pwd)
                    networks_dict["5g"]["security"] = hostapd_constants.ENT_STRING
                    networks_dict["5g"].update(radius_conf_pwd)
                    self.ent_networks_pwd = self.user_params["ent_networks_pwd"]

                    network_list_2g.append(networks_dict["2g"])
                    network_list_5g.append(networks_dict["5g"])

            orig_network_list_5g = copy.copy(network_list_5g)
            orig_network_list_2g = copy.copy(network_list_2g)

            if len(network_list_5g) > 1:
                self.config_5g = self._generate_legacy_ap_config(network_list_5g)
            if len(network_list_2g) > 1:
                self.config_2g = self._generate_legacy_ap_config(network_list_2g)

            self.access_points[count].start_ap(self.config_2g)
            self.access_points[count].start_ap(self.config_5g)
            self.populate_bssid(count, self.access_points[count], orig_network_list_5g,
                                orig_network_list_2g)

        # Repeat configuration on the second router.
        if mirror_ap and ap_count == 2:
            self.access_points[AP_2].start_ap(self.config_2g)
            self.access_points[AP_2].start_ap(self.config_5g)
            self.populate_bssid(AP_2, self.access_points[AP_2],
                orig_network_list_5g, orig_network_list_2g)

    def _kill_processes(self, ap, daemon):
        """ Kill hostapd and dhcpd daemons

        Args:
            ap: AP to cleanup
            daemon: process to kill

        Returns: True/False if killing process is successful
        """
        self.log.info("Killing %s" % daemon)
        pids = ap.ssh.run('pidof %s' % daemon, ignore_status=True)
        if pids.stdout:
            ap.ssh.run('kill %s' % pids.stdout, ignore_status=True)
        time.sleep(3)
        pids = ap.ssh.run('pidof %s' % daemon, ignore_status=True)
        if pids.stdout:
            return False
        return True

    def _cleanup_hostapd_and_dhcpd(self, count):
        """ Check if AP was cleaned up properly

        Kill hostapd and dhcpd processes if cleanup was not successful in the
        last run

        Args:
            count: AP to check

        Returns:
            New AccessPoint object if AP required cleanup

        Raises:
            Error: if the AccessPoint timed out to setup
        """
        ap = self.access_points[count]
        phy_ifaces = ap.interfaces.get_physical_interface()
        kill_hostapd = False
        for iface in phy_ifaces:
            if '2g_' in iface or '5g_' in iface or 'xg_' in iface:
                kill_hostapd = True
                break

        if not kill_hostapd:
            return

        self.log.debug("Cleanup AP")
        if not self._kill_processes(ap, 'hostapd') or \
            not self._kill_processes(ap, 'dhcpd'):
              raise("Failed to cleanup AP")

        ap.__init__(self.user_params['AccessPoint'][count])

    def _generate_legacy_ap_config(self, network_list):
        bss_settings = []
        wlan_2g = self.access_points[AP_1].wlan_2g
        wlan_5g = self.access_points[AP_1].wlan_5g
        ap_settings = network_list.pop(0)
        # TODO:(bmahadev) This is a bug. We should not have to pop the first
        # network in the list and treat it as a separate case. Instead,
        # create_ap_preset() should be able to take NULL ssid and security and
        # build config based on the bss_Settings alone.
        hostapd_config_settings = network_list.pop(0)
        for network in network_list:
            if "password" in network:
                bss_settings.append(
                    hostapd_bss_settings.BssSettings(
                        name=network["SSID"],
                        ssid=network["SSID"],
                        hidden=network["hiddenSSID"],
                        security=hostapd_security.Security(
                            security_mode=network["security"],
                            password=network["password"])))
            elif "wepKeys" in network:
                bss_settings.append(
                    hostapd_bss_settings.BssSettings(
                        name=network["SSID"],
                        ssid=network["SSID"],
                        hidden=network["hiddenSSID"],
                        security=hostapd_security.Security(
                            security_mode=network["security"],
                            password=network["wepKeys"][0])))
            elif network["security"] == hostapd_constants.ENT_STRING:
                bss_settings.append(
                    hostapd_bss_settings.BssSettings(
                        name=network["SSID"],
                        ssid=network["SSID"],
                        hidden=network["hiddenSSID"],
                        security=hostapd_security.Security(
                            security_mode=network["security"],
                            radius_server_ip=network["radius_server_ip"],
                            radius_server_port=network["radius_server_port"],
                            radius_server_secret=network["radius_server_secret"])))
            else:
                bss_settings.append(
                    hostapd_bss_settings.BssSettings(
                        name=network["SSID"],
                        ssid=network["SSID"],
                        hidden=network["hiddenSSID"]))
        if "password" in hostapd_config_settings:
            config = hostapd_ap_preset.create_ap_preset(
                iface_wlan_2g=wlan_2g,
                iface_wlan_5g=wlan_5g,
                channel=ap_settings["channel"],
                ssid=hostapd_config_settings["SSID"],
                hidden=hostapd_config_settings["hiddenSSID"],
                security=hostapd_security.Security(
                    security_mode=hostapd_config_settings["security"],
                    password=hostapd_config_settings["password"]),
                bss_settings=bss_settings)
        elif "wepKeys" in hostapd_config_settings:
            config = hostapd_ap_preset.create_ap_preset(
                iface_wlan_2g=wlan_2g,
                iface_wlan_5g=wlan_5g,
                channel=ap_settings["channel"],
                ssid=hostapd_config_settings["SSID"],
                hidden=hostapd_config_settings["hiddenSSID"],
                security=hostapd_security.Security(
                    security_mode=hostapd_config_settings["security"],
                    password=hostapd_config_settings["wepKeys"][0]),
                bss_settings=bss_settings)
        else:
            config = hostapd_ap_preset.create_ap_preset(
                iface_wlan_2g=wlan_2g,
                iface_wlan_5g=wlan_5g,
                channel=ap_settings["channel"],
                ssid=hostapd_config_settings["SSID"],
                hidden=hostapd_config_settings["hiddenSSID"],
                bss_settings=bss_settings)
        return config

    def configure_packet_capture(
            self,
            channel_5g=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            channel_2g=hostapd_constants.AP_DEFAULT_CHANNEL_2G):
        """Configure packet capture for 2G and 5G bands.

        Args:
            channel_5g: Channel to set the monitor mode to for 5G band.
            channel_2g: Channel to set the monitor mode to for 2G band.
        """
        self.packet_capture = self.packet_capture[0]
        result = self.packet_capture.configure_monitor_mode(
            hostapd_constants.BAND_2G, channel_2g)
        if not result:
            raise ValueError("Failed to configure channel for 2G band")

        result = self.packet_capture.configure_monitor_mode(
            hostapd_constants.BAND_5G, channel_5g)
        if not result:
            raise ValueError("Failed to configure channel for 5G band.")

    @staticmethod
    def wifi_test_wrap(fn):
        def _safe_wrap_test_case(self, *args, **kwargs):
            test_id = "%s:%s:%s" % (self.__class__.__name__, self.test_name,
                                    self.log_begin_time.replace(' ', '-'))
            self.test_id = test_id
            self.result_detail = ""
            tries = int(self.user_params.get("wifi_auto_rerun", 3))
            for ad in self.android_devices:
                ad.log_path = self.log_path
            for i in range(tries + 1):
                result = True
                if i > 0:
                    log_string = "[Test Case] RETRY:%s %s" % (i, self.test_name)
                    self.log.info(log_string)
                    self._teardown_test(self.test_name)
                    self._setup_test(self.test_name)
                try:
                    result = fn(self, *args, **kwargs)
                except signals.TestFailure as e:
                    self.log.warn("Error msg: %s" % e)
                    if self.result_detail:
                        signal.details = self.result_detail
                    result = False
                except signals.TestSignal:
                    if self.result_detail:
                        signal.details = self.result_detail
                    raise
                except Exception as e:
                    self.log.exception(e)
                    asserts.fail(self.result_detail)
                if result is False:
                    if i < tries:
                        continue
                else:
                    break
            if result is not False:
                asserts.explicit_pass(self.result_detail)
            else:
                asserts.fail(self.result_detail)

        return _safe_wrap_test_case
