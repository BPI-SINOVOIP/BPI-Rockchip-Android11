#!/usr/bin/env python3.4
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

import pprint
import queue
import threading
import time

import acts.base_test
import acts.test_utils.wifi.wifi_test_utils as wutils
import acts.test_utils.tel.tel_test_utils as tutils

from acts import asserts
from acts import signals
from acts import utils
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.bt_test_utils import enable_bluetooth
from acts.test_utils.bt.bt_test_utils import disable_bluetooth
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
WifiEnums = wutils.WifiEnums

WAIT_FOR_AUTO_CONNECT = 40
WAIT_BEFORE_CONNECTION = 30

TIMEOUT = 5
PING_ADDR = 'www.google.com'

class WifiStressTest(WifiBaseTest):
    """WiFi Stress test class.

    Test Bed Requirement:
    * Two Android device
    * Several Wi-Fi networks visible to the device, including an open Wi-Fi
      network.
    """
    def __init__(self, configs):
        super().__init__(configs)
        self.enable_packet_log = True

    def setup_class(self):
        super().setup_class()

        self.dut = self.android_devices[0]
        # Note that test_stress_softAP_startup_and_stop_5g will always fail
        # when testing with a single device.
        if len(self.android_devices) > 1:
            self.dut_client = self.android_devices[1]
        else:
            self.dut_client = None
        wutils.wifi_test_device_init(self.dut)
        req_params = []
        opt_param = [
            "open_network", "reference_networks", "iperf_server_address",
            "stress_count", "stress_hours", "attn_vals", "pno_interval",
            "iperf_server_port"]
        self.unpack_userparams(
            req_param_names=req_params, opt_param_names=opt_param)

        if "AccessPoint" in self.user_params:
            self.legacy_configure_ap_and_start(ap_count=2)

        asserts.assert_true(
            len(self.reference_networks) > 0,
            "Need at least one reference network with psk.")
        self.wpa_2g = self.reference_networks[0]["2g"]
        self.wpa_5g = self.reference_networks[0]["5g"]
        self.open_2g = self.open_network[0]["2g"]
        self.open_5g = self.open_network[0]["5g"]
        self.networks = [self.wpa_2g, self.wpa_5g, self.open_2g, self.open_5g]

    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()

    def teardown_test(self):
        super().teardown_test()
        if self.dut.droid.wifiIsApEnabled():
            wutils.stop_wifi_tethering(self.dut)
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        wutils.reset_wifi(self.dut)

    def teardown_class(self):
        wutils.reset_wifi(self.dut)
        if "AccessPoint" in self.user_params:
            del self.user_params["reference_networks"]
            del self.user_params["open_network"]

    """Helper Functions"""

    def scan_and_connect_by_ssid(self, ad, network):
        """Scan for network and connect using network information.

        Args:
            network: A dictionary representing the network to connect to.

        """
        ssid = network[WifiEnums.SSID_KEY]
        wutils.start_wifi_connection_scan_and_ensure_network_found(ad, ssid)
        wutils.wifi_connect(ad, network, num_of_tries=3)

    def scan_and_connect_by_id(self, network, net_id):
        """Scan for network and connect using network id.

        Args:
            net_id: Integer specifying the network id of the network.

        """
        ssid = network[WifiEnums.SSID_KEY]
        wutils.start_wifi_connection_scan_and_ensure_network_found(self.dut,
            ssid)
        wutils.wifi_connect_by_id(self.dut, net_id)

    def run_ping(self, sec):
        """Run ping for given number of seconds.

        Args:
            sec: Time in seconds to run teh ping traffic.

        """
        self.log.info("Running ping for %d seconds" % sec)
        result = self.dut.adb.shell("ping -w %d %s" %(sec, PING_ADDR),
            timeout=sec+1)
        self.log.debug("Ping Result = %s" % result)
        if "100% packet loss" in result:
            raise signals.TestFailure("100% packet loss during ping")

    def start_youtube_video(self, url=None, secs=60):
        """Start a youtube video and check if it's playing.

        Args:
            url: The URL of the youtube video to play.
            secs: Time to play video in seconds.

        """
        ad = self.dut
        ad.log.info("Start a youtube video")
        ad.ensure_screen_on()
        video_played = False
        for count in range(2):
            ad.unlock_screen()
            ad.adb.shell('am start -a android.intent.action.VIEW -d "%s"' % url)
            if tutils.wait_for_state(ad.droid.audioIsMusicActive, True, 15, 1):
                ad.log.info("Started a video in youtube.")
                # Play video for given seconds.
                time.sleep(secs)
                video_played = True
                break
        if not video_played:
            raise signals.TestFailure("Youtube video did not start. Current WiFi "
                "state is %d" % self.dut.droid.wifiCheckState())

    def add_networks(self, ad, networks):
        """Add Wi-Fi networks to an Android device and verify the networks were
        added correctly.

        Args:
            ad: the AndroidDevice object to add networks to.
            networks: a list of dicts, each dict represents a Wi-Fi network.
        """
        for network in networks:
            ret = ad.droid.wifiAddNetwork(network)
            asserts.assert_true(ret != -1, "Failed to add network %s" %
                                network)
            ad.droid.wifiEnableNetwork(ret, 0)
        configured_networks = ad.droid.wifiGetConfiguredNetworks()
        self.log.debug("Configured networks: %s", configured_networks)

    def connect_and_verify_connected_ssid(self, expected_con, is_pno=False):
        """Start a scan to get the DUT connected to an AP and verify the DUT
        is connected to the correct SSID.

        Args:
            expected_con: The expected info of the network to we expect the DUT
                to roam to.
        """
        connection_info = self.dut.droid.wifiGetConnectionInfo()
        self.log.info("Triggering network selection from %s to %s",
                      connection_info[WifiEnums.SSID_KEY],
                      expected_con[WifiEnums.SSID_KEY])
        self.attenuators[0].set_atten(0)
        if is_pno:
            self.log.info("Wait %ss for PNO to trigger.", self.pno_interval)
            time.sleep(self.pno_interval)
        else:
            # force start a single scan so we don't have to wait for the scheduled scan.
            wutils.start_wifi_connection_scan_and_return_status(self.dut)
            self.log.info("Wait 60s for network selection.")
            time.sleep(60)
        try:
            self.log.info("Connected to %s network after network selection"
                          % self.dut.droid.wifiGetConnectionInfo())
            expected_ssid = expected_con[WifiEnums.SSID_KEY]
            verify_con = {WifiEnums.SSID_KEY: expected_ssid}
            wutils.verify_wifi_connection_info(self.dut, verify_con)
            self.log.info("Connected to %s successfully after network selection",
                          expected_ssid)
        finally:
            pass

    def run_long_traffic(self, sec, args, q):
        try:
            # Start IPerf traffic
            self.log.info("Running iperf client {}".format(args))
            result, data = self.dut.run_iperf_client(self.iperf_server_address,
                args, timeout=sec+1)
            if not result:
                self.log.debug("Error occurred in iPerf traffic.")
                self.run_ping(sec)
            q.put(True)
        except:
            q.put(False)

    """Tests"""

    @test_tracker_info(uuid="cd0016c6-58cf-4361-b551-821c0b8d2554")
    def test_stress_toggle_wifi_state(self):
        """Toggle WiFi state ON and OFF for N times."""
        for count in range(self.stress_count):
            """Test toggling wifi"""
            try:
                self.log.debug("Going from on to off.")
                wutils.wifi_toggle_state(self.dut, False)
                self.log.debug("Going from off to on.")
                startTime = time.time()
                wutils.wifi_toggle_state(self.dut, True)
                startup_time = time.time() - startTime
                self.log.debug("WiFi was enabled on the device in %s s." %
                    startup_time)
            except:
                signals.TestFailure(details="", extras={"Iterations":"%d" %
                    self.stress_count, "Pass":"%d" %count})
        raise signals.TestPass(details="", extras={"Iterations":"%d" %
            self.stress_count, "Pass":"%d" %(count+1)})

    @test_tracker_info(uuid="4e591cec-9251-4d52-bc6e-6621507524dc")
    def test_stress_toggle_wifi_state_bluetooth_on(self):
        """Toggle WiFi state ON and OFF for N times when bluetooth ON."""
        enable_bluetooth(self.dut.droid, self.dut.ed)
        for count in range(self.stress_count):
            """Test toggling wifi"""
            try:
                self.log.debug("Going from on to off.")
                wutils.wifi_toggle_state(self.dut, False)
                self.log.debug("Going from off to on.")
                startTime = time.time()
                wutils.wifi_toggle_state(self.dut, True)
                startup_time = time.time() - startTime
                self.log.debug("WiFi was enabled on the device in %s s." %
                    startup_time)
            except:
                signals.TestFailure(details="", extras={"Iterations":"%d" %
                    self.stress_count, "Pass":"%d" %count})
        disable_bluetooth(self.dut.droid)
        raise signals.TestPass(details="", extras={"Iterations":"%d" %
            self.stress_count, "Pass":"%d" %(count+1)})

    @test_tracker_info(uuid="49e3916a-9580-4bf7-a60d-a0f2545dcdde")
    def test_stress_connect_traffic_disconnect_5g(self):
        """Test to connect and disconnect from a network for N times.

           Steps:
               1. Scan and connect to a network.
               2. Run IPerf to upload data for few seconds.
               3. Disconnect.
               4. Repeat 1-3.

        """
        for count in range(self.stress_count):
            try:
                net_id = self.dut.droid.wifiAddNetwork(self.wpa_5g)
                asserts.assert_true(net_id != -1, "Add network %r failed" % self.wpa_5g)
                self.scan_and_connect_by_id(self.wpa_5g, net_id)
                # Start IPerf traffic from phone to server.
                # Upload data for 10s.
                args = "-p {} -t {}".format(self.iperf_server_port, 10)
                self.log.info("Running iperf client {}".format(args))
                result, data = self.dut.run_iperf_client(self.iperf_server_address, args)
                if not result:
                    self.log.debug("Error occurred in iPerf traffic.")
                    self.run_ping(10)
                wutils.wifi_forget_network(self.dut,self.wpa_5g[WifiEnums.SSID_KEY])
                time.sleep(WAIT_BEFORE_CONNECTION)
            except:
                raise signals.TestFailure("Network connect-disconnect failed."
                    "Look at logs", extras={"Iterations":"%d" %
                        self.stress_count, "Pass":"%d" %count})
        raise signals.TestPass(details="", extras={"Iterations":"%d" %
            self.stress_count, "Pass":"%d" %(count+1)})

    @test_tracker_info(uuid="e9827dff-0755-43ec-8b50-1f9756958460")
    def test_stress_connect_long_traffic_5g(self):
        """Test to connect to network and hold connection for few hours.

           Steps:
               1. Scan and connect to a network.
               2. Run IPerf to download data for few hours.
               3. Run IPerf to upload data for few hours.
               4. Verify no WiFi disconnects/data interruption.

        """
        self.scan_and_connect_by_ssid(self.dut, self.wpa_5g)
        self.scan_and_connect_by_ssid(self.dut_client, self.wpa_5g)

        q = queue.Queue()
        sec = self.stress_hours * 60 * 60
        start_time = time.time()

        dl_args = "-p {} -t {} -R".format(self.iperf_server_port, sec)
        dl = threading.Thread(target=self.run_long_traffic, args=(sec, dl_args, q))
        dl.start()
        dl.join()

        total_time = time.time() - start_time
        self.log.debug("WiFi state = %d" %self.dut.droid.wifiCheckState())
        while(q.qsize() > 0):
            if not q.get():
                raise signals.TestFailure("Network long-connect failed.",
                    extras={"Total Hours":"%d" %self.stress_hours,
                    "Seconds Run":"%d" %total_time})
        raise signals.TestPass(details="", extras={"Total Hours":"%d" %
            self.stress_hours, "Seconds Run":"%d" %total_time})

    def test_stress_youtube_5g(self):
        """Test to connect to network and play various youtube videos.

           Steps:
               1. Scan and connect to a network.
               2. Loop through and play a list of youtube videos.
               3. Verify no WiFi disconnects/data interruption.

        """
        # List of Youtube 4K videos.
        videos = ["https://www.youtube.com/watch?v=TKmGU77INaM",
                  "https://www.youtube.com/watch?v=WNCl-69POro",
                  "https://www.youtube.com/watch?v=dVkK36KOcqs",
                  "https://www.youtube.com/watch?v=0wCC3aLXdOw",
                  "https://www.youtube.com/watch?v=rN6nlNC9WQA",
                  "https://www.youtube.com/watch?v=RK1K2bCg4J8"]
        try:
            self.scan_and_connect_by_ssid(self.dut, self.wpa_5g)
            start_time = time.time()
            for video in videos:
                self.start_youtube_video(url=video, secs=10*60)
        except:
            total_time = time.time() - start_time
            raise signals.TestFailure("The youtube stress test has failed."
                "WiFi State = %d" %self.dut.droid.wifiCheckState(),
                extras={"Total Hours":"1", "Seconds Run":"%d" %total_time})
        total_time = time.time() - start_time
        self.log.debug("WiFi state = %d" %self.dut.droid.wifiCheckState())
        raise signals.TestPass(details="", extras={"Total Hours":"1",
            "Seconds Run":"%d" %total_time})

    @test_tracker_info(uuid="d367c83e-5b00-4028-9ed8-f7b875997d13")
    def test_stress_wifi_failover(self):
        """This test does aggressive failover to several networks in list.

           Steps:
               1. Add and enable few networks.
               2. Let device auto-connect.
               3. Remove the connected network.
               4. Repeat 2-3.
               5. Device should connect to a network until all networks are
                  exhausted.

        """
        for count in range(int(self.stress_count/4)):
            wutils.reset_wifi(self.dut)
            ssids = list()
            for network in self.networks:
                ssids.append(network[WifiEnums.SSID_KEY])
                ret = self.dut.droid.wifiAddNetwork(network)
                asserts.assert_true(ret != -1, "Add network %r failed" % network)
                self.dut.droid.wifiEnableNetwork(ret, 0)
            self.dut.droid.wifiStartScan()
            time.sleep(WAIT_FOR_AUTO_CONNECT)
            cur_network = self.dut.droid.wifiGetConnectionInfo()
            cur_ssid = cur_network[WifiEnums.SSID_KEY]
            self.log.info("Cur_ssid = %s" % cur_ssid)
            for i in range(0,len(self.networks)):
                self.log.debug("Forget network %s" % cur_ssid)
                wutils.wifi_forget_network(self.dut, cur_ssid)
                time.sleep(WAIT_FOR_AUTO_CONNECT)
                cur_network = self.dut.droid.wifiGetConnectionInfo()
                cur_ssid = cur_network[WifiEnums.SSID_KEY]
                self.log.info("Cur_ssid = %s" % cur_ssid)
                if i == len(self.networks) - 1:
                    break
                if cur_ssid not in ssids:
                    raise signals.TestFailure("Device did not failover to the "
                        "expected network. SSID = %s" % cur_ssid)
            network_config = self.dut.droid.wifiGetConfiguredNetworks()
            self.log.info("Network Config = %s" % network_config)
            if len(network_config):
                raise signals.TestFailure("All the network configurations were not "
                    "removed. Configured networks = %s" % network_config,
                        extras={"Iterations":"%d" % self.stress_count,
                            "Pass":"%d" %(count*4)})
        raise signals.TestPass(details="", extras={"Iterations":"%d" %
            self.stress_count, "Pass":"%d" %((count+1)*4)})

    @test_tracker_info(uuid="2c19e8d1-ac16-4d7e-b309-795144e6b956")
    def test_stress_softAP_startup_and_stop_5g(self):
        """Test to bring up softAP and down for N times.

        Steps:
            1. Bring up softAP on 5G.
            2. Check for softAP on teh client device.
            3. Turn ON WiFi.
            4. Verify softAP is turned down and WiFi is up.

        """
        ap_ssid = "softap_" + utils.rand_ascii_str(8)
        ap_password = utils.rand_ascii_str(8)
        self.dut.log.info("softap setup: %s %s", ap_ssid, ap_password)
        config = {wutils.WifiEnums.SSID_KEY: ap_ssid}
        config[wutils.WifiEnums.PWD_KEY] = ap_password
        # Set country code explicitly to "US".
        wutils.set_wifi_country_code(self.dut, wutils.WifiEnums.CountryCode.US)
        wutils.set_wifi_country_code(self.dut_client, wutils.WifiEnums.CountryCode.US)
        for count in range(self.stress_count):
            initial_wifi_state = self.dut.droid.wifiCheckState()
            wutils.start_wifi_tethering(self.dut,
                ap_ssid,
                ap_password,
                WifiEnums.WIFI_CONFIG_APBAND_5G)
            wutils.start_wifi_connection_scan_and_ensure_network_found(
                self.dut_client, ap_ssid)
            wutils.stop_wifi_tethering(self.dut)
            asserts.assert_false(self.dut.droid.wifiIsApEnabled(),
                                 "SoftAp failed to shutdown!")
            # Give some time for WiFi to come back to previous state.
            time.sleep(2)
            cur_wifi_state = self.dut.droid.wifiCheckState()
            if initial_wifi_state != cur_wifi_state:
               raise signals.TestFailure("Wifi state was %d before softAP and %d now!" %
                    (initial_wifi_state, cur_wifi_state),
                        extras={"Iterations":"%d" % self.stress_count,
                            "Pass":"%d" %count})
        raise signals.TestPass(details="", extras={"Iterations":"%d" %
            self.stress_count, "Pass":"%d" %(count+1)})

    @test_tracker_info(uuid="eb22e26b-95d1-4580-8c76-85dfe6a42a0f")
    def test_stress_wifi_roaming(self):
        AP1_network = self.reference_networks[0]["5g"]
        AP2_network = self.reference_networks[1]["5g"]
        wutils.set_attns(self.attenuators, "AP1_on_AP2_off")
        self.scan_and_connect_by_ssid(self.dut, AP1_network)
        # Reduce iteration to half because each iteration does two roams.
        for count in range(int(self.stress_count/2)):
            self.log.info("Roaming iteration %d, from %s to %s", count,
                           AP1_network, AP2_network)
            try:
                wutils.trigger_roaming_and_validate(self.dut, self.attenuators,
                    "AP1_off_AP2_on", AP2_network)
                self.log.info("Roaming iteration %d, from %s to %s", count,
                               AP2_network, AP1_network)
                wutils.trigger_roaming_and_validate(self.dut, self.attenuators,
                    "AP1_on_AP2_off", AP1_network)
            except:
                raise signals.TestFailure("Roaming failed. Look at logs",
                    extras={"Iterations":"%d" %self.stress_count, "Pass":"%d" %
                        (count*2)})
        raise signals.TestPass(details="", extras={"Iterations":"%d" %
            self.stress_count, "Pass":"%d" %((count+1)*2)})

    @test_tracker_info(uuid="e8ae8cd2-c315-4c08-9eb3-83db65b78a58")
    def test_stress_network_selector_2G_connection(self):
        """
            1. Add one saved 2G network to DUT.
            2. Move the DUT in range.
            3. Verify the DUT is connected to the network.
            4. Move the DUT out of range
            5. Repeat step 2-4
        """
        for attenuator in self.attenuators:
            attenuator.set_atten(95)
        # add a saved network to DUT
        networks = [self.reference_networks[0]['2g']]
        self.add_networks(self.dut, networks)
        for count in range(self.stress_count):
            self.connect_and_verify_connected_ssid(self.reference_networks[0]['2g'])
            # move the DUT out of range
            self.attenuators[0].set_atten(95)
            time.sleep(10)
        wutils.set_attns(self.attenuators, "default")
        raise signals.TestPass(details="", extras={"Iterations":"%d" %
            self.stress_count, "Pass":"%d" %(count+1)})

    @test_tracker_info(uuid="5d5d14cb-3cd1-4b3d-8c04-0d6f4b764b6b")
    def test_stress_pno_connection_to_2g(self):
        """Test PNO triggered autoconnect to a network for N times

        Steps:
        1. Save 2Ghz valid network configuration in the device.
        2. Screen off DUT
        3. Attenuate 5Ghz network and wait for a few seconds to trigger PNO.
        4. Check the device connected to 2Ghz network automatically.
        5. Repeat step 3-4
        """
        for attenuator in self.attenuators:
            attenuator.set_atten(95)
        # add a saved network to DUT
        networks = [self.reference_networks[0]['2g']]
        self.add_networks(self.dut, networks)
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        for count in range(self.stress_count):
            self.connect_and_verify_connected_ssid(self.reference_networks[0]['2g'], is_pno=True)
            wutils.wifi_forget_network(
                    self.dut, networks[0][WifiEnums.SSID_KEY])
            # move the DUT out of range
            self.attenuators[0].set_atten(95)
            time.sleep(10)
            self.add_networks(self.dut, networks)
        wutils.set_attns(self.attenuators, "default")
        raise signals.TestPass(details="", extras={"Iterations":"%d" %
            self.stress_count, "Pass":"%d" %(count+1)})
