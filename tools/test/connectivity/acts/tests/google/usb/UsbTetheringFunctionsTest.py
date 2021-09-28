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

import re
import time
from queue import Empty

from acts import utils
from acts import base_test
from acts.libs.proc import job
from acts import signals
from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.tel import tel_test_utils as tutils
from acts.test_utils.tel import tel_defines
from acts.test_utils.tel.anritsu_utils import wait_for_sms_sent_success
from acts.test_utils.tel.tel_defines import EventMmsSentSuccess

# Time it takes for the usb tethering IP to
# show up in ifconfig and function waiting.
DEFAULT_SETTLE_TIME = 5
WifiEnums = wutils.WifiEnums
USB_CHARGE_MODE = 'svc usb setFunctions'
USB_TETHERING_MODE = 'svc usb setFunctions rndis'
USB_MTP_MODE = 'svc usb setFunctions mtp'
DEVICE_IP_ADDRESS = 'ip address'


class UsbTetheringFunctionsTest(base_test.BaseTestClass):
    """Tests for usb tethering throughput test.

    Test Bed Requirement:
        * One Android device.
        * Wi-Fi networks visible to the device.
        * Sim card with data call.
    """

    def setup_class(self):
        self.dut = self.android_devices[0]
        req_params = ['wifi_network', 'receiver_number', 'ping_count']
        self.unpack_userparams(req_param_names=req_params)

        self.ssid_map = {}
        for network in self.wifi_network:
            SSID = network['SSID'].replace('-', '_')
            self.ssid_map[SSID] = network
        self.dut.droid.setMobileDataEnabled()
        if not tutils.verify_internet_connection(
                self.dut.log, self.dut, retries=3):
            tutils.abort_all_tests(self.dut.log, 'Internet connection Failed.')

    def setup_test(self):
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()
        self.dut.unlock_screen()

    def teardown_test(self):
        wutils.wifi_toggle_state(self.dut, False)
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        self.dut.droid.setMobileDataEnabled()
        self.dut.droid.connectivityStopTethering(tel_defines.TETHERING_WIFI)
        self.dut.stop_services()
        # Set usb function back to charge mode.
        self.dut.adb.shell(USB_CHARGE_MODE)
        self.dut.adb.wait_for_device()
        self.dut.start_services()

    def teardown_class(self):
        wutils.reset_wifi(self.dut)

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)
        self.dut.cat_adb_log(test_name, begin_time)

    def enable_usb_tethering(self, attempts=3):
        """Stop SL4A service and enable usb tethering.

        Logic steps are
        1. Stop SL4A service.
        2. Enable usb tethering.
        3. Restart SL4A service.
        4. Check usb tethering enabled.
        5. Attempt if fail to enable usb tethering.

        Args:
            attempts: number of times for enable usb tethering.

        Raises:
            TestFailure when unable to find rndis string.
        """
        for i in range(attempts):
            self.dut.stop_services()
            self.dut.adb.shell(USB_TETHERING_MODE, ignore_status=True)
            self.dut.adb.wait_for_device()
            self.dut.start_services()
            if 'rndis' in self.dut.adb.shell(DEVICE_IP_ADDRESS):
                self.log.info('Usb tethering is enabled.')
                return
            else:
                self.log.info('Enable usb tethering - attempt %d' % (i + 1))
        raise signals.TestFailure('Unable to enable USB tethering.')

    def connect_to_wifi_network(self, network):
        """Connection logic for wifi network.

        Args:
            network: Dictionary with network info.
        """
        SSID = network[WifiEnums.SSID_KEY]
        self.dut.ed.clear_all_events()
        wutils.start_wifi_connection_scan(self.dut)
        scan_results = self.dut.droid.wifiGetScanResults()
        wutils.assert_network_in_list({WifiEnums.SSID_KEY: SSID}, scan_results)
        wutils.wifi_connect(self.dut, network, num_of_tries=3)

    def enable_wifi_hotspot(self):
        """Enable wifi hotspot."""
        sap_config = wutils.create_softap_config()
        wutils.start_wifi_tethering(self.dut,
                                    sap_config[wutils.WifiEnums.SSID_KEY],
                                    sap_config[wutils.WifiEnums.PWD_KEY],
                                    wutils.WifiEnums.WIFI_CONFIG_APBAND_2G)

    def get_rndis_interface(self):
        """Check rndis interface after usb tethering enable.

        Returns:
            Usb tethering interface from Android device.

        Raises:
            TestFailure when unable to find correct usb tethering interface.
        """
        time.sleep(DEFAULT_SETTLE_TIME)
        check_usb_tethering = job.run('ifconfig').stdout
        # A regex that stores the tethering interface in group 1.
        tethered_interface_regex = r'^(enp.*?):.*?broadcast 192.168.42.255'
        match = re.search(tethered_interface_regex, check_usb_tethering,
                          re.DOTALL + re.MULTILINE)
        if match:
            return match.group(1)
        else:
            raise signals.TestFailure(
                'Unable to find tethering interface. The device may not be tethered.'
            )

    def can_ping(self, ip, extra_params='', count=10):
        """Run ping test and check and check ping lost rate.

        Args:
            ip: ip address for ping.
            extra_params: params for ping test.
            count: default ping count.

        Returns:
            True: If no packet loss.
            False: Otherwise.
        """
        out = job.run(
            'ping -c {} {} {}'.format(count, extra_params, ip),
            ignore_status=True).stdout
        self.log.info(out)
        return '0%' in out.split(' ')

    def can_ping_through_usb_interface(self):
        """Run ping test and check result after usb tethering enabled.

        Raises:
            TestFailure when unable to ping through usb tethering interface.
        """
        ip = '8.8.8.8'
        interface = self.get_rndis_interface()
        if not self.can_ping(
                ip, '-I {}'.format(interface), count=self.ping_count):
            raise signals.TestFailure(
                'Fail to ping through usb tethering interface.')

    def enable_usb_mtp(self):
        """Enable usb mtp mode.

        Raises:
            TestFailure when unable to set mtp mode.
        """
        try:
            self.dut.stop_services()
            self.dut.adb.shell(USB_MTP_MODE, ignore_status=True)
            self.dut.adb.wait_for_device()
            self.dut.start_services()
        except:
            raise signals.TestFailure('Device can not enable mtp mode.')

    def check_sms_send_status(self, message='usb_sms_test'):
        """Send a SMS and check send status.

        Args:
            message: SMS string.

        Raises:
            Exception when unable to send SMS.
        """
        self.dut.droid.smsSendTextMessage(self.receiver_number, message, False)
        self.log.info('Waiting for SMS sent event')
        test_status = wait_for_sms_sent_success(self.log, self.dut)
        if not test_status:
            raise Exception('Failed to send SMS')

    def check_mms_send_status(self,
                              subject='usb_subject',
                              message='usb_mms_test'):
        """Send a MMS and check send status.

        Args:
            subject: MMS subject.
            message: MMS string.

        Raises:
            Exception when unable to send MMS.
        """
        # Permission require for send MMS.
        self.dut.adb.shell('su root setenforce 0')
        self.dut.droid.smsSendMultimediaMessage(self.receiver_number, subject,
                                                message)
        self.log.info('Waiting for MMS sent event')
        test_status = self.wait_for_mms_sent_success()
        if not test_status:
            raise Exception('Failed to send MMS')

    def wait_for_mms_sent_success(self, time_to_wait=60):
        """Check MMS send status.

        Args:
            time_to_wait: Time out for check MMS.

        Returns:
            status = MMS send status.
        """
        mms_sent_event = EventMmsSentSuccess
        try:
            event = self.dut.ed.pop_event(mms_sent_event, time_to_wait)
            self.log.info(event)
        except Empty:
            self.log.error('Timeout: Expected event is not received.')
            return False
        return True

    @test_tracker_info(uuid="7c2ae85e-32a2-416e-a65e-c15a15e76f86")
    def test_usb_tethering_wifi_only(self):
        """Enable usb tethering with wifi only then executing ping test.

        Steps:
        1. Stop SL4A services.
        2. Enable usb tethering.
        3. Restart SL4A services.
        4. Mobile data disable and wifi enable.
        5. Run ping test through usb tethering interface.
        """
        self.enable_usb_tethering()
        self.log.info('Disable mobile data.')
        self.dut.droid.setMobileDataEnabled(False)
        self.log.info('Enable wifi.')
        wutils.wifi_toggle_state(self.dut, True)
        self.connect_to_wifi_network(
            self.ssid_map[self.wifi_network[0]['SSID']])
        self.can_ping_through_usb_interface()

    @test_tracker_info(uuid="8910b07b-0beb-4d9d-b901-c4195b4e0930")
    def test_usb_tethering_data_only(self):
        """Enable usb tethering with data only then executing ping test.

        Steps:
        1. Stop SL4A services.
        2. Enable usb tethering.
        3. Restart SL4A services.
        4. Wifi disable and mobile data enable.
        5. Run ping test through usb tethering interface.
        """
        self.enable_usb_tethering()
        wutils.wifi_toggle_state(self.dut, False)
        self.dut.droid.setMobileDataEnabled()
        time.sleep(DEFAULT_SETTLE_TIME)
        self.can_ping_through_usb_interface()

    @test_tracker_info(uuid="f971806e-e003-430a-bc80-321f128d31cb")
    def test_usb_tethering_after_airplane_mode(self):
        """Enable usb tethering after airplane mode then executing ping test.

        Steps:
        1. Stop SL4A services.
        2. Enable usb tethering.
        3. Restart SL4A services.
        4. Wifi disable and mobile data enable.
        5. Enable/disable airplane mode.
        6. Run ping test through usb tethering interface.
        """
        self.enable_usb_tethering()
        wutils.wifi_toggle_state(self.dut, False)
        self.log.info('Enable airplane mode.')
        utils.force_airplane_mode(self.dut, True)
        self.log.info('Disable airplane mode.')
        utils.force_airplane_mode(self.dut, False)
        time.sleep(DEFAULT_SETTLE_TIME)
        self.can_ping_through_usb_interface()

    @test_tracker_info(uuid="db1c561d-67bd-47d7-b65e-d882f0e2641e")
    def test_usb_tethering_coexist_wifi_hotspot(self):
        """Enable usb tethering with hotspot then executing ping test.

        Steps:
        1. Enable hotspot
        2. Stop SL4A services.
        3. Enable usb tethering.
        4. Restart SL4A services.
        5. Run ping test through tethering interface during hotspot enable.
        6. Run ping test through tethering interface during hotspot disable.
        """
        self.log.info('Enable wifi hotspot.')
        self.enable_wifi_hotspot()
        self.enable_usb_tethering()
        self.log.info('Ping test with hotspot enable.')
        self.can_ping_through_usb_interface()
        self.log.info('Disable wifi hotspot.')
        self.dut.droid.connectivityStopTethering(tel_defines.TETHERING_WIFI)
        self.log.info('Ping test with hotspot disable.')
        self.can_ping_through_usb_interface()

    @test_tracker_info(uuid="7018abdb-049e-41aa-a4f9-e11012369d9b")
    def test_usb_tethering_after_mtp(self):
        """Enable usb tethering after mtp enable then executing ping test.

        Steps:
        1. Stop SL4A services.
        2. Enable usb tethering.
        3. Restart SL4A services.
        4. Enable usb mtp mode.
        5. Enable usb tethering and mtp will disable.
        6. Run ping test through usb tethering interface.
        """
        self.enable_usb_tethering()
        self.log.info('Enable usb mtp mode.')
        self.enable_usb_mtp()
        # Turn on mtp mode for 5 sec.
        time.sleep(DEFAULT_SETTLE_TIME)
        self.enable_usb_tethering()
        self.log.info('Usb tethering enable, usb mtp mode disabled.')
        self.can_ping_through_usb_interface()

    @test_tracker_info(uuid="f1ba2cbc-1cb2-4b8a-92eb-9b366c3f4c37")
    def test_usb_tethering_after_sms_mms(self):
        """Enable usb tethering after send sms and mms then executing ping test.

        Steps:
        1. Stop SL4A services.
        2. Enable usb tethering.
        3. Restart SL4A services.
        4. Send a sms and mms.
        5. Run ping test through usb tethering interface.
        """
        self.enable_usb_tethering()
        self.log.info('Start send SMS and check status.')
        self.check_sms_send_status()
        time.sleep(DEFAULT_SETTLE_TIME)
        self.log.info('Start send MMS and check status.')
        self.check_mms_send_status()
        self.can_ping_through_usb_interface()

    @test_tracker_info(uuid="e6586b30-4886-4c3e-8225-633aa9333968")
    def test_usb_tethering_after_reboot(self):
        """Enable usb tethering after reboot then executing ping test.

        Steps:
        1. Reboot device.
        2. Stop SL4A services.
        3. Enable usb tethering.
        4. Restart SL4A services.
        5. Run ping test through usb tethering interface.
        """
        self.enable_usb_tethering()
        self.log.info('Reboot device.')
        self.dut.reboot()
        self.dut.droid.setMobileDataEnabled()
        self.log.info('Start usb tethering and ping test.')
        self.enable_usb_tethering()
        self.can_ping_through_usb_interface()

    @test_tracker_info(uuid="40093ab8-6db4-4af4-97ae-9467bf33bf23")
    def test_usb_tethering_after_wipe(self):
        """Enable usb tethering after wipe.

        Steps:
        1. Enable usb tethering.
        2. Wipe device.
        3. Wake up device and unlock screen.
        4. Enable usb tethering.
        5. Run ping test through usb tethering interface.
        """
        self.enable_usb_tethering()
        tutils.fastboot_wipe(self.dut)
        time.sleep(DEFAULT_SETTLE_TIME)
        # Skip setup wizard after wipe.
        self.dut.adb.shell(
            'am start -a com.android.setupwizard.EXIT', ignore_status=True)
        self.dut.droid.setMobileDataEnabled()
        self.dut.droid.wakeUpNow()
        self.dut.unlock_screen()
        self.enable_usb_tethering()
        self.can_ping_through_usb_interface()