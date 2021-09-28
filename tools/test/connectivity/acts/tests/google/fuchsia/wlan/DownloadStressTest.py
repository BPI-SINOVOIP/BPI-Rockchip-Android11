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
Script for testing various download stress scenarios.

"""
import os
import threading
import uuid

from acts.base_test import BaseTestClass
from acts import signals
from acts.controllers.ap_lib import hostapd_constants
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import setup_ap_and_associate
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.fuchsia import utils
from acts.test_utils.tel.tel_test_utils import setup_droid_properties
from acts.utils import rand_ascii_str


class DownloadStressTest(BaseTestClass):
    # Default number of test iterations here.
    # Override using parameter in config file.
    # Eg: "download_stress_test_iterations": "10"
    num_of_iterations = 3

    # Timeout for download thread in seconds
    download_timeout_s = 60 * 5

    # Download urls
    url_20MB = 'http://ipv4.download.thinkbroadband.com/20MB.zip'
    url_40MB = 'http://ipv4.download.thinkbroadband.com/40MB.zip'
    url_60MB = 'http://ipv4.download.thinkbroadband.com/60MB.zip'
    url_512MB = 'http://ipv4.download.thinkbroadband.com/512MB.zip'

    # Constants used in test_one_large_multiple_small_downloads
    download_small_url = url_20MB
    download_large_url = url_512MB
    num_of_small_downloads = 5
    download_threads_result = []

    def setup_class(self):
        super().setup_class()
        self.ssid = rand_ascii_str(10)
        self.fd = self.fuchsia_devices[0]
        self.wlan_device = create_wlan_device(self.fd)
        self.ap = self.access_points[0]
        self.num_of_iterations = int(
            self.user_params.get("download_stress_test_iterations",
                                 self.num_of_iterations))

        setup_ap_and_associate(
            access_point=self.ap,
            client=self.wlan_device,
            profile_name='whirlwind',
            channel=hostapd_constants.AP_DEFAULT_CHANNEL_2G,
            ssid=self.ssid)

    def teardown_test(self):
        self.download_threads_result.clear()
        self.wlan_device.disconnect()
        self.wlan_device.reset_wifi()
        self.ap.stop_all_aps()

    def test_download_small(self):
        self.log.info("Downloading small file")
        return self.download_file(self.url_20MB)

    def test_download_large(self):
        return self.download_file(self.url_512MB)

    def test_continuous_download(self):
        for x in range(0, self.num_of_iterations):
            if not self.download_file(self.url_512MB):
                return False
        return True

    def download_file(self, url):
        self.log.info("Start downloading: %s" % url)
        return utils.http_file_download_by_curl(
            self.fd,
            url,
            additional_args='--max-time %d --silent' % self.download_timeout_s)

    def download_thread(self, url):
        download_status = self.download_file(url)
        if download_status:
            self.log.info("Success downloading: %s" % url)
        else:
            self.log.info("Failure downloading: %s" % url)

        self.download_threads_result.append(download_status)
        return download_status

    def test_multi_downloads(self):
        download_urls = [self.url_20MB, self.url_40MB, self.url_60MB]
        download_threads = []

        try:
            # Start multiple downloads at the same time
            for index, url in enumerate(download_urls):
                self.log.info('Create and start thread %d.' % index)
                t = threading.Thread(target=self.download_thread, args=(url, ))
                download_threads.append(t)
                t.start()

            # Wait for all threads to complete or timeout
            for t in download_threads:
                t.join(self.download_timeout_s)

        finally:
            is_alive = False

            for index, t in enumerate(download_threads):
                if t.isAlive():
                    t = None
                    is_alive = True

            if is_alive:
                raise signals.TestFailure('Thread %d timedout' % index)

        for index in range(0, len(self.download_threads_result)):
            if not self.download_threads_result[index]:
                self.log.info("Download failed for %d" % index)
                raise signals.TestFailure(
                    'Thread %d failed to download' % index)
                return False

        return True

    def test_one_large_multiple_small_downloads(self):
        for index in range(self.num_of_iterations):
            download_threads = []
            try:
                large_thread = threading.Thread(
                    target=self.download_thread,
                    args=(self.download_large_url, ))
                download_threads.append(large_thread)
                large_thread.start()

                for i in range(self.num_of_small_downloads):
                    # Start small file download
                    t = threading.Thread(
                        target=self.download_thread,
                        args=(self.download_small_url, ))
                    download_threads.append(t)
                    t.start()
                    # Wait for thread to exit before starting the next iteration
                    t.join(self.download_timeout_s)

                # Wait for the large file download thread to complete
                large_thread.join(self.download_timeout_s)

            finally:
                is_alive = False

                for index, t in enumerate(download_threads):
                    if t.isAlive():
                        t = None
                        is_alive = True

                if is_alive:
                    raise signals.TestFailure('Thread %d timedout' % index)

            for index in range(0, len(self.download_threads_result)):
                if not self.download_threads_result[index]:
                    self.log.info("Download failed for %d" % index)
                    raise signals.TestFailure(
                        'Thread %d failed to download' % index)
                    return False

            # Clear results before looping again
            self.download_threads_result.clear()

        return True
