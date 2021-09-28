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

import re
import sys
import random
import time

import acts.controllers.packet_capture as packet_capture
import acts.signals as signals
import acts.test_utils.wifi.rpm_controller_utils as rutils
import acts.test_utils.wifi.wifi_datastore_utils as dutils
import acts.test_utils.wifi.wifi_test_utils as wutils

from acts import asserts
from acts.base_test import BaseTestClass
from acts.controllers.ap_lib import hostapd_constants
from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest

WifiEnums = wutils.WifiEnums

WAIT_BEFORE_CONNECTION = 1
SINGLE_BAND = 1
DUAL_BAND = 2

TIMEOUT = 60
TEST = 'test_'
PING_ADDR = 'www.google.com'

NUM_LINK_PROBES = 3
PROBE_DELAY_SEC = 1


class WifiChaosTest(WifiBaseTest):
    """ Tests for wifi IOT

        Test Bed Requirement:
          * One Android device
          * Wi-Fi IOT networks visible to the device
    """

    def __init__(self, configs):
        BaseTestClass.__init__(self, configs)
        self.generate_interop_tests()

    def randomize_testcases(self):
        """Randomize the list of hosts and build a random order of tests,
           based on SSIDs, keeping all the relevant bands of an AP together.

        """
        temp_tests = list()
        hosts = self.user_params['interop_host']

        random.shuffle(hosts)

        for host in hosts:
            ssid_2g = None
            ssid_5g = None
            info = dutils.show_device(host)

            # Based on the information in datastore, add to test list if
            # AP has 2G band.
            if 'ssid_2g' in info:
                ssid_2g = info['ssid_2g']
                temp_tests.append(TEST + ssid_2g)

            # Based on the information in datastore, add to test list if
            # AP has 5G band.
            if 'ssid_5g' in info:
                ssid_5g = info['ssid_5g']
                temp_tests.append(TEST + ssid_5g)

        self.tests = temp_tests

    def generate_interop_testcase(self, base_test, testcase_name, ssid_dict):
        """Generates a single test case from the given data.

        Args:
            base_test: The base test case function to run.
            testcase_name: The name of the test case.
            ssid_dict: The information about the network under test.
        """
        ssid = testcase_name
        test_tracker_uuid = ssid_dict[testcase_name]['uuid']
        hostname = ssid_dict[testcase_name]['host']
        if not testcase_name.startswith('test_'):
            testcase_name = 'test_%s' % testcase_name
        test_case = test_tracker_info(uuid=test_tracker_uuid)(
            lambda: base_test(ssid, hostname))
        setattr(self, testcase_name, test_case)
        self.tests.append(testcase_name)

    def generate_interop_tests(self):
        for ssid_dict in self.user_params['interop_ssid']:
            testcase_name = list(ssid_dict)[0]
            self.generate_interop_testcase(self.interop_base_test,
                                           testcase_name, ssid_dict)
        self.randomize_testcases()

    def setup_class(self):
        super().setup_class()
        self.dut = self.android_devices[0]
        self.admin = 'admin' + str(random.randint(1000001, 12345678))
        wutils.wifi_test_device_init(self.dut)
        # Set country code explicitly to "US".
        wutils.set_wifi_country_code(self.dut, wutils.WifiEnums.CountryCode.US)

        asserts.assert_true(
            self.lock_pcap(),
            "Could not lock a Packet Capture. Aborting Interop test.")

        wutils.wifi_toggle_state(self.dut, True)

    def lock_pcap(self):
        """Lock a Packet Capturere to use for the test."""

        # Get list of devices from the datastore.
        locked_pcap = False
        devices = dutils.get_devices()

        for device in devices:

            device_name = device['hostname']
            device_type = device['ap_label']
            if device_type == 'PCAP' and not device['lock_status']:
                if dutils.lock_device(device_name, self.admin):
                    self.pcap_host = device_name
                    host = device['ip_address']
                    self.log.info("Locked Packet Capture device: %s" % device_name)
                    locked_pcap = True
                    break
                else:
                    self.log.warning("Failed to lock %s PCAP." % device_name)

        if not locked_pcap:
            return False

        pcap_config = {'ssh_config':{'user':'root'} }
        pcap_config['ssh_config']['host'] = host

        self.pcap = packet_capture.PacketCapture(pcap_config)
        return True

    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()

    def on_pass(self, test_name, begin_time):
        wutils.stop_pcap(self.pcap, self.pcap_procs, True)

    def on_fail(self, test_name, begin_time):
        # Sleep to ensure all failed packets are captured.
        time.sleep(5)
        wutils.stop_pcap(self.pcap, self.pcap_procs, False)
        super().on_fail(test_name, begin_time)

    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()

    def teardown_class(self):
        # Unlock the PCAP.
        if not dutils.unlock_device(self.pcap_host):
            self.log.warning("Failed to unlock %s PCAP. Check in datastore.")


    """Helper Functions"""

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
        self.log.info("Finding Gateway...")
        route_response = self.dut.adb.shell("ip route get 8.8.8.8")
        gateway_ip = re.search('via (.*) dev', str(route_response)).group(1)
        self.log.info("Gateway IP = %s" % gateway_ip)
        self.log.info("Running ping for %d seconds" % sec)
        result = self.dut.adb.shell("ping -w %d %s" % (sec, gateway_ip),
                                    timeout=sec + 1)
        self.log.debug("Ping Result = %s" % result)
        if "100% packet loss" in result:
            raise signals.TestFailure("100% packet loss during ping")

    def send_link_probes(self, network):
        """
        Send link probes, and verify that the device and AP did not crash.
        Also verify that at least one link probe succeeded.

        Steps:
        1. Send a few link probes.
        2. Ensure that the device and AP did not crash (by checking that the
           device remains connected to the expected network).
        """
        results = wutils.send_link_probes(
            self.dut, NUM_LINK_PROBES, PROBE_DELAY_SEC)

        self.log.info("Link Probe results: %s" % (results,))

        wifi_info = self.dut.droid.wifiGetConnectionInfo()
        expected = network[WifiEnums.SSID_KEY]
        actual = wifi_info[WifiEnums.SSID_KEY]
        asserts.assert_equal(
            expected, actual,
            "Device did not remain connected after sending link probes!")

    def unlock_and_turn_off_ap(self, hostname, rpm_port, rpm_ip):
        """UNlock the AP in datastore and turn off the AP.

        Args:
            hostname: Hostname of the AP.
            rpm_port: Port number on the RPM for the AP.
            rpm_ip: RPM's IP address.

        """
        # Un-Lock AP in datastore.
        self.log.debug("Un-lock AP in datastore")
        if not dutils.unlock_device(hostname):
            self.log.warning("Failed to unlock %s AP. Check AP in datastore.")
        # Turn OFF AP from the RPM port.
        rutils.turn_off_ap(rpm_port, rpm_ip)

    def run_connect_disconnect(self, network, hostname, rpm_port, rpm_ip,
                               release_ap):
        """Run connect/disconnect to a given network in loop.

           Args:
               network: Dict, network information.
               hostname: Hostanme of the AP to connect to.
               rpm_port: Port number on the RPM for the AP.
               rpm_ip: Port number on the RPM for the AP.
               release_ap: Flag to determine if we should turn off the AP yet.

           Raises: TestFailure if the network connection fails.

        """
        for attempt in range(5):
            try:
                begin_time = time.time()
                ssid = network[WifiEnums.SSID_KEY]
                net_id = self.dut.droid.wifiAddNetwork(network)
                asserts.assert_true(net_id != -1, "Add network %s failed" % network)
                self.log.info("Connecting to %s" % ssid)
                self.scan_and_connect_by_id(network, net_id)
                self.run_ping(10)
                # TODO(b/133369482): uncomment once bug is resolved
                # self.send_link_probes(network)
                wutils.wifi_forget_network(self.dut, ssid)
                time.sleep(WAIT_BEFORE_CONNECTION)
            except Exception as e:
                self.log.error("Connection to %s network failed on the %d "
                               "attempt with exception %s." % (ssid, attempt, e))
                # TODO:(bmahadev) Uncomment after scan issue is fixed.
                # self.dut.take_bug_report(ssid, begin_time)
                # self.dut.cat_adb_log(ssid, begin_time)
                if release_ap:
                    self.unlock_and_turn_off_ap(hostname, rpm_port, rpm_ip)
                raise signals.TestFailure("Failed to connect to %s" % ssid)

    def get_band_and_chan(self, ssid):
        """Get the band and channel information from SSID.

        Args:
            ssid: SSID of the network.

        """
        ssid_info = ssid.split('_')
        self.band = ssid_info[-1]
        for item in ssid_info:
            # Skip over the router model part.
            if 'ch' in item and item != ssid_info[0]:
                self.chan = re.search(r'(\d+)',item).group(0)
                return
        raise signals.TestFailure("Channel information not found in SSID.")

    def interop_base_test(self, ssid, hostname):
        """Base test for all the connect-disconnect interop tests.

        Args:
            ssid: string, SSID of the network to connect to.
            hostname: string, hostname of the AP.

        Steps:
            1. Lock AP in datstore.
            2. Turn on AP on the rpm switch.
            3. Run connect-disconnect in loop.
            4. Turn off AP on the rpm switch.
            5. Unlock AP in datastore.

        """
        network = {}
        network['password'] = 'password'
        network['SSID'] = ssid
        release_ap = False
        wutils.reset_wifi(self.dut)

        # Lock AP in datastore.
        self.log.info("Lock AP in datastore")

        ap_info = dutils.show_device(hostname)

        # If AP is locked by a different test admin, then we skip.
        if ap_info['lock_status'] and ap_info['locked_by'] != self.admin:
            raise signals.TestSkip("AP %s is locked, skipping test" % hostname)

        if not dutils.lock_device(hostname, self.admin):
            self.log.warning("Failed to lock %s AP. Unlock AP in datastore"
                             " and try again.")
            raise signals.TestFailure("Failed to lock AP")

        band = SINGLE_BAND
        if ('ssid_2g' in ap_info) and ('ssid_5g' in ap_info):
            band = DUAL_BAND
        if (band == SINGLE_BAND) or (
                band == DUAL_BAND and '5G' in ssid):
            release_ap = True

        # Get AP RPM attributes and Turn ON AP.
        rpm_ip = ap_info['rpm_ip']
        rpm_port = ap_info['rpm_port']

        rutils.turn_on_ap(self.pcap, ssid, rpm_port, rpm_ip=rpm_ip)
        self.log.info("Finished turning ON AP.")
        # Experimental. Some APs take upto a min to come online.
        time.sleep(60)

        self.get_band_and_chan(ssid)
        self.pcap.configure_monitor_mode(self.band, self.chan)
        self.pcap_procs = wutils.start_pcap(
                self.pcap, self.band.lower(), self.test_name)
        self.run_connect_disconnect(network, hostname, rpm_port, rpm_ip,
                                    release_ap)

        # Un-lock only if it's a single band AP or we are running the last band.
        if release_ap:
            self.unlock_and_turn_off_ap(hostname, rpm_port, rpm_ip)
