#!/usr/bin/env python3
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

import itertools
import pprint
import time

import acts.signals
import acts.test_utils.wifi.wifi_test_utils as wutils

from acts import asserts
from acts.test_decorators import test_tracker_info
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.controllers import iperf_server as ipf

import json
import logging
import math
import os
from acts import utils
import csv

WifiEnums = wutils.WifiEnums


class WifiIOTTwPkg1Test(WifiBaseTest):
    """ Tests for wifi IOT

        Test Bed Requirement:
          * One Android device
          * Wi-Fi IOT networks visible to the device
    """

    def setup_class(self):
        super().setup_class()

        self.dut = self.android_devices[0]
        wutils.wifi_test_device_init(self.dut)

        req_params = [ "iot_networks", ]
        opt_params = [ "open_network",
                       "iperf_server_address","iperf_port_arg",
                       "pdu_address" , "pduon_wait_time","pduon_address"
        ]
        self.unpack_userparams(req_param_names=req_params,
                               opt_param_names=opt_params)

        asserts.assert_true(
            len(self.iot_networks) > 0,
            "Need at least one iot network with psk.")

        if getattr(self, 'open_network', False):
            self.iot_networks.append(self.open_network)

        wutils.wifi_toggle_state(self.dut, True)
        if "iperf_server_address" in self.user_params:
            self.iperf_server = self.iperf_servers[0]

        # create hashmap for testcase name and SSIDs
        self.iot_test_prefix = "test_iot_connection_to_"
        self.ssid_map = {}
        for network in self.iot_networks:
            SSID = network['SSID'].replace('-','_')
            self.ssid_map[SSID] = network

        # create folder for IOT test result
        self.log_path = os.path.join(logging.log_path, "IOT_results")
        os.makedirs(self.log_path, exist_ok=True)

        Header=("test_name","throughput_TX","throughput_RX")
        self.csv_write(Header)

        # check pdu_address
        if "pdu_address" and "pduon_wait_time" in self.user_params:
            self.pdu_func()

    def setup_test(self):
        super().setup_test()
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()

    def teardown_test(self):
        super().teardown_test()
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()
        wutils.reset_wifi(self.dut)

    def teardown_class(self):
        if "iperf_server_address" in self.user_params:
            self.iperf_server.stop()

    """Helper Functions"""

    def connect_to_wifi_network(self, network):
        """Connection logic for open and psk wifi networks.

        Args:
            params: Dictionary with network info.
        """
        SSID = network[WifiEnums.SSID_KEY]
        self.dut.ed.clear_all_events()
        wutils.start_wifi_connection_scan(self.dut)
        scan_results = self.dut.droid.wifiGetScanResults()
        wutils.assert_network_in_list({WifiEnums.SSID_KEY: SSID}, scan_results)
        wutils.wifi_connect(self.dut, network, num_of_tries=3)

    def run_iperf_client(self, network):
        """Run iperf TX throughput after connection.

        Args:
            params: Dictionary with network info.
        """
        if "iperf_server_address" in self.user_params:

            # Add iot_result
            iot_result = []
            self.iperf_server.start(tag="TX_server_{}".format(
                self.current_test_name))
            wait_time = 5
            SSID = network[WifiEnums.SSID_KEY]
            self.log.info("Starting iperf traffic TX through {}".format(SSID))
            time.sleep(wait_time)
            port_arg = "-p {} -J {}".format(self.iperf_server.port,self.iperf_port_arg)
            success, data = self.dut.run_iperf_client(self.iperf_server_address,
                                                      port_arg)
            # Parse and log result
            client_output_path = os.path.join(
                self.iperf_server.log_path, "IperfDUT,{},TX_client_{}".format(
                    self.iperf_server.port,self.current_test_name))
            with open(client_output_path, 'w') as out_file:
                out_file.write("\n".join(data))
            self.iperf_server.stop()

            iperf_file = self.iperf_server.log_files[-1]
            try:
                iperf_result = ipf.IPerfResult(iperf_file)
                curr_throughput = math.fsum(iperf_result.instantaneous_rates)
            except:
                self.log.warning(
                    "ValueError: Cannot get iperf result. Setting to 0")
                curr_throughput = 0
            iot_result.append(curr_throughput)
            self.log.info("Throughput is {0:.2f} Mbps".format(curr_throughput))

            self.log.debug(pprint.pformat(data))
            asserts.assert_true(success, "Error occurred in iPerf traffic.")
            return iot_result

    def run_iperf_server(self, network):
        """Run iperf RX throughput after connection.

        Args:
            params: Dictionary with network info.

        Returns:
            iot_result: dict containing iot_results
        """
        if "iperf_server_address" in self.user_params:

            # Add iot_result
            iot_result = []
            self.iperf_server.start(tag="RX_client_{}".format(
                self.current_test_name))
            wait_time = 5
            SSID = network[WifiEnums.SSID_KEY]
            self.log.info("Starting iperf traffic RX through {}".format(SSID))
            time.sleep(wait_time)
            port_arg = "-p {} -J -R {}".format(self.iperf_server.port,self.iperf_port_arg)
            success, data = self.dut.run_iperf_client(self.iperf_server_address,
                                                      port_arg)
            client_output_path = os.path.join(
                self.iperf_server.log_path, "IperfDUT,{},RX_server_{}".format(
                    self.iperf_server.port,self.current_test_name))
            with open(client_output_path, 'w') as out_file:
                out_file.write("\n".join(data))
            self.iperf_server.stop()

            iperf_file = client_output_path
            try:
                iperf_result = ipf.IPerfResult(iperf_file)
                curr_throughput = math.fsum(iperf_result.instantaneous_rates)
            except:
                self.log.warning(
                    "ValueError: Cannot get iperf result. Setting to 0")
                curr_throughput = 0
            iot_result.append(curr_throughput)
            self.log.info("Throughput is {0:.2f} Mbps".format(curr_throughput))

            self.log.debug(pprint.pformat(data))
            asserts.assert_true(success, "Error occurred in iPerf traffic.")
            return iot_result

    def iperf_test_func(self,network):
        """Main function to test iperf TX/RX.

        Args:
            params: Dictionary with network info
        """
        # Initialize
        iot_result = {}

        # Run RvR and log result
        iot_result["throughput_TX"] = self.run_iperf_client(network)
        iot_result["throughput_RX"] = self.run_iperf_server(network)
        iot_result["test_name"] = self.current_test_name

        # Save output as text file
        results_file_path = "{}/{}.json".format(self.log_path,
                                                self.current_test_name)
        with open(results_file_path, 'w') as results_file:
            json.dump(iot_result, results_file, indent=4)

        data=(iot_result["test_name"],iot_result["throughput_TX"][0],
              iot_result["throughput_RX"][0])
        self.csv_write(data)

    def csv_write(self,data):
        with open("{}/Result.csv".format(self.log_path), "a", newline="") as csv_file:
            csv_writer = csv.writer(csv_file,delimiter=',')
            csv_writer.writerow(data)
            csv_file.close()

    def pdu_func(self):
        """control Power Distribution Units on local machine.

        Logic steps are
        1. Turn off PDU for all port.
        2. Turn on PDU for specified port.
        """
        out_file_name = "PDU.log"
        self.full_out_path = os.path.join(self.log_path, out_file_name)
        cmd = "curl http://snmp:1234@{}/offs.cgi?led=11111111> {}".format(self.pdu_address,
                                                                          self.full_out_path)
        self.pdu_process = utils.start_standing_subprocess(cmd)
        wait_time = 10
        self.log.info("Starting set PDU to OFF")
        time.sleep(wait_time)
        self.full_out_path = os.path.join(self.log_path, out_file_name)
        cmd = "curl http://snmp:1234@{}/ons.cgi?led={}> {}".format(self.pdu_address,
                                                                   self.pduon_address,
                                                                   self.full_out_path)
        self.pdu_process = utils.start_standing_subprocess(cmd)
        wait_time = int("{}".format(self.pduon_wait_time))
        self.log.info("Starting set PDU to ON for port1,"
                      "wait for {}s".format(self.pduon_wait_time))
        time.sleep(wait_time)
        self.log.info("PDU setup complete")

    def connect_to_wifi_network_and_run_iperf(self, network):
        """Connection logic for open and psk wifi networks.

        Logic steps are
        1. Connect to the network.
        2. Run iperf throghput.

        Args:
            params: A dictionary with network info.
        """
        self.connect_to_wifi_network(network)
        self.iperf_test_func(network)

    """Tests"""

    @test_tracker_info(uuid="0e4ad6ed-595c-4629-a4c9-c6be9c3c58e0")
    def test_iot_connection_to_ASUS_RT_AC68U_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="a76d8acc-808e-4a5d-a52b-5ba07d07b810")
    def test_iot_connection_to_ASUS_RT_AC68U_5G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="659a3e5e-07eb-4905-9cda-92e959c7b674")
    def test_iot_connection_to_D_Link_DIR_868L_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="6bcfd736-30fc-48a8-b4fb-723d1d113f3c")
    def test_iot_connection_to_D_Link_DIR_868L_5G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="c9da945a-2c4a-44e1-881d-adf307b39b21")
    def test_iot_connection_to_TP_LINK_WR940N_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="db0d224d-df81-401f-bf35-08ad02e41a71")
    def test_iot_connection_to_ASUS_RT_N66U_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="845ff1d6-618d-40f3-81c3-6ed3a0751fde")
    def test_iot_connection_to_ASUS_RT_N66U_5G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="6908039b-ccc9-4777-a0f1-3494ce642014")
    def test_iot_connection_to_ASUS_RT_AC54U_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="2647c15f-2aad-47d7-8dee-b2ee1ac4cef6")
    def test_iot_connection_to_ASUS_RT_AC54U_5G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="99678f66-ddf1-454d-87e4-e55177ec380d")
    def test_iot_connection_to_ASUS_RT_N56U_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="4dd75e81-9a8e-44fd-9449-09f5ab8a63c3")
    def test_iot_connection_to_ASUS_RT_N56U_5G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="315397ce-50d5-4abf-a11c-1abcaef832d3")
    def test_iot_connection_to_BELKIN_F9K1002v1_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="05ba464a-b1ef-4ac1-a32f-c919ec4aa1dd")
    def test_iot_connection_to_CISCO_E1200_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="04912868-4a47-40ce-877e-4e4c89849557")
    def test_iot_connection_to_TP_LINK_C2_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="53517a21-3802-4185-b8bb-6eaace063a42")
    def test_iot_connection_to_TP_LINK_C2_5G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="71c08c1c-415d-4da4-a151-feef43fb6ad8")
    def test_iot_connection_to_ASUS_RT_AC66U_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="2322c155-07d1-47c9-bd21-2e358e3df6ee")
    def test_iot_connection_to_ASUS_RT_AC66U_5G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.connect_to_wifi_network_and_run_iperf(self.ssid_map[ssid_key])
