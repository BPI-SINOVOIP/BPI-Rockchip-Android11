#
#   Copyright 2018 - The Android Open Source Project
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
import random
import socket
import threading
import time

from acts import asserts
from acts import base_test
from acts import signals
from acts import test_runner
from acts.controllers import adb
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.tel.tel_test_utils import _check_file_existance
from acts.test_utils.tel.tel_test_utils import _generate_file_directory_and_file_name
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.net.connectivity_const import MULTIPATH_PREFERENCE_NONE as NONE
from acts.test_utils.net.connectivity_const import MULTIPATH_PREFERENCE_HANDOVER as HANDOVER
from acts.test_utils.net.connectivity_const import MULTIPATH_PREFERENCE_RELIABILITY as RELIABILITY
from acts.test_utils.net.connectivity_const import MULTIPATH_PREFERENCE_PERFORMANCE as PERFORMANCE

DOWNLOAD_PATH = "/sdcard/Download/"
RELIABLE = RELIABILITY | HANDOVER
TIMEOUT = 6

class DataCostTest(base_test.BaseTestClass):
    """ Tests for Wifi Tethering """

    def setup_class(self):
        """ Setup devices for tethering and unpack params """

        req_params = ("wifi_network", "download_file")
        self.unpack_userparams(req_params)

        for ad in self.android_devices:
            nutils.verify_lte_data_and_tethering_supported(ad)

        self.tcpdump_pid = None

    def teardown_class(self):
        """ Reset settings to default """
        for ad in self.android_devices:
            sub_id = str(ad.droid.telephonyGetSubscriberId())
            ad.droid.connectivityFactoryResetNetworkPolicies(sub_id)
            ad.droid.connectivitySetDataWarningLimit(sub_id, -1)
            wutils.reset_wifi(ad)


    def teardown_test(self):
        if self.tcpdump_pid:
            nutils.stop_tcpdump(self.dut, self.tcpdump_pid, self.test_name)
            self.tcpdump_pid = None

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)

    """ Helper functions """

    def _clear_netstats(self, ad):
        """ Clear netstats stored on device

        Args:
            ad: Android device object
        """
        ad.log.info("Clear netstats record.")
        ad.adb.shell("rm /data/system/netstats/*")
        asserts.assert_equal("", ad.adb.shell("ls /data/system/netstats/"),
                             "Fail to clear netstats.")
        ad.reboot()
        time.sleep(30)
        nutils.verify_lte_data_and_tethering_supported(ad)
        self._check_multipath_preference_from_dumpsys(ad)

    def _check_multipath_preference_from_dumpsys(self, ad):
        """ Check cell multipath_preference from dumpsys

        Args:
            ad: Android device object
        """
        out = ad.adb.shell("dumpsys connectivity | grep budget")
        asserts.assert_true(out, "Fail to get status from dumpsys.")
        ad.log.info("MultipathPolicyTracker: %s" % out)
        asserts.assert_true(
            "HANDOVER|RELIABILITY" in out,
            "Cell multipath preference should be HANDOVER|RELIABILITY."
        )

    def _get_total_data_usage_for_device(self, ad, conn_type, sub_id):
        """ Get total data usage in MB for device

        Args:
            ad: Android device object
            conn_type: MOBILE/WIFI data usage
            sub_id: subscription id

        Returns:
            Data usage in MB
        """
        # end time should be in milli seconds and at least 2 hours more than the
        # actual end time. NetStats:bucket is of size 2 hours and to ensure to
        # get the most recent data usage, end_time should be +2hours
        end_time = int(time.time() * 1000) + 2 * 1000 * 60 * 60
        data_usage = ad.droid.connectivityQuerySummaryForDevice(
            conn_type, sub_id, 0, end_time)
        data_usage /= 1000.0 * 1000.0 # convert data_usage to MB
        self.log.info("Total data usage is: %s" % data_usage)
        return data_usage

    def _check_if_multipath_preference_valid(self, val, exp):
        """ Check if multipath value is same as expected

        Args:
            val: multipath preference for the network
            exp: expected multipath preference value
        """
        if exp == NONE:
            asserts.assert_true(val == exp, "Multipath value should be 0")
        else:
            asserts.assert_true(val >= exp,
                                "Multipath value should be at least %s" % exp)

    def _verify_multipath_preferences(self,
                                      ad,
                                      wifi_pref,
                                      cell_pref,
                                      wifi_network,
                                      cell_network):
        """ Verify mutlipath preferences for wifi and cell networks

        Args:
            ad: Android device object
            wifi_pref: Expected multipath value for wifi network
            cell_pref: Expected multipath value for cell network
            wifi_network: Wifi network id on the device
            cell_network: Cell network id on the device
        """
        wifi_multipath = \
            ad.droid.connectivityGetMultipathPreferenceForNetwork(wifi_network)
        cell_multipath = \
            ad.droid.connectivityGetMultipathPreferenceForNetwork(cell_network)
        self.log.info("WiFi multipath preference: %s" % wifi_multipath)
        self.log.info("Cell multipath preference: %s" % cell_multipath)
        self.log.info("Checking multipath preference for wifi")
        self._check_if_multipath_preference_valid(wifi_multipath, wifi_pref)
        self.log.info("Checking multipath preference for cell")
        self._check_if_multipath_preference_valid(cell_multipath, cell_pref)

    """ Test Cases """

    @test_tracker_info(uuid="e86c8108-3e84-4668-bae4-e5d2c8c27910")
    def test_multipath_preference_low_data_limit(self):
        """ Verify multipath preference when mobile data limit is low

        Steps:
            1. DUT has WiFi and LTE data
            2. Set mobile data usage limit to low value
            3. Verify that multipath preference is 0 for cell network
        """
        # set vars
        ad = self.android_devices[0]
        self.dut = ad
        self._clear_netstats(ad)
        self.tcpdump_pid = nutils.start_tcpdump(ad, self.test_name)

        sub_id = str(ad.droid.telephonyGetSubscriberId())
        cell_network = ad.droid.connectivityGetActiveNetwork()
        self.log.info("cell network %s" % cell_network)
        wutils.wifi_connect(ad, self.wifi_network)
        wifi_network = ad.droid.connectivityGetActiveNetwork()
        self.log.info("wifi network %s" % wifi_network)

        # verify mulipath preference values
        self._verify_multipath_preferences(
            ad, RELIABLE, RELIABLE, wifi_network, cell_network)

        # set low data limit on mobile data
        total_pre = self._get_total_data_usage_for_device(ad, 0, sub_id)
        self.log.info("Setting data usage limit to %sMB" % (total_pre + 5))
        ad.droid.connectivitySetDataUsageLimit(
            sub_id, int((total_pre + 5) * 1000.0 * 1000.0))
        self.log.info("Setting data warning limit to %sMB" % (total_pre + 5))
        ad.droid.connectivitySetDataWarningLimit(
            sub_id, int((total_pre + 5) * 1000.0 * 1000.0))

        # verify multipath preference values
        curr_time = time.time()
        while time.time() < curr_time + TIMEOUT:
            try:
                self._verify_multipath_preferences(
                    ad, RELIABLE, NONE, wifi_network, cell_network)
                return True
            except signals.TestFailure as e:
                self.log.debug("%s" % e)
            time.sleep(1)
        return False

    @test_tracker_info(uuid="a2781411-d880-476a-9f40-2c67e0f97db9")
    def test_multipath_preference_data_download(self):
        """ Verify multipath preference when large file is downloaded

        Steps:
            1. DUT has WiFi and LTE data
            2. WiFi is active network
            3. Download large file over cell network
            4. Verify multipath preference on cell network is 0
        """
        # set vars
        ad = self.android_devices[1]
        self.dut = ad
        self._clear_netstats(ad)
        self.tcpdump_pid = nutils.start_tcpdump(ad, self.test_name)

        cell_network = ad.droid.connectivityGetActiveNetwork()
        self.log.info("cell network %s" % cell_network)
        wutils.wifi_connect(ad, self.wifi_network)
        wifi_network = ad.droid.connectivityGetActiveNetwork()
        self.log.info("wifi network %s" % wifi_network)

        # verify multipath preference for wifi and cell networks
        self._verify_multipath_preferences(
            ad, RELIABLE, RELIABLE, wifi_network, cell_network)

        # download file with cell network
        ad.droid.connectivityNetworkOpenConnection(cell_network,
                                                   self.download_file)
        file_folder, file_name = _generate_file_directory_and_file_name(
            self.download_file, DOWNLOAD_PATH)
        file_path = os.path.join(file_folder, file_name)
        self.log.info("File path: %s" % file_path)
        if _check_file_existance(ad, file_path):
            self.log.info("File exists. Removing file %s" % file_name)
            ad.adb.shell("rm -rf %s%s" % (DOWNLOAD_PATH, file_name))

        #  verify multipath preference values
        curr_time = time.time()
        while time.time() < curr_time + TIMEOUT:
            try:
                self._verify_multipath_preferences(
                    ad, RELIABLE, NONE, wifi_network, cell_network)
                return True
            except signals.TestFailure as e:
                self.log.debug("%s" % e)
            time.sleep(1)
        return False

    # TODO gmoturu@: Need to add tests that use the mobility rig and test when
    # the WiFi signal is poor and data signal is good.
