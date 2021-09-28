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

import csv
import itertools
import os
import re
import time

from collections import OrderedDict
from functools import partial

import acts.base_test
import acts.controllers.rohdeschwarz_lib.cmw500 as cmw500
from acts.test_utils.coex.hotspot_utils import band_channel_map
from acts.test_utils.coex.hotspot_utils import supported_lte_bands
from acts.test_utils.coex.hotspot_utils import tdd_band_list
from acts.test_utils.coex.hotspot_utils import wifi_channel_map
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode
from acts.test_utils.wifi.wifi_test_utils import reset_wifi
from acts.test_utils.wifi.wifi_test_utils import start_wifi_tethering
from acts.test_utils.wifi.wifi_test_utils import stop_wifi_tethering
from acts.test_utils.wifi.wifi_test_utils import wifi_connect
from acts.test_utils.wifi.wifi_test_utils import WifiEnums

BANDWIDTH_2G = 20
CNSS_LOG_PATH = '/data/vendor/wifi/wlan_logs'
CNSS_CMD = 'cnss_diag -f -s'


class HotspotWiFiChannelTest(acts.base_test.BaseTestClass):
    """Idea behind this test is to check which wifi channel gets picked with
    different lte bands(low, mid and high frequencies) when connected via
    hotspot from secondary device. As of now there is no failure condition
    to check the channel picked for the particular lte band.
    """
    def __init__(self, controllers):
        super().__init__(controllers)
        req_params = ['callbox_params', 'network', 'lte_bands', 'hotspot_mode']
        self.unpack_userparams(req_params)
        self.tests = self.generate_test_cases()

    def setup_class(self):
        self.pri_ad = self.android_devices[0]
        self.sec_ad = self.android_devices[1]
        self.cmw = cmw500.Cmw500(self.callbox_params['host'],
                                 self.callbox_params['port'])
        # Get basestation object.
        self.bts = self.cmw.get_base_station()
        csv_header = ('Hotspot Mode', 'lte_band', 'LTE_dl_channel',
                      'LTE_ul_freq', 'LTE_dl_freq', 'wifi_channel',
                      'wifi_bandwidth')
        self.write_data_to_csv(csv_header)

    def setup_test(self):
        self.pri_ad.adb.shell_nb(CNSS_CMD)

    def teardown_test(self):
        self.pri_ad.adb.shell('killall cnss_diag')
        stop_wifi_tethering(self.pri_ad)
        reset_wifi(self.sec_ad)
        cnss_path = os.path.join(self.log_path, 'wlan_logs')
        os.makedirs(cnss_path, exist_ok=True)
        self.pri_ad.pull_files([CNSS_LOG_PATH], os.path.join(
            cnss_path, self.current_test_name))
        self.pri_ad.adb.shell('rm -rf {}'.format(CNSS_LOG_PATH))

    def teardown_class(self):
        self.cmw.disconnect()

    def write_data_to_csv(self, data):
        """Writes the data to csv file

        Args:
            data: data to be written into csv.
        """
        with open('{}/test_data.csv'.format(self.log_path), 'a',
                  newline="") as cf:
            csv_writer = csv.writer(cf, delimiter=',')
            csv_writer.writerow(data)
            cf.close()

    def generate_test_cases(self):
        # find and run only the supported bands.
        lte_band = list(set(self.lte_bands).intersection(supported_lte_bands))

        if len(lte_band) == 0:
            # if lte_band in config is empty run all bands.
            lte_band = supported_lte_bands

        test_cases = []
        for hmode, lband in itertools.product(self.hotspot_mode, lte_band):
            for channel in band_channel_map.get(lband):
                test_case_name = ('test_hotspot_lte_band_{}_channel_{}_wifi'
                                  '_band_{}'.format(lband, channel, hmode))
                test_params = OrderedDict(
                    lte_band=lband,
                    LTE_dl_channel=channel,
                    hotspot_mode=hmode,
                )
                setattr(self, test_case_name, partial(self.set_hotspot_params,
                                                      test_params))
                test_cases.append(test_case_name)

        return test_cases

    def set_hotspot_params(self, test_params):
        """Set up hot spot parameters.

        Args:
            test_params: Contains band and frequency of current test.
        """
        self.setup_lte_and_attach(test_params['lte_band'],
                                  test_params['LTE_dl_channel'])
        band = test_params['hotspot_mode'].lower()
        self.initiate_wifi_tethering_and_connect(band)
        test_params['LTE_ul_freq'] = self.bts.ul_frequency
        test_params['LTE_dl_freq'] = self.bts.dl_frequency
        test_params['wifi_channel'] = self.get_wifi_channel(self.sec_ad)
        test_params['wifi_bandwidth'] = self.get_wifi_bandwidth(self.sec_ad)
        data = (test_params['hotspot_mode'], test_params['lte_band'],
                test_params['LTE_dl_channel'], test_params['LTE_ul_freq'],
                test_params['LTE_dl_freq'], test_params['wifi_channel'],
                test_params['wifi_bandwidth'])

        self.write_data_to_csv(data)

    def setup_lte_and_attach(self, band, channel):
        """Setup callbox and attaches the device.

        Args:
            band: lte band to configure.
            channel: channel to set for band.
        """
        toggle_airplane_mode(self.log, self.pri_ad, True)

        # Reset system
        self.cmw.reset()

        if band in tdd_band_list:
            self.bts.duplex_mode = cmw500.DuplexMode.TDD

        # Turn ON LTE signalling
        self.cmw.switch_lte_signalling(cmw500.LteState.LTE_ON)

        # Set Signalling params
        self.cmw.enable_packet_switching()
        self.bts.downlink_power_level = '-59.8'

        self.bts.band = band
        self.bts.bandwidth = cmw500.LteBandwidth.BANDWIDTH_5MHz
        self.bts.dl_channel = channel
        time.sleep(1)
        self.log.info('Callbox settings: band: {}, bandwidth: {}, '
                      'dl_channel: {}, '.format(self.bts.band,
                                                self.bts.bandwidth,
                                                self.bts.dl_channel
                                                ))

        toggle_airplane_mode(self.log, self.pri_ad, False)
        self.log.info('Waiting for device to attach.')
        self.cmw.wait_for_attached_state()
        self.log.info('Device attached with callbox.')
        self.log.debug('Waiting for connected state.')
        self.cmw.wait_for_rrc_state(cmw500.LTE_CONN_RESP)
        self.log.info('Device connected with callbox')

    def initiate_wifi_tethering_and_connect(self, wifi_band=None):
        """Initiates wifi tethering and connects wifi.

        Args:
            wifi_band: Hotspot mode to set.
        """
        if wifi_band == '2g':
            wband = WifiEnums.WIFI_CONFIG_APBAND_2G
        elif wifi_band == '5g':
            wutils.set_wifi_country_code(self.pri_ad, WifiEnums.CountryCode.US)
            wutils.set_wifi_country_code(self.sec_ad, WifiEnums.CountryCode.US)
            wband = WifiEnums.WIFI_CONFIG_APBAND_5G
        elif wifi_band == 'auto':
            wband = WifiEnums.WIFI_CONFIG_APBAND_AUTO
        else:
            raise ValueError('Invalid hotspot mode.')

        start_wifi_tethering(self.pri_ad, self.network['SSID'],
                             self.network['password'], band=wband)

        wifi_connect(self.sec_ad, self.network, check_connectivity=False)

    def get_wifi_channel(self, ad):
        """Get the Wifi Channel for the SSID connected.

        Args:
            ad: Android device to get channel.

        Returns:
            wifi_channel: WiFi channel of connected device,

        Raises:
            Value Error on Failure.
        """
        out = ad.adb.shell('wpa_cli status')
        match = re.search('freq=.*', out)
        if match:
            freq = match.group(0).split('=')[1]
            wifi_channel = wifi_channel_map[int(freq)]
            self.log.info('Channel Chosen: {}'.format(wifi_channel))
            return wifi_channel
        else:
            raise ValueError('Wifi connection inactive.')

    def get_wifi_bandwidth(self, ad):
        """Gets the Wifi Bandwidth for the SSID connected.

        Args:
            ad: Android device to get bandwidth.

        Returns:
            bandwidth: if connected wifi is 5GHz.
            2G_BANDWIDTH: if connected wifi is 2GHz,
        """
        out = ad.adb.shell('iw wlan0 link')
        match = re.search(r'[0-9.]+MHz', out)
        if match:
            bandwidth = match.group(0).strip('MHz')
            return bandwidth
        else:
            return BANDWIDTH_2G
