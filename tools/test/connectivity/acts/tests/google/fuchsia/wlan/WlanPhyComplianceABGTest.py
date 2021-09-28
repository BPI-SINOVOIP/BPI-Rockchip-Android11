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

from acts import utils

from acts.controllers.ap_lib import hostapd_ap_preset
from acts.controllers.ap_lib import hostapd_constants
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import validate_setup_ap_and_associate
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest


class WlanPhyComplianceABGTest(WifiBaseTest):
    """Tests for validating 11a, 11b, and 11g PHYS.

    Test Bed Requirement:
    * One Android device or Fuchsia device
    * One Access Point
    """
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
            # Default is an android device, just like the other tests
            self.dut = create_wlan_device(self.android_devices[0])

        self.access_point = self.access_points[0]
        open_network = self.get_open_network(False, [])
        open_network_min_len = self.get_open_network(
            False, [],
            ssid_length_2g=hostapd_constants.AP_SSID_MIN_LENGTH_2G,
            ssid_length_5g=hostapd_constants.AP_SSID_MIN_LENGTH_5G)
        open_network_max_len = self.get_open_network(
            False, [],
            ssid_length_2g=hostapd_constants.AP_SSID_MAX_LENGTH_2G,
            ssid_length_5g=hostapd_constants.AP_SSID_MAX_LENGTH_5G)
        self.open_network_2g = open_network['2g']
        self.open_network_5g = open_network['5g']
        self.open_network_max_len_2g = open_network_max_len['2g']
        self.open_network_max_len_2g['SSID'] = (
            self.open_network_max_len_2g['SSID'][3:])
        self.open_network_max_len_5g = open_network_max_len['5g']
        self.open_network_max_len_5g['SSID'] = (
            self.open_network_max_len_5g['SSID'][3:])
        self.open_network_min_len_2g = open_network_min_len['2g']
        self.open_network_min_len_2g['SSID'] = (
            self.open_network_min_len_2g['SSID'][3:])
        self.open_network_min_len_5g = open_network_min_len['5g']
        self.open_network_min_len_5g['SSID'] = (
            self.open_network_min_len_5g['SSID'][3:])
        self.utf8_ssid_2g = '2𝔤_𝔊𝔬𝔬𝔤𝔩𝔢'
        self.utf8_ssid_5g = '5𝔤_𝔊𝔬𝔬𝔤𝔩𝔢'

        self.access_point.stop_all_aps()

    def setup_test(self):
        if hasattr(self, "android_devices"):
            for ad in self.android_devices:
                ad.droid.wakeLockAcquireBright()
                ad.droid.wakeUpNow()
        self.dut.wifi_toggle_state(True)

    def teardown_test(self):
        if hasattr(self, "android_devices"):
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

    def test_associate_11b_only_long_preamble(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            preamble=False)

    def test_associate_11b_only_short_preamble(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            preamble=True)

    def test_associate_11b_only_minimal_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            beacon_interval=15)

    def test_associate_11b_only_maximum_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            beacon_interval=1024)

    def test_associate_11b_only_frag_threshold_430(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            frag_threshold=430)

    def test_associate_11b_only_rts_threshold_256(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            rts_threshold=256)

    def test_associate_11b_only_rts_256_frag_430(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            rts_threshold=256,
            frag_threshold=430)

    def test_associate_11b_only_high_dtim_low_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            dtim_period=3,
            beacon_interval=100)

    def test_associate_11b_only_low_dtim_high_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            dtim_period=1,
            beacon_interval=300)

    def test_associate_11b_only_with_WMM_with_default_values(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_11B_DEFAULT_PARAMS)

    def test_associate_11b_only_with_WMM_with_non_default_values(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_NON_DEFAULT_PARAMS)

    def test_associate_11b_only_with_WMM_ACM_on_BK(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11b_only_with_WMM_ACM_on_BE(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BE)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11b_only_with_WMM_ACM_on_VI(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_VI)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11b_only_with_WMM_ACM_on_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11b_only_with_WMM_ACM_on_BK_BE_VI(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_BE,
            hostapd_constants.WMM_ACM_VI)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11b_only_with_WMM_ACM_on_BK_BE_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_BE,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11b_only_with_WMM_ACM_on_BK_VI_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_VI,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11b_only_with_WMM_ACM_on_BE_VI_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BE, hostapd_constants.WMM_ACM_VI,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11b_only_with_country_code(self):
        country_info = utils.merge_dicts(
            hostapd_constants.ENABLE_IEEE80211D,
            hostapd_constants.COUNTRY_STRING['ALL'],
            hostapd_constants.COUNTRY_CODE['UNITED_STATES'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=country_info)

    def test_associate_11b_only_with_non_country_code(self):
        country_info = utils.merge_dicts(
            hostapd_constants.ENABLE_IEEE80211D,
            hostapd_constants.COUNTRY_STRING['ALL'],
            hostapd_constants.COUNTRY_CODE['NON_COUNTRY'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=country_info)

    def test_associate_11b_only_with_hidden_ssid(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            hidden=True)

    def test_associate_11b_only_with_vendor_ie_in_beacon_correct_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'])

    def test_associate_11b_only_with_vendor_ie_in_beacon_zero_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'])

    def test_associate_11b_only_with_vendor_ie_in_assoc_correct_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_association_response'])

    def test_associate_11b_only_with_vendor_ie_in_assoc_zero_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.VENDOR_IE[
                'zero_length_association_'
                'response_without_data'])

    def test_associate_11a_only_long_preamble(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            preamble=False)

    def test_associate_11a_only_short_preamble(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            preamble=True)

    def test_associate_11a_only_minimal_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            beacon_interval=15)

    def test_associate_11a_only_maximum_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            beacon_interval=1024)

    def test_associate_11a_only_frag_threshold_430(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            frag_threshold=430)

    def test_associate_11a_only_rts_threshold_256(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            rts_threshold=256)

    def test_associate_11a_only_rts_256_frag_430(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            rts_threshold=256,
            frag_threshold=430)

    def test_associate_11a_only_high_dtim_low_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            dtim_period=3,
            beacon_interval=100)

    def test_associate_11a_only_low_dtim_high_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            dtim_period=1,
            beacon_interval=300)

    def test_associate_11a_only_with_WMM_with_default_values(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.
            WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS)

    def test_associate_11a_only_with_WMM_with_non_default_values(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_NON_DEFAULT_PARAMS)

    def test_associate_11a_only_with_WMM_ACM_on_BK(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11a_only_with_WMM_ACM_on_BE(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BE)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11a_only_with_WMM_ACM_on_VI(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_VI)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11a_only_with_WMM_ACM_on_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11a_only_with_WMM_ACM_on_BK_BE_VI(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_BE,
            hostapd_constants.WMM_ACM_VI)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11a_only_with_WMM_ACM_on_BK_BE_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_BE,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11a_only_with_WMM_ACM_on_BK_VI_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_VI,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11a_only_with_WMM_ACM_on_BE_VI_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BE, hostapd_constants.WMM_ACM_VI,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11a_only_with_country_code(self):
        country_info = utils.merge_dicts(
            hostapd_constants.ENABLE_IEEE80211D,
            hostapd_constants.COUNTRY_STRING['ALL'],
            hostapd_constants.COUNTRY_CODE['UNITED_STATES'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            additional_ap_parameters=country_info)

    def test_associate_11a_only_with_non_country_code(self):
        country_info = utils.merge_dicts(
            hostapd_constants.ENABLE_IEEE80211D,
            hostapd_constants.COUNTRY_STRING['ALL'],
            hostapd_constants.COUNTRY_CODE['NON_COUNTRY'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            additional_ap_parameters=country_info)

    def test_associate_11a_only_with_hidden_ssid(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            hidden=True)

    def test_associate_11a_only_with_vendor_ie_in_beacon_correct_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'])

    def test_associate_11a_only_with_vendor_ie_in_beacon_zero_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'])

    def test_associate_11a_only_with_vendor_ie_in_assoc_correct_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_association_response'])

    def test_associate_11a_only_with_vendor_ie_in_assoc_zero_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.VENDOR_IE[
                'zero_length_association_'
                'response_without_data'])

    def test_associate_11g_only_long_preamble(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            preamble=False,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_short_preamble(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            preamble=True,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_minimal_beacon_interval(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            beacon_interval=15,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_maximum_beacon_interval(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            beacon_interval=1024,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_frag_threshold_430(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            frag_threshold=430,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_rts_threshold_256(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            rts_threshold=256,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_rts_256_frag_430(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            rts_threshold=256,
            frag_threshold=430,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_high_dtim_low_beacon_interval(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            dtim_period=3,
            beacon_interval=100,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_low_dtim_high_beacon_interval(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            dtim_period=1,
            beacon_interval=300,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_with_WMM_with_default_values(self):
        data_rates = utils.merge_dicts(
            hostapd_constants.OFDM_DATA_RATES,
            hostapd_constants.OFDM_ONLY_BASIC_RATES,
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_with_WMM_with_non_default_values(self):
        data_rates = utils.merge_dicts(
            hostapd_constants.OFDM_DATA_RATES,
            hostapd_constants.OFDM_ONLY_BASIC_RATES,
            hostapd_constants.WMM_NON_DEFAULT_PARAMS)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_with_WMM_ACM_on_BK(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11g_only_with_WMM_ACM_on_BE(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BE, data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11g_only_with_WMM_ACM_on_VI(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_VI, data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11g_only_with_WMM_ACM_on_VO(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_VO, data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11g_only_with_WMM_ACM_on_BK_BE_VI(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_BE,
            hostapd_constants.WMM_ACM_VI, data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11g_only_with_WMM_ACM_on_BK_BE_VO(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_BE,
            hostapd_constants.WMM_ACM_VO, data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11g_only_with_WMM_ACM_on_BK_VI_VO(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_VI,
            hostapd_constants.WMM_ACM_VO, data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11g_only_with_WMM_ACM_on_BE_VI_VO(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BE, hostapd_constants.WMM_ACM_VI,
            hostapd_constants.WMM_ACM_VO, data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11g_only_with_country_code(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        country_info = utils.merge_dicts(
            hostapd_constants.ENABLE_IEEE80211D,
            hostapd_constants.COUNTRY_STRING['ALL'],
            hostapd_constants.COUNTRY_CODE['UNITED_STATES'], data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=country_info)

    def test_associate_11g_only_with_non_country_code(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        country_info = utils.merge_dicts(
            hostapd_constants.ENABLE_IEEE80211D,
            hostapd_constants.COUNTRY_STRING['ALL'],
            hostapd_constants.COUNTRY_CODE['NON_COUNTRY'], data_rates)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=country_info)

    def test_associate_11g_only_with_hidden_ssid(self):
        data_rates = utils.merge_dicts(hostapd_constants.OFDM_DATA_RATES,
                                       hostapd_constants.OFDM_ONLY_BASIC_RATES)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            hidden=True,
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_with_vendor_ie_in_beacon_correct_length(self):
        data_rates = utils.merge_dicts(
            hostapd_constants.OFDM_DATA_RATES,
            hostapd_constants.OFDM_ONLY_BASIC_RATES,
            hostapd_constants.VENDOR_IE['correct_length_beacon'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_with_vendor_ie_in_beacon_zero_length(self):
        data_rates = utils.merge_dicts(
            hostapd_constants.OFDM_DATA_RATES,
            hostapd_constants.OFDM_ONLY_BASIC_RATES,
            hostapd_constants.VENDOR_IE['zero_length_beacon_without_data'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_with_vendor_ie_in_assoc_correct_length(self):
        data_rates = utils.merge_dicts(
            hostapd_constants.OFDM_DATA_RATES,
            hostapd_constants.OFDM_ONLY_BASIC_RATES,
            hostapd_constants.VENDOR_IE['correct_length_association_response'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_with_vendor_ie_in_assoc_zero_length(self):
        data_rates = utils.merge_dicts(
            hostapd_constants.OFDM_DATA_RATES,
            hostapd_constants.OFDM_ONLY_BASIC_RATES,
            hostapd_constants.VENDOR_IE['correct_length_association_response'],
            hostapd_constants.VENDOR_IE['zero_length_association_'
                                        'response_without_data'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=data_rates)

    def test_associate_11bg_only_long_preamble(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            preamble=False)

    def test_associate_11bg_short_preamble(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            preamble=True)

    def test_associate_11bg_minimal_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            beacon_interval=15)

    def test_associate_11bg_maximum_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            beacon_interval=1024)

    def test_associate_11bg_frag_threshold_430(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            frag_threshold=430)

    def test_associate_11bg_rts_threshold_256(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            rts_threshold=256)

    def test_associate_11bg_rts_256_frag_430(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            rts_threshold=256,
            frag_threshold=430)

    def test_associate_11bg_high_dtim_low_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            dtim_period=3,
            beacon_interval=100)

    def test_associate_11bg_low_dtim_high_beacon_interval(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            dtim_period=1,
            beacon_interval=300)

    def test_associate_11bg_with_WMM_with_default_values(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.
            WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS)

    def test_associate_11bg_with_WMM_with_non_default_values(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_NON_DEFAULT_PARAMS)

    def test_associate_11bg_with_WMM_ACM_on_BK(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11bg_with_WMM_ACM_on_BE(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BE)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11bg_with_WMM_ACM_on_VI(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_VI)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11bg_with_WMM_ACM_on_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11bg_with_WMM_ACM_on_BK_BE_VI(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_BE,
            hostapd_constants.WMM_ACM_VI)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11bg_with_WMM_ACM_on_BK_BE_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_BE,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11bg_with_WMM_ACM_on_BK_VI_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BK, hostapd_constants.WMM_ACM_VI,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11bg_with_WMM_ACM_on_BE_VI_VO(self):
        wmm_acm_bits_enabled = utils.merge_dicts(
            hostapd_constants.WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            hostapd_constants.WMM_ACM_BE, hostapd_constants.WMM_ACM_VI,
            hostapd_constants.WMM_ACM_VO)
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=wmm_acm_bits_enabled)

    def test_associate_11bg_with_country_code(self):
        country_info = utils.merge_dicts(
            hostapd_constants.ENABLE_IEEE80211D,
            hostapd_constants.COUNTRY_STRING['ALL'],
            hostapd_constants.COUNTRY_CODE['UNITED_STATES'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=country_info)

    def test_associate_11bg_with_non_country_code(self):
        country_info = utils.merge_dicts(
            hostapd_constants.ENABLE_IEEE80211D,
            hostapd_constants.COUNTRY_STRING['ALL'],
            hostapd_constants.COUNTRY_CODE['NON_COUNTRY'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=country_info)

    def test_associate_11bg_only_with_hidden_ssid(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            hidden=True)

    def test_associate_11bg_with_vendor_ie_in_beacon_correct_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'])

    def test_associate_11bg_with_vendor_ie_in_beacon_zero_length(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'])

    def test_associate_11g_only_with_vendor_ie_in_assoc_correct_length(self):
        data_rates = utils.merge_dicts(
            hostapd_constants.OFDM_DATA_RATES,
            hostapd_constants.OFDM_ONLY_BASIC_RATES,
            hostapd_constants.VENDOR_IE['correct_length_association_response'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=data_rates)

    def test_associate_11g_only_with_vendor_ie_in_assoc_zero_length(self):
        data_rates = utils.merge_dicts(
            hostapd_constants.OFDM_DATA_RATES,
            hostapd_constants.OFDM_ONLY_BASIC_RATES,
            hostapd_constants.VENDOR_IE['correct_length_association_response'],
            hostapd_constants.VENDOR_IE['zero_length_association_'
                                        'response_without_data'])
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ag_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_2g['SSID'],
            additional_ap_parameters=data_rates)

    def test_minimum_ssid_length_2g_11n_20mhz(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_min_len_2g['SSID'])

    def test_minimum_ssid_length_5g_11ac_80mhz(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_min_len_5g['SSID'])

    def test_maximum_ssid_length_2g_11n_20mhz(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.open_network_max_len_2g['SSID'])

    def test_maximum_ssid_length_5g_11ac_80mhz(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.open_network_max_len_5g['SSID'])

    def test_ssid_with_UTF8_characters_2g_11n_20mhz(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.utf8_ssid_2g)

    def test_ssid_with_UTF8_characters_5g_11ac_80mhz(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='whirlwind_11ab_legacy',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.utf8_ssid_5g)
