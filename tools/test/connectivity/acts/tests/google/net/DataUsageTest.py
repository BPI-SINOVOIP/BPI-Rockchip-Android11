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

import threading
import time

from acts import asserts
from acts import base_test
from acts import utils
from acts.controllers import adb
from acts.controllers.ap_lib import hostapd_constants
from acts.test_decorators import test_tracker_info
from acts.test_utils.net import connectivity_const as cconst
from acts.test_utils.net import connectivity_test_utils as cutils
from acts.test_utils.net import net_test_utils as nutils
from acts.test_utils.net.net_test_utils import start_tcpdump
from acts.test_utils.net.net_test_utils import stop_tcpdump
from acts.test_utils.tel import tel_test_utils as ttutils
from acts.test_utils.tel.tel_test_utils import get_operator_name
from acts.test_utils.tel.tel_test_utils import http_file_download_by_chrome
from acts.test_utils.wifi import wifi_test_utils as wutils
import queue
from queue import Empty


conn_test_class = "com.android.tests.connectivity.uid.ConnectivityTestActivity"
android_os_class = "com.quicinc.cne.CNEService.CNEServiceApp"
instr_cmd = ("am instrument -w -e command grant-all "
             "com.android.permissionutils/.PermissionInstrumentation")

HOUR_IN_MILLIS = 1000 * 60 * 60
BYTE_TO_MB_ANDROID = 1000.0 * 1000.0
BYTE_TO_MB = 1024.0 * 1024.0
DOWNLOAD_PATH = "/sdcard/download/"
DATA_USG_ERR = 2.2
DATA_ERR = 0.2
TIMEOUT = 2 * 60
INC_DATA = 10
AP_PASSPHRASE_LENGTH_2G = hostapd_constants.AP_PASSPHRASE_LENGTH_2G
AP_SSID_LENGTH_2G = hostapd_constants.AP_SSID_LENGTH_2G
RETRY_SLEEP = 3
WIFI_SLEEP = 180


class DataUsageTest(base_test.BaseTestClass):
    """Data usage tests.

    Requirements:
       Two Pixel devices - one device with TMO and other with VZW SIM
       WiFi network - Wifi network to test wifi data usage
    """

    def __init__(self, controllers):
        base_test.BaseTestClass.__init__(self, controllers)

    def setup_class(self):
        """Setup devices for tests and unpack params."""

        # unpack user params
        req_params = ("wifi_network",
                      "file_size",
                      "download_file_ipv4",
                      "download_file_ipv6",
                      "dbs_supported_models")
        self.unpack_userparams(req_params)
        self.file_path = DOWNLOAD_PATH + self.download_file_ipv4.split("/")[-1]
        self.file_size = float(self.file_size)

        for ad in self.android_devices:
            self.log.info("Operator on %s is %s" % \
                          (ad.serial, get_operator_name(self.log, ad)))
            ad.reboot()
            nutils.verify_lte_data_and_tethering_supported(ad)
            nutils.set_chrome_browser_permissions(ad)
            cutils.set_private_dns(ad, cconst.PRIVATE_DNS_MODE_OFF)
            try:
                ad.adb.shell(instr_cmd)
            except adb.AdbError:
                self.log.warn("cmd %s failed on %s" % (instr_cmd, ad.serial))
        self.tcpdumps = []
        self.hs_enabled = []

    def setup_test(self):
        for ad in self.android_devices:
            self.tcpdumps.append(start_tcpdump(ad, self.test_name))
            ad.droid.wakeLockAcquireBright()
            ad.droid.wakeUpNow()

    def teardown_test(self):
        for ad in self.hs_enabled:
            try:
                wutils.stop_wifi_tethering(ad)
            except Exception as e:
              self.log.warn("Failed to stop wifi tethering: %s" % e)
        self.hs_enabled = []
        for i in range(len(self.android_devices)):
            ad = self.android_devices[i]
            stop_tcpdump(ad, self.tcpdumps[i], self.test_name)
            wutils.reset_wifi(ad)
            sub_id = str(ad.droid.telephonyGetSubscriberId())
            ad.droid.connectivityFactoryResetNetworkPolicies(sub_id)
            ad.droid.telephonyToggleDataConnection(True)
            ad.droid.wakeLockRelease()
            ad.droid.goToSleepNow()
        self.tcpdumps = []

    def teardown_class(self):
        for ad in self.android_devices:
            cutils.set_private_dns(ad, cconst.PRIVATE_DNS_MODE_OPPORTUNISTIC)

    def on_fail(self, test_name, begin_time):
        for ad in self.android_devices:
            ad.take_bug_report(test_name, begin_time)

    ### Helper functions

    def _download_data_through_app(self, ad, file_link):
        """Download data through app on DUT.

        Args:
            ad: DUT to download the file on
            file_link: http link of the file to download

        Returns:
            True if file download is successful, False if not
        """
        intent = ad.droid.createIntentForClassName(conn_test_class)
        json_obj = {"url": file_link}
        ad.droid.launchForResultWithIntent(intent, json_obj)
        download_status = False
        end_time = time.time() + TIMEOUT
        while time.time() < end_time:
            download_status = ttutils._check_file_existance(
                ad, self.file_path, self.file_size * BYTE_TO_MB)
            if download_status:
                self.log.info("Delete file: %s", self.file_path)
                ad.adb.shell("rm %s" % self.file_path, ignore_status=True)
                break
            time.sleep(RETRY_SLEEP*2)  # wait to check if download is complete
        return download_status

    def _get_total_data_usage_for_device(self, ad, conn_type):
        """Get total data usage in MB for device.

        Args:
            ad: android device object
            conn_type: MOBILE/WIFI data usage

        Returns:
            Data usage in MB
        """
        sub_id = str(ad.droid.telephonyGetSubscriberId())
        end_time = int(time.time() * 1000) + 2 * HOUR_IN_MILLIS
        data_usage = ad.droid.connectivityQuerySummaryForDevice(
            conn_type, sub_id, 0, end_time)
        data_usage /= BYTE_TO_MB_ANDROID
        self.log.info("Total data usage is: %s" % data_usage)
        return data_usage

    def _get_data_usage_for_uid_rx(self, ad, conn_type, uid):
        """Get data usage for UID in Rx Bytes.

        Args:
            ad: DUT to get data usage from
            conn_type: MOBILE/WIFI data usage
            uid: UID of the app

        Returns:
            Data usage in MB
        """
        subscriber_id = ad.droid.telephonyGetSubscriberId()
        end_time = int(time.time() * 1000) + 2 * HOUR_IN_MILLIS
        data_usage = ad.droid.connectivityQueryDetailsForUidRxBytes(
            conn_type, subscriber_id, 0, end_time, uid)
        return data_usage/BYTE_TO_MB_ANDROID

    def _get_data_usage_for_device_rx(self, ad, conn_type):
        """Get total data usage in rx bytes for device.

        Args:
            ad: DUT to get data usage from
            conn_type: MOBILE/WIFI data usage

        Returns:
            Data usage in MB
        """
        subscriber_id = ad.droid.telephonyGetSubscriberId()
        end_time = int(time.time() * 1000) + 2 * HOUR_IN_MILLIS
        data_usage = ad.droid.connectivityQuerySummaryForDeviceRxBytes(
            conn_type, subscriber_id, 0, end_time)
        return data_usage/BYTE_TO_MB_ANDROID

    def _get_data_usage(self, ad, conn_type):
        """Get data usage.

        Args:
            ad: DUT to get data usage from
            conn_type: MOBILE/WIFI data usage

        Returns:
            Tuple of Android Os app, Conn UID app, Total data usages
        """
        android_os_uid = ad.droid.getUidForPackage(android_os_class)
        conn_test_uid = ad.droid.getUidForPackage(conn_test_class)
        aos = self._get_data_usage_for_uid_rx(ad, conn_type, android_os_uid)
        app = self._get_data_usage_for_uid_rx(ad, conn_type, conn_test_uid)
        tot = self._get_data_usage_for_device_rx(ad, conn_type)
        self.log.info("Android Os data usage: %s" % aos)
        self.log.info("Conn UID Test data usage: %s" % app)
        self.log.info("Total data usage: %s" % tot)
        return (aos, app, tot)

    def _listen_for_network_callback(self, ad, event, q):
        """Verify network callback for data usage.

        Args:
            ad: DUT to get the network callback for
            event: Network callback event
            q: queue object

        Returns:
            True: if the expected network callback found, False if not
        """
        key = ad.droid.connectivityRegisterDefaultNetworkCallback()
        ad.droid.connectivityNetworkCallbackStartListeningForEvent(key, event)

        curr_time = time.time()
        status = False
        while time.time() < curr_time + TIMEOUT:
            try:
                nc_event = ad.ed.pop_event("NetworkCallback")
                self.log.info("Received: %s" %
                              nc_event["data"]["networkCallbackEvent"])
                if nc_event["data"]["networkCallbackEvent"] == event:
                    status = True
                    break
            except Empty:
                pass

        ad.droid.connectivityNetworkCallbackStopListeningForEvent(key, event)
        ad.droid.connectivityUnregisterNetworkCallback(key)
        q.put(status)

    def _test_data_usage_downlink(self,
                                  dut_a,
                                  dut_b,
                                  file_link,
                                  data_usage,
                                  tethering=False,
                                  connect_wifi=False,
                                  app_inc=True):
        """Verify data usage.

        Steps:
            1. Get the current data usage of ConnUIDTest and Android OS apps
            2. DUT is on LTE data (on wifi if connect_wifi is True)
            3. Download file of size xMB through ConnUIDTest app
            4. Verify that data usage of Android OS app did not change
            5. Verify that data usage of ConnUIDTest app increased by ~xMB
            6. Verify that data usage of device also increased by ~xMB

        Args:
            dut_a: DUT on which data usage need to be verified
            dut_b: DUT on which data is downloaded
            file_link: Link used to download data
            data_usage: specifies if MOBILE or WIFI data is calculated
            tethering: if True, enable hotspot on dut_a and connect dut_b to it
            connect_wifi: if True, connect dut_a to a wifi network
            app_inc: if True, verify app data usage on dut_a is increased
        """
        if connect_wifi:
            dut_a.droid.telephonyToggleDataConnection(False)
            wutils.start_wifi_connection_scan_and_ensure_network_found(
                dut_a, self.wifi_network["SSID"])
            wutils.wifi_connect(dut_a, self.wifi_network)
            # sleep for some time after connecting to wifi. some apps use wifi
            # data when connected initially. waiting after connecting to wifi
            # ensures correct wifi data usage calculation
            time.sleep(WIFI_SLEEP)

        if tethering:
            ssid = "hs_%s" % utils.rand_ascii_str(AP_SSID_LENGTH_2G)
            pwd = utils.rand_ascii_str(AP_PASSPHRASE_LENGTH_2G)
            network = {wutils.WifiEnums.SSID_KEY: ssid,
                       wutils.WifiEnums.PWD_KEY: pwd}
            dut_b.droid.telephonyToggleDataConnection(False)
            wutils.start_wifi_tethering(dut_a,
                                        network[wutils.WifiEnums.SSID_KEY],
                                        network[wutils.WifiEnums.PWD_KEY])
            self.hs_enabled.append(dut_a)
            wutils.start_wifi_connection_scan_and_ensure_network_found(
                dut_b, network[wutils.WifiEnums.SSID_KEY])
            wutils.wifi_connect(dut_b, network)
            # sleep for some time after connecting to wifi. though wifi hotspot
            # is metered, some apps would still use wifi data when connected
            # initially. this ensures correct wifi data usage calculation
            time.sleep(WIFI_SLEEP)

        # get pre data usage
        (aos_pre, app_pre, total_pre) = self._get_data_usage(dut_a, data_usage)

        # download file through app
        status = self._download_data_through_app(dut_b, file_link)
        asserts.assert_true(status, "Failed to download file: %s" % file_link)

        # get new data usage
        aos_exp = DATA_ERR
        app_exp = self.file_size + DATA_USG_ERR
        file_size = self.file_size
        if not app_inc:
            app_exp = DATA_ERR
            file_size = 0
        total_exp = self.file_size + DATA_USG_ERR

        # somtimes data usage is not increased immediately.
        # re-tries until the data usage is closed to expected value.
        curr_time = time.time()
        while time.time() < curr_time + TIMEOUT:
            (aos_pst, app_pst, total_pst) = self._get_data_usage(dut_a,
                                                                 data_usage)
            if total_pst - total_pre >= self.file_size and \
                app_pst - app_pre >= file_size:
                  # wait for some time to verify that data doesn't increase
                  time.sleep(RETRY_SLEEP*2)
                  break
            time.sleep(RETRY_SLEEP)  # wait before retry
        (aos_pst, app_pst, total_pst) = self._get_data_usage(dut_a, data_usage)

        # verify data usage
        aos_diff = aos_pst - aos_pre
        app_diff = app_pst - app_pre
        total_diff = total_pst - total_pre
        self.log.info("Usage of Android OS increased: %sMB, expected: %sMB" % \
                      (aos_diff, aos_exp))
        self.log.info("Usage of ConnUID app increased: %sMB, expected: %sMB" % \
                      (app_diff, app_exp))
        self.log.info("Usage on the device increased: %sMB, expected: %sMB" % \
                      (total_diff, total_exp))

        asserts.assert_true(
            (aos_diff < aos_exp) and (file_size <= app_diff < app_exp) and \
                (self.file_size <= total_diff < total_exp),
            "Incorrect data usage count")

    def _test_data_usage_limit_downlink(self,
                                        dut_a,
                                        dut_b,
                                        file_link,
                                        data_usage,
                                        tethering=False):
        """Verify data usage limit reached.

        Steps:
            1. Set the data usage limit to current data usage + 10MB
            2. If tested for tethered device, start wifi hotspot and
               connect DUT to it.
            3. If tested for tethered device, download 20MB data from
               tethered device else download on the same DUT
            4. Verify file download stops and data limit reached

        Args:
            dut_a: DUT on which data usage limit needs to be verified
            dut_b: DUT on which data needs to be downloaded
            file_link: specifies if the link is IPv4 or IPv6
            data_usage: specifies if MOBILE or WIFI data usage
            tethering: if wifi hotspot should be enabled on dut_a
        """
        sub_id = str(dut_a.droid.telephonyGetSubscriberId())

        # enable hotspot and connect dut_b to it.
        if tethering:
            ssid = "hs_%s" % utils.rand_ascii_str(AP_SSID_LENGTH_2G)
            pwd = utils.rand_ascii_str(AP_PASSPHRASE_LENGTH_2G)
            network = {wutils.WifiEnums.SSID_KEY: ssid,
                       wutils.WifiEnums.PWD_KEY: pwd}
            dut_b.droid.telephonyToggleDataConnection(False)
            wutils.start_wifi_tethering(dut_a,
                                        network[wutils.WifiEnums.SSID_KEY],
                                        network[wutils.WifiEnums.PWD_KEY])
            self.hs_enabled.append(dut_a)
            wutils.start_wifi_connection_scan_and_ensure_network_found(
                dut_b, network[wutils.WifiEnums.SSID_KEY])
            wutils.wifi_connect(dut_b, network)

        # get pre mobile data usage
        total_pre = self._get_total_data_usage_for_device(dut_a, data_usage)

        # set data usage limit to current usage limit + 10MB
        self.log.info("Set data usage limit to %sMB" % (total_pre + INC_DATA))
        dut_a.droid.connectivitySetDataUsageLimit(
            sub_id, int((total_pre + INC_DATA) * BYTE_TO_MB_ANDROID))

        # download file on dut_b and look for BlockedStatusChanged callback
        q = queue.Queue()
        t = threading.Thread(target=self._listen_for_network_callback,
                             args=(dut_a, "BlockedStatusChanged", q))
        t.daemon = True
        t.start()
        status = http_file_download_by_chrome(
            dut_b, file_link, self.file_size, True, TIMEOUT)
        t.join()
        cb_res = q.get()

        # verify file download fails and expected callback is recevied
        asserts.assert_true(cb_res,
                            "Failed to verify blocked status network callback")
        asserts.assert_true(not status,
                            "File download successful. Expected to fail")

    ### Test Cases

    @test_tracker_info(uuid="b2d9b36c-3a1c-47ca-a9c1-755450abb20c")
    def test_mobile_data_usage_downlink_ipv4_tmo(self):
        """Verify mobile data usage over IPv4 and TMO carrier."""
        self._test_data_usage_downlink(self.android_devices[0],
                                       self.android_devices[0],
                                       self.download_file_ipv4,
                                       cconst.TYPE_MOBILE)

    @test_tracker_info(uuid="fbbb58ed-d573-4bbd-bd5d-0bc540507896")
    def test_mobile_data_usage_downlink_ipv6_tmo(self):
        """Verify mobile data usage over IPv6 and TMO carrier."""
        self._test_data_usage_downlink(self.android_devices[0],
                                       self.android_devices[0],
                                       self.download_file_ipv6,
                                       cconst.TYPE_MOBILE)

    @test_tracker_info(uuid="6562d626-2271-4d93-96c0-f33138635330")
    def test_mobile_data_usage_downlink_ipv4_vzw(self):
        """Verify mobile data usage over IPv4 and VZW carrier."""
        self._test_data_usage_downlink(self.android_devices[1],
                                       self.android_devices[1],
                                       self.download_file_ipv4,
                                       cconst.TYPE_MOBILE)

    @test_tracker_info(uuid="2edf94cf-d937-459a-a7e4-2c679803c4d3")
    def test_mobile_data_usage_downlink_ipv6_vzw(self):
        """Verify mobile data usage over IPv6 and VZW carrier."""
        self._test_data_usage_downlink(self.android_devices[1],
                                       self.android_devices[1],
                                       self.download_file_ipv6,
                                       cconst.TYPE_MOBILE)

    @test_tracker_info(uuid="fe1390e5-635c-49a9-b050-032e66f52f40")
    def test_wifi_tethering_mobile_data_usage_downlink_ipv4_tmo(self):
        """Verify mobile data usage with tethered device over IPv4 and TMO."""
        self._test_data_usage_downlink(self.android_devices[0],
                                       self.android_devices[1],
                                       self.download_file_ipv4,
                                       cconst.TYPE_MOBILE,
                                       True,
                                       False,
                                       False)

    @test_tracker_info(uuid="d7fde6b2-6672-484a-a2fd-47858f5a163f")
    def test_wifi_tethering_mobile_data_usage_downlink_ipv6_tmo(self):
        """Verify mobile data usage with tethered device over IPv6 and TMO."""
        self._test_data_usage_downlink(self.android_devices[0],
                                       self.android_devices[1],
                                       self.download_file_ipv6,
                                       cconst.TYPE_MOBILE,
                                       True,
                                       False,
                                       False)

    @test_tracker_info(uuid="6285ef69-37a8-4069-8cb2-21dc266a57c3")
    def test_wifi_tethering_mobile_data_usage_downlink_ipv4_vzw(self):
        """Verify mobile data usage with tethered device over IPv4 and VZW."""
        self._test_data_usage_downlink(self.android_devices[1],
                                       self.android_devices[0],
                                       self.download_file_ipv4,
                                       cconst.TYPE_MOBILE,
                                       True,
                                       False,
                                       False)

    @test_tracker_info(uuid="565fc7e3-d7cf-43cc-8982-4f17999cf579")
    def test_wifi_tethering_mobile_data_usage_downlink_ipv6_vzw(self):
        """Verify mobile data usage with tethered device over IPv6 and VZW."""
        self._test_data_usage_downlink(self.android_devices[1],
                                       self.android_devices[0],
                                       self.download_file_ipv6,
                                       cconst.TYPE_MOBILE,
                                       True,
                                       False,
                                       False)

    @test_tracker_info(uuid="72ddb42a-5942-4a6a-8b20-2181c41b2765")
    def test_wifi_data_usage_downlink_ipv4(self):
        """Verify wifi data usage over IPv4."""
        self._test_data_usage_downlink(self.android_devices[0],
                                       self.android_devices[0],
                                       self.download_file_ipv4,
                                       cconst.TYPE_WIFI,
                                       False,
                                       True,
                                       True)

    @test_tracker_info(uuid="76b316f5-2946-4757-b5d6-62a8b1fd627e")
    def test_wifi_data_usage_downlink_ipv6(self):
        """Verify wifi data usage over IPv6."""
        self._test_data_usage_downlink(self.android_devices[0],
                                       self.android_devices[0],
                                       self.download_file_ipv6,
                                       cconst.TYPE_WIFI,
                                       False,
                                       True,
                                       True)

    @test_tracker_info(uuid="4be5f2d4-9bc6-4190-813e-044f00d94aa6")
    def test_wifi_tethering_wifi_data_usage_downlink_ipv4(self):
        """Verify wifi data usage with tethered device over IPv4."""
        asserts.skip_if(
            self.android_devices[0].model not in self.dbs_supported_models,
            "Device %s does not support dual interfaces." %
            self.android_devices[0].model)
        self._test_data_usage_downlink(self.android_devices[0],
                                       self.android_devices[1],
                                       self.download_file_ipv4,
                                       cconst.TYPE_WIFI,
                                       True,
                                       True,
                                       False)

    @test_tracker_info(uuid="ac4750fd-20d9-451d-a85b-79fdbaa7da97")
    def test_data_usage_limit_downlink_ipv4_tmo(self):
        """Verify data limit for TMO with IPv4 link."""
        self._test_data_usage_limit_downlink(self.android_devices[0],
                                             self.android_devices[0],
                                             self.download_file_ipv4,
                                             cconst.TYPE_MOBILE)

    @test_tracker_info(uuid="6598732e-f4d3-40c7-a73b-e89bb3d196c5")
    def test_data_usage_limit_downlink_ipv6_tmo(self):
        """Verify data limit for TMO with IPv6 link."""
        self._test_data_usage_limit_downlink(self.android_devices[0],
                                             self.android_devices[0],
                                             self.download_file_ipv6,
                                             cconst.TYPE_MOBILE)

    @test_tracker_info(uuid="d1ef0405-cf58-4141-94c7-cfa0c9d14438")
    def test_data_usage_limit_downlink_ipv4_vzw(self):
        """Verify data limit for VZW with IPv4 link."""
        self._test_data_usage_limit_downlink(self.android_devices[1],
                                             self.android_devices[1],
                                             self.download_file_ipv4,
                                             cconst.TYPE_MOBILE)

    @test_tracker_info(uuid="18d169d9-fc4f-4782-8f5f-fc96b8976d03")
    def test_data_usage_limit_downlink_ipv6_vzw(self):
        """Verify data limit for VZW with IPv6 link."""
        self._test_data_usage_limit_downlink(self.android_devices[1],
                                             self.android_devices[1],
                                             self.download_file_ipv6,
                                             cconst.TYPE_MOBILE)

    @test_tracker_info(uuid="7c9ab330-9645-4030-bb1e-dcce126944a2")
    def test_wifi_tethering_data_usage_limit_downlink_ipv4_tmo(self):
        """Verify data limit for TMO from tethered device with IPv4 link."""
        self._test_data_usage_limit_downlink(self.android_devices[0],
                                             self.android_devices[1],
                                             self.download_file_ipv4,
                                             cconst.TYPE_MOBILE,
                                             True)

    @test_tracker_info(uuid="1cafdaf7-a41e-4d19-b08a-ae094d796426")
    def test_wifi_tethering_data_usage_limit_downlink_ipv6_tmo(self):
        """Verify data limit for TMO from tethered device with IPv6 link."""
        self._test_data_usage_limit_downlink(self.android_devices[0],
                                             self.android_devices[1],
                                             self.download_file_ipv6,
                                             cconst.TYPE_MOBILE,
                                             True)

    @test_tracker_info(uuid="bd5a8f93-4e2f-474d-a01d-4bb5117d5350")
    def test_wifi_tethering_data_usage_limit_downlink_ipv4_vzw(self):
        """Verify data limit for VZW from tethered device with IPv4 link."""
        self._test_data_usage_limit_downlink(self.android_devices[1],
                                             self.android_devices[0],
                                             self.download_file_ipv4,
                                             cconst.TYPE_MOBILE,
                                             True)

    @test_tracker_info(uuid="ec082fe5-6af1-425e-ace6-4726930bf62f")
    def test_wifi_tethering_data_usage_limit_downlink_ipv6_vzw(self):
        """Verify data limit for VZW from tethered device with IPv6 link."""
        self._test_data_usage_limit_downlink(self.android_devices[1],
                                             self.android_devices[0],
                                             self.download_file_ipv6,
                                             cconst.TYPE_MOBILE,
                                             True)
