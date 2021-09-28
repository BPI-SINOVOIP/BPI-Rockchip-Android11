#
#  Copyright 2019 - The Android Open Source Project
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import math
import os

import time

import threading
from acts import utils
from acts import signals
from acts import asserts
from acts.controllers import attenuator
from acts.controllers.sl4a_lib import rpc_client
from acts.test_decorators import test_tracker_info
from acts.test_utils.net.net_test_utils import start_tcpdump, stop_tcpdump
from acts.test_utils.wifi import wifi_test_utils as wutils
from acts.test_utils.wifi.wifi_test_utils import WifiEnums
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.utils import stop_standing_subprocess

TCPDUMP_PATH = '/data/local/tmp/tcpdump'


class WifiIFSTwTest(WifiBaseTest):
    """Tests for wifi IFS

        Test Bed Requirement:
            *One Android device
            *Two Visible Wi-Fi Access Points
            *One attenuator with 4 ports
    """

    def setup_class(self):
        """Setup required dependencies from config file and configure
        the required networks for testing roaming.

        Returns:
            True if successfully configured the requirements for testing.
        """
        super().setup_class()
        self.simulation_thread_running = False
        self.atten_roaming_count = 0
        self.start_db = 30
        self.roaming_cycle_seconds = 20
        self.fail_count = 0
        self.retry_pass_count = 0
        self.ping_count = 0

        self.dut = self.android_devices[0]
        wutils.wifi_test_device_init(self.dut)
        req_params = ["attenuators", "ifs_params"]
        opt_param = []

        self.unpack_userparams(
            req_param_names=req_params, opt_param_names=opt_param)

        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start(ap_count=2, same_ssid=True)

        wutils.wifi_toggle_state(self.dut, True)
        if "ifs_params" in self.user_params:
            self.attn_start_db = self.ifs_params[0]["start_db"]
            self.gateway = self.ifs_params[0]["gateway"]
            self.roaming_cycle_seconds = self.ifs_params[0][
                "roaming_cycle_seconds"]
            self.total_test_hour = self.ifs_params[0]["total_test_hour"]
            self.log_capture_period_hour = self.ifs_params[0][
                "log_capture_period_hour"]
            self.on_active_port = self.ifs_params[0]["on_active_port"]
            asserts.assert_true(
                len(self.on_active_port) == 2, "Need setup 2 port.")

        self.tcpdump_pid = None
        utils.set_location_service(self.dut, True)

    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()
        self.dut.unlock_screen()
        self.tcpdump_pid = start_tcpdump(self.dut, self.test_name)

    def teardown_class(self):
        self.dut.ed.clear_all_events()

    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        wutils.reset_wifi(self.dut)

    def simulate_roaming(self):
        """
        To simulate user move between ap1 and ap2:

        1. Move to ap2:
            Set ap1's signal attenuation gradually changed from 0 to max_db
            Set ap2's signal attenuation gradually changed from start_db to 0

        2. Then move to ap1:
            Set ap1's signal attenuation gradually changed from start_db to 0
            Set ap2's signal attenuation gradually changed from 0 to max_db

        * 0<start_db<max_db
        """
        attn_max = 95
        attn_min = 0

        #on_active_port value should between [0-1,2-3]
        active_attenuator = {
            "1": self.attenuators[self.on_active_port[0]],
            "2": self.attenuators[self.on_active_port[1]]
        }

        for attenuator in self.attenuators:
            attenuator.set_atten(attn_max)

        self.simulation_thread_running = True
        while self.simulation_thread_running:
            active_attenuator["1"].set_atten(attn_min)
            active_attenuator["2"].set_atten(attn_max)
            self.log_attens()
            time.sleep(10)

            active_attenuator["2"].set_atten(self.start_db)
            self.log_attens()
            time.sleep(5)
            for i in range(self.roaming_cycle_seconds):
                db1 = math.ceil(attn_max / self.roaming_cycle_seconds *
                                (i + 1))
                db2 = self.start_db - math.ceil(
                    self.start_db / self.roaming_cycle_seconds * (i + 1))
                active_attenuator["1"].set_atten(db1)
                active_attenuator["2"].set_atten(db2)
                self.log_attens()
                time.sleep(1)

            active_attenuator["1"].set_atten(self.start_db)
            self.log_attens()
            time.sleep(5)
            for i in range(self.roaming_cycle_seconds):
                db1 = math.ceil(attn_max / self.roaming_cycle_seconds *
                                (i + 1))
                db2 = self.start_db - math.ceil(
                    self.start_db / self.roaming_cycle_seconds * (i + 1))
                active_attenuator["1"].set_atten(db2)
                active_attenuator["2"].set_atten(db1)
                self.log_attens()
                time.sleep(1)
            self.atten_roaming_count += 1

    def catch_log(self):
        """Capture logs include bugreport, ANR, mount,ps,vendor,tcpdump"""

        self.log.info("Get log for regular capture.")
        file_name = time.strftime("%Y-%m-%d_%H:%M:%S", time.localtime())
        current_path = os.path.join(self.dut.log_path, file_name)
        os.makedirs(current_path, exist_ok=True)
        serial_number = self.dut.serial

        try:
            out = self.dut.adb.shell("bugreportz", timeout=240)
            if not out.startswith("OK"):
                raise AndroidDeviceError(
                    'Failed to take bugreport on %s: %s' % (serial_number,
                                                            out),
                    serial=serial_number)
            br_out_path = out.split(':')[1].strip().split()[0]
            self.dut.adb.pull("%s %s" % (br_out_path, self.dut.log_path))
            self.dut.adb.pull("/data/anr {}".format(current_path), timeout=600)
            self.dut.adb._exec_adb_cmd("shell", "mount > {}".format(
                os.path.join(current_path, "mount.txt")))
            self.dut.adb._exec_adb_cmd("shell", "ps > {}".format(
                os.path.join(current_path, "ps.txt")))
            self.dut.adb.pull("/data/misc/logd {}".format(current_path))
            self.dut.adb.pull(
                "/data/vendor {}".format(current_path), timeout=800)
            stop_tcpdump(
                self.dut, self.tcpdump_pid, file_name, adb_pull_timeout=600)
            self.tcpdump_pid = start_tcpdump(self.dut, file_name)
        except TimeoutError as e:
            self.log.error(e)

    def http_request(self, url="https://www.google.com/"):
        """Get the result via string from target url

        Args:
            url: target url to loading

        Returns:
            True if http_request pass
        """

        self.ping_count += 1
        try:
            self.dut.droid.httpRequestString(url)
            self.log.info("httpRequest Finish")
            time.sleep(1)
            return True
        except rpc_client.Sl4aApiError as e:
            self.log.warning("httpRequest Fail.")
            self.log.warning(e)
            # Set check delay if http request fail during device roaming.
            # Except finish roaming within 10s.
            time.sleep(10)
            self.log.warning("Ping Google DNS response : {}".format(
                self.can_ping("8.8.8.8")))
            for gate in self.gateway:
                ping_result = self.can_ping(gate)
                self.log.warning("Ping AP Gateway[{}] response : {}".format(
                    gate, ping_result))
                if ping_result:
                    self.retry_pass_count += 1
                    return True
            self.fail_count += 1
            return False

    def log_attens(self):
        """Log DB from channels"""

        attenuation = ', '.join('{:>5.2f}dB '.format(atten.get_atten())
                                for atten in self.attenuators)
        self.log.debug('[Attenuation] %s', attenuation)

    def can_ping(self, ip_addr):
        """A function to check ping pass.

        Args:
            ip_addr: target ip address to ping

        Returns:
            True if ping pass
        """
        ping_result = self.dut.adb.shell("ping -c 1 {}".format(ip_addr))
        return '0%' in ping_result.split(' ')

    def browsing_test(self, stress_hour_time):
        """Continue stress http_request and capture log if any fail

        Args:
            stress_hour_time: hour of time to stress http_request
        """
        t = threading.Thread(target=self.simulate_roaming)
        t.start()
        start_time = time.time()
        http_request_failed = False
        while time.time() < start_time + stress_hour_time * 3600:
            if not self.http_request():
                http_request_failed = True
        self.simulation_thread_running = False
        t.join()
        if http_request_failed:
            self.catch_log()
        else:
            stop_standing_subprocess(self.tcpdump_pid)
            file_name = time.strftime("%Y-%m-%d_%H:%M:%S", time.localtime())
            self.tcpdump_pid = start_tcpdump(self.dut, file_name)

    def test_roaming(self):
        network = self.reference_networks[0]["2g"]
        wutils.connect_to_wifi_network(self.dut, network)

        time.sleep(10)
        test_time_slot = int(
            self.total_test_hour / self.log_capture_period_hour)
        edge_time_slot = int(
            self.total_test_hour % self.log_capture_period_hour)

        for i in range(test_time_slot):
            self.browsing_test(self.log_capture_period_hour)
        if edge_time_slot:
            self.browsing_test(edge_time_slot)

        self.log.info("Total roaming times: {}".format(
            self.atten_roaming_count))
        self.log.info("Total ping times: {}".format(self.ping_count))
        self.log.info("Retry pass times: {}".format(self.retry_pass_count))
        self.log.info("Total fail times: {}".format(self.fail_count))
        if self.fail_count:
            signals.TestFailure(
                'Find roaming fail condition',
                extras={
                    'Roaming fail times': self.fail_count
                })
