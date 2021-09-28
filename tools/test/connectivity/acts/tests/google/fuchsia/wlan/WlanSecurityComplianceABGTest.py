#!/usr/bin/env python3
#
#   Copyright 2019 - The Android secure Source Project
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
import re

from functools import wraps

from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib.hostapd_security import Security
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import validate_setup_ap_and_associate
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.utils import rand_ascii_str
from acts.utils import rand_hex_str

AP_11ABG_PROFILE_NAME = 'whirlwind_11ag_legacy'


def create_security_profile(test_func):
    """Decorator for generating hostapd security profile object based on the
    test name.
    Args:
        test_func: The test function
    Returns:
        security_profile_generator: The function that generates the security
            profile object
    """
    @wraps(test_func)
    def security_profile_generator(self, *args, **kwargs):
        """Function that looks at the name of the function and determines what
        the security profile should be based on what items are in the name

        Example: A function with the name sec_wpa_wpa2_ptk_ccmp_tkip would
            return a security profile that has wpa and wpa2 configure with a
            ptk cipher of ccmp or tkip. Removing one of those options would
            drop it from the config.

        Args:
            self: The object of the WlanSecurityComplianceABGTest class.
            *args: args that were sent to the original test function
            **kwargs: kwargs that were sent to the original test function
        Returns:
            The original fuction that was called
        """
        security = re.search(r'sec(.*?)ptk_(.*)', test_func.__name__)
        security_mode = security.group(1)
        ptk_type = security.group(2)
        wpa_cipher = None
        wpa2_cipher = None
        if '_wpa_' in security_mode and '_wpa2_' in security_mode:
            security_mode = 'wpa/wpa2'
        elif '_wep_' in security_mode:
            security_mode = 'wep'
        elif '_wpa_' in security_mode:
            security_mode = 'wpa'
        elif '_wpa2_' in security_mode:
            security_mode = 'wpa2'
        elif '_wpa3_' in security_mode:
            security_mode = 'wpa3'
        if 'tkip' in ptk_type and 'ccmp' in ptk_type:
            wpa_cipher = 'TKIP CCMP'
            wpa2_cipher = 'TKIP CCMP'
        elif 'tkip' in ptk_type:
            wpa_cipher = 'TKIP'
            wpa2_cipher = 'TKIP'
        elif 'ccmp' in ptk_type:
            wpa_cipher = 'CCMP'
            wpa2_cipher = 'CCMP'
        if 'max_length_password' in test_func.__name__:
            password = rand_ascii_str(
                hostapd_constants.MAX_WPA_PASSWORD_LENGTH)
        elif 'max_length_psk' in test_func.__name__:
            password = str(rand_hex_str(
                hostapd_constants.MAX_WPA_PSK_LENGTH)).lower()
        elif 'wep_5_chars' in test_func.__name__:
            password = rand_ascii_str(5)
        elif 'wep_13_chars' in test_func.__name__:
            password = rand_ascii_str(13)
        elif 'wep_10_hex' in test_func.__name__:
            password = str(rand_hex_str(10)).lower()
        elif 'wep_26_hex' in test_func.__name__:
            password = str(rand_hex_str(26)).lower()
        else:
            password = rand_ascii_str(hostapd_constants.MIN_WPA_PSK_LENGTH)
        self.security_profile = Security(security_mode=security_mode,
                                         password=password,
                                         wpa_cipher=wpa_cipher,
                                         wpa2_cipher=wpa2_cipher)
        self.client_password = password
        return test_func(self, *args, *kwargs)

    return security_profile_generator


class WlanSecurityComplianceABGTest(WifiBaseTest):
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
        secure_network = self.get_psk_network(False, [],
                                              ssid_length_2g=15,
                                              ssid_length_5g=15)
        self.secure_network_2g = secure_network['2g']
        self.secure_network_5g = secure_network['5g']
        self.security_profile = None
        self.client_password = None

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

    @create_security_profile
    def test_associate_11a_sec_open_wep_5_chars_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['open'])

    @create_security_profile
    def test_associate_11a_sec_open_wep_13_chars_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['open'])

    @create_security_profile
    def test_associate_11a_sec_open_wep_10_hex_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['open'])

    @create_security_profile
    def test_associate_11a_sec_open_wep_26_hex_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['open'])

    @create_security_profile
    def test_associate_11a_sec_shared_wep_5_chars_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['shared'])

    @create_security_profile
    def test_associate_11a_sec_shared_wep_13_chars_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['shared'])

    @create_security_profile
    def test_associate_11a_sec_shared_wep_10_hex_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['shared'])

    @create_security_profile
    def test_associate_11a_sec_shared_wep_26_hex_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['shared'])

    @create_security_profile
    def test_associate_11a_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_frag_430_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_high_dtim_low_beacon_int_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.LOW_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_low_dtim_high_beacon_int_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            dtim_period=hostapd_constants.LOW_DTIM,
            beacon_interval=hostapd_constants.HIGH_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_with_WMM_with_default_values_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.
            WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_correct_length_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_zero_length_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_similar_to_wpa_ie_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['simliar_to_wpa'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_sec_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_frag_430_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_high_dtim_low_beacon_int_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.LOW_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_low_dtim_high_beacon_int_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            dtim_period=hostapd_constants.LOW_DTIM,
            beacon_interval=hostapd_constants.HIGH_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_with_WMM_with_default_values_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_correct_length_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_zero_length_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_similar_to_wpa_ie_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['simliar_to_wpa'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_psk_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_frag_430_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_high_dtim_low_beacon_int_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.LOW_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_low_dtim_high_beacon_int_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            dtim_period=hostapd_constants.LOW_DTIM,
            beacon_interval=hostapd_constants.HIGH_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_with_WMM_with_default_values_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_correct_length_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_zero_length_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_similar_to_wpa_ie_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['simliar_to_wpa'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_sec_wpa3_sae_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_sec_wpa3_sae_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa3_sae_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_max_length_password_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa3_sae_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_frag_430_sec_wpa3_sae_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa3_sae_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_sec_wpa3_sae_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_rts_256_frag_430_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_high_dtim_low_beacon_int_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.LOW_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_low_dtim_high_beacon_int_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            dtim_period=hostapd_constants.LOW_DTIM,
            beacon_interval=hostapd_constants.HIGH_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11a_with_WMM_with_default_values_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.
            WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_correct_length_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_zero_length_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11a_with_vendor_ie_in_beacon_similar_to_wpa_ie_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.secure_network_5g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['simliar_to_wpa'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_sec_open_wep_5_chars_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['open'])

    @create_security_profile
    def test_associate_11bg_sec_open_wep_13_chars_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['open'])

    @create_security_profile
    def test_associate_11bg_sec_open_wep_10_hex_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['open'])

    @create_security_profile
    def test_associate_11bg_sec_open_wep_26_hex_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['open'])

    @create_security_profile
    def test_associate_11bg_sec_shared_wep_5_chars_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['shared'])

    @create_security_profile
    def test_associate_11bg_sec_shared_wep_13_chars_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['shared'])

    @create_security_profile
    def test_associate_11bg_sec_shared_wep_10_hex_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['shared'])

    @create_security_profile
    def test_associate_11bg_sec_shared_wep_26_hex_ptk_none(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False,
            additional_ap_parameters=hostapd_constants.WEP_AUTH['shared'])

    @create_security_profile
    def test_associate_11bg_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_frag_430_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_high_dtim_low_beacon_int_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.LOW_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_low_dtim_high_beacon_int_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            dtim_period=hostapd_constants.LOW_DTIM,
            beacon_interval=hostapd_constants.HIGH_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_with_WMM_with_default_values_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_correct_length_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_zero_length_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_similar_to_wpa_ie_sec_wpa_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['simliar_to_wpa'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_sec_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_frag_430_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_high_dtim_low_beacon_int_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.LOW_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_low_dtim_high_beacon_int_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.HIGH_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_with_WMM_with_default_values_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_correct_length_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_zero_length_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_similar_to_wpa_ie_sec_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['simliar_to_wpa'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa_wpa2_psk_ptk_tkip(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa_wpa2_psk_ptk_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_psk_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa_wpa2_psk_ptk_tkip(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa_wpa2_psk_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_frag_430_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_high_dtim_low_beacon_int_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.LOW_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_low_dtim_high_beacon_int_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            dtim_period=hostapd_constants.LOW_DTIM,
            beacon_interval=hostapd_constants.HIGH_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_with_WMM_with_default_values_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.
            WMM_PHYS_11A_11G_11N_11AC_DEFAULT_PARAMS,
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_correct_length_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_zero_length_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_similar_to_wpa_ie_sec_wpa_wpa2_psk_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['simliar_to_wpa'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_sec_wpa3_sae_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_sec_wpa3_sae_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa3_sae_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_max_length_password_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa3_sae_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_frag_430_sec_wpa3_sae_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa3_sae_ptk_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_sec_wpa3_sae_ptk_tkip_or_ccmp(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_rts_256_frag_430_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            security=self.security_profile,
            password=self.client_password,
            rts_threshold=256,
            frag_threshold=430,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_high_dtim_low_beacon_int_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            dtim_period=hostapd_constants.HIGH_DTIM,
            beacon_interval=hostapd_constants.LOW_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_low_dtim_high_beacon_int_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            dtim_period=hostapd_constants.LOW_DTIM,
            beacon_interval=hostapd_constants.HIGH_BEACON_INTERVAL,
            security=self.security_profile,
            password=self.client_password,
            force_wmm=False)

    @create_security_profile
    def test_associate_11bg_with_WMM_with_default_values_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            force_wmm=True,
            additional_ap_parameters=hostapd_constants.WMM_11B_DEFAULT_PARAMS,
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_correct_length_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['correct_length_beacon'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_zero_length_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['zero_length_beacon_without_data'],
            security=self.security_profile,
            password=self.client_password)

    @create_security_profile
    def test_associate_11bg_with_vendor_ie_in_beacon_similar_to_wpa_ie_sec_wpa3_sae_ptk_tkip_or_ccmp(
        self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name=AP_11ABG_PROFILE_NAME,
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.secure_network_2g['SSID'],
            additional_ap_parameters=hostapd_constants.
            VENDOR_IE['simliar_to_wpa'],
            security=self.security_profile,
            password=self.client_password)
