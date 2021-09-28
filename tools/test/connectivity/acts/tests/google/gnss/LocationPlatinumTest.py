#!/usr/bin/env python3.5
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
from collections import namedtuple

from acts import asserts
from acts import signals
from acts import utils
from acts.base_test import BaseTestClass
from acts.test_utils.gnss import gnss_test_utils as gutils
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.tel import tel_test_utils as tutils

TEST_PACKAGE_NAME = 'com.google.android.apps.maps'
LOCATION_PERMISSIONS = [
    'android.permission.ACCESS_FINE_LOCATION',
    'android.permission.ACCESS_COARSE_LOCATION'
]
BACKGROUND_LOCATION_PERMISSION = 'android.permission.ACCESS_BACKGROUND_LOCATION'


class LocationPlatinumTest(BaseTestClass):
    """Location Platinum Tests"""

    def setup_class(self):
        super().setup_class()
        self.ad = self.android_devices[0]
        req_params = [
            # A { SSID, password } dictionary. Password is optional.
            'wifi_network',
            # A [latitude, longitude] list to identify test location.
            'test_location',
            # Cold Start Criteria, a int to define the criteria.
            'cs_criteria',
            # Warm Start Criteria, a int to define the criteria.
            'ws_criteria',
            # Hot Start Criteria, a int to define the criteria.
            'hs_criteria',
            # NetworkLocationProvide Criteria, a int to define the criteria.
            'nlp_criteria',
            # A list to identify QXDM log path.
            'qdsp6m_path'
        ]
        self.unpack_userparams(req_param_names=req_params)

        # Init test types Cold Start, Warm Start and Hot Start.
        test_type = namedtuple('Type', ['command', 'criteria'])
        self.test_types = {
            'cs': test_type('Cold Start', self.cs_criteria),
            'ws': test_type('Warm Start', self.ws_criteria),
            'hs': test_type('Hot Start', self.hs_criteria)
        }
        gutils._init_device(self.ad)
        self.begin_time = utils.get_current_epoch_time()
        gutils.clear_logd_gnss_qxdm_log(self.ad)
        tutils.start_qxdm_logger(self.ad, self.begin_time)
        tutils.start_adb_tcpdump(self.ad)

    def setup_test(self):
        """Prepare device with mobile data, wifi and gps ready for test """
        if int(self.ad.adb.shell('settings get secure location_mode')) != 3:
            self.ad.adb.shell('settings put secure location_mode 3')
        if not self.ad.droid.wifiCheckState():
            wutils.wifi_toggle_state(self.ad, True)
            gutils.connect_to_wifi_network(self.ad, self.wifi_network)
        if int(self.ad.adb.shell('settings get global mobile_data')) != 1:
            gutils.set_mobile_data(self.ad, True)
        self.grant_location_permission(True)
        self.ad.adb.shell('pm grant com.android.gpstool %s' %
                          BACKGROUND_LOCATION_PERMISSION)

    def teardown_class(self):
        tutils.stop_qxdm_logger(self.ad)
        gutils.get_gnss_qxdm_log(self.ad, self.qdsp6m_path)
        tutils.stop_adb_tcpdump(self.ad)
        tutils.get_tcpdump_log(self.ad, 'location_platinum', self.begin_time)
        self.ad.take_bug_report('location_platinum', self.begin_time)

    def grant_location_permission(self, option):
        """Grant or revoke location related permission.

        Args:
            option: Boolean to grant or revoke location related permissions.
        """
        action = 'grant' if option else 'revoke'
        for permission in LOCATION_PERMISSIONS:
            self.ad.log.info('%s permission:%s' % (action, permission))
            self.ad.adb.shell('pm %s %s %s' %
                              (action, TEST_PACKAGE_NAME, permission))

    def confirm_permission_usage_window(self):
        """ allow the script to confirm permission keep in use"""
        time.sleep(1)
        for _ in range(3):
            # Press Tab for 3 times
            self.ad.adb.shell('input keyevent 61')
        # Press Enter to confirm using current permission set.
        self.ad.adb.shell('input keyevent 66')
        time.sleep(1)

    def get_and_verify_ttff(self, mode):
        """Retrieve ttff with designate mode.

        Args:
            mode: A string for identify gnss test mode.
        """
        if mode in self.test_types:
            test_type = self.test_types.get(mode)
        else:
            raise signals.TestFailure('Unrecognized mode %s' % mode)

        gutils.process_gnss_by_gtw_gpstool(self.ad,
                                           self.test_types['cs'].criteria)
        begin_time = gutils.get_current_epoch_time()
        gutils.start_ttff_by_gtw_gpstool(
            self.ad, ttff_mode=mode, iteration=1, aid_data=True)
        ttff_data = gutils.process_ttff_by_gtw_gpstool(self.ad, begin_time,
                                                       self.test_location)
        result = gutils.check_ttff_data(
            self.ad,
            ttff_data,
            ttff_mode=test_type.command,
            criteria=test_type.criteria)
        asserts.assert_true(
            result,
            '%s TTFF fails to reach designated criteria' % test_type.command)

    # Test cases
    def test_gnss_cold_ttff(self):
        """
            1. Send intent to GPSTool for cold start test.
            2. Retrieve ttff and validate with target criteria.
        """
        self.get_and_verify_ttff('cs')

    def test_gnss_warm_ttff(self):
        """
            1. Send intent to GPSTool for warm start test.
            2. Retrieve ttff and validate with target criteria.
        """
        self.get_and_verify_ttff('ws')

    def test_gnss_hot_ttff(self):
        """
            1. Send intent to GPSTool for hot start test.
            2. Retrieve ttff and validate with target criteria.
        """
        self.get_and_verify_ttff('hs')

    def test_nlp_available_by_wifi(self):
        """
            1. Disable mobile data.
            2. Send intent to GPSTool for NLP.
            3. Retrieve response time and validate with criteria.
        """
        gutils.set_mobile_data(self.ad, False)
        asserts.assert_true(
            gutils.check_network_location(
                self.ad, 1, 'networkLocationType=wifi', self.nlp_criteria),
            'Fail to get NLP from wifi')

    def test_nlp_available_by_cell(self):
        """
            1. Disable wifi.
            2. Send intent to GPSTool for NLP.
            3. Retrieve response time and validate with criteria.
        """
        wutils.wifi_toggle_state(self.ad, False)
        asserts.assert_true(
            gutils.check_network_location(
                self.ad, 1, 'networkLocationType=cell', self.nlp_criteria),
            'Fail to get NLP from cell')

    def test_toggle_location_setting_off_on_report_location(self):
        """
            1. Toggle location setting off on.
            2. Open Google Map and ask for location.
            3. Validate there are location fix in logcat.
        """
        self.ad.adb.shell('settings put secure location_mode 0')
        self.ad.adb.shell('settings put secure location_mode 3')
        gutils.launch_google_map(self.ad)
        asserts.assert_true(
            gutils.check_location_api(self.ad, retries=1),
            'DUT failed to receive location fix')

    def test_toggle_location_setting_off_not_report_location(self):
        """
            1. Toggle location setting off.
            2. Open Google Map and ask for location.
            3. Validate there is no location fix in logcat.
        """
        self.ad.adb.shell('settings put secure location_mode 0')
        gutils.launch_google_map(self.ad)
        asserts.assert_false(
            gutils.check_location_api(self.ad, retries=1),
            'DUT Still receive location fix')

    def test_toggle_location_permission_off_on(self):
        """
            1. Toggle Google Map location permission off on.
            2. Open Google Map and ask for location.
            3. Validate there are location fix in logcat.
        """
        self.grant_location_permission(False)
        self.grant_location_permission(True)
        gutils.launch_google_map(self.ad)
        asserts.assert_true(
            gutils.check_location_api(self.ad, retries=1),
            'DUT fail to receive location fix')

    def test_toggle_location_permission_off(self):
        """
            1. Toggle Google Map location permission off.
            2. Open Google Map and ask for location.
            3. Validate there is no location fix in logcat.
        """
        self.grant_location_permission(False)
        gutils.launch_google_map(self.ad)
        asserts.assert_false(
            gutils.check_location_api(self.ad, retries=1),
            'DUT still receive location fix')

    def test_location_only_in_use_state(self):
        """
            1. Revoke ACCESS_BACKGROUBND_LOCATION permission on GPSTool.
            2. Open GPSTool for tracking.
            3. Validate there are location fix in logcat.
            4. Turn GPSTool from foreground to background by press home.
            5. Wait 60 seconds for app clean up.
            6. Validate GPSTool is skipping for location update in logcat.
        """
        self.ad.log.info('Revoke background permission')
        self.ad.adb.shell('pm revoke com.android.gpstool %s' %
                          BACKGROUND_LOCATION_PERMISSION)
        gutils.start_gnss_by_gtw_gpstool(self.ad, True)
        self.confirm_permission_usage_window()
        asserts.assert_true(
            gutils.check_location_api(self.ad, retries=1),
            'APP failed to receive location fix in foreground')
        self.ad.log.info('Trun GPSTool from foreground to background')
        self.ad.adb.shell('input keyevent 3')
        begin_time = utils.get_current_epoch_time()
        self.ad.log.info('Wait 60 seconds for app clean up')
        time.sleep(60)
        result = self.ad.search_logcat(
            'skipping loc update for no op app: com.android.gpstool',
            begin_time)
        asserts.assert_true(
            result, 'APP still receive location fix in background')
