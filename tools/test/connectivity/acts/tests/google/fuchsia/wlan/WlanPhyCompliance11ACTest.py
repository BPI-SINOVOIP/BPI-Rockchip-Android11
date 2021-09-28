#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
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
import re

from acts import utils
from acts.controllers.ap_lib.hostapd_security import Security
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib import hostapd_config
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import validate_setup_ap_and_associate
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.utils import rand_ascii_str

# AC Capabilities
"""
Capabilities Not Supported on Whirlwind:
    - Supported Channel Width ([VHT160], [VHT160-80PLUS80]): 160mhz and 80+80
        unsupported
    - SU Beamformer [SU-BEAMFORMER]
    - SU Beamformee [SU-BEAMFORMEE]
    - MU Beamformer [MU-BEAMFORMER]
    - MU Beamformee [MU-BEAMFORMEE]
    - BF Antenna ([BF-ANTENNA-2], [BF-ANTENNA-3], [BF-ANTENNA-4])
    - Rx STBC 2, 3, & 4 ([RX-STBC-12],[RX-STBC-123],[RX-STBC-124])
    - VHT Link Adaptation ([VHT-LINK-ADAPT2],[VHT-LINK-ADAPT3])
    - VHT TXOP Power Save [VHT-TXOP-PS]
    - HTC-VHT [HTC-VHT]
"""
VHT_MAX_MPDU_LEN = [
    hostapd_constants.AC_CAPABILITY_MAX_MPDU_7991,
    hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454, ''
]
RXLDPC = [hostapd_constants.AC_CAPABILITY_RXLDPC, '']
SHORT_GI_80 = [hostapd_constants.AC_CAPABILITY_SHORT_GI_80, '']
TX_STBC = [hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1, '']
RX_STBC = [hostapd_constants.AC_CAPABILITY_RX_STBC_1, '']
MAX_A_MPDU = [
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP0,
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP1,
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP2,
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP3,
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP4,
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP5,
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP6,
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7, ''
]
RX_ANTENNA = [hostapd_constants.AC_CAPABILITY_RX_ANTENNA_PATTERN, '']
TX_ANTENNA = [hostapd_constants.AC_CAPABILITY_TX_ANTENNA_PATTERN, '']

# Default 11N Capabilities
N_CAPABS_40MHZ = [
    hostapd_constants.N_CAPABILITY_LDPC, hostapd_constants.N_CAPABILITY_SGI20,
    hostapd_constants.N_CAPABILITY_RX_STBC1,
    hostapd_constants.N_CAPABILITY_SGI20, hostapd_constants.N_CAPABILITY_SGI40,
    hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935,
    hostapd_constants.N_CAPABILITY_HT40_PLUS
]

N_CAPABS_20MHZ = [
    hostapd_constants.N_CAPABILITY_LDPC, hostapd_constants.N_CAPABILITY_SGI20,
    hostapd_constants.N_CAPABILITY_RX_STBC1,
    hostapd_constants.N_CAPABILITY_SGI20,
    hostapd_constants.N_CAPABILITY_MAX_AMSDU_7935,
    hostapd_constants.N_CAPABILITY_HT20
]

# Default wpa2 profile.
WPA2_SECURITY = Security(security_mode=hostapd_constants.WPA2_STRING,
                         password=rand_ascii_str(20),
                         wpa_cipher=hostapd_constants.WPA2_DEFAULT_CIPER,
                         wpa2_cipher=hostapd_constants.WPA2_DEFAULT_CIPER)


def generate_test_name(settings):
    """Generates a test name string based on the ac_capabilities for
    a test case.

    Args:
        settings: a dict with the test settings (bandwidth, security, ac_capabs)

    Returns:
        A string test case name
    """
    chbw = settings['chbw']
    sec = 'wpa2' if settings['security'] else 'open'
    ret = []
    for cap in hostapd_constants.AC_CAPABILITIES_MAPPING.keys():
        if cap in settings['ac_capabilities']:
            ret.append(hostapd_constants.AC_CAPABILITIES_MAPPING[cap])
    return 'test_11ac_%smhz_%s_%s' % (chbw, sec, ''.join(ret))


# 6912 test cases
class WlanPhyCompliance11ACTest(WifiBaseTest):
    """Tests for validating 11ac PHYS.

    Test Bed Requirement:
    * One Android device or Fuchsia device
    * One Access Point
    """
    def __init__(self, controllers):
        WifiBaseTest.__init__(self, controllers)
        self.tests = [
            'test_11ac_capabilities_20mhz_open',
            'test_11ac_capabilities_40mhz_open',
            'test_11ac_capabilities_80mhz_open',
            'test_11ac_capabilities_20mhz_wpa2',
            'test_11ac_capabilities_40mhz_wpa2',
            'test_11ac_capabilities_80mhz_wpa2'
        ]
        if 'debug_11ac_tests' in self.user_params:
            self.tests.append('test_11ac_capabilities_debug')

    def setup_class(self):
        super().setup_class()
        if 'dut' in self.user_params:
            if self.user_params['dut'] == 'fuchsia_devices':
                self.dut = create_wlan_device(self.fuchsia_devices[0])
            elif self.user_params['dut'] == 'android_devices':
                self.dut = create_wlan_device(self.android_devices[0])
            else:
                raise ValueError('Invalid DUT specified in config. (%s)' %
                                 self.user_params['dut'])
        else:
            self.dut = create_wlan_device(self.android_devices[0])

        self.access_point = self.access_points[0]
        self.android_devices = getattr(self, 'android_devices', [])
        self.access_point.stop_all_aps()

    def setup_test(self):
        for ad in self.android_devices:
            ad.droid.wakeLockAcquireBright()
            ad.droid.wakeUpNow()
        self.dut.wifi_toggle_state(True)

    def teardown_test(self):
        for ad in self.android_devices:
            ad.droid.wakeLockRelease()
            ad.droid.goToSleepNow()
        self.dut.turn_location_off_and_scan_toggle_off()
        self.dut.disconnect()
        self.dut.reset_wifi()
        self.access_point.stop_all_aps()

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)
        self.dut.get_log(test_name, begin_time)

    def setup_and_connect(self, ap_settings):
        """Uses ap_settings to set up ap and then attempts to associate a DUT.

        Args:
            ap_settings: a dict containing test case settings, including
                bandwidth, security, n_capabilities, and ac_capabilities

        """
        security = ap_settings['security']
        chbw = ap_settings['chbw']
        password = None
        if security:
            password = security.password
        n_capabilities = ap_settings['n_capabilities']
        ac_capabilities = ap_settings['ac_capabilities']

        validate_setup_ap_and_associate(access_point=self.access_point,
                                        client=self.dut,
                                        profile_name='whirlwind',
                                        mode=hostapd_constants.MODE_11AC_MIXED,
                                        channel=36,
                                        n_capabilities=n_capabilities,
                                        ac_capabilities=ac_capabilities,
                                        force_wmm=True,
                                        ssid=utils.rand_ascii_str(20),
                                        security=security,
                                        vht_bandwidth=chbw,
                                        password=password)

    # 864 test cases
    def test_11ac_capabilities_20mhz_open(self):
        test_list = []
        for combination in itertools.product(VHT_MAX_MPDU_LEN, RXLDPC, RX_STBC,
                                             TX_STBC, MAX_A_MPDU, RX_ANTENNA,
                                             TX_ANTENNA):
            test_list.append({
                'chbw': 20,
                'security': None,
                'n_capabilities': N_CAPABS_20MHZ,
                'ac_capabilities': combination
            })
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)

    # 864 test cases
    def test_11ac_capabilities_40mhz_open(self):
        test_list = []
        for combination in itertools.product(VHT_MAX_MPDU_LEN, RXLDPC, RX_STBC,
                                             TX_STBC, MAX_A_MPDU, RX_ANTENNA,
                                             TX_ANTENNA):
            test_list.append({
                'chbw': 40,
                'security': None,
                'n_capabilities': N_CAPABS_40MHZ,
                'ac_capabilities': combination
            })
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)

    # 1728 test cases
    def test_11ac_capabilities_80mhz_open(self):
        test_list = []
        for combination in itertools.product(VHT_MAX_MPDU_LEN, RXLDPC,
                                             SHORT_GI_80, RX_STBC, TX_STBC,
                                             MAX_A_MPDU, RX_ANTENNA,
                                             TX_ANTENNA):
            test_list.append({
                'chbw': 80,
                'security': None,
                'n_capabilities': N_CAPABS_40MHZ,
                'ac_capabilities': combination
            })
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)

    # 864 test cases
    def test_11ac_capabilities_20mhz_wpa2(self):
        test_list = []
        for combination in itertools.product(VHT_MAX_MPDU_LEN, RXLDPC, RX_STBC,
                                             TX_STBC, MAX_A_MPDU, RX_ANTENNA,
                                             TX_ANTENNA):
            test_list.append({
                'chbw': 20,
                'security': WPA2_SECURITY,
                'n_capabilities': N_CAPABS_20MHZ,
                'ac_capabilities': combination
            })
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)

    # 864 test cases
    def test_11ac_capabilities_40mhz_wpa2(self):
        test_list = []
        for combination in itertools.product(VHT_MAX_MPDU_LEN, RXLDPC, RX_STBC,
                                             TX_STBC, MAX_A_MPDU, RX_ANTENNA,
                                             TX_ANTENNA):
            test_list.append({
                'chbw': 40,
                'security': WPA2_SECURITY,
                'n_capabilities': N_CAPABS_40MHZ,
                'ac_capabilities': combination
            })
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)

    # 1728 test cases
    def test_11ac_capabilities_80mhz_wpa2(self):
        test_list = []
        for combination in itertools.product(VHT_MAX_MPDU_LEN, RXLDPC,
                                             SHORT_GI_80, RX_STBC, TX_STBC,
                                             MAX_A_MPDU, RX_ANTENNA,
                                             TX_ANTENNA):
            test_list.append({
                'chbw': 80,
                'security': WPA2_SECURITY,
                'n_capabilities': N_CAPABS_40MHZ,
                'ac_capabilities': combination
            })
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)

    def test_11ac_capabilities_debug(self):
        chbw_sec_capabs = re.compile(
            r'.*?([0-9]*)mhz.*?(open|wpa2).*?(\[.*\])', re.IGNORECASE)
        test_list = []
        for test_name in self.user_params['debug_11ac_tests']:
            test_to_run = re.match(chbw_sec_capabs, test_name)
            chbw = int(test_to_run.group(1))
            security = test_to_run.group(2)
            capabs = test_to_run.group(3)
            if chbw >= 40:
                n_capabs = N_CAPABS_40MHZ
            else:
                n_capabs = N_CAPABS_20MHZ
            if security.lower() == 'open':
                security = None
            elif security.lower() == 'wpa2':
                security = WPA2_SECURITY
            if capabs:
                ac_capabs_strings = re.findall(r'\[.*?\]', capabs)
                ac_capabs = [
                    hostapd_constants.AC_CAPABILITIES_MAPPING_INVERSE[k]
                    for k in ac_capabs_strings
                ]
            test_list.append({
                'chbw': chbw,
                'security': security,
                'n_capabilities': n_capabs,
                'ac_capabilities': ac_capabs
            })
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)
