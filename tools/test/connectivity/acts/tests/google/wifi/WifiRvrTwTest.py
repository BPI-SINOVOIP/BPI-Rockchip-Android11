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

import serial
import sys


WifiEnums = wutils.WifiEnums


class WifiRvrTWTest(WifiBaseTest):
    """ Tests for wifi RVR performance

        Test Bed Requirement:
          * One Android device
          * Wi-Fi networks visible to the device
    """
    TEST_TIMEOUT = 10

    def setup_class(self):
        super().setup_class()

        self.dut = self.android_devices[0]
        wutils.wifi_test_device_init(self.dut)

        req_params = [ "iot_networks","rvr_test_params"]
        opt_params = [ "angle_params","usb_port"]
        self.unpack_userparams(req_param_names=req_params,
                               opt_param_names=opt_params)

        asserts.assert_true(
            len(self.iot_networks) > 0,
            "Need at least one iot network with psk.")

        wutils.wifi_toggle_state(self.dut, True)
        if "rvr_test_params" in self.user_params:
            self.iperf_server = self.iperf_servers[0]
            self.MaxdB= self.rvr_test_params ["rvr_atten_MaxDB"]
            self.MindB= self.rvr_test_params ["rvr_atten_MinDB"]
            self.stepdB= self.rvr_test_params ["rvr_atten_step"]

        if "angle_params" in self.user_params:
            self.angle = self.angle_params

        if "usb_port" in self.user_params:
            self.T1=self.readport(self.usb_port["turntable"])
            self.ATT1=self.readport(self.usb_port["atten1"])
            self.ATT2=self.readport(self.usb_port["atten2"])
            self.ATT3=self.readport(self.usb_port["atten3"])

        # create hashmap for testcase name and SSIDs
        self.iot_test_prefix = "test_iot_connection_to_"
        self.ssid_map = {}
        for network in self.iot_networks:
            SSID = network['SSID'].replace('-','_')
            self.ssid_map[SSID] = network

        # create folder for rvr test result
        self.log_path = os.path.join(logging.log_path, "rvr_results")
        os.makedirs(self.log_path, exist_ok=True)

        Header=("test_SSID","Turn table (angle)","Attenuator(dBm)",
                "TX throughput (Mbps)","RX throughput (Mbps)",
                "RSSI","Link speed","Frequency")
        self.csv_write(Header)

    def setup_test(self):
        self.dut.droid.wakeLockAcquireBright()
        self.dut.droid.wakeUpNow()

    def teardown_test(self):
        self.dut.droid.wakeLockRelease()
        self.dut.droid.goToSleepNow()

    def teardown_class(self):
        if "rvr_test_params" in self.user_params:
            self.iperf_server.stop()

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)
        self.dut.cat_adb_log(test_name, begin_time)

    """Helper Functions"""

    def csv_write(self,data):
        """Output .CSV file for test result.

        Args:
            data: Dict containing attenuation, throughput and other meta data.
        """
        with open("{}/Result.csv".format(self.log_path), "a", newline="") as csv_file:
            csv_writer = csv.writer(csv_file,delimiter=',')
            csv_writer.writerow(data)
            csv_file.close()

    def readport(self,com):
        """Read com port for current test.

        Args:
            com: Attenuator or turn table com port
        """
        port=serial.Serial(com,9600,timeout=1)
        time.sleep(1)
        return port

    def getdB(self,port):
        """Get attenuator dB for current test.

        Args:
            port: Attenuator com port
        """
        port.write('V?;'.encode())
        dB=port.readline().decode()
        dB=dB.strip(';')
        dB=dB[dB.find('V')+1:]
        return int(dB)

    def setdB(self,port,dB):
        """Setup attenuator dB for current test.

        Args:
            port: Attenuator com port
            dB: Attenuator setup dB
        """
        if dB<0:
            dB=0
        elif dB>101:
            dB=101
        self.log.info("Set dB to "+str(dB))
        InputdB=str('V')+str(dB)+str(';')
        port.write(InputdB.encode())
        time.sleep(0.1)

    def set_Three_Att_dB(self,port1,port2,port3,dB):
        """Setup 3 attenuator dB for current test.

        Args:
            port1: Attenuator1 com port
            port1: Attenuator2 com port
            port1: Attenuator com port
            dB: Attenuator setup dB
        """
        self.setdB(port1,dB)
        self.setdB(port2,dB)
        self.setdB(port3,dB)
        self.checkdB(port1,dB)
        self.checkdB(port2,dB)
        self.checkdB(port3,dB)

    def checkdB(self,port,dB):
        """Check attenuator dB for current test.

        Args:
            port: Attenuator com port
            dB: Attenuator setup dB
        """
        retry=0
        while self.getdB(port)!=dB and retry<10:
            retry=retry+1
            self.log.info("Current dB = "+str(self.getdB(port)))
            self.log.info("Fail to set Attenuator to "+str(dB)+", "
                          +str(retry)+" times try to reset")
            self.setdB(port,dB)
        if retry ==10:
            self.log.info("Retry Attenuator fail for 9 cycles, end test!")
            sys.exit()
        return 0

    def getDG(self,port):
        """Get turn table angle for current test.

        Args:
            port: Turn table com port
        """
        DG = ""
        port.write('DG?;'.encode())
        time.sleep(0.1)
        data = port.readline().decode('utf-8')
        for i in range(len(data)):
            if (data[i].isdigit()) == True:
                DG = DG + data[i]
        if DG == "":
            return -1
        return int(DG)

    def setDG(self,port,DG):
        """Setup turn table angle for current test.

        Args:
            port: Turn table com port
            DG: Turn table setup angle
        """
        if DG>359:
            DG=359
        elif DG<0:
            DG=0
        self.log.info("Set angle to "+str(DG))
        InputDG=str('DG')+str(DG)+str(';')
        port.write(InputDG.encode())

    def checkDG(self,port,DG):
        """Check turn table angle for current test.

        Args:
            port: Turn table com port
            DG: Turn table setup angle
        """
        retrytime = self.TEST_TIMEOUT
        retry = 0
        while self.getDG(port)!=DG and retry<retrytime:
            retry=retry+1
            self.log.info('Current angle = '+str(self.getDG(port)))
            self.log.info('Fail set angle to '+str(DG)+', '+str(retry)+' times try to reset')
            self.setDG(port,DG)
            time.sleep(10)
        if retry == retrytime:
            self.log.info('Retry turntable fail for '+str(retry)+' cycles, end test!')
            sys.exit()
        return 0

    def getwifiinfo(self):
        """Get WiFi RSSI/ link speed/ frequency for current test.

        Returns:
            [RSSI,LS,FR]: WiFi RSSI/ link speed/ frequency
        """
        def is_number(string):
            for i in string:
                if i.isdigit() == False:
                    if (i=="-" or i=="."):
                        continue
                    return str(-1)
            return string

        try:
            cmd = "adb shell iw wlan0 link"
            wifiinfo = utils.subprocess.check_output(cmd,shell=True,
                                                     timeout=self.TEST_TIMEOUT)
            # Check RSSI
            RSSI = wifiinfo.decode("utf-8")[wifiinfo.decode("utf-8").find("signal:") +
                                            7:wifiinfo.decode("utf-8").find("dBm") - 1]
            RSSI = RSSI.strip(' ')
            RSSI = is_number(RSSI)
            # Check link speed
            LS = wifiinfo.decode("utf-8")[wifiinfo.decode("utf-8").find("bitrate:") +
                                          8:wifiinfo.decode("utf-8").find("Bit/s") - 2]
            LS = LS.strip(' ')
            LS = is_number(LS)
            # Check frequency
            FR = wifiinfo.decode("utf-8")[wifiinfo.decode("utf-8").find("freq:") +
                                          6:wifiinfo.decode("utf-8").find("freq:") + 10]
            FR = FR.strip(' ')
            FR = is_number(FR)
        except:
            return -1, -1, -1
        return [RSSI,LS,FR]

    def post_process_results(self, rvr_result):
        """Saves JSON formatted results.

        Args:
            rvr_result: Dict containing attenuation, throughput and other meta
            data
        """
        # Save output as text file
        data=(rvr_result["test_name"],rvr_result["test_angle"],rvr_result["test_dB"],
              rvr_result["throughput_TX"][0],rvr_result["throughput_RX"][0],
              rvr_result["test_RSSI"],rvr_result["test_LS"],rvr_result["test_FR"])
        self.csv_write(data)

        results_file_path = "{}/{}_angle{}_{}dB.json".format(self.log_path,
                                                        self.ssid,
                                                        self.angle[self.ag],self.DB)
        with open(results_file_path, 'w') as results_file:
            json.dump(rvr_result, results_file, indent=4)

    def connect_to_wifi_network(self, network):
        """Connection logic for psk wifi networks.

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

        Returns:
            rvr_result: Dict containing rvr_results
        """
        rvr_result = []
        self.iperf_server.start(tag="TX_server_{}_angle{}_{}dB".format(
            self.ssid,self.angle[self.ag],self.DB))
        wait_time = 5
        SSID = network[WifiEnums.SSID_KEY]
        self.log.info("Starting iperf traffic TX through {}".format(SSID))
        time.sleep(wait_time)
        port_arg = "-p {} -J {}".format(self.iperf_server.port,
                                        self.rvr_test_params["iperf_port_arg"])
        success, data = self.dut.run_iperf_client(
            self.rvr_test_params["iperf_server_address"],
            port_arg,
            timeout=self.rvr_test_params["iperf_duration"] + self.TEST_TIMEOUT)
        # Parse and log result
        client_output_path = os.path.join(
            self.iperf_server.log_path, "IperfDUT,{},TX_client_{}_angle{}_{}dB".format(
                self.iperf_server.port,self.ssid,self.angle[self.ag],self.DB))
        with open(client_output_path, 'w') as out_file:
            out_file.write("\n".join(data))
        self.iperf_server.stop()

        iperf_file = self.iperf_server.log_files[-1]
        try:
            iperf_result = ipf.IPerfResult(iperf_file)
            curr_throughput = (math.fsum(iperf_result.instantaneous_rates[
                self.rvr_test_params["iperf_ignored_interval"]:-1]) / len(
                    iperf_result.instantaneous_rates[self.rvr_test_params[
                        "iperf_ignored_interval"]:-1])) * 8 * (1.024**2)
        except:
            self.log.warning(
                "ValueError: Cannot get iperf result. Setting to 0")
            curr_throughput = 0
        rvr_result.append(curr_throughput)
        self.log.info("TX Throughput at {0:.2f} dB is {1:.2f} Mbps".format(
            self.DB, curr_throughput))

        self.log.debug(pprint.pformat(data))
        asserts.assert_true(success, "Error occurred in iPerf traffic.")
        return rvr_result

    def run_iperf_server(self, network):
        """Run iperf RX throughput after connection.

        Args:
            params: Dictionary with network info.

        Returns:
            rvr_result: Dict containing rvr_results
        """
        rvr_result = []
        self.iperf_server.start(tag="RX_client_{}_angle{}_{}dB".format(
            self.ssid,self.angle[self.ag],self.DB))
        wait_time = 5
        SSID = network[WifiEnums.SSID_KEY]
        self.log.info("Starting iperf traffic RX through {}".format(SSID))
        time.sleep(wait_time)
        port_arg = "-p {} -J -R {}".format(self.iperf_server.port,
                                           self.rvr_test_params["iperf_port_arg"])
        success, data = self.dut.run_iperf_client(
            self.rvr_test_params["iperf_server_address"],
            port_arg,
            timeout=self.rvr_test_params["iperf_duration"] + self.TEST_TIMEOUT)
        # Parse and log result
        client_output_path = os.path.join(
        self.iperf_server.log_path, "IperfDUT,{},RX_server_{}_angle{}_{}dB".format(
            self.iperf_server.port,self.ssid,self.angle[self.ag],self.DB))
        with open(client_output_path, 'w') as out_file:
            out_file.write("\n".join(data))
        self.iperf_server.stop()

        iperf_file = client_output_path
        try:
            iperf_result = ipf.IPerfResult(iperf_file)
            curr_throughput = (math.fsum(iperf_result.instantaneous_rates[
                self.rvr_test_params["iperf_ignored_interval"]:-1]) / len(
                    iperf_result.instantaneous_rates[self.rvr_test_params[
                        "iperf_ignored_interval"]:-1])) * 8 * (1.024**2)
        except:
            self.log.warning(
                "ValueError: Cannot get iperf result. Setting to 0")
            curr_throughput = 0
        rvr_result.append(curr_throughput)
        self.log.info("RX Throughput at {0:.2f} dB is {1:.2f} Mbps".format(
            self.DB, curr_throughput))

        self.log.debug(pprint.pformat(data))
        asserts.assert_true(success, "Error occurred in iPerf traffic.")
        return rvr_result

    def iperf_test_func(self,network):
        """Main function to test iperf TX/RX.

        Args:
            params: Dictionary with network info
        """
        if "rvr_test_params" in self.user_params:
            # Initialize
            rvr_result = {}
            # Run RvR and log result
            wifiinfo = self.getwifiinfo()
            rvr_result["throughput_TX"] = self.run_iperf_client(network)
            rvr_result["throughput_RX"] = self.run_iperf_server(network)
            rvr_result["test_name"] = self.ssid
            rvr_result["test_angle"] = self.angle[self.ag]
            rvr_result["test_dB"] = self.DB
            rvr_result["test_RSSI"] = wifiinfo[0]
            rvr_result["test_LS"] = wifiinfo[1]
            rvr_result["test_FR"] = wifiinfo[2]
            self.post_process_results(rvr_result)

    def rvr_test(self,network):
        """Test function to run RvR.

        The function runs an RvR test in the current device/AP configuration.
        Function is called from another wrapper function that sets up the
        testbed for the RvR test

        Args:
            params: Dictionary with network info
        """
        wait_time = 5
        utils.subprocess.check_output('adb root', shell=True, timeout=20)
        self.ssid = network[WifiEnums.SSID_KEY]
        self.log.info("Start rvr test")
        for i in range(len(self.angle)):
          self.setDG(self.T1,self.angle[i])
          time.sleep(wait_time)
          self.checkDG(self.T1,self.angle[i])
          self.set_Three_Att_dB(self.ATT1,self.ATT2,self.ATT3,0)
          time.sleep(wait_time)
          self.connect_to_wifi_network(network)
          self.set_Three_Att_dB(self.ATT1,self.ATT2,self.ATT3,self.MindB)
          for j in range(self.MindB,self.MaxdB+self.stepdB,self.stepdB):
            self.DB=j
            self.ag=i
            self.set_Three_Att_dB(self.ATT1,self.ATT2,self.ATT3,self.DB)
            self.iperf_test_func(network)
          wutils.reset_wifi(self.dut)

    """Tests"""

    @test_tracker_info(uuid="93816af8-4c63-45f8-b296-cb49fae0b158")
    def test_iot_connection_to_RVR_2G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.rvr_test(self.ssid_map[ssid_key])

    @test_tracker_info(uuid="e1a67e13-946f-4d91-aa73-3f945438a1ac")
    def test_iot_connection_to_RVR_5G(self):
        ssid_key = self.current_test_name.replace(self.iot_test_prefix, "")
        self.rvr_test(self.ssid_map[ssid_key])