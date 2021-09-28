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
from acts.controllers.ap_lib.hostapd_security import Security
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import validate_setup_ap_and_associate
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest


class VapeInteropTest(WifiBaseTest):
    """Tests interoperability with mock third party AP profiles.

    Test Bed Requirement:
    * One Android or Fuchsia Device
    * One Whirlwind Access Point
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

        # Same for both 2g and 5g
        self.ssid = utils.rand_ascii_str(hostapd_constants.AP_SSID_LENGTH_2G)
        self.password = utils.rand_ascii_str(
            hostapd_constants.AP_PASSPHRASE_LENGTH_2G)
        self.security_profile_wpa2 = Security(
            security_mode=hostapd_constants.WPA2_STRING,
            password=self.password,
            wpa2_cipher=hostapd_constants.WPA2_DEFAULT_CIPER)

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

    def test_associate_actiontec_pk5000_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='actiontec_pk5000',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_actiontec_pk5000_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='actiontec_pk5000',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_actiontec_mi424wr_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='actiontec_mi424wr',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_actiontec_mi424wr_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='actiontec_mi424wr',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtac66u_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac66u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_asus_rtac66u_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac66u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtac66u_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac66u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_asus_rtac66u_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac66u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtac86u_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac86u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_asus_rtac86u_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac86u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtac86u_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac86u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_asus_rtac86u_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac86u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtac5300_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac5300',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_asus_rtac5300_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac5300',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtac5300_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac5300',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_asus_rtac5300_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtac5300',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtn56u_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtn56u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_asus_rtn56u_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtn56u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtn56u_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtn56u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_asus_rtn56u_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtn56u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtn66u_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtn66u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_asus_rtn66u_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtn66u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_asus_rtn66u_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtn66u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_asus_rtn66u_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='asus_rtn66u',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_belkin_f9k1001v5_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='belkin_f9k1001v5',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_belkin_f9k1001v5_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='belkin_f9k1001v5',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_linksys_ea4500_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_ea4500',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_linksys_ea4500_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_ea4500',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_linksys_ea4500_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_ea4500',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_linksys_ea4500_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_ea4500',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_linksys_ea9500_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_ea9500',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_linksys_ea9500_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_ea9500',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_linksys_ea9500_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_ea9500',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_linksys_ea9500_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_ea9500',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_linksys_wrt1900acv2_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_wrt1900acv2',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_linksys_wrt1900acv2_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_wrt1900acv2',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_linksys_wrt1900acv2_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_wrt1900acv2',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_linksys_wrt1900acv2_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='linksys_wrt1900acv2',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_netgear_r7000_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='netgear_r7000',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_netgear_r7000_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='netgear_r7000',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_netgear_r7000_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='netgear_r7000',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_netgear_r7000_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='netgear_r7000',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_netgear_wndr3400_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='netgear_wndr3400',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_netgear_wndr3400_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='netgear_wndr3400',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_netgear_wndr3400_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='netgear_wndr3400',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_netgear_wndr3400_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='netgear_wndr3400',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_securifi_almond_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='securifi_almond',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_securifi_almond_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='securifi_almond',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_tplink_archerc5_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_archerc5',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_tplink_archerc5_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_archerc5',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_tplink_archerc5_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_archerc5',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_tplink_archerc5_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_archerc5',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_tplink_archerc7_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_archerc7',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_tplink_archerc7_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_archerc7',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_tplink_archerc7_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_archerc7',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_tplink_archerc7_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_archerc7',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_tplink_c1200_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_c1200',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_tplink_c1200_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_c1200',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_tplink_c1200_5ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_c1200',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid)

    def test_associate_tplink_c1200_5ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_c1200',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_5G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)

    def test_associate_tplink_tlwr940n_24ghz_open(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_tlwr940n',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def test_associate_tplink_tlwr940n_24ghz_wpa2(self):
        validate_setup_ap_and_associate(
            access_point=self.access_point,
            client=self.dut,
            profile_name='tplink_tlwr940n',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid,
            security=self.security_profile_wpa2,
            password=self.password)