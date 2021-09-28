#!/usr/bin/env python3
#
#   Copyright 2016 Google, Inc.
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

import logging
import os
import re
import shutil
import time

from collections import namedtuple
from enum import IntEnum
from queue import Empty

from acts import asserts
from acts import context
from acts import signals
from acts import utils
from acts.controllers import attenuator
from acts.controllers.ap_lib import hostapd_security
from acts.controllers.ap_lib import hostapd_ap_preset
from acts.controllers.ap_lib.hostapd_constants import BAND_2G
from acts.controllers.ap_lib.hostapd_constants import BAND_5G
from acts.test_utils.wifi import wifi_constants
from acts.test_utils.tel import tel_defines

# Default timeout used for reboot, toggle WiFi and Airplane mode,
# for the system to settle down after the operation.
DEFAULT_TIMEOUT = 10
# Number of seconds to wait for events that are supposed to happen quickly.
# Like onSuccess for start background scan and confirmation on wifi state
# change.
SHORT_TIMEOUT = 30
ROAMING_TIMEOUT = 30
WIFI_CONNECTION_TIMEOUT_DEFAULT = 30
# Speed of light in m/s.
SPEED_OF_LIGHT = 299792458

DEFAULT_PING_ADDR = "https://www.google.com/robots.txt"

CNSS_DIAG_CONFIG_PATH = "/data/vendor/wifi/cnss_diag/"
CNSS_DIAG_CONFIG_FILE = "cnss_diag.conf"

ROAMING_ATTN = {
        "AP1_on_AP2_off": [
            0,
            0,
            95,
            95
        ],
        "AP1_off_AP2_on": [
            95,
            95,
            0,
            0
        ],
        "default": [
            0,
            0,
            0,
            0
        ]
    }


class WifiEnums():

    SSID_KEY = "SSID" # Used for Wifi & SoftAp
    SSID_PATTERN_KEY = "ssidPattern"
    NETID_KEY = "network_id"
    BSSID_KEY = "BSSID" # Used for Wifi & SoftAp
    BSSID_PATTERN_KEY = "bssidPattern"
    PWD_KEY = "password" # Used for Wifi & SoftAp
    frequency_key = "frequency"
    HIDDEN_KEY = "hiddenSSID" # Used for Wifi & SoftAp
    IS_APP_INTERACTION_REQUIRED = "isAppInteractionRequired"
    IS_USER_INTERACTION_REQUIRED = "isUserInteractionRequired"
    IS_SUGGESTION_METERED = "isMetered"
    PRIORITY = "priority"
    SECURITY = "security" # Used for Wifi & SoftAp

    # Used for SoftAp
    AP_BAND_KEY = "apBand"
    AP_CHANNEL_KEY = "apChannel"
    AP_MAXCLIENTS_KEY = "MaxNumberOfClients"
    AP_SHUTDOWNTIMEOUT_KEY = "ShutdownTimeoutMillis"
    AP_SHUTDOWNTIMEOUTENABLE_KEY = "AutoShutdownEnabled"
    AP_CLIENTCONTROL_KEY = "ClientControlByUserEnabled"
    AP_ALLOWEDLIST_KEY = "AllowedClientList"
    AP_BLOCKEDLIST_KEY = "BlockedClientList"

    WIFI_CONFIG_SOFTAP_BAND_2G = 1
    WIFI_CONFIG_SOFTAP_BAND_5G = 2
    WIFI_CONFIG_SOFTAP_BAND_2G_5G = 3
    WIFI_CONFIG_SOFTAP_BAND_6G = 4
    WIFI_CONFIG_SOFTAP_BAND_2G_6G = 5
    WIFI_CONFIG_SOFTAP_BAND_5G_6G = 6
    WIFI_CONFIG_SOFTAP_BAND_ANY = 7

    # DO NOT USE IT for new test case! Replaced by WIFI_CONFIG_SOFTAP_BAND_
    WIFI_CONFIG_APBAND_2G = WIFI_CONFIG_SOFTAP_BAND_2G
    WIFI_CONFIG_APBAND_5G = WIFI_CONFIG_SOFTAP_BAND_5G
    WIFI_CONFIG_APBAND_AUTO = WIFI_CONFIG_SOFTAP_BAND_2G_5G

    WIFI_CONFIG_APBAND_2G_OLD = 0
    WIFI_CONFIG_APBAND_5G_OLD = 1
    WIFI_CONFIG_APBAND_AUTO_OLD = -1

    WIFI_WPS_INFO_PBC = 0
    WIFI_WPS_INFO_DISPLAY = 1
    WIFI_WPS_INFO_KEYPAD = 2
    WIFI_WPS_INFO_LABEL = 3
    WIFI_WPS_INFO_INVALID = 4

    class SoftApSecurityType():
        OPEN = "NONE"
        WPA2 = "WPA2_PSK"
        WPA3_SAE_TRANSITION = "WPA3_SAE_TRANSITION"
        WPA3_SAE = "WPA3_SAE"

    class CountryCode():
        AUSTRALIA = "AU"
        CHINA = "CN"
        GERMANY = "DE"
        JAPAN = "JP"
        UK = "GB"
        US = "US"
        UNKNOWN = "UNKNOWN"

    # Start of Macros for EAP
    # EAP types
    class Eap(IntEnum):
        NONE = -1
        PEAP = 0
        TLS = 1
        TTLS = 2
        PWD = 3
        SIM = 4
        AKA = 5
        AKA_PRIME = 6
        UNAUTH_TLS = 7

    # EAP Phase2 types
    class EapPhase2(IntEnum):
        NONE = 0
        PAP = 1
        MSCHAP = 2
        MSCHAPV2 = 3
        GTC = 4

    class Enterprise:
        # Enterprise Config Macros
        EMPTY_VALUE = "NULL"
        EAP = "eap"
        PHASE2 = "phase2"
        IDENTITY = "identity"
        ANON_IDENTITY = "anonymous_identity"
        PASSWORD = "password"
        SUBJECT_MATCH = "subject_match"
        ALTSUBJECT_MATCH = "altsubject_match"
        DOM_SUFFIX_MATCH = "domain_suffix_match"
        CLIENT_CERT = "client_cert"
        CA_CERT = "ca_cert"
        ENGINE = "engine"
        ENGINE_ID = "engine_id"
        PRIVATE_KEY_ID = "key_id"
        REALM = "realm"
        PLMN = "plmn"
        FQDN = "FQDN"
        FRIENDLY_NAME = "providerFriendlyName"
        ROAMING_IDS = "roamingConsortiumIds"
        OCSP = "ocsp"
    # End of Macros for EAP

    # Macros for wifi p2p.
    WIFI_P2P_SERVICE_TYPE_ALL = 0
    WIFI_P2P_SERVICE_TYPE_BONJOUR = 1
    WIFI_P2P_SERVICE_TYPE_UPNP = 2
    WIFI_P2P_SERVICE_TYPE_VENDOR_SPECIFIC = 255

    class ScanResult:
        CHANNEL_WIDTH_20MHZ = 0
        CHANNEL_WIDTH_40MHZ = 1
        CHANNEL_WIDTH_80MHZ = 2
        CHANNEL_WIDTH_160MHZ = 3
        CHANNEL_WIDTH_80MHZ_PLUS_MHZ = 4

    # Macros for wifi rtt.
    class RttType(IntEnum):
        TYPE_ONE_SIDED = 1
        TYPE_TWO_SIDED = 2

    class RttPeerType(IntEnum):
        PEER_TYPE_AP = 1
        PEER_TYPE_STA = 2  # Requires NAN.
        PEER_P2P_GO = 3
        PEER_P2P_CLIENT = 4
        PEER_NAN = 5

    class RttPreamble(IntEnum):
        PREAMBLE_LEGACY = 0x01
        PREAMBLE_HT = 0x02
        PREAMBLE_VHT = 0x04

    class RttBW(IntEnum):
        BW_5_SUPPORT = 0x01
        BW_10_SUPPORT = 0x02
        BW_20_SUPPORT = 0x04
        BW_40_SUPPORT = 0x08
        BW_80_SUPPORT = 0x10
        BW_160_SUPPORT = 0x20

    class Rtt(IntEnum):
        STATUS_SUCCESS = 0
        STATUS_FAILURE = 1
        STATUS_FAIL_NO_RSP = 2
        STATUS_FAIL_REJECTED = 3
        STATUS_FAIL_NOT_SCHEDULED_YET = 4
        STATUS_FAIL_TM_TIMEOUT = 5
        STATUS_FAIL_AP_ON_DIFF_CHANNEL = 6
        STATUS_FAIL_NO_CAPABILITY = 7
        STATUS_ABORTED = 8
        STATUS_FAIL_INVALID_TS = 9
        STATUS_FAIL_PROTOCOL = 10
        STATUS_FAIL_SCHEDULE = 11
        STATUS_FAIL_BUSY_TRY_LATER = 12
        STATUS_INVALID_REQ = 13
        STATUS_NO_WIFI = 14
        STATUS_FAIL_FTM_PARAM_OVERRIDE = 15

        REASON_UNSPECIFIED = -1
        REASON_NOT_AVAILABLE = -2
        REASON_INVALID_LISTENER = -3
        REASON_INVALID_REQUEST = -4

    class RttParam:
        device_type = "deviceType"
        request_type = "requestType"
        BSSID = "bssid"
        channel_width = "channelWidth"
        frequency = "frequency"
        center_freq0 = "centerFreq0"
        center_freq1 = "centerFreq1"
        number_burst = "numberBurst"
        interval = "interval"
        num_samples_per_burst = "numSamplesPerBurst"
        num_retries_per_measurement_frame = "numRetriesPerMeasurementFrame"
        num_retries_per_FTMR = "numRetriesPerFTMR"
        lci_request = "LCIRequest"
        lcr_request = "LCRRequest"
        burst_timeout = "burstTimeout"
        preamble = "preamble"
        bandwidth = "bandwidth"
        margin = "margin"

    RTT_MARGIN_OF_ERROR = {
        RttBW.BW_80_SUPPORT: 2,
        RttBW.BW_40_SUPPORT: 5,
        RttBW.BW_20_SUPPORT: 5
    }

    # Macros as specified in the WifiScanner code.
    WIFI_BAND_UNSPECIFIED = 0  # not specified
    WIFI_BAND_24_GHZ = 1  # 2.4 GHz band
    WIFI_BAND_5_GHZ = 2  # 5 GHz band without DFS channels
    WIFI_BAND_5_GHZ_DFS_ONLY = 4  # 5 GHz band with DFS channels
    WIFI_BAND_5_GHZ_WITH_DFS = 6  # 5 GHz band with DFS channels
    WIFI_BAND_BOTH = 3  # both bands without DFS channels
    WIFI_BAND_BOTH_WITH_DFS = 7  # both bands with DFS channels

    REPORT_EVENT_AFTER_BUFFER_FULL = 0
    REPORT_EVENT_AFTER_EACH_SCAN = 1
    REPORT_EVENT_FULL_SCAN_RESULT = 2

    SCAN_TYPE_LOW_LATENCY = 0
    SCAN_TYPE_LOW_POWER = 1
    SCAN_TYPE_HIGH_ACCURACY = 2

    # US Wifi frequencies
    ALL_2G_FREQUENCIES = [2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452,
                          2457, 2462]
    DFS_5G_FREQUENCIES = [5260, 5280, 5300, 5320, 5500, 5520, 5540, 5560, 5580,
                          5600, 5620, 5640, 5660, 5680, 5700, 5720]
    NONE_DFS_5G_FREQUENCIES = [5180, 5200, 5220, 5240, 5745, 5765, 5785, 5805,
                               5825]
    ALL_5G_FREQUENCIES = DFS_5G_FREQUENCIES + NONE_DFS_5G_FREQUENCIES

    band_to_frequencies = {
        WIFI_BAND_24_GHZ: ALL_2G_FREQUENCIES,
        WIFI_BAND_5_GHZ: NONE_DFS_5G_FREQUENCIES,
        WIFI_BAND_5_GHZ_DFS_ONLY: DFS_5G_FREQUENCIES,
        WIFI_BAND_5_GHZ_WITH_DFS: ALL_5G_FREQUENCIES,
        WIFI_BAND_BOTH: ALL_2G_FREQUENCIES + NONE_DFS_5G_FREQUENCIES,
        WIFI_BAND_BOTH_WITH_DFS: ALL_5G_FREQUENCIES + ALL_2G_FREQUENCIES
    }

    # All Wifi frequencies to channels lookup.
    freq_to_channel = {
        2412: 1,
        2417: 2,
        2422: 3,
        2427: 4,
        2432: 5,
        2437: 6,
        2442: 7,
        2447: 8,
        2452: 9,
        2457: 10,
        2462: 11,
        2467: 12,
        2472: 13,
        2484: 14,
        4915: 183,
        4920: 184,
        4925: 185,
        4935: 187,
        4940: 188,
        4945: 189,
        4960: 192,
        4980: 196,
        5035: 7,
        5040: 8,
        5045: 9,
        5055: 11,
        5060: 12,
        5080: 16,
        5170: 34,
        5180: 36,
        5190: 38,
        5200: 40,
        5210: 42,
        5220: 44,
        5230: 46,
        5240: 48,
        5260: 52,
        5280: 56,
        5300: 60,
        5320: 64,
        5500: 100,
        5520: 104,
        5540: 108,
        5560: 112,
        5580: 116,
        5600: 120,
        5620: 124,
        5640: 128,
        5660: 132,
        5680: 136,
        5700: 140,
        5745: 149,
        5765: 153,
        5785: 157,
        5805: 161,
        5825: 165,
    }

    # All Wifi channels to frequencies lookup.
    channel_2G_to_freq = {
        1: 2412,
        2: 2417,
        3: 2422,
        4: 2427,
        5: 2432,
        6: 2437,
        7: 2442,
        8: 2447,
        9: 2452,
        10: 2457,
        11: 2462,
        12: 2467,
        13: 2472,
        14: 2484
    }

    channel_5G_to_freq = {
        183: 4915,
        184: 4920,
        185: 4925,
        187: 4935,
        188: 4940,
        189: 4945,
        192: 4960,
        196: 4980,
        7: 5035,
        8: 5040,
        9: 5045,
        11: 5055,
        12: 5060,
        16: 5080,
        34: 5170,
        36: 5180,
        38: 5190,
        40: 5200,
        42: 5210,
        44: 5220,
        46: 5230,
        48: 5240,
        52: 5260,
        56: 5280,
        60: 5300,
        64: 5320,
        100: 5500,
        104: 5520,
        108: 5540,
        112: 5560,
        116: 5580,
        120: 5600,
        124: 5620,
        128: 5640,
        132: 5660,
        136: 5680,
        140: 5700,
        149: 5745,
        153: 5765,
        157: 5785,
        161: 5805,
        165: 5825
    }


class WifiChannelBase:
    ALL_2G_FREQUENCIES = []
    DFS_5G_FREQUENCIES = []
    NONE_DFS_5G_FREQUENCIES = []
    ALL_5G_FREQUENCIES = DFS_5G_FREQUENCIES + NONE_DFS_5G_FREQUENCIES
    MIX_CHANNEL_SCAN = []

    def band_to_freq(self, band):
        _band_to_frequencies = {
            WifiEnums.WIFI_BAND_24_GHZ: self.ALL_2G_FREQUENCIES,
            WifiEnums.WIFI_BAND_5_GHZ: self.NONE_DFS_5G_FREQUENCIES,
            WifiEnums.WIFI_BAND_5_GHZ_DFS_ONLY: self.DFS_5G_FREQUENCIES,
            WifiEnums.WIFI_BAND_5_GHZ_WITH_DFS: self.ALL_5G_FREQUENCIES,
            WifiEnums.WIFI_BAND_BOTH:
            self.ALL_2G_FREQUENCIES + self.NONE_DFS_5G_FREQUENCIES,
            WifiEnums.WIFI_BAND_BOTH_WITH_DFS:
            self.ALL_5G_FREQUENCIES + self.ALL_2G_FREQUENCIES
        }
        return _band_to_frequencies[band]


class WifiChannelUS(WifiChannelBase):
    # US Wifi frequencies
    ALL_2G_FREQUENCIES = [2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452,
                          2457, 2462]
    NONE_DFS_5G_FREQUENCIES = [5180, 5200, 5220, 5240, 5745, 5765, 5785, 5805,
                               5825]
    MIX_CHANNEL_SCAN = [2412, 2437, 2462, 5180, 5200, 5280, 5260, 5300, 5500,
                        5320, 5520, 5560, 5700, 5745, 5805]

    def __init__(self, model=None):
        self.DFS_5G_FREQUENCIES = [5260, 5280, 5300, 5320, 5500, 5520,
                                   5540, 5560, 5580, 5600, 5620, 5640,
                                   5660, 5680, 5700, 5720]
        self.ALL_5G_FREQUENCIES = self.DFS_5G_FREQUENCIES + self.NONE_DFS_5G_FREQUENCIES


class WifiReferenceNetworks:
    """ Class to parse and return networks of different band and
        auth type from reference_networks
    """
    def __init__(self, obj):
        self.reference_networks = obj
        self.WIFI_2G = "2g"
        self.WIFI_5G = "5g"

        self.secure_networks_2g = []
        self.secure_networks_5g = []
        self.open_networks_2g = []
        self.open_networks_5g = []
        self._parse_networks()

    def _parse_networks(self):
        for network in self.reference_networks:
            for key in network:
                if key == self.WIFI_2G:
                    if "password" in network[key]:
                        self.secure_networks_2g.append(network[key])
                    else:
                        self.open_networks_2g.append(network[key])
                else:
                    if "password" in network[key]:
                        self.secure_networks_5g.append(network[key])
                    else:
                        self.open_networks_5g.append(network[key])

    def return_2g_secure_networks(self):
        return self.secure_networks_2g

    def return_5g_secure_networks(self):
        return self.secure_networks_5g

    def return_2g_open_networks(self):
        return self.open_networks_2g

    def return_5g_open_networks(self):
        return self.open_networks_5g

    def return_secure_networks(self):
        return self.secure_networks_2g + self.secure_networks_5g

    def return_open_networks(self):
        return self.open_networks_2g + self.open_networks_5g


def _assert_on_fail_handler(func, assert_on_fail, *args, **kwargs):
    """Wrapper function that handles the bahevior of assert_on_fail.

    When assert_on_fail is True, let all test signals through, which can
    terminate test cases directly. When assert_on_fail is False, the wrapper
    raises no test signals and reports operation status by returning True or
    False.

    Args:
        func: The function to wrap. This function reports operation status by
              raising test signals.
        assert_on_fail: A boolean that specifies if the output of the wrapper
                        is test signal based or return value based.
        args: Positional args for func.
        kwargs: Name args for func.

    Returns:
        If assert_on_fail is True, returns True/False to signal operation
        status, otherwise return nothing.
    """
    try:
        func(*args, **kwargs)
        if not assert_on_fail:
            return True
    except signals.TestSignal:
        if assert_on_fail:
            raise
        return False


def assert_network_in_list(target, network_list):
    """Makes sure a specified target Wi-Fi network exists in a list of Wi-Fi
    networks.

    Args:
        target: A dict representing a Wi-Fi network.
                E.g. {WifiEnums.SSID_KEY: "SomeNetwork"}
        network_list: A list of dicts, each representing a Wi-Fi network.
    """
    match_results = match_networks(target, network_list)
    asserts.assert_true(
        match_results, "Target network %s, does not exist in network list %s" %
        (target, network_list))


def match_networks(target_params, networks):
    """Finds the WiFi networks that match a given set of parameters in a list
    of WiFi networks.

    To be considered a match, the network should contain every key-value pair
    of target_params

    Args:
        target_params: A dict with 1 or more key-value pairs representing a Wi-Fi network.
                       E.g { 'SSID': 'wh_ap1_5g', 'BSSID': '30:b5:c2:33:e4:47' }
        networks: A list of dict objects representing WiFi networks.

    Returns:
        The networks that match the target parameters.
    """
    results = []
    asserts.assert_true(target_params,
                        "Expected networks object 'target_params' is empty")
    for n in networks:
        add_network = 1
        for k, v in target_params.items():
            if k not in n:
                add_network = 0
                break
            if n[k] != v:
                add_network = 0
                break
        if add_network:
            results.append(n)
    return results


def wait_for_wifi_state(ad, state, assert_on_fail=True):
    """Waits for the device to transition to the specified wifi state

    Args:
        ad: An AndroidDevice object.
        state: Wifi state to wait for.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        If assert_on_fail is False, function returns True if the device transitions
        to the specified state, False otherwise. If assert_on_fail is True, no return value.
    """
    return _assert_on_fail_handler(
        _wait_for_wifi_state, assert_on_fail, ad, state=state)


def _wait_for_wifi_state(ad, state):
    """Toggles the state of wifi.

    TestFailure signals are raised when something goes wrong.

    Args:
        ad: An AndroidDevice object.
        state: Wifi state to wait for.
    """
    if state == ad.droid.wifiCheckState():
        # Check if the state is already achieved, so we don't wait for the
        # state change event by mistake.
        return
    ad.droid.wifiStartTrackingStateChange()
    fail_msg = "Device did not transition to Wi-Fi state to %s on %s." % (state,
                                                           ad.serial)
    try:
        ad.ed.wait_for_event(wifi_constants.WIFI_STATE_CHANGED,
                             lambda x: x["data"]["enabled"] == state,
                             SHORT_TIMEOUT)
    except Empty:
        asserts.assert_equal(state, ad.droid.wifiCheckState(), fail_msg)
    finally:
        ad.droid.wifiStopTrackingStateChange()


def wifi_toggle_state(ad, new_state=None, assert_on_fail=True):
    """Toggles the state of wifi.

    Args:
        ad: An AndroidDevice object.
        new_state: Wifi state to set to. If None, opposite of the current state.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        If assert_on_fail is False, function returns True if the toggle was
        successful, False otherwise. If assert_on_fail is True, no return value.
    """
    return _assert_on_fail_handler(
        _wifi_toggle_state, assert_on_fail, ad, new_state=new_state)


def _wifi_toggle_state(ad, new_state=None):
    """Toggles the state of wifi.

    TestFailure signals are raised when something goes wrong.

    Args:
        ad: An AndroidDevice object.
        new_state: The state to set Wi-Fi to. If None, opposite of the current
                   state will be set.
    """
    if new_state is None:
        new_state = not ad.droid.wifiCheckState()
    elif new_state == ad.droid.wifiCheckState():
        # Check if the new_state is already achieved, so we don't wait for the
        # state change event by mistake.
        return
    ad.droid.wifiStartTrackingStateChange()
    ad.log.info("Setting Wi-Fi state to %s.", new_state)
    ad.ed.clear_all_events()
    # Setting wifi state.
    ad.droid.wifiToggleState(new_state)
    time.sleep(2)
    fail_msg = "Failed to set Wi-Fi state to %s on %s." % (new_state,
                                                           ad.serial)
    try:
        ad.ed.wait_for_event(wifi_constants.WIFI_STATE_CHANGED,
                             lambda x: x["data"]["enabled"] == new_state,
                             SHORT_TIMEOUT)
    except Empty:
        asserts.assert_equal(new_state, ad.droid.wifiCheckState(), fail_msg)
    finally:
        ad.droid.wifiStopTrackingStateChange()


def reset_wifi(ad):
    """Clears all saved Wi-Fi networks on a device.

    This will turn Wi-Fi on.

    Args:
        ad: An AndroidDevice object.

    """
    networks = ad.droid.wifiGetConfiguredNetworks()
    if not networks:
        return
    for n in networks:
        ad.droid.wifiForgetNetwork(n['networkId'])
        try:
            event = ad.ed.pop_event(wifi_constants.WIFI_FORGET_NW_SUCCESS,
                                    SHORT_TIMEOUT)
        except Empty:
            logging.warning("Could not confirm the removal of network %s.", n)
    # Check again to see if there's any network left.
    asserts.assert_true(
        not ad.droid.wifiGetConfiguredNetworks(),
        "Failed to remove these configured Wi-Fi networks: %s" % networks)


def toggle_airplane_mode_on_and_off(ad):
    """Turn ON and OFF Airplane mode.

    ad: An AndroidDevice object.
    Returns: Assert if turning on/off Airplane mode fails.

    """
    ad.log.debug("Toggling Airplane mode ON.")
    asserts.assert_true(
        utils.force_airplane_mode(ad, True),
        "Can not turn on airplane mode on: %s" % ad.serial)
    time.sleep(DEFAULT_TIMEOUT)
    ad.log.debug("Toggling Airplane mode OFF.")
    asserts.assert_true(
        utils.force_airplane_mode(ad, False),
        "Can not turn on airplane mode on: %s" % ad.serial)
    time.sleep(DEFAULT_TIMEOUT)


def toggle_wifi_off_and_on(ad):
    """Turn OFF and ON WiFi.

    ad: An AndroidDevice object.
    Returns: Assert if turning off/on WiFi fails.

    """
    ad.log.debug("Toggling wifi OFF.")
    wifi_toggle_state(ad, False)
    time.sleep(DEFAULT_TIMEOUT)
    ad.log.debug("Toggling wifi ON.")
    wifi_toggle_state(ad, True)
    time.sleep(DEFAULT_TIMEOUT)


def wifi_forget_network(ad, net_ssid):
    """Remove configured Wifi network on an android device.

    Args:
        ad: android_device object for forget network.
        net_ssid: ssid of network to be forget

    """
    networks = ad.droid.wifiGetConfiguredNetworks()
    if not networks:
        return
    for n in networks:
        if net_ssid in n[WifiEnums.SSID_KEY]:
            ad.droid.wifiForgetNetwork(n['networkId'])
            try:
                event = ad.ed.pop_event(wifi_constants.WIFI_FORGET_NW_SUCCESS,
                                        SHORT_TIMEOUT)
            except Empty:
                asserts.fail("Failed to remove network %s." % n)


def wifi_test_device_init(ad):
    """Initializes an android device for wifi testing.

    0. Make sure SL4A connection is established on the android device.
    1. Disable location service's WiFi scan.
    2. Turn WiFi on.
    3. Clear all saved networks.
    4. Set country code to US.
    5. Enable WiFi verbose logging.
    6. Sync device time with computer time.
    7. Turn off cellular data.
    8. Turn off ambient display.
    """
    utils.require_sl4a((ad, ))
    ad.droid.wifiScannerToggleAlwaysAvailable(False)
    msg = "Failed to turn off location service's scan."
    asserts.assert_true(not ad.droid.wifiScannerIsAlwaysAvailable(), msg)
    wifi_toggle_state(ad, True)
    reset_wifi(ad)
    ad.droid.wifiEnableVerboseLogging(1)
    msg = "Failed to enable WiFi verbose logging."
    asserts.assert_equal(ad.droid.wifiGetVerboseLoggingLevel(), 1, msg)
    # We don't verify the following settings since they are not critical.
    # Set wpa_supplicant log level to EXCESSIVE.
    output = ad.adb.shell("wpa_cli -i wlan0 -p -g@android:wpa_wlan0 IFNAME="
                          "wlan0 log_level EXCESSIVE")
    ad.log.info("wpa_supplicant log change status: %s", output)
    utils.sync_device_time(ad)
    ad.droid.telephonyToggleDataConnection(False)
    set_wifi_country_code(ad, WifiEnums.CountryCode.US)
    utils.set_ambient_display(ad, False)

def set_wifi_country_code(ad, country_code):
    """Sets the wifi country code on the device.

    Args:
        ad: An AndroidDevice object.
        country_code: 2 letter ISO country code

    Raises:
        An RpcException if unable to set the country code.
    """
    try:
        ad.adb.shell("cmd wifi force-country-code enabled %s" % country_code)
    except ad.adb.AdbError as e:
        ad.droid.wifiSetCountryCode(WifiEnums.CountryCode.US)


def start_wifi_connection_scan(ad):
    """Starts a wifi connection scan and wait for results to become available.

    Args:
        ad: An AndroidDevice object.
    """
    ad.ed.clear_all_events()
    ad.droid.wifiStartScan()
    try:
        ad.ed.pop_event("WifiManagerScanResultsAvailable", 60)
    except Empty:
        asserts.fail("Wi-Fi results did not become available within 60s.")


def start_wifi_connection_scan_and_return_status(ad):
    """
    Starts a wifi connection scan and wait for results to become available
    or a scan failure to be reported.

    Args:
        ad: An AndroidDevice object.
    Returns:
        True: if scan succeeded & results are available
        False: if scan failed
    """
    ad.ed.clear_all_events()
    ad.droid.wifiStartScan()
    try:
        events = ad.ed.pop_events(
            "WifiManagerScan(ResultsAvailable|Failure)", 60)
    except Empty:
        asserts.fail(
            "Wi-Fi scan results/failure did not become available within 60s.")
    # If there are multiple matches, we check for atleast one success.
    for event in events:
        if event["name"] == "WifiManagerScanResultsAvailable":
            return True
        elif event["name"] == "WifiManagerScanFailure":
            ad.log.debug("Scan failure received")
    return False


def start_wifi_connection_scan_and_check_for_network(ad, network_ssid,
                                                     max_tries=3):
    """
    Start connectivity scans & checks if the |network_ssid| is seen in
    scan results. The method performs a max of |max_tries| connectivity scans
    to find the network.

    Args:
        ad: An AndroidDevice object.
        network_ssid: SSID of the network we are looking for.
        max_tries: Number of scans to try.
    Returns:
        True: if network_ssid is found in scan results.
        False: if network_ssid is not found in scan results.
    """
    for num_tries in range(max_tries):
        if start_wifi_connection_scan_and_return_status(ad):
            scan_results = ad.droid.wifiGetScanResults()
            match_results = match_networks(
                {WifiEnums.SSID_KEY: network_ssid}, scan_results)
            if len(match_results) > 0:
                return True
    return False


def start_wifi_connection_scan_and_ensure_network_found(ad, network_ssid,
                                                        max_tries=3):
    """
    Start connectivity scans & ensure the |network_ssid| is seen in
    scan results. The method performs a max of |max_tries| connectivity scans
    to find the network.
    This method asserts on failure!

    Args:
        ad: An AndroidDevice object.
        network_ssid: SSID of the network we are looking for.
        max_tries: Number of scans to try.
    """
    ad.log.info("Starting scans to ensure %s is present", network_ssid)
    assert_msg = "Failed to find " + network_ssid + " in scan results" \
        " after " + str(max_tries) + " tries"
    asserts.assert_true(start_wifi_connection_scan_and_check_for_network(
        ad, network_ssid, max_tries), assert_msg)


def start_wifi_connection_scan_and_ensure_network_not_found(ad, network_ssid,
                                                            max_tries=3):
    """
    Start connectivity scans & ensure the |network_ssid| is not seen in
    scan results. The method performs a max of |max_tries| connectivity scans
    to find the network.
    This method asserts on failure!

    Args:
        ad: An AndroidDevice object.
        network_ssid: SSID of the network we are looking for.
        max_tries: Number of scans to try.
    """
    ad.log.info("Starting scans to ensure %s is not present", network_ssid)
    assert_msg = "Found " + network_ssid + " in scan results" \
        " after " + str(max_tries) + " tries"
    asserts.assert_false(start_wifi_connection_scan_and_check_for_network(
        ad, network_ssid, max_tries), assert_msg)


def start_wifi_background_scan(ad, scan_setting):
    """Starts wifi background scan.

    Args:
        ad: android_device object to initiate connection on.
        scan_setting: A dict representing the settings of the scan.

    Returns:
        If scan was started successfully, event data of success event is returned.
    """
    idx = ad.droid.wifiScannerStartBackgroundScan(scan_setting)
    event = ad.ed.pop_event("WifiScannerScan{}onSuccess".format(idx),
                            SHORT_TIMEOUT)
    return event['data']


def start_wifi_tethering(ad, ssid, password, band=None, hidden=None):
    """Starts wifi tethering on an android_device.

    Args:
        ad: android_device to start wifi tethering on.
        ssid: The SSID the soft AP should broadcast.
        password: The password the soft AP should use.
        band: The band the soft AP should be set on. It should be either
            WifiEnums.WIFI_CONFIG_APBAND_2G or WifiEnums.WIFI_CONFIG_APBAND_5G.
        hidden: boolean to indicate if the AP needs to be hidden or not.

    Returns:
        No return value. Error checks in this function will raise test failure signals
    """
    config = {WifiEnums.SSID_KEY: ssid}
    if password:
        config[WifiEnums.PWD_KEY] = password
    if band:
        config[WifiEnums.AP_BAND_KEY] = band
    if hidden:
      config[WifiEnums.HIDDEN_KEY] = hidden
    asserts.assert_true(
        ad.droid.wifiSetWifiApConfiguration(config),
        "Failed to update WifiAp Configuration")
    ad.droid.wifiStartTrackingTetherStateChange()
    ad.droid.connectivityStartTethering(tel_defines.TETHERING_WIFI, False)
    try:
        ad.ed.pop_event("ConnectivityManagerOnTetheringStarted")
        ad.ed.wait_for_event("TetherStateChanged",
                             lambda x: x["data"]["ACTIVE_TETHER"], 30)
        ad.log.debug("Tethering started successfully.")
    except Empty:
        msg = "Failed to receive confirmation of wifi tethering starting"
        asserts.fail(msg)
    finally:
        ad.droid.wifiStopTrackingTetherStateChange()


def save_wifi_soft_ap_config(ad, wifi_config, band=None, hidden=None,
                             security=None, password=None,
                             channel=None, max_clients=None,
                             shutdown_timeout_enable=None,
                             shutdown_timeout_millis=None,
                             client_control_enable=None,
                             allowedList=None, blockedList=None):
    """ Save a soft ap configuration and verified
    Args:
        ad: android_device to set soft ap configuration.
        wifi_config: a soft ap configuration object, at least include SSID.
        band: specifies the band for the soft ap.
        hidden: specifies the soft ap need to broadcast its SSID or not.
        security: specifies the security type for the soft ap.
        password: specifies the password for the soft ap.
        channel: specifies the channel for the soft ap.
        max_clients: specifies the maximum connected client number.
        shutdown_timeout_enable: specifies the auto shut down enable or not.
        shutdown_timeout_millis: specifies the shut down timeout value.
        client_control_enable: specifies the client control enable or not.
        allowedList: specifies allowed clients list.
        blockedList: specifies blocked clients list.
    """
    if security and password:
       wifi_config[WifiEnums.SECURITY] = security
       wifi_config[WifiEnums.PWD_KEY] = password
    if band:
        wifi_config[WifiEnums.AP_BAND_KEY] = band
    if hidden:
        wifi_config[WifiEnums.HIDDEN_KEY] = hidden
    if channel and band:
        wifi_config[WifiEnums.AP_BAND_KEY] = band
        wifi_config[WifiEnums.AP_CHANNEL_KEY] = channel
    if max_clients:
        wifi_config[WifiEnums.AP_MAXCLIENTS_KEY] = max_clients
    if shutdown_timeout_enable:
        wifi_config[
            WifiEnums.AP_SHUTDOWNTIMEOUTENABLE_KEY] = shutdown_timeout_enable
    if shutdown_timeout_millis:
        wifi_config[
            WifiEnums.AP_SHUTDOWNTIMEOUT_KEY] = shutdown_timeout_millis
    if client_control_enable:
        wifi_config[WifiEnums.AP_CLIENTCONTROL_KEY] = client_control_enable
    if allowedList:
        wifi_config[WifiEnums.AP_ALLOWEDLIST_KEY] = allowedList
    if blockedList:
        wifi_config[WifiEnums.AP_BLOCKEDLIST_KEY] = blockedList

    if WifiEnums.AP_CHANNEL_KEY in wifi_config and wifi_config[
        WifiEnums.AP_CHANNEL_KEY] == 0:
        del wifi_config[WifiEnums.AP_CHANNEL_KEY]

    if WifiEnums.SECURITY in wifi_config and wifi_config[
        WifiEnums.SECURITY] == WifiEnums.SoftApSecurityType.OPEN:
        del wifi_config[WifiEnums.SECURITY]
        del wifi_config[WifiEnums.PWD_KEY]

    asserts.assert_true(ad.droid.wifiSetWifiApConfiguration(wifi_config),
                        "Failed to set WifiAp Configuration")

    wifi_ap = ad.droid.wifiGetApConfiguration()
    asserts.assert_true(
        wifi_ap[WifiEnums.SSID_KEY] == wifi_config[WifiEnums.SSID_KEY],
        "Hotspot SSID doesn't match")
    if WifiEnums.SECURITY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.SECURITY] == wifi_config[WifiEnums.SECURITY],
            "Hotspot Security doesn't match")
    if WifiEnums.PWD_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.PWD_KEY] == wifi_config[WifiEnums.PWD_KEY],
            "Hotspot Password doesn't match")

    if WifiEnums.HIDDEN_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.HIDDEN_KEY] == wifi_config[WifiEnums.HIDDEN_KEY],
            "Hotspot hidden setting doesn't match")

    if WifiEnums.AP_BAND_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.AP_BAND_KEY] == wifi_config[WifiEnums.AP_BAND_KEY],
            "Hotspot Band doesn't match")
    if WifiEnums.AP_CHANNEL_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.AP_CHANNEL_KEY] == wifi_config[
            WifiEnums.AP_CHANNEL_KEY], "Hotspot Channel doesn't match")
    if WifiEnums.AP_MAXCLIENTS_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.AP_MAXCLIENTS_KEY] == wifi_config[
            WifiEnums.AP_MAXCLIENTS_KEY], "Hotspot Max Clients doesn't match")
    if WifiEnums.AP_SHUTDOWNTIMEOUTENABLE_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.AP_SHUTDOWNTIMEOUTENABLE_KEY] == wifi_config[
            WifiEnums.AP_SHUTDOWNTIMEOUTENABLE_KEY],
            "Hotspot ShutDown feature flag doesn't match")
    if WifiEnums.AP_SHUTDOWNTIMEOUT_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.AP_SHUTDOWNTIMEOUT_KEY] == wifi_config[
            WifiEnums.AP_SHUTDOWNTIMEOUT_KEY],
            "Hotspot ShutDown timeout setting doesn't match")
    if WifiEnums.AP_CLIENTCONTROL_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.AP_CLIENTCONTROL_KEY] == wifi_config[
            WifiEnums.AP_CLIENTCONTROL_KEY],
            "Hotspot Client control flag doesn't match")
    if WifiEnums.AP_ALLOWEDLIST_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.AP_ALLOWEDLIST_KEY] == wifi_config[
            WifiEnums.AP_ALLOWEDLIST_KEY],
            "Hotspot Allowed List doesn't match")
    if WifiEnums.AP_BLOCKEDLIST_KEY in wifi_config:
        asserts.assert_true(
            wifi_ap[WifiEnums.AP_BLOCKEDLIST_KEY] == wifi_config[
            WifiEnums.AP_BLOCKEDLIST_KEY],
            "Hotspot Blocked List doesn't match")

def start_wifi_tethering_saved_config(ad):
    """ Turn on wifi hotspot with a config that is already saved """
    ad.droid.wifiStartTrackingTetherStateChange()
    ad.droid.connectivityStartTethering(tel_defines.TETHERING_WIFI, False)
    try:
        ad.ed.pop_event("ConnectivityManagerOnTetheringStarted")
        ad.ed.wait_for_event("TetherStateChanged",
                             lambda x: x["data"]["ACTIVE_TETHER"], 30)
    except:
        asserts.fail("Didn't receive wifi tethering starting confirmation")
    finally:
        ad.droid.wifiStopTrackingTetherStateChange()


def stop_wifi_tethering(ad):
    """Stops wifi tethering on an android_device.
    Args:
        ad: android_device to stop wifi tethering on.
    """
    ad.droid.wifiStartTrackingTetherStateChange()
    ad.droid.connectivityStopTethering(tel_defines.TETHERING_WIFI)
    try:
        ad.ed.pop_event("WifiManagerApDisabled", 30)
        ad.ed.wait_for_event("TetherStateChanged",
                             lambda x: not x["data"]["ACTIVE_TETHER"], 30)
    except Empty:
        msg = "Failed to receive confirmation of wifi tethering stopping"
        asserts.fail(msg)
    finally:
        ad.droid.wifiStopTrackingTetherStateChange()


def toggle_wifi_and_wait_for_reconnection(ad,
                                          network,
                                          num_of_tries=1,
                                          assert_on_fail=True):
    """Toggle wifi state and then wait for Android device to reconnect to
    the provided wifi network.

    This expects the device to be already connected to the provided network.

    Logic steps are
     1. Ensure that we're connected to the network.
     2. Turn wifi off.
     3. Wait for 10 seconds.
     4. Turn wifi on.
     5. Wait for the "connected" event, then confirm the connected ssid is the
        one requested.

    Args:
        ad: android_device object to initiate connection on.
        network: A dictionary representing the network to await connection. The
                 dictionary must have the key "SSID".
        num_of_tries: An integer that is the number of times to try before
                      delaring failure. Default is 1.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        If assert_on_fail is False, function returns True if the toggle was
        successful, False otherwise. If assert_on_fail is True, no return value.
    """
    return _assert_on_fail_handler(
        _toggle_wifi_and_wait_for_reconnection,
        assert_on_fail,
        ad,
        network,
        num_of_tries=num_of_tries)


def _toggle_wifi_and_wait_for_reconnection(ad, network, num_of_tries=3):
    """Toggle wifi state and then wait for Android device to reconnect to
    the provided wifi network.

    This expects the device to be already connected to the provided network.

    Logic steps are
     1. Ensure that we're connected to the network.
     2. Turn wifi off.
     3. Wait for 10 seconds.
     4. Turn wifi on.
     5. Wait for the "connected" event, then confirm the connected ssid is the
        one requested.

    This will directly fail a test if anything goes wrong.

    Args:
        ad: android_device object to initiate connection on.
        network: A dictionary representing the network to await connection. The
                 dictionary must have the key "SSID".
        num_of_tries: An integer that is the number of times to try before
                      delaring failure. Default is 1.
    """
    expected_ssid = network[WifiEnums.SSID_KEY]
    # First ensure that we're already connected to the provided network.
    verify_con = {WifiEnums.SSID_KEY: expected_ssid}
    verify_wifi_connection_info(ad, verify_con)
    # Now toggle wifi state and wait for the connection event.
    wifi_toggle_state(ad, False)
    time.sleep(10)
    wifi_toggle_state(ad, True)
    ad.droid.wifiStartTrackingStateChange()
    try:
        connect_result = None
        for i in range(num_of_tries):
            try:
                connect_result = ad.ed.pop_event(wifi_constants.WIFI_CONNECTED,
                                                 30)
                break
            except Empty:
                pass
        asserts.assert_true(connect_result,
                            "Failed to connect to Wi-Fi network %s on %s" %
                            (network, ad.serial))
        logging.debug("Connection result on %s: %s.", ad.serial,
                      connect_result)
        actual_ssid = connect_result['data'][WifiEnums.SSID_KEY]
        asserts.assert_equal(actual_ssid, expected_ssid,
                             "Connected to the wrong network on %s."
                             "Expected %s, but got %s." %
                             (ad.serial, expected_ssid, actual_ssid))
        logging.info("Connected to Wi-Fi network %s on %s", actual_ssid,
                     ad.serial)
    finally:
        ad.droid.wifiStopTrackingStateChange()


def wait_for_connect(ad, expected_ssid=None, expected_id=None, tries=2,
                     assert_on_fail=True):
    """Wait for a connect event.

    This will directly fail a test if anything goes wrong.

    Args:
        ad: An Android device object.
        expected_ssid: SSID of the network to connect to.
        expected_id: Network Id of the network to connect to.
        tries: An integer that is the number of times to try before failing.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        Returns a value only if assert_on_fail is false.
        Returns True if the connection was successful, False otherwise.
    """
    return _assert_on_fail_handler(
        _wait_for_connect, assert_on_fail, ad, expected_ssid, expected_id,
        tries)


def _wait_for_connect(ad, expected_ssid=None, expected_id=None, tries=2):
    """Wait for a connect event.

    Args:
        ad: An Android device object.
        expected_ssid: SSID of the network to connect to.
        expected_id: Network Id of the network to connect to.
        tries: An integer that is the number of times to try before failing.
    """
    ad.droid.wifiStartTrackingStateChange()
    try:
        connect_result = _wait_for_connect_event(
            ad, ssid=expected_ssid, id=expected_id, tries=tries)
        asserts.assert_true(connect_result,
                            "Failed to connect to Wi-Fi network %s" %
                            expected_ssid)
        ad.log.debug("Wi-Fi connection result: %s.", connect_result)
        actual_ssid = connect_result['data'][WifiEnums.SSID_KEY]
        if expected_ssid:
            asserts.assert_equal(actual_ssid, expected_ssid,
                                 "Connected to the wrong network")
        actual_id = connect_result['data'][WifiEnums.NETID_KEY]
        if expected_id:
            asserts.assert_equal(actual_id, expected_id,
                                 "Connected to the wrong network")
        ad.log.info("Connected to Wi-Fi network %s.", actual_ssid)
    except Empty:
        asserts.fail("Failed to start connection process to %s" %
                     expected_ssid)
    except Exception as error:
        ad.log.error("Failed to connect to %s with error %s", expected_ssid,
                     error)
        raise signals.TestFailure("Failed to connect to %s network" %
                                  expected_ssid)
    finally:
        ad.droid.wifiStopTrackingStateChange()


def _wait_for_connect_event(ad, ssid=None, id=None, tries=1):
    """Wait for a connect event on queue and pop when available.

    Args:
        ad: An Android device object.
        ssid: SSID of the network to connect to.
        id: Network Id of the network to connect to.
        tries: An integer that is the number of times to try before failing.

    Returns:
        A dict with details of the connection data, which looks like this:
        {
         'time': 1485460337798,
         'name': 'WifiNetworkConnected',
         'data': {
                  'rssi': -27,
                  'is_24ghz': True,
                  'mac_address': '02:00:00:00:00:00',
                  'network_id': 1,
                  'BSSID': '30:b5:c2:33:d3:fc',
                  'ip_address': 117483712,
                  'link_speed': 54,
                  'supplicant_state': 'completed',
                  'hidden_ssid': False,
                  'SSID': 'wh_ap1_2g',
                  'is_5ghz': False}
        }

    """
    conn_result = None

    # If ssid and network id is None, just wait for any connect event.
    if id is None and ssid is None:
        for i in range(tries):
            try:
                conn_result = ad.ed.pop_event(wifi_constants.WIFI_CONNECTED, 30)
                break
            except Empty:
                pass
    else:
    # If ssid or network id is specified, wait for specific connect event.
        for i in range(tries):
            try:
                conn_result = ad.ed.pop_event(wifi_constants.WIFI_CONNECTED, 30)
                if id and conn_result['data'][WifiEnums.NETID_KEY] == id:
                    break
                elif ssid and conn_result['data'][WifiEnums.SSID_KEY] == ssid:
                    break
            except Empty:
                pass

    return conn_result


def wait_for_disconnect(ad, timeout=10):
    """Wait for a disconnect event within the specified timeout.

    Args:
        ad: Android device object.
        timeout: Timeout in seconds.

    """
    try:
        ad.droid.wifiStartTrackingStateChange()
        event = ad.ed.pop_event("WifiNetworkDisconnected", timeout)
    except Empty:
        raise signals.TestFailure("Device did not disconnect from the network")
    finally:
        ad.droid.wifiStopTrackingStateChange()


def ensure_no_disconnect(ad, duration=10):
    """Ensure that there is no disconnect for the specified duration.

    Args:
        ad: Android device object.
        duration: Duration in seconds.

    """
    try:
        ad.droid.wifiStartTrackingStateChange()
        event = ad.ed.pop_event("WifiNetworkDisconnected", duration)
        raise signals.TestFailure("Device disconnected from the network")
    except Empty:
        pass
    finally:
        ad.droid.wifiStopTrackingStateChange()


def connect_to_wifi_network(ad, network, assert_on_fail=True,
        check_connectivity=True, hidden=False):
    """Connection logic for open and psk wifi networks.

    Args:
        ad: AndroidDevice to use for connection
        network: network info of the network to connect to
        assert_on_fail: If true, errors from wifi_connect will raise
                        test failure signals.
        hidden: Is the Wifi network hidden.
    """
    if hidden:
        start_wifi_connection_scan_and_ensure_network_not_found(
            ad, network[WifiEnums.SSID_KEY])
    else:
        start_wifi_connection_scan_and_ensure_network_found(
            ad, network[WifiEnums.SSID_KEY])
    wifi_connect(ad,
                 network,
                 num_of_tries=3,
                 assert_on_fail=assert_on_fail,
                 check_connectivity=check_connectivity)


def connect_to_wifi_network_with_id(ad, network_id, network_ssid):
    """Connect to the given network using network id and verify SSID.

    Args:
        network_id: int Network Id of the network.
        network_ssid: string SSID of the network.

    Returns: True if connect using network id was successful;
             False otherwise.

    """
    start_wifi_connection_scan_and_ensure_network_found(ad, network_ssid)
    wifi_connect_by_id(ad, network_id)
    connect_data = ad.droid.wifiGetConnectionInfo()
    connect_ssid = connect_data[WifiEnums.SSID_KEY]
    ad.log.debug("Expected SSID = %s Connected SSID = %s" %
                   (network_ssid, connect_ssid))
    if connect_ssid != network_ssid:
        return False
    return True


def wifi_connect(ad, network, num_of_tries=1, assert_on_fail=True,
        check_connectivity=True):
    """Connect an Android device to a wifi network.

    Initiate connection to a wifi network, wait for the "connected" event, then
    confirm the connected ssid is the one requested.

    This will directly fail a test if anything goes wrong.

    Args:
        ad: android_device object to initiate connection on.
        network: A dictionary representing the network to connect to. The
                 dictionary must have the key "SSID".
        num_of_tries: An integer that is the number of times to try before
                      delaring failure. Default is 1.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        Returns a value only if assert_on_fail is false.
        Returns True if the connection was successful, False otherwise.
    """
    return _assert_on_fail_handler(
        _wifi_connect, assert_on_fail, ad, network, num_of_tries=num_of_tries,
          check_connectivity=check_connectivity)


def _wifi_connect(ad, network, num_of_tries=1, check_connectivity=True):
    """Connect an Android device to a wifi network.

    Initiate connection to a wifi network, wait for the "connected" event, then
    confirm the connected ssid is the one requested.

    This will directly fail a test if anything goes wrong.

    Args:
        ad: android_device object to initiate connection on.
        network: A dictionary representing the network to connect to. The
                 dictionary must have the key "SSID".
        num_of_tries: An integer that is the number of times to try before
                      delaring failure. Default is 1.
    """
    asserts.assert_true(WifiEnums.SSID_KEY in network,
                        "Key '%s' must be present in network definition." %
                        WifiEnums.SSID_KEY)
    ad.droid.wifiStartTrackingStateChange()
    expected_ssid = network[WifiEnums.SSID_KEY]
    ad.droid.wifiConnectByConfig(network)
    ad.log.info("Starting connection process to %s", expected_ssid)
    try:
        event = ad.ed.pop_event(wifi_constants.CONNECT_BY_CONFIG_SUCCESS, 30)
        connect_result = _wait_for_connect_event(
            ad, ssid=expected_ssid, tries=num_of_tries)
        asserts.assert_true(connect_result,
                            "Failed to connect to Wi-Fi network %s on %s" %
                            (network, ad.serial))
        ad.log.debug("Wi-Fi connection result: %s.", connect_result)
        actual_ssid = connect_result['data'][WifiEnums.SSID_KEY]
        asserts.assert_equal(actual_ssid, expected_ssid,
                             "Connected to the wrong network on %s." %
                             ad.serial)
        ad.log.info("Connected to Wi-Fi network %s.", actual_ssid)

        if check_connectivity:
            internet = validate_connection(ad, DEFAULT_PING_ADDR)
            if not internet:
                raise signals.TestFailure("Failed to connect to internet on %s" %
                                          expected_ssid)
    except Empty:
        asserts.fail("Failed to start connection process to %s on %s" %
                     (network, ad.serial))
    except Exception as error:
        ad.log.error("Failed to connect to %s with error %s", expected_ssid,
                     error)
        raise signals.TestFailure("Failed to connect to %s network" % network)

    finally:
        ad.droid.wifiStopTrackingStateChange()


def wifi_connect_by_id(ad, network_id, num_of_tries=3, assert_on_fail=True):
    """Connect an Android device to a wifi network using network Id.

    Start connection to the wifi network, with the given network Id, wait for
    the "connected" event, then verify the connected network is the one requested.

    This will directly fail a test if anything goes wrong.

    Args:
        ad: android_device object to initiate connection on.
        network_id: Integer specifying the network id of the network.
        num_of_tries: An integer that is the number of times to try before
                      delaring failure. Default is 1.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        Returns a value only if assert_on_fail is false.
        Returns True if the connection was successful, False otherwise.
    """
    _assert_on_fail_handler(_wifi_connect_by_id, assert_on_fail, ad,
                            network_id, num_of_tries)


def _wifi_connect_by_id(ad, network_id, num_of_tries=1):
    """Connect an Android device to a wifi network using it's network id.

    Start connection to the wifi network, with the given network id, wait for
    the "connected" event, then verify the connected network is the one requested.

    Args:
        ad: android_device object to initiate connection on.
        network_id: Integer specifying the network id of the network.
        num_of_tries: An integer that is the number of times to try before
                      delaring failure. Default is 1.
    """
    ad.droid.wifiStartTrackingStateChange()
    # Clear all previous events.
    ad.ed.clear_all_events()
    ad.droid.wifiConnectByNetworkId(network_id)
    ad.log.info("Starting connection to network with id %d", network_id)
    try:
        event = ad.ed.pop_event(wifi_constants.CONNECT_BY_NETID_SUCCESS, 60)
        connect_result = _wait_for_connect_event(
            ad, id=network_id, tries=num_of_tries)
        asserts.assert_true(connect_result,
                            "Failed to connect to Wi-Fi network using network id")
        ad.log.debug("Wi-Fi connection result: %s", connect_result)
        actual_id = connect_result['data'][WifiEnums.NETID_KEY]
        asserts.assert_equal(actual_id, network_id,
                             "Connected to the wrong network on %s."
                             "Expected network id = %d, but got %d." %
                             (ad.serial, network_id, actual_id))
        expected_ssid = connect_result['data'][WifiEnums.SSID_KEY]
        ad.log.info("Connected to Wi-Fi network %s with %d network id.",
                     expected_ssid, network_id)

        internet = validate_connection(ad, DEFAULT_PING_ADDR)
        if not internet:
            raise signals.TestFailure("Failed to connect to internet on %s" %
                                      expected_ssid)
    except Empty:
        asserts.fail("Failed to connect to network with id %d on %s" %
                    (network_id, ad.serial))
    except Exception as error:
        ad.log.error("Failed to connect to network with id %d with error %s",
                      network_id, error)
        raise signals.TestFailure("Failed to connect to network with network"
                                  " id %d" % network_id)
    finally:
        ad.droid.wifiStopTrackingStateChange()


def wifi_connect_using_network_request(ad, network, network_specifier,
                                       num_of_tries=3, assert_on_fail=True):
    """Connect an Android device to a wifi network using network request.

    Trigger a network request with the provided network specifier,
    wait for the "onMatch" event, ensure that the scan results in "onMatch"
    event contain the specified network, then simulate the user granting the
    request with the specified network selected. Then wait for the "onAvailable"
    network callback indicating successful connection to network.

    This will directly fail a test if anything goes wrong.

    Args:
        ad: android_device object to initiate connection on.
        network_specifier: A dictionary representing the network specifier to
                           use.
        network: A dictionary representing the network to connect to. The
                 dictionary must have the key "SSID".
        num_of_tries: An integer that is the number of times to try before
                      delaring failure.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        Returns a value only if assert_on_fail is false.
        Returns True if the connection was successful, False otherwise.
    """
    _assert_on_fail_handler(_wifi_connect_using_network_request, assert_on_fail,
                            ad, network, network_specifier, num_of_tries)


def _wifi_connect_using_network_request(ad, network, network_specifier,
                                        num_of_tries=3):
    """Connect an Android device to a wifi network using network request.

    Trigger a network request with the provided network specifier,
    wait for the "onMatch" event, ensure that the scan results in "onMatch"
    event contain the specified network, then simulate the user granting the
    request with the specified network selected. Then wait for the "onAvailable"
    network callback indicating successful connection to network.

    Args:
        ad: android_device object to initiate connection on.
        network_specifier: A dictionary representing the network specifier to
                           use.
        network: A dictionary representing the network to connect to. The
                 dictionary must have the key "SSID".
        num_of_tries: An integer that is the number of times to try before
                      delaring failure.
    """
    ad.droid.wifiRequestNetworkWithSpecifier(network_specifier)
    ad.log.info("Sent network request with %s", network_specifier)
    # Need a delay here because UI interaction should only start once wifi
    # starts processing the request.
    time.sleep(wifi_constants.NETWORK_REQUEST_CB_REGISTER_DELAY_SEC)
    _wait_for_wifi_connect_after_network_request(ad, network, num_of_tries)


def wait_for_wifi_connect_after_network_request(ad, network, num_of_tries=3,
                                                assert_on_fail=True):
    """
    Simulate and verify the connection flow after initiating the network
    request.

    Wait for the "onMatch" event, ensure that the scan results in "onMatch"
    event contain the specified network, then simulate the user granting the
    request with the specified network selected. Then wait for the "onAvailable"
    network callback indicating successful connection to network.

    Args:
        ad: android_device object to initiate connection on.
        network: A dictionary representing the network to connect to. The
                 dictionary must have the key "SSID".
        num_of_tries: An integer that is the number of times to try before
                      delaring failure.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        Returns a value only if assert_on_fail is false.
        Returns True if the connection was successful, False otherwise.
    """
    _assert_on_fail_handler(_wait_for_wifi_connect_after_network_request,
                            assert_on_fail, ad, network, num_of_tries)


def _wait_for_wifi_connect_after_network_request(ad, network, num_of_tries=3):
    """
    Simulate and verify the connection flow after initiating the network
    request.

    Wait for the "onMatch" event, ensure that the scan results in "onMatch"
    event contain the specified network, then simulate the user granting the
    request with the specified network selected. Then wait for the "onAvailable"
    network callback indicating successful connection to network.

    Args:
        ad: android_device object to initiate connection on.
        network: A dictionary representing the network to connect to. The
                 dictionary must have the key "SSID".
        num_of_tries: An integer that is the number of times to try before
                      delaring failure.
    """
    asserts.assert_true(WifiEnums.SSID_KEY in network,
                        "Key '%s' must be present in network definition." %
                        WifiEnums.SSID_KEY)
    ad.droid.wifiStartTrackingStateChange()
    expected_ssid = network[WifiEnums.SSID_KEY]
    ad.droid.wifiRegisterNetworkRequestMatchCallback()
    # Wait for the platform to scan and return a list of networks
    # matching the request
    try:
        matched_network = None
        for _ in [0,  num_of_tries]:
            on_match_event = ad.ed.pop_event(
                wifi_constants.WIFI_NETWORK_REQUEST_MATCH_CB_ON_MATCH, 60)
            asserts.assert_true(on_match_event,
                                "Network request on match not received.")
            matched_scan_results = on_match_event["data"]
            ad.log.debug("Network request on match results %s",
                         matched_scan_results)
            matched_network = match_networks(
                {WifiEnums.SSID_KEY: network[WifiEnums.SSID_KEY]},
                matched_scan_results)
            if matched_network:
                break;

        asserts.assert_true(
            matched_network, "Target network %s not found" % network)

        ad.droid.wifiSendUserSelectionForNetworkRequestMatch(network)
        ad.log.info("Sent user selection for network request %s",
                    expected_ssid)

        # Wait for the platform to connect to the network.
        on_available_event = ad.ed.pop_event(
            wifi_constants.WIFI_NETWORK_CB_ON_AVAILABLE, 60)
        asserts.assert_true(on_available_event,
                            "Network request on available not received.")
        connected_network = on_available_event["data"]
        ad.log.info("Connected to network %s", connected_network)
        asserts.assert_equal(connected_network[WifiEnums.SSID_KEY],
                             expected_ssid,
                             "Connected to the wrong network."
                             "Expected %s, but got %s." %
                             (network, connected_network))
    except Empty:
        asserts.fail("Failed to connect to %s" % expected_ssid)
    except Exception as error:
        ad.log.error("Failed to connect to %s with error %s",
                     (expected_ssid, error))
        raise signals.TestFailure("Failed to connect to %s network" % network)
    finally:
        ad.droid.wifiStopTrackingStateChange()


def wifi_passpoint_connect(ad, passpoint_network, num_of_tries=1,
                           assert_on_fail=True):
    """Connect an Android device to a wifi network.

    Initiate connection to a wifi network, wait for the "connected" event, then
    confirm the connected ssid is the one requested.

    This will directly fail a test if anything goes wrong.

    Args:
        ad: android_device object to initiate connection on.
        passpoint_network: SSID of the Passpoint network to connect to.
        num_of_tries: An integer that is the number of times to try before
                      delaring failure. Default is 1.
        assert_on_fail: If True, error checks in this function will raise test
                        failure signals.

    Returns:
        If assert_on_fail is False, function returns network id, if the connect was
        successful, False otherwise. If assert_on_fail is True, no return value.
    """
    _assert_on_fail_handler(_wifi_passpoint_connect, assert_on_fail, ad,
                            passpoint_network, num_of_tries = num_of_tries)


def _wifi_passpoint_connect(ad, passpoint_network, num_of_tries=1):
    """Connect an Android device to a wifi network.

    Initiate connection to a wifi network, wait for the "connected" event, then
    confirm the connected ssid is the one requested.

    This will directly fail a test if anything goes wrong.

    Args:
        ad: android_device object to initiate connection on.
        passpoint_network: SSID of the Passpoint network to connect to.
        num_of_tries: An integer that is the number of times to try before
                      delaring failure. Default is 1.
    """
    ad.droid.wifiStartTrackingStateChange()
    expected_ssid = passpoint_network
    ad.log.info("Starting connection process to passpoint %s", expected_ssid)

    try:
        connect_result = _wait_for_connect_event(
            ad, expected_ssid, num_of_tries)
        asserts.assert_true(connect_result,
                            "Failed to connect to WiFi passpoint network %s on"
                            " %s" % (expected_ssid, ad.serial))
        ad.log.info("Wi-Fi connection result: %s.", connect_result)
        actual_ssid = connect_result['data'][WifiEnums.SSID_KEY]
        asserts.assert_equal(actual_ssid, expected_ssid,
                             "Connected to the wrong network on %s." % ad.serial)
        ad.log.info("Connected to Wi-Fi passpoint network %s.", actual_ssid)

        internet = validate_connection(ad, DEFAULT_PING_ADDR)
        if not internet:
            raise signals.TestFailure("Failed to connect to internet on %s" %
                                      expected_ssid)
    except Exception as error:
        ad.log.error("Failed to connect to passpoint network %s with error %s",
                      expected_ssid, error)
        raise signals.TestFailure("Failed to connect to %s passpoint network" %
                                   expected_ssid)

    finally:
        ad.droid.wifiStopTrackingStateChange()


def delete_passpoint(ad, fqdn):
    """Delete a required Passpoint configuration."""
    try:
        ad.droid.removePasspointConfig(fqdn)
        return True
    except Exception as error:
        ad.log.error("Failed to remove passpoint configuration with FQDN=%s "
                     "and error=%s" , fqdn, error)
        return False


def start_wifi_single_scan(ad, scan_setting):
    """Starts wifi single shot scan.

    Args:
        ad: android_device object to initiate connection on.
        scan_setting: A dict representing the settings of the scan.

    Returns:
        If scan was started successfully, event data of success event is returned.
    """
    idx = ad.droid.wifiScannerStartScan(scan_setting)
    event = ad.ed.pop_event("WifiScannerScan%sonSuccess" % idx, SHORT_TIMEOUT)
    ad.log.debug("Got event %s", event)
    return event['data']


def track_connection(ad, network_ssid, check_connection_count):
    """Track wifi connection to network changes for given number of counts

    Args:
        ad: android_device object for forget network.
        network_ssid: network ssid to which connection would be tracked
        check_connection_count: Integer for maximum number network connection
                                check.
    Returns:
        True if connection to given network happen, else return False.
    """
    ad.droid.wifiStartTrackingStateChange()
    while check_connection_count > 0:
        connect_network = ad.ed.pop_event("WifiNetworkConnected", 120)
        ad.log.info("Connected to network %s", connect_network)
        if (WifiEnums.SSID_KEY in connect_network['data'] and
                connect_network['data'][WifiEnums.SSID_KEY] == network_ssid):
            return True
        check_connection_count -= 1
    ad.droid.wifiStopTrackingStateChange()
    return False


def get_scan_time_and_channels(wifi_chs, scan_setting, stime_channel):
    """Calculate the scan time required based on the band or channels in scan
    setting

    Args:
        wifi_chs: Object of channels supported
        scan_setting: scan setting used for start scan
        stime_channel: scan time per channel

    Returns:
        scan_time: time required for completing a scan
        scan_channels: channel used for scanning
    """
    scan_time = 0
    scan_channels = []
    if "band" in scan_setting and "channels" not in scan_setting:
        scan_channels = wifi_chs.band_to_freq(scan_setting["band"])
    elif "channels" in scan_setting and "band" not in scan_setting:
        scan_channels = scan_setting["channels"]
    scan_time = len(scan_channels) * stime_channel
    for channel in scan_channels:
        if channel in WifiEnums.DFS_5G_FREQUENCIES:
            scan_time += 132  #passive scan time on DFS
    return scan_time, scan_channels


def start_wifi_track_bssid(ad, track_setting):
    """Start tracking Bssid for the given settings.

    Args:
      ad: android_device object.
      track_setting: Setting for which the bssid tracking should be started

    Returns:
      If tracking started successfully, event data of success event is returned.
    """
    idx = ad.droid.wifiScannerStartTrackingBssids(
        track_setting["bssidInfos"], track_setting["apLostThreshold"])
    event = ad.ed.pop_event("WifiScannerBssid{}onSuccess".format(idx),
                            SHORT_TIMEOUT)
    return event['data']


def convert_pem_key_to_pkcs8(in_file, out_file):
    """Converts the key file generated by us to the format required by
    Android using openssl.

    The input file must have the extension "pem". The output file must
    have the extension "der".

    Args:
        in_file: The original key file.
        out_file: The full path to the converted key file, including
        filename.
    """
    asserts.assert_true(in_file.endswith(".pem"), "Input file has to be .pem.")
    asserts.assert_true(
        out_file.endswith(".der"), "Output file has to be .der.")
    cmd = ("openssl pkcs8 -inform PEM -in {} -outform DER -out {} -nocrypt"
           " -topk8").format(in_file, out_file)
    utils.exe_cmd(cmd)


def validate_connection(ad, ping_addr=DEFAULT_PING_ADDR, wait_time=15,
                        ping_gateway=True):
    """Validate internet connection by pinging the address provided.

    Args:
        ad: android_device object.
        ping_addr: address on internet for pinging.
        wait_time: wait for some time before validating connection

    Returns:
        ping output if successful, NULL otherwise.
    """
    # wait_time to allow for DHCP to complete.
    for i in range(wait_time):
        if ad.droid.connectivityNetworkIsConnected():
            break
        time.sleep(1)
    ping = False
    try:
        ping = ad.droid.httpPing(ping_addr)
        ad.log.info("Http ping result: %s.", ping)
    except:
        pass
    if not ping and ping_gateway:
        ad.log.info("Http ping failed. Pinging default gateway")
        gw = ad.droid.connectivityGetIPv4DefaultGateway()
        result = ad.adb.shell("ping -c 6 {}".format(gw))
        ad.log.info("Default gateway ping result: %s" % result)
        ping = False if "100% packet loss" in result else True
    return ping


#TODO(angli): This can only verify if an actual value is exactly the same.
# Would be nice to be able to verify an actual value is one of serveral.
def verify_wifi_connection_info(ad, expected_con):
    """Verifies that the information of the currently connected wifi network is
    as expected.

    Args:
        expected_con: A dict representing expected key-value pairs for wifi
            connection. e.g. {"SSID": "test_wifi"}
    """
    current_con = ad.droid.wifiGetConnectionInfo()
    case_insensitive = ["BSSID", "supplicant_state"]
    ad.log.debug("Current connection: %s", current_con)
    for k, expected_v in expected_con.items():
        # Do not verify authentication related fields.
        if k == "password":
            continue
        msg = "Field %s does not exist in wifi connection info %s." % (
            k, current_con)
        if k not in current_con:
            raise signals.TestFailure(msg)
        actual_v = current_con[k]
        if k in case_insensitive:
            actual_v = actual_v.lower()
            expected_v = expected_v.lower()
        msg = "Expected %s to be %s, actual %s is %s." % (k, expected_v, k,
                                                          actual_v)
        if actual_v != expected_v:
            raise signals.TestFailure(msg)


def check_autoconnect_to_open_network(ad, conn_timeout=WIFI_CONNECTION_TIMEOUT_DEFAULT):
    """Connects to any open WiFI AP
     Args:
         timeout value in sec to wait for UE to connect to a WiFi AP
     Returns:
         True if UE connects to WiFi AP (supplicant_state = completed)
         False if UE fails to complete connection within WIFI_CONNECTION_TIMEOUT time.
    """
    if ad.droid.wifiCheckState():
        return True
    ad.droid.wifiToggleState()
    wifi_connection_state = None
    timeout = time.time() + conn_timeout
    while wifi_connection_state != "completed":
        wifi_connection_state = ad.droid.wifiGetConnectionInfo()[
            'supplicant_state']
        if time.time() > timeout:
            ad.log.warning("Failed to connect to WiFi AP")
            return False
    return True


def expand_enterprise_config_by_phase2(config):
    """Take an enterprise config and generate a list of configs, each with
    a different phase2 auth type.

    Args:
        config: A dict representing enterprise config.

    Returns
        A list of enterprise configs.
    """
    results = []
    phase2_types = WifiEnums.EapPhase2
    if config[WifiEnums.Enterprise.EAP] == WifiEnums.Eap.PEAP:
        # Skip unsupported phase2 types for PEAP.
        phase2_types = [WifiEnums.EapPhase2.GTC, WifiEnums.EapPhase2.MSCHAPV2]
    for phase2_type in phase2_types:
        # Skip a special case for passpoint TTLS.
        if (WifiEnums.Enterprise.FQDN in config and
                phase2_type == WifiEnums.EapPhase2.GTC):
            continue
        c = dict(config)
        c[WifiEnums.Enterprise.PHASE2] = phase2_type.value
        results.append(c)
    return results


def generate_eap_test_name(config, ad=None):
    """ Generates a test case name based on an EAP configuration.

    Args:
        config: A dict representing an EAP credential.
        ad object: Redundant but required as the same param is passed
                   to test_func in run_generated_tests

    Returns:
        A string representing the name of a generated EAP test case.
    """
    eap = WifiEnums.Eap
    eap_phase2 = WifiEnums.EapPhase2
    Ent = WifiEnums.Enterprise
    name = "test_connect-"
    eap_name = ""
    for e in eap:
        if e.value == config[Ent.EAP]:
            eap_name = e.name
            break
    if "peap0" in config[WifiEnums.SSID_KEY].lower():
        eap_name = "PEAP0"
    if "peap1" in config[WifiEnums.SSID_KEY].lower():
        eap_name = "PEAP1"
    name += eap_name
    if Ent.PHASE2 in config:
        for e in eap_phase2:
            if e.value == config[Ent.PHASE2]:
                name += "-{}".format(e.name)
                break
    return name


def group_attenuators(attenuators):
    """Groups a list of attenuators into attenuator groups for backward
    compatibility reasons.

    Most legacy Wi-Fi setups have two attenuators each connected to a separate
    AP. The new Wi-Fi setup has four attenuators, each connected to one channel
    on an AP, so two of them are connected to one AP.

    To make the existing scripts work in the new setup, when the script needs
    to attenuate one AP, it needs to set attenuation on both attenuators
    connected to the same AP.

    This function groups attenuators properly so the scripts work in both
    legacy and new Wi-Fi setups.

    Args:
        attenuators: A list of attenuator objects, either two or four in length.

    Raises:
        signals.TestFailure is raised if the attenuator list does not have two
        or four objects.
    """
    attn0 = attenuator.AttenuatorGroup("AP0")
    attn1 = attenuator.AttenuatorGroup("AP1")
    # Legacy testbed setup has two attenuation channels.
    num_of_attns = len(attenuators)
    if num_of_attns == 2:
        attn0.add(attenuators[0])
        attn1.add(attenuators[1])
    elif num_of_attns == 4:
        attn0.add(attenuators[0])
        attn0.add(attenuators[1])
        attn1.add(attenuators[2])
        attn1.add(attenuators[3])
    else:
        asserts.fail(("Either two or four attenuators are required for this "
                      "test, but found %s") % num_of_attns)
    return [attn0, attn1]


def set_attns(attenuator, attn_val_name, roaming_attn=ROAMING_ATTN):
    """Sets attenuation values on attenuators used in this test.

    Args:
        attenuator: The attenuator object.
        attn_val_name: Name of the attenuation value pair to use.
        roaming_attn: Dictionary specifying the attenuation params.
    """
    logging.info("Set attenuation values to %s", roaming_attn[attn_val_name])
    try:
        attenuator[0].set_atten(roaming_attn[attn_val_name][0])
        attenuator[1].set_atten(roaming_attn[attn_val_name][1])
        attenuator[2].set_atten(roaming_attn[attn_val_name][2])
        attenuator[3].set_atten(roaming_attn[attn_val_name][3])
    except:
        logging.exception("Failed to set attenuation values %s.",
                       attn_val_name)
        raise

def set_attns_steps(attenuators,
                    atten_val_name,
                    roaming_attn=ROAMING_ATTN,
                    steps=10,
                    wait_time=12):
    """Set attenuation values on attenuators used in this test. It will change
    the attenuation values linearly from current value to target value step by
    step.

    Args:
        attenuators: The list of attenuator objects that you want to change
                     their attenuation value.
        atten_val_name: Name of the attenuation value pair to use.
        roaming_attn: Dictionary specifying the attenuation params.
        steps: Number of attenuator changes to reach the target value.
        wait_time: Sleep time for each change of attenuator.
    """
    logging.info("Set attenuation values to %s in %d step(s)",
            roaming_attn[atten_val_name], steps)
    start_atten = [attenuator.get_atten() for attenuator in attenuators]
    target_atten = roaming_attn[atten_val_name]
    for current_step in range(steps):
        progress = (current_step + 1) / steps
        for i, attenuator in enumerate(attenuators):
            amount_since_start = (target_atten[i] - start_atten[i]) * progress
            attenuator.set_atten(round(start_atten[i] + amount_since_start))
        time.sleep(wait_time)


def trigger_roaming_and_validate(dut,
                                 attenuator,
                                 attn_val_name,
                                 expected_con,
                                 roaming_attn=ROAMING_ATTN):
    """Sets attenuators to trigger roaming and validate the DUT connected
    to the BSSID expected.

    Args:
        attenuator: The attenuator object.
        attn_val_name: Name of the attenuation value pair to use.
        expected_con: The network information of the expected network.
        roaming_attn: Dictionary specifying the attenaution params.
    """
    expected_con = {
        WifiEnums.SSID_KEY: expected_con[WifiEnums.SSID_KEY],
        WifiEnums.BSSID_KEY: expected_con["bssid"],
    }
    set_attns_steps(attenuator, attn_val_name, roaming_attn)

    verify_wifi_connection_info(dut, expected_con)
    expected_bssid = expected_con[WifiEnums.BSSID_KEY]
    logging.info("Roamed to %s successfully", expected_bssid)
    if not validate_connection(dut):
        raise signals.TestFailure("Fail to connect to internet on %s" %
                                      expected_bssid)

def create_softap_config():
    """Create a softap config with random ssid and password."""
    ap_ssid = "softap_" + utils.rand_ascii_str(8)
    ap_password = utils.rand_ascii_str(8)
    logging.info("softap setup: %s %s", ap_ssid, ap_password)
    config = {
        WifiEnums.SSID_KEY: ap_ssid,
        WifiEnums.PWD_KEY: ap_password,
    }
    return config

def start_softap_and_verify(ad, band):
    """Bring-up softap and verify AP mode and in scan results.

    Args:
        band: The band to use for softAP.

    Returns: dict, the softAP config.

    """
    # Register before start the test.
    callbackId = ad.dut.droid.registerSoftApCallback()
    # Check softap info value is default
    frequency, bandwdith = get_current_softap_info(ad.dut, callbackId, True)
    asserts.assert_true(frequency == 0, "Softap frequency is not reset")
    asserts.assert_true(bandwdith == 0, "Softap bandwdith is not reset")

    config = create_softap_config()
    start_wifi_tethering(ad.dut,
                         config[WifiEnums.SSID_KEY],
                         config[WifiEnums.PWD_KEY], band=band)
    asserts.assert_true(ad.dut.droid.wifiIsApEnabled(),
                         "SoftAp is not reported as running")
    start_wifi_connection_scan_and_ensure_network_found(ad.dut_client,
        config[WifiEnums.SSID_KEY])

    # Check softap info can get from callback succeed and assert value should be
    # valid.
    frequency, bandwdith = get_current_softap_info(ad.dut, callbackId, True)
    asserts.assert_true(frequency > 0, "Softap frequency is not valid")
    asserts.assert_true(bandwdith > 0, "Softap bandwdith is not valid")
    # Unregister callback
    ad.dut.droid.unregisterSoftApCallback(callbackId)

    return config

def wait_for_expected_number_of_softap_clients(ad, callbackId,
        expected_num_of_softap_clients):
    """Wait for the number of softap clients to be updated as expected.
    Args:
        callbackId: Id of the callback associated with registering.
        expected_num_of_softap_clients: expected number of softap clients.
    """
    eventStr = wifi_constants.SOFTAP_CALLBACK_EVENT + str(
            callbackId) + wifi_constants.SOFTAP_NUMBER_CLIENTS_CHANGED
    clientData = ad.ed.pop_event(eventStr, SHORT_TIMEOUT)['data']
    clientCount = clientData[wifi_constants.SOFTAP_NUMBER_CLIENTS_CALLBACK_KEY]
    clientMacAddresses = clientData[wifi_constants.SOFTAP_CLIENTS_MACS_CALLBACK_KEY]
    asserts.assert_equal(clientCount, expected_num_of_softap_clients,
            "The number of softap clients doesn't match the expected number")
    asserts.assert_equal(len(clientMacAddresses), expected_num_of_softap_clients,
                         "The number of mac addresses doesn't match the expected number")
    for macAddress in clientMacAddresses:
        asserts.assert_true(checkMacAddress(macAddress), "An invalid mac address was returned")

def checkMacAddress(input):
    """Validate whether a string is a valid mac address or not.

    Args:
        input: The string to validate.

    Returns: True/False, returns true for a valid mac address and false otherwise.
    """
    macValidationRegex = "[0-9a-f]{2}([-:]?)[0-9a-f]{2}(\\1[0-9a-f]{2}){4}$"
    if re.match(macValidationRegex, input.lower()):
        return True
    return False

def wait_for_expected_softap_state(ad, callbackId, expected_softap_state):
    """Wait for the expected softap state change.
    Args:
        callbackId: Id of the callback associated with registering.
        expected_softap_state: The expected softap state.
    """
    eventStr = wifi_constants.SOFTAP_CALLBACK_EVENT + str(
            callbackId) + wifi_constants.SOFTAP_STATE_CHANGED
    asserts.assert_equal(ad.ed.pop_event(eventStr,
            SHORT_TIMEOUT)['data'][wifi_constants.
            SOFTAP_STATE_CHANGE_CALLBACK_KEY],
            expected_softap_state,
            "Softap state doesn't match with expected state")

def get_current_number_of_softap_clients(ad, callbackId):
    """pop up all of softap client updated event from queue.
    Args:
        callbackId: Id of the callback associated with registering.

    Returns:
        If exist aleast callback, returns last updated number_of_softap_clients.
        Returns None when no any match callback event in queue.
    """
    eventStr = wifi_constants.SOFTAP_CALLBACK_EVENT + str(
            callbackId) + wifi_constants.SOFTAP_NUMBER_CLIENTS_CHANGED
    events = ad.ed.pop_all(eventStr)
    for event in events:
        num_of_clients = event['data'][wifi_constants.
                SOFTAP_NUMBER_CLIENTS_CALLBACK_KEY]
    if len(events) == 0:
        return None
    return num_of_clients

def get_current_softap_info(ad, callbackId, least_one):
    """pop up all of softap info changed event from queue.
    Args:
        callbackId: Id of the callback associated with registering.
        least_one: Wait for the info callback event before pop all.
    Returns:
        Returns last updated information of softap.
    """
    eventStr = wifi_constants.SOFTAP_CALLBACK_EVENT + str(
            callbackId) + wifi_constants.SOFTAP_INFO_CHANGED
    ad.log.info("softap info dump from eventStr %s",
                eventStr)
    frequency = 0
    bandwidth = 0
    if (least_one):
        event = ad.ed.pop_event(eventStr, SHORT_TIMEOUT)
        frequency = event['data'][wifi_constants.
                SOFTAP_INFO_FREQUENCY_CALLBACK_KEY]
        bandwidth = event['data'][wifi_constants.
                SOFTAP_INFO_BANDWIDTH_CALLBACK_KEY]
        ad.log.info("softap info updated, frequency is %s, bandwidth is %s",
            frequency, bandwidth)

    events = ad.ed.pop_all(eventStr)
    for event in events:
        frequency = event['data'][wifi_constants.
                SOFTAP_INFO_FREQUENCY_CALLBACK_KEY]
        bandwidth = event['data'][wifi_constants.
                SOFTAP_INFO_BANDWIDTH_CALLBACK_KEY]
    ad.log.info("softap info, frequency is %s, bandwidth is %s",
            frequency, bandwidth)
    return frequency, bandwidth



def get_ssrdumps(ad):
    """Pulls dumps in the ssrdump dir
    Args:
        ad: android device object.
        test_name: test case name
    """
    logs = ad.get_file_names("/data/vendor/ssrdump/")
    if logs:
        ad.log.info("Pulling ssrdumps %s", logs)
        log_path = os.path.join(ad.device_log_path, "SSRDUMPS_%s" % ad.serial)
        os.makedirs(log_path, exist_ok=True)
        ad.pull_files(logs, log_path)
    ad.adb.shell("find /data/vendor/ssrdump/ -type f -delete")

def start_pcap(pcap, wifi_band, test_name):
    """Start packet capture in monitor mode.

    Args:
        pcap: packet capture object
        wifi_band: '2g' or '5g' or 'dual'
        test_name: test name to be used for pcap file name

    Returns:
        Dictionary with wifi band as key and the tuple
        (pcap Process object, log directory) as the value
    """
    log_dir = os.path.join(
        context.get_current_context().get_full_output_path(), 'PacketCapture')
    os.makedirs(log_dir, exist_ok=True)
    if wifi_band == 'dual':
        bands = [BAND_2G, BAND_5G]
    else:
        bands = [wifi_band]
    procs = {}
    for band in bands:
        proc = pcap.start_packet_capture(band, log_dir, test_name)
        procs[band] = (proc, os.path.join(log_dir, test_name))
    return procs


def stop_pcap(pcap, procs, test_status=None):
    """Stop packet capture in monitor mode.

    Since, the pcap logs in monitor mode can be very large, we will
    delete them if they are not required. 'test_status' if True, will delete
    the pcap files. If False, we will keep them.

    Args:
        pcap: packet capture object
        procs: dictionary returned by start_pcap
        test_status: status of the test case
    """
    for proc, fname in procs.values():
        pcap.stop_packet_capture(proc)

    if test_status:
        shutil.rmtree(os.path.dirname(fname))

def verify_mac_not_found_in_pcap(ad, mac, packets):
    """Verify that a mac address is not found in the captured packets.

    Args:
        ad: android device object
        mac: string representation of the mac address
        packets: packets obtained by rdpcap(pcap_fname)
    """
    for pkt in packets:
        logging.debug("Packet Summary = %s", pkt.summary())
        if mac in pkt.summary():
            asserts.fail("Device %s caught Factory MAC: %s in packet sniffer."
                         "Packet = %s" % (ad.serial, mac, pkt.show()))

def verify_mac_is_found_in_pcap(ad, mac, packets):
    """Verify that a mac address is found in the captured packets.

    Args:
        ad: android device object
        mac: string representation of the mac address
        packets: packets obtained by rdpcap(pcap_fname)
    """
    for pkt in packets:
        if mac in pkt.summary():
            return
    asserts.fail("Did not find MAC = %s in packet sniffer."
                 "for device %s" % (mac, ad.serial))

def start_cnss_diags(ads, cnss_diag_file, pixel_models):
    for ad in ads:
        start_cnss_diag(ad, cnss_diag_file, pixel_models)


def start_cnss_diag(ad, cnss_diag_file, pixel_models):
    """Start cnss_diag to record extra wifi logs

    Args:
        ad: android device object.
        cnss_diag_file: cnss diag config file to push to device.
        pixel_models: pixel devices.
    """
    if ad.model not in pixel_models:
        ad.log.info("Device not supported to collect pixel logger")
        return
    if ad.model in wifi_constants.DEVICES_USING_LEGACY_PROP:
        prop = wifi_constants.LEGACY_CNSS_DIAG_PROP
    else:
        prop = wifi_constants.CNSS_DIAG_PROP
    if ad.adb.getprop(prop) != 'true':
        if not int(ad.adb.shell("ls -l %s%s | wc -l" %
                                (CNSS_DIAG_CONFIG_PATH,
                                 CNSS_DIAG_CONFIG_FILE))):
            ad.adb.push("%s %s" % (cnss_diag_file, CNSS_DIAG_CONFIG_PATH))
        ad.adb.shell("find /data/vendor/wifi/cnss_diag/wlan_logs/ -type f -delete")
        ad.adb.shell("setprop %s true" % prop, ignore_status=True)


def stop_cnss_diags(ads, pixel_models):
    for ad in ads:
        stop_cnss_diag(ad, pixel_models)


def stop_cnss_diag(ad, pixel_models):
    """Stops cnss_diag

    Args:
        ad: android device object.
        pixel_models: pixel devices.
    """
    if ad.model not in pixel_models:
        ad.log.info("Device not supported to collect pixel logger")
        return
    if ad.model in wifi_constants.DEVICES_USING_LEGACY_PROP:
        prop = wifi_constants.LEGACY_CNSS_DIAG_PROP
    else:
        prop = wifi_constants.CNSS_DIAG_PROP
    ad.adb.shell("setprop %s false" % prop, ignore_status=True)


def get_cnss_diag_log(ad):
    """Pulls the cnss_diag logs in the wlan_logs dir
    Args:
        ad: android device object.
    """
    logs = ad.get_file_names("/data/vendor/wifi/cnss_diag/wlan_logs/")
    if logs:
        ad.log.info("Pulling cnss_diag logs %s", logs)
        log_path = os.path.join(ad.device_log_path, "CNSS_DIAG_%s" % ad.serial)
        os.makedirs(log_path, exist_ok=True)
        ad.pull_files(logs, log_path)


LinkProbeResult = namedtuple('LinkProbeResult', (
    'is_success', 'stdout', 'elapsed_time', 'failure_reason'))


def send_link_probe(ad):
    """Sends a link probe to the currently connected AP, and returns whether the
    probe succeeded or not.

    Args:
         ad: android device object
    Returns:
        LinkProbeResult namedtuple
    """
    stdout = ad.adb.shell('cmd wifi send-link-probe')
    asserts.assert_false('Error' in stdout or 'Exception' in stdout,
                         'Exception while sending link probe: ' + stdout)

    is_success = False
    elapsed_time = None
    failure_reason = None
    if 'succeeded' in stdout:
        is_success = True
        elapsed_time = next(
            (int(token) for token in stdout.split() if token.isdigit()), None)
    elif 'failed with reason' in stdout:
        failure_reason = next(
            (int(token) for token in stdout.split() if token.isdigit()), None)
    else:
        asserts.fail('Unexpected link probe result: ' + stdout)

    return LinkProbeResult(
        is_success=is_success, stdout=stdout,
        elapsed_time=elapsed_time, failure_reason=failure_reason)


def send_link_probes(ad, num_probes, delay_sec):
    """Sends a sequence of link probes to the currently connected AP, and
    returns whether the probes succeeded or not.

    Args:
         ad: android device object
         num_probes: number of probes to perform
         delay_sec: delay time between probes, in seconds
    Returns:
        List[LinkProbeResult] one LinkProbeResults for each probe
    """
    logging.info('Sending link probes')
    results = []
    for _ in range(num_probes):
        # send_link_probe() will also fail the test if it sees an exception
        # in the stdout of the adb shell command
        result = send_link_probe(ad)
        logging.info('link probe results: ' + str(result))
        results.append(result)
        time.sleep(delay_sec)

    return results


def ap_setup(test, index, ap, network, bandwidth=80, channel=6):
        """Set up the AP with provided network info.

        Args:
            test: the calling test class object.
            index: int, index of the AP.
            ap: access_point object of the AP.
            network: dict with information of the network, including ssid,
                     password and bssid.
            bandwidth: the operation bandwidth for the AP, default 80MHz.
            channel: the channel number for the AP.
        Returns:
            brconfigs: the bridge interface configs
        """
        bss_settings = []
        ssid = network[WifiEnums.SSID_KEY]
        test.access_points[index].close()
        time.sleep(5)

        # Configure AP as required.
        if "password" in network.keys():
            password = network["password"]
            security = hostapd_security.Security(
                security_mode="wpa", password=password)
        else:
            security = hostapd_security.Security(security_mode=None, password=None)
        config = hostapd_ap_preset.create_ap_preset(
                                                    channel=channel,
                                                    ssid=ssid,
                                                    security=security,
                                                    bss_settings=bss_settings,
                                                    vht_bandwidth=bandwidth,
                                                    profile_name='whirlwind',
                                                    iface_wlan_2g=ap.wlan_2g,
                                                    iface_wlan_5g=ap.wlan_5g)
        ap.start_ap(config)
        logging.info("AP started on channel {} with SSID {}".format(channel, ssid))


def turn_ap_off(test, AP):
    """Bring down hostapd on the Access Point.
    Args:
        test: The test class object.
        AP: int, indicating which AP to turn OFF.
    """
    hostapd_2g = test.access_points[AP-1]._aps['wlan0'].hostapd
    if hostapd_2g.is_alive():
        hostapd_2g.stop()
        logging.debug('Turned WLAN0 AP%d off' % AP)
    hostapd_5g = test.access_points[AP-1]._aps['wlan1'].hostapd
    if hostapd_5g.is_alive():
        hostapd_5g.stop()
        logging.debug('Turned WLAN1 AP%d off' % AP)


def turn_ap_on(test, AP):
    """Bring up hostapd on the Access Point.
    Args:
        test: The test class object.
        AP: int, indicating which AP to turn ON.
    """
    hostapd_2g = test.access_points[AP-1]._aps['wlan0'].hostapd
    if not hostapd_2g.is_alive():
        hostapd_2g.start(hostapd_2g.config)
        logging.debug('Turned WLAN0 AP%d on' % AP)
    hostapd_5g = test.access_points[AP-1]._aps['wlan1'].hostapd
    if not hostapd_5g.is_alive():
        hostapd_5g.start(hostapd_5g.config)
        logging.debug('Turned WLAN1 AP%d on' % AP)


def turn_location_off_and_scan_toggle_off(ad):
    """Turns off wifi location scans."""
    utils.set_location_service(ad, False)
    ad.droid.wifiScannerToggleAlwaysAvailable(False)
    msg = "Failed to turn off location service's scan."
    asserts.assert_true(not ad.droid.wifiScannerIsAlwaysAvailable(), msg)


def set_softap_channel(dut, ap_iface='wlan1', cs_count=10, channel=2462):
    """ Set SoftAP mode channel

    Args:
        dut: android device object
        ap_iface: interface of SoftAP mode.
        cs_count: how many beacon frames before switch channel, default = 10
        channel: a wifi channel.
    """
    chan_switch_cmd = 'hostapd_cli -i {} chan_switch {} {}'
    chan_switch_cmd_show = chan_switch_cmd.format(ap_iface,cs_count,channel)
    dut.log.info('adb shell {}'.format(chan_switch_cmd_show))
    chan_switch_result = dut.adb.shell(chan_switch_cmd.format(ap_iface,
                                                              cs_count,
                                                              channel))
    if chan_switch_result == 'OK':
        dut.log.info('switch hotspot channel to {}'.format(channel))
        return chan_switch_result

    asserts.fail("Failed to switch hotspot channel")

def get_wlan0_link(dut):
    """ get wlan0 interface status"""
    get_wlan0 = 'wpa_cli -iwlan0 -g@android:wpa_wlan0 IFNAME=wlan0 status'
    out = dut.adb.shell(get_wlan0)
    out = dict(re.findall(r'(\S+)=(".*?"|\S+)', out))
    asserts.assert_true("ssid" in out,
                        "Client doesn't connect to any network")
    return out
