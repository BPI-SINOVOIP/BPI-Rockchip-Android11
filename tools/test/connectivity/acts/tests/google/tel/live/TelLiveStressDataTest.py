#!/usr/bin/env python3
#
#   Copyright 2019 - Google
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
"""
    Test Script for Telephony Stress data Test
"""
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_test_utils import iperf_test_by_adb
from acts.test_utils.tel.tel_test_utils import iperf_udp_test_by_adb


class TelLiveStressDataTest(TelephonyBaseTest):
    def setup_class(self):
        super().setup_class()
        self.ad = self.android_devices[0]
        self.iperf_server_address = self.user_params.get("iperf_server",
                                                         '0.0.0.0')
        self.iperf_srv_tcp_port = self.user_params.get("iperf_server_tcp_port",
                                                       0)
        self.iperf_srv_udp_port = self.user_params.get("iperf_server_udp_port",
                                                       0)
        self.test_duration = self.user_params.get("data_stress_duration", 60)

        return True

    @test_tracker_info(uuid="190fdeb1-541e-455f-9f37-762a8e55c07f")
    @TelephonyBaseTest.tel_test_wrap
    def test_tcp_upload_stress(self):
        return iperf_test_by_adb(self.log,
                                 self.ad,
                                 self.iperf_server_address,
                                 self.iperf_srv_tcp_port,
                                 False,
                                 self.test_duration)

    @test_tracker_info(uuid="af9805f8-6ed5-4e05-823e-d88dcef45637")
    @TelephonyBaseTest.tel_test_wrap
    def test_tcp_download_stress(self):
        return iperf_test_by_adb(self.log,
                                 self.ad,
                                 self.iperf_server_address,
                                 self.iperf_srv_tcp_port,
                                 True,
                                 self.test_duration)

    @test_tracker_info(uuid="55bf5e09-dc7b-40bc-843f-31fed076ffe4")
    @TelephonyBaseTest.tel_test_wrap
    def test_udp_upload_stress(self):
        return iperf_udp_test_by_adb(self.log,
                                     self.ad,
                                     self.iperf_server_address,
                                     self.iperf_srv_udp_port,
                                     False,
                                     self.test_duration)

    @test_tracker_info(uuid="02ae88b2-d597-45df-ab5a-d701d1125a0f")
    @TelephonyBaseTest.tel_test_wrap
    def test_udp_download_stress(self):
        return iperf_udp_test_by_adb(self.log,
                                     self.ad,
                                     self.iperf_server_address,
                                     self.iperf_srv_udp_port,
                                     True,
                                     self.test_duration)
