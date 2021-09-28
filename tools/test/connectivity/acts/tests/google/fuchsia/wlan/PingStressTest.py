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
"""
Script for exercising various ping scenarios

"""
from acts.base_test import BaseTestClass

import os
import threading
import uuid

from acts import signals
from acts.controllers.ap_lib import hostapd_constants
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import setup_ap_and_associate
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.tel.tel_test_utils import setup_droid_properties
from acts.test_utils.fuchsia import utils
from acts.utils import rand_ascii_str


class PingStressTest(BaseTestClass):
    # Timeout for ping thread in seconds
    ping_thread_timeout_s = 60 * 5

    # List to capture ping results
    ping_threads_result = []

    # IP addresses used in pings
    google_dns_1 = '8.8.8.8'
    google_dns_2 = '8.8.4.4'

    def setup_class(self):
        super().setup_class()

        self.ssid = rand_ascii_str(10)
        self.fd = self.fuchsia_devices[0]
        self.wlan_device = create_wlan_device(self.fd)
        self.ap = self.access_points[0]
        setup_ap_and_associate(access_point=self.ap,
                               client=self.wlan_device,
                               profile_name='whirlwind',
                               channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
                               ssid=self.ssid)

    def teardown_test(self):
        self.wlan_device.disconnect()
        self.wlan_device.reset_wifi()
        self.ap.stop_all_aps()

    def send_ping(self,
                  dest_ip,
                  count=3,
                  interval=1000,
                  timeout=1000,
                  size=25):
        ping_result = self.wlan_device.ping(dest_ip, count, interval, timeout,
                                            size)
        if ping_result:
            self.log.info('Ping was successful.')
        else:
            if '8.8' in dest_ip:
                raise signals.TestFailure('Ping was unsuccessful. Consider '
                                          'possibility of server failure.')
            else:
                raise signals.TestFailure('Ping was unsuccessful.')
        return True

    def ping_thread(self, dest_ip):
        ping_result = self.wlan_device.ping(dest_ip, count=10, size=50)
        if ping_result:
            self.log.info('Success pinging: %s' % dest_ip)
        else:
            self.log.info('Failure pinging: %s' % dest_ip)

        self.ping_threads_result.append(ping_result)

    def test_simple_ping(self):
        return self.send_ping(self.google_dns_1)

    def test_ping_local(self):
        return self.send_ping('127.0.0.1')

    def test_ping_AP(self):
        return self.send_ping(self.ap.ssh_settings.hostname)

    def test_ping_with_params(self):
        return self.send_ping(self.google_dns_1,
                              count=5,
                              interval=800,
                              size=50)

    def test_long_ping(self):
        return self.send_ping(self.google_dns_1, count=50)

    def test_medium_packet_ping(self):
        return self.send_ping(self.google_dns_1, size=64)

    def test_medium_packet_long_ping(self):
        return self.send_ping(self.google_dns_1,
                              count=50,
                              timeout=1500,
                              size=64)

    def test_large_packet_ping(self):
        return self.send_ping(self.google_dns_1, size=500)

    def test_large_packet_long_ping(self):
        return self.send_ping(self.google_dns_1,
                              count=50,
                              timeout=5000,
                              size=500)

    def test_simultaneous_pings(self):
        ping_urls = [
            self.google_dns_1, self.google_dns_2, self.google_dns_1,
            self.google_dns_2
        ]
        ping_threads = []

        try:
            # Start multiple ping at the same time
            for index, url in enumerate(ping_urls):
                self.log.info('Create and start thread %d.' % index)
                t = threading.Thread(target=self.ping_thread, args=(url, ))
                ping_threads.append(t)
                t.start()

            # Wait for all threads to complete or timeout
            for t in ping_threads:
                t.join(self.ping_thread_timeout_s)

        finally:
            is_alive = False

            for index, t in enumerate(ping_threads):
                if t.isAlive():
                    t = None
                    is_alive = True

            if is_alive:
                raise signals.TestFailure('Thread %d timedout' % index)

        for index in range(0, len(self.ping_threads_result)):
            if not self.ping_threads_result[index]:
                self.log.info("Ping failed for %d" % index)
                raise signals.TestFailure('Thread %d failed to ping. '
                                          'Consider possibility of server '
                                          'failure' % index)
        return True
