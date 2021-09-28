#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
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

from acts import utils
from acts.controllers.ap_lib import hostapd_constants
from acts.controllers.ap_lib import hostapd_config
from acts.test_utils.abstract_devices.wlan_device import create_wlan_device
from acts.test_utils.abstract_devices.utils_lib.wlan_utils import validate_setup_ap_and_associate
from acts.test_utils.wifi.WifiBaseTest import WifiBaseTest
from acts.utils import rand_ascii_str

# 12, 13 outside the US
# 14 in Japan, DSSS and CCK only
CHANNELS_24 = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]

# 32, 34 for 20 and 40 mhz in Europe
# 34, 42, 46 have mixed international support
# 144 is supported by some chips
CHANNELS_5 = [
    36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128,
    132, 136, 140, 144, 149, 153, 157, 161, 165
]

BANDWIDTH_24 = [20, 40]
BANDWIDTH_5 = [20, 40, 80]

N_CAPABILITIES_DEFAULT = [
    hostapd_constants.N_CAPABILITY_LDPC, hostapd_constants.N_CAPABILITY_SGI20,
    hostapd_constants.N_CAPABILITY_SGI40,
    hostapd_constants.N_CAPABILITY_TX_STBC,
    hostapd_constants.N_CAPABILITY_RX_STBC1
]

AC_CAPABILITIES_DEFAULT = [
    hostapd_constants.AC_CAPABILITY_MAX_MPDU_11454,
    hostapd_constants.AC_CAPABILITY_RXLDPC,
    hostapd_constants.AC_CAPABILITY_SHORT_GI_80,
    hostapd_constants.AC_CAPABILITY_TX_STBC_2BY1,
    hostapd_constants.AC_CAPABILITY_RX_STBC_1,
    hostapd_constants.AC_CAPABILITY_MAX_A_MPDU_LEN_EXP7,
    hostapd_constants.AC_CAPABILITY_RX_ANTENNA_PATTERN,
    hostapd_constants.AC_CAPABILITY_TX_ANTENNA_PATTERN
]


def generate_test_name(settings):
    """Generates a string test name based on the channel and band

    Args:
        settings: A dict with 'channel' and 'bandwidth' keys.

    Returns:
        A string that represents a test case name.
    """
    return 'test_channel_%s_bandwidth_%smhz' % (settings['channel'],
                                                settings['bandwidth'])


class ChannelSweepTest(WifiBaseTest):
    """Tests for associating with all 2.5 and 5 channels and bands.

    Testbed Requirement:
    * One ACTS compatible device (dut)
    * One Access Point
    """
    def __init__(self, controllers):
        WifiBaseTest.__init__(self, controllers)
        self.tests = ['test_24ghz_channels', 'test_5ghz_channels']
        if 'debug_channel_sweep_tests' in self.user_params:
            self.tests.append('test_channel_sweep_debug')

    def setup_class(self):
        super().setup_class()
        if 'dut' in self.user_params:
            if self.user_params['dut'] == 'fuchsia_devices':
                self.dut = create_wlan_device(self.fuchsia_devices[0])
            elif self.user_params['dut'] == 'android_devices':
                self.dut = create_wlan_device(self.android_devices[0])
            else:
                raise ValueError('Invalid DUT specified in config. (%s)' %
                                 self.user_params['dut'])
        else:
            # Default is an android device, just like the other tests
            self.dut = create_wlan_device(self.android_devices[0])

        self.android_devices = getattr(self, 'android_devices', [])
        self.access_point = self.access_points[0]
        self.access_point.stop_all_aps()

    def setup_test(self):
        for ad in self.android_devices:
            ad.droid.wakeLockAcquireBright()
            ad.droid.wakeUpNow()
        self.dut.wifi_toggle_state(True)

    def teardown_test(self):
        for ad in self.android_devices:
            ad.droid.wakeLockRelease()
            ad.droid.goToSleepNow()
        self.dut.turn_location_off_and_scan_toggle_off()
        self.dut.disconnect()
        self.dut.reset_wifi()
        self.access_point.stop_all_aps()

    def on_fail(self, test_name, begin_time):
        self.dut.take_bug_report(test_name, begin_time)
        self.dut.get_log(test_name, begin_time)

    def setup_and_connect(self, settings):
        """Generates a hostapd config based on the provided channel and
        bandwidth, starts AP with that config, and attempts to associate the
        dut.

        Args:
            settings: A dict with 'channel' and 'bandwidth' keys.
        """
        channel = settings['channel']
        bandwidth = settings['bandwidth']
        if channel > 14:
            vht_bandwidth = bandwidth
        else:
            vht_bandwidth = None

        if bandwidth == 20:
            n_capabilities = N_CAPABILITIES_DEFAULT + [
                hostapd_constants.N_CAPABILITY_HT20
            ]
        elif bandwidth == 40 or bandwidth == 80:
            if hostapd_config.ht40_plus_allowed(channel):
                extended_channel = [hostapd_constants.N_CAPABILITY_HT40_PLUS]
            elif hostapd_config.ht40_minus_allowed(channel):
                extended_channel = [hostapd_constants.N_CAPABILITY_HT40_MINUS]
            else:
                raise ValueError('Invalid Channel: %s' % channel)
            n_capabilities = N_CAPABILITIES_DEFAULT + extended_channel
        else:
            raise ValueError('Invalid Bandwidth: %s' % bandwidth)

        validate_setup_ap_and_associate(access_point=self.access_point,
                                        client=self.dut,
                                        profile_name='whirlwind',
                                        channel=channel,
                                        n_capabilities=n_capabilities,
                                        ac_capabilities=None,
                                        force_wmm=True,
                                        ssid=utils.rand_ascii_str(20),
                                        vht_bandwidth=vht_bandwidth)

    def create_test_settings(self, channels, bandwidths):
        """Creates a list of test configurations to run from the product of the
        given channels list and bandwidths list.

        Args:
            channels: A list of ints representing the channels to test.
            bandwidths: A list of ints representing the bandwidths to test on
                those channels.

        Returns:
            A list of dictionaries containing 'channel' and 'bandwidth' keys,
                one for each test combination to be run.
        """
        test_list = []
        for combination in itertools.product(channels, bandwidths):
            test_settings = {
                'channel': combination[0],
                'bandwidth': combination[1]
            }
            test_list.append(test_settings)
        return test_list

    def test_24ghz_channels(self):
        """Runs setup_and_connect for 2.4GHz channels on 20 and 40 MHz bands."""
        test_list = self.create_test_settings(CHANNELS_24, BANDWIDTH_24)
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)

    def test_5ghz_channels(self):
        """Runs setup_and_connect for 5GHz channels on 20, 40, and 80 MHz bands.
        """
        test_list = self.create_test_settings(CHANNELS_5, BANDWIDTH_5)
        # Channel 165 is 20mhz only
        test_list.remove({'channel': 165, 'bandwidth': 40})
        test_list.remove({'channel': 165, 'bandwidth': 80})
        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)

    def test_channel_sweep_debug(self):
        """Runs test cases defined in the ACTS config file for debugging
        purposes.

        Usage:
            1. Add 'debug_channel_sweep_tests' to ACTS config with list of
                tests to run matching pattern:
                test_channel_<CHANNEL>_bandwidth_<BANDWIDTH>
            2. Run test_channel_sweep_debug test.
        """
        allowed_channels = CHANNELS_24 + CHANNELS_5
        chan_band_pattern = re.compile(
            r'.*channel_([0-9]*)_.*bandwidth_([0-9]*)')
        test_list = []
        for test_title in self.user_params['debug_channel_sweep_tests']:
            test = re.match(chan_band_pattern, test_title)
            if test:
                channel = int(test.group(1))
                bandwidth = int(test.group(2))
                if channel not in allowed_channels:
                    raise ValueError("Invalid channel: %s" % channel)
                if channel <= 14 and bandwidth not in BANDWIDTH_24:
                    raise ValueError(
                        "Channel %s does not support bandwidth %s" %
                        (channel, bandwidth))
                test_settings = {'channel': channel, 'bandwidth': bandwidth}
                test_list.append(test_settings)

        self.run_generated_testcases(self.setup_and_connect,
                                     settings=test_list,
                                     name_func=generate_test_name)
