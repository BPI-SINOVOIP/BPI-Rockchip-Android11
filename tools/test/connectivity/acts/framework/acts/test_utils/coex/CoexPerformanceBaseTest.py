#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

import json
import os
import time
from collections import defaultdict

from acts.metrics.loggers.blackbox import BlackboxMetricLogger
from acts.test_utils.bt.bt_test_utils import disable_bluetooth
from acts.test_utils.coex.audio_test_utils import AudioCaptureResult
from acts.test_utils.coex.audio_test_utils import get_audio_capture_device
from acts.test_utils.coex.CoexBaseTest import CoexBaseTest
from acts.test_utils.coex.coex_test_utils import bokeh_chart_plot
from acts.test_utils.coex.coex_test_utils import collect_bluetooth_manager_dumpsys_logs
from acts.test_utils.coex.coex_test_utils import multithread_func
from acts.test_utils.coex.coex_test_utils import wifi_connection_check
from acts.test_utils.wifi.wifi_test_utils import wifi_connect
from acts.test_utils.wifi.wifi_test_utils import wifi_test_device_init
from acts.utils import get_current_epoch_time

RSSI_POLL_RESULTS = 'Monitoring , Handle: 0x0003, POLL'
RSSI_RESULTS = 'Monitoring , Handle: 0x0003, '


def get_atten_range(start, stop, step):
    """Function to derive attenuation range for tests.

    Args:
        start: Start attenuation value.
        stop: Stop attenuation value.
        step: Step attenuation value.
    """
    temp = start
    while temp < stop:
        yield temp
        temp += step


class CoexPerformanceBaseTest(CoexBaseTest):
    """Base test class for performance tests.

    Attributes:
        rvr : Dict to save attenuation, throughput, fixed_attenuation.
        a2dp_streaming : Used to denote a2dp test cases.
    """

    def __init__(self, controllers):
        super().__init__(controllers)
        self.a2dp_streaming = False
        self.rvr = {}
        self.bt_range_metric = BlackboxMetricLogger.for_test_case(
            metric_name='bt_range')
        self.wifi_max_atten_metric = BlackboxMetricLogger.for_test_case(
            metric_name='wifi_max_atten')
        self.wifi_min_atten_metric = BlackboxMetricLogger.for_test_case(
            metric_name='wifi_min_atten')
        self.wifi_range_metric = BlackboxMetricLogger.for_test_case(
            metric_name='wifi_range_metric')

    def setup_class(self):
        req_params = ['test_params', 'Attenuator']
        opt_params = ['audio_params']
        self.unpack_userparams(req_params, opt_params)
        if hasattr(self, 'Attenuator'):
            self.num_atten = self.attenuators[0].instrument.num_atten
        else:
            self.log.error('Attenuator should be connected to run tests.')
            return False
        for i in range(self.num_atten):
            self.attenuators[i].set_atten(0)
        super().setup_class()
        self.performance_files_list = []
        if "performance_result_path" in self.user_params["test_params"]:
            self.performance_files_list = [
                os.path.join(self.test_params["performance_result_path"],
                             files) for files in os.listdir(
                                 self.test_params["performance_result_path"])
            ]
        self.bt_atten_range = list(get_atten_range(
                            self.test_params["bt_atten_start"],
                            self.test_params["bt_atten_stop"],
                            self.test_params["bt_atten_step"]))
        self.wifi_atten_range = list(get_atten_range(
                            self.test_params["attenuation_start"],
                            self.test_params["attenuation_stop"],
                            self.test_params["attenuation_step"]))

    def setup_test(self):
        if ('a2dp_streaming' in self.current_test_name and
                hasattr(self, 'audio_params')):
            self.audio = get_audio_capture_device(self.sec_ad, self.audio_params)
            self.a2dp_streaming = True
        for i in range(self.num_atten):
            self.attenuators[i].set_atten(0)
        if not wifi_connection_check(self.pri_ad, self.network["SSID"]):
            wifi_connect(self.pri_ad, self.network, num_of_tries=5)
        super().setup_test()

    def teardown_test(self):
        self.performance_baseline_check()
        for i in range(self.num_atten):
            self.attenuators[i].set_atten(0)
            current_atten = int(self.attenuators[i].get_atten())
            self.log.debug(
                "Setting attenuation to zero : Current atten {} : {}".format(
                    self.attenuators[i], current_atten))
        self.a2dp_streaming = False
        if not disable_bluetooth(self.pri_ad.droid):
            self.log.info("Failed to disable bluetooth")
            return False
        self.destroy_android_and_relay_object()
        self.rvr = {}

    def teardown_class(self):
        self.reset_wifi_and_store_results()

    def set_attenuation_and_run_iperf(self, called_func=None):
        """Sets attenuation and runs iperf for Attenuation max value.

        Args:
            called_func : Function object to run.

        Returns:
            True if Pass
            False if Fail
        """
        self.attenuators[self.num_atten - 1].set_atten(0)
        self.rvr["bt_attenuation"] = []
        self.rvr["test_name"] = self.current_test_name
        self.rvr["bt_gap_analysis"] = {}
        self.rvr["bt_range"] = []
        status_flag = True
        for bt_atten in self.bt_atten_range:
            self.rvr[bt_atten] = {}
            self.rvr[bt_atten]["fixed_attenuation"] = (
                self.test_params["fixed_attenuation"][str(
                    self.network["channel"])])
            self.log.info('Setting bt attenuation to: {} dB'.format(bt_atten))
            self.attenuators[self.num_atten - 1].set_atten(bt_atten)
            for i in range(self.num_atten - 1):
                self.attenuators[i].set_atten(0)
            if not wifi_connection_check(self.pri_ad, self.network["SSID"]):
                wifi_test_device_init(self.pri_ad)
                wifi_connect(self.pri_ad, self.network, num_of_tries=5)
            adb_rssi_results = self.pri_ad.search_logcat(RSSI_RESULTS)
            if adb_rssi_results:
                self.log.debug(adb_rssi_results[-1])
                self.log.info('Android device: {}'.format(
                    (adb_rssi_results[-1]['log_message']).split(',')[5]))
            (self.rvr[bt_atten]["throughput_received"],
             self.rvr[bt_atten]["a2dp_packet_drop"],
             status_flag) = self.rvr_throughput(bt_atten, called_func)
            self.wifi_max_atten_metric.metric_value = max(self.rvr[bt_atten]
                                                          ["attenuation"])
            self.wifi_min_atten_metric.metric_value = min(self.rvr[bt_atten]
                                                          ["attenuation"])

            if self.rvr[bt_atten]["throughput_received"]:
                for i, atten in enumerate(self.rvr[bt_atten]["attenuation"]):
                    if self.rvr[bt_atten]["throughput_received"][i] < 1.0:
                        self.wifi_range_metric.metric_value = (
                            self.rvr[bt_atten]["attenuation"][i-1])
                        break
                else:
                    self.wifi_range_metric.metric_value = max(
                        self.rvr[bt_atten]["attenuation"])
            else:
                self.wifi_range_metric.metric_value = max(
                    self.rvr[bt_atten]["attenuation"])
            if self.a2dp_streaming:
                if not any(x > 0 for x in self.a2dp_dropped_list):
                    self.rvr[bt_atten]["a2dp_packet_drop"] = []
        if not self.rvr["bt_range"]:
            self.rvr["bt_range"].append(0)
        return status_flag

    def rvr_throughput(self, bt_atten, called_func=None):
        """Sets attenuation and runs the function passed.

        Args:
            bt_atten: Bluetooth attenuation.
            called_func: Functions object to run parallely.

        Returns:
            Throughput, a2dp_drops and True/False.
        """
        self.iperf_received = []
        self.iperf_variables.received = []
        self.a2dp_dropped_list = []
        self.rvr["bt_attenuation"].append(bt_atten)
        self.rvr[bt_atten]["audio_artifacts"] = {}
        self.rvr[bt_atten]["attenuation"] = []
        self.rvr["bt_gap_analysis"][bt_atten] = {}
        for atten in self.wifi_atten_range:
            tag = '{}_{}'.format(bt_atten, atten)
            self.rvr[bt_atten]["attenuation"].append(
                atten + self.rvr[bt_atten]["fixed_attenuation"])
            self.log.info('Setting wifi attenuation to: {} dB'.format(atten))
            for i in range(self.num_atten - 1):
                self.attenuators[i].set_atten(atten)
            if not wifi_connection_check(self.pri_ad, self.network["SSID"]):
                self.iperf_received.append(0)
                return self.iperf_received, self.a2dp_dropped_list, False
            time.sleep(5)  # Time for attenuation to set.
            begin_time = get_current_epoch_time()
            if self.a2dp_streaming:
                self.audio.start()
            if called_func:
                if not multithread_func(self.log, called_func):
                    self.iperf_received.append(float(str(
                        self.iperf_variables.received[-1]).strip("Mb/s")))
                    return self.iperf_received, self.a2dp_dropped_list, False
            else:
                self.run_iperf_and_get_result()

            adb_rssi_poll_results = self.pri_ad.search_logcat(
                RSSI_POLL_RESULTS, begin_time)
            adb_rssi_results = self.pri_ad.search_logcat(
                RSSI_RESULTS, begin_time)
            if adb_rssi_results:
                self.log.debug(adb_rssi_poll_results)
                self.log.debug(adb_rssi_results[-1])
                self.log.info('Android device: {}'.format((
                    adb_rssi_results[-1]['log_message']).split(',')[5]))
            if self.a2dp_streaming:
                self.path = self.audio.stop()
                analysis_path = AudioCaptureResult(
                    self.path).audio_quality_analysis(self.audio_params)
                with open(analysis_path) as f:
                    self.rvr[bt_atten]["audio_artifacts"][atten] = f.readline()
                content = json.loads(self.rvr[bt_atten]["audio_artifacts"][atten])
                self.rvr["bt_gap_analysis"][bt_atten][atten] = {}
                for idx, data in enumerate(content["quality_result"]):
                    if data['artifacts']['delay_during_playback']:
                        self.rvr["bt_gap_analysis"][bt_atten][atten][idx] = (
                                data['artifacts']['delay_during_playback'])
                        self.rvr["bt_range"].append(bt_atten)
                    else:
                        self.rvr["bt_gap_analysis"][bt_atten][atten][idx] = 0
                file_path = collect_bluetooth_manager_dumpsys_logs(
                    self.pri_ad, self.current_test_name)
                self.a2dp_dropped_list.append(
                    self.a2dp_dumpsys.parse(file_path))
            self.iperf_received.append(
                    float(str(self.iperf_variables.throughput[-1]).strip("Mb/s")))
        for i in range(self.num_atten - 1):
            self.attenuators[i].set_atten(0)
        return self.iperf_received, self.a2dp_dropped_list, True

    def performance_baseline_check(self):
        """Checks for performance_result_path in config. If present, plots
        comparision chart else plot chart for that particular test run.

        Returns:
            True if success, False otherwise.
        """
        if self.rvr:
            with open(self.json_file, 'a') as results_file:
                json.dump({str(k): v for k, v in self.rvr.items()},
                          results_file, indent=4, sort_keys=True)
            self.bt_range_metric.metric_value = self.rvr["bt_range"][0]
            self.log.info('First occurrence of audio gap in bt '
                          'range: {}'.format(self.bt_range_metric.metric_value))
            self.log.info('Bluetooth min range: '
                          '{} dB'.format(min(self.rvr['bt_attenuation'])))
            self.log.info('Bluetooth max range: '
                          '{} dB'.format(max(self.rvr['bt_attenuation'])))
            self.plot_graph_for_attenuation()
            if not self.performance_files_list:
                self.log.warning('Performance file list is empty. Could not '
                                 'calculate throughput limits')
                return
            self.throughput_pass_fail_check()
        else:
            self.log.error("Throughput dict empty!")
            return False
        return True

    def plot_graph_for_attenuation(self):
        """Plots graph and add as JSON formatted results for attenuation with
        respect to its iperf values.
        """
        data_sets = defaultdict(dict)
        legends = defaultdict(list)

        x_label = 'WIFI Attenuation (dB)'
        y_label = []

        fig_property = {
            "title": self.current_test_name,
            "x_label": x_label,
            "linewidth": 3,
            "markersize": 10
        }

        for bt_atten in self.rvr["bt_attenuation"]:
            y_label.insert(0, 'Throughput (Mbps)')
            legends[bt_atten].insert(
                0, str("BT Attenuation @ %sdB" % bt_atten))
            data_sets[bt_atten]["attenuation"] = (
                self.rvr[bt_atten]["attenuation"])
            data_sets[bt_atten]["throughput_received"] = (
                self.rvr[bt_atten]["throughput_received"])

        if self.a2dp_streaming:
            for bt_atten in self.bt_atten_range:
                legends[bt_atten].insert(
                    0, ('Packet drops(in %) @ {}dB'.format(bt_atten)))
                data_sets[bt_atten]["a2dp_attenuation"] = (
                    self.rvr[bt_atten]["attenuation"])
                data_sets[bt_atten]["a2dp_packet_drops"] = (
                    self.rvr[bt_atten]["a2dp_packet_drop"])
            y_label.insert(0, "Packets Dropped")
        fig_property["y_label"] = y_label
        shaded_region = None

        if "performance_result_path" in self.user_params["test_params"]:
            shaded_region = self.comparision_results_calculation(data_sets, legends)

        output_file_path = os.path.join(self.pri_ad.log_path,
                                        self.current_test_name,
                                        "attenuation_plot.html")
        bokeh_chart_plot(list(self.rvr["bt_attenuation"]),
                         data_sets,
                         legends,
                         fig_property,
                         shaded_region=shaded_region,
                         output_file_path=output_file_path)

    def comparision_results_calculation(self, data_sets, legends):
        """Compares rvr results with baseline values by calculating throughput
        limits.

        Args:
            data_sets: including lists of x_data and lists of y_data.
                ex: [[[x_data1], [x_data2]], [[y_data1],[y_data2]]]
            legends: list of legend for each curve.

        Returns:
            None if test_file is not found, otherwise shaded_region
            will be returned.
        """
        try:
            attenuation_path = next(
                file_name for file_name in self.performance_files_list
                if self.current_test_name in file_name
            )
        except StopIteration:
            self.log.warning("Test_file not found. "
                             "No comparision values to calculate")
            return
        with open(attenuation_path, 'r') as throughput_file:
            throughput_results = json.load(throughput_file)
        for bt_atten in self.bt_atten_range:
            throughput_received = []
            user_attenuation = []
            legends[bt_atten].insert(
                0, ('Performance Results @ {}dB'.format(bt_atten)))
            for att in self.rvr[bt_atten]["attenuation"]:
                attenuation = att - self.rvr[bt_atten]["fixed_attenuation"]
                throughput_received.append(throughput_results[str(bt_atten)]
                    ["throughput_received"][attenuation])
                user_attenuation.append(att)
            data_sets[bt_atten][
                "user_attenuation"] = user_attenuation
            data_sets[bt_atten]["user_throughput"] = throughput_received
        throughput_limits = self.get_throughput_limits(attenuation_path)
        shaded_region = defaultdict(dict)
        for bt_atten in self.bt_atten_range:
            shaded_region[bt_atten] = {
                "x_vector": throughput_limits[bt_atten]["attenuation"],
                "lower_limit":
                throughput_limits[bt_atten]["lower_limit"],
                "upper_limit":
                throughput_limits[bt_atten]["upper_limit"]
            }
        return shaded_region

    def total_attenuation(self, performance_dict):
        """Calculates attenuation with adding fixed attenuation.

        Args:
            performance_dict: dict containing attenuation and fixed attenuation.

        Returns:
            Total attenuation is returned.
        """
        if "fixed_attenuation" in self.test_params:
            total_atten = [
                att + performance_dict["fixed_attenuation"]
                for att in performance_dict["attenuation"]
            ]
            return total_atten

    def throughput_pass_fail_check(self):
        """Check the test result and decide if it passed or failed
        by comparing with throughput limits.The pass/fail tolerances are
        provided in the config file.

        Returns:
            None if test_file is not found, True if successful,
            False otherwise.
        """
        try:
            performance_path = next(
                file_name for file_name in self.performance_files_list
                if self.current_test_name in file_name
            )
        except StopIteration:
            self.log.warning("Test_file not found. Couldn't "
                             "calculate throughput limits")
            return
        throughput_limits = self.get_throughput_limits(performance_path)

        failure_count = 0
        for bt_atten in self.bt_atten_range:
            for idx, current_throughput in enumerate(
                    self.rvr[bt_atten]["throughput_received"]):
                current_att = self.rvr[bt_atten]["attenuation"][idx]
                if (current_throughput <
                        (throughput_limits[bt_atten]["lower_limit"][idx]) or
                        current_throughput >
                        (throughput_limits[bt_atten]["upper_limit"][idx])):
                    failure_count = failure_count + 1
                    self.log.info(
                        "Throughput at {} dB attenuation is beyond limits. "
                        "Throughput is {} Mbps. Expected within [{}, {}] Mbps.".
                        format(
                            current_att, current_throughput,
                            throughput_limits[bt_atten]["lower_limit"][idx],
                            throughput_limits[bt_atten]["upper_limit"][
                                idx]))
            if failure_count >= self.test_params["failure_count_tolerance"]:
                self.log.error(
                    "Test failed. Found {} points outside throughput limits.".
                    format(failure_count))
                return False
            self.log.info(
                "Test passed. Found {} points outside throughput limits.".
                format(failure_count))
            return True

    def get_throughput_limits(self, performance_path):
        """Compute throughput limits for current test.

        Checks the RvR test result and compares to a throughput limits for
        the same configuration. The pass/fail tolerances are provided in the
        config file.

        Args:
            performance_path: path to baseline file used to generate limits

        Returns:
            throughput_limits: dict containing attenuation and throughput
            limit data
        """
        with open(performance_path, 'r') as performance_file:
            performance_results = json.load(performance_file)
        throughput_limits = defaultdict(dict)
        for bt_atten in self.bt_atten_range:
            performance_attenuation = (self.total_attenuation(
                performance_results[str(bt_atten)]))
            attenuation = []
            lower_limit = []
            upper_limit = []
            for idx, current_throughput in enumerate(
                    self.rvr[bt_atten]["throughput_received"]):
                current_att = self.rvr[bt_atten]["attenuation"][idx]
                att_distances = [
                    abs(current_att - performance_att)
                    for performance_att in performance_attenuation
                ]
                sorted_distances = sorted(
                    enumerate(att_distances), key=lambda x: x[1])
                closest_indeces = [dist[0] for dist in sorted_distances[0:3]]
                closest_throughputs = [
                    performance_results[str(bt_atten)]["throughput_received"][
                        index] for index in closest_indeces
                ]
                closest_throughputs.sort()
                attenuation.append(current_att)
                lower_limit.append(
                    max(closest_throughputs[0] -
                        max(self.test_params["abs_tolerance"],
                            closest_throughputs[0] *
                            self.test_params["pct_tolerance"] / 100), 0))
                upper_limit.append(closest_throughputs[-1] + max(
                    self.test_params["abs_tolerance"], closest_throughputs[-1] *
                    self.test_params["pct_tolerance"] / 100))
            throughput_limits[bt_atten]["attenuation"] = attenuation
            throughput_limits[bt_atten]["lower_limit"] = lower_limit
            throughput_limits[bt_atten]["upper_limit"] = upper_limit
        return throughput_limits

