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

import os
import time
import unittest
from unittest.mock import patch

from acts.controllers.ap_lib import radvd_constants
from acts.controllers.ap_lib.radvd import Error
from acts.controllers.ap_lib.radvd import Radvd

from acts.controllers.ap_lib.radvd_config import RadvdConfig

RADVD_PREFIX = 'fd00::/64'
SEARCH_FILE = ('acts.controllers.utils_lib.commands.shell.'
               'ShellCommand.search_file')
DELETE_FILE = ('acts.controllers.utils_lib.commands.shell.ShellCommand.'
               'delete_file')

CORRECT_COMPLEX_RADVD_CONFIG = ("""interface wlan0 {
    IgnoreIfMissing on;
    AdvSendAdvert off;
    UnicastOnly on;
    MaxRtrAdvInterval 60;
    MinRtrAdvInterval 5;
    MinDelayBetweenRAs 5;
    AdvManagedFlag off;
    AdvOtherConfigFlag on;
    AdvLinkMTU 1400;
    AdvReachableTime 3600000;
    AdvRetransTimer 10;
    AdvCurHopLimit 50;
    AdvDefaultLifetime 8000;
    AdvDefaultPreference off;
    AdvSourceLLAddress on;
    AdvHomeAgentFlag off;
    AdvHomeAgentInfo on;
    HomeAgentLifetime 100;
    HomeAgentPreference 100;
    AdvMobRtrSupportFlag off;
    AdvIntervalOpt on;
    prefix fd00::/64
    {
        AdvOnLink off;
        AdvAutonomous on;
        AdvRouterAddr off;
        AdvValidLifetime 86400;
        AdvPreferredLifetime 14400;
        Base6to4Interface NA;
    };
    clients
    {
        fe80::c66d:3c75:2cec:1d72;
        fe80::c66d:3c75:2cec:1d73;
    };
    route fd00::/64 {
        AdvRouteLifetime 1024;
        AdvRoutePreference high;
    };
    RDNSS 2401:fa00:480:7a00:4d56:5373:4549:1e29 2401:fa00:480:7a00:4d56:5373:4549:1e30 {
        AdvRDNSSPreference 8;
        AdvRDNSSOpen on;
        AdvRDNSSLifetime 1025;
    };
};""".replace("    ", "\t"))

CORRECT_SIMPLE_RADVD_CONFIG = ("""interface wlan0 {
    AdvSendAdvert on;
    prefix fd00::/64
    {
        AdvOnLink on;
        AdvAutonomous on;
    };
};""".replace("    ", "\t"))


def delete_file_mock(file_to_delete):
    if os.path.exists(file_to_delete):
        os.remove(file_to_delete)


def write_configs_mock(config_file_with_path, output_config):
    with open(config_file_with_path, 'w+') as config_fileId:
        config_fileId.write(output_config)


class RadvdTest(unittest.TestCase):
    @patch('acts.controllers.utils_lib.commands.shell.ShellCommand.kill')
    def test_radvd_ikill(self, kill):
        kill.return_value = True
        radvd_mock = Radvd('mock_runner', 'wlan0')
        self.assertIsNone(radvd_mock.stop())

    @patch('acts.controllers.utils_lib.commands.shell.ShellCommand.is_alive')
    def test_radvd_is_alive_True(self, is_alive_mock):
        is_alive_mock.return_value = True
        radvd_mock = Radvd('mock_runner', 'wlan0')
        self.assertTrue(radvd_mock.is_alive())

    @patch('acts.controllers.utils_lib.commands.shell.ShellCommand.is_alive')
    def test_radvd_is_alive_False(self, is_alive_mock):
        is_alive_mock.return_value = False
        radvd_mock = Radvd('mock_runner', 'wlan0')
        self.assertFalse(radvd_mock.is_alive())

    @patch('acts.controllers.ap_lib.radvd.Radvd._scan_for_errors')
    @patch('acts.controllers.ap_lib.radvd.Radvd.is_alive')
    def test_wait_for_process_process_alive(self, is_alive_mock,
                                            _scan_for_errors_mock):
        is_alive_mock.return_value = True
        _scan_for_errors_mock.return_value = True
        radvd_mock = Radvd('mock_runner', 'wlan0')
        self.assertIsNone(radvd_mock._wait_for_process(timeout=2))

    @patch('acts.controllers.ap_lib.radvd.Radvd.is_alive')
    @patch(SEARCH_FILE)
    def test_scan_for_errors_is_dead(self, search_file_mock, is_alive_mock):
        is_alive_mock.return_value = False
        search_file_mock.return_value = False
        radvd_mock = Radvd('mock_runner', 'wlan0')
        with self.assertRaises(Error) as context:
            radvd_mock._scan_for_errors(True)
        self.assertTrue('Radvd failed to start' in str(context.exception))

    @patch('acts.controllers.ap_lib.radvd.Radvd.is_alive')
    @patch(SEARCH_FILE)
    def test_scan_for_errors_exited_prematurely(self, search_file_mock,
                                                is_alive_mock):
        is_alive_mock.return_value = True
        search_file_mock.return_value = True
        radvd_mock = Radvd('mock_runner', 'wlan0')
        with self.assertRaises(Error) as context:
            radvd_mock._scan_for_errors(True)
        self.assertTrue('Radvd exited prematurely.' in str(context.exception))

    @patch('acts.controllers.ap_lib.radvd.Radvd.is_alive')
    @patch(SEARCH_FILE)
    def test_scan_for_errors_success(self, search_file_mock, is_alive_mock):
        is_alive_mock.return_value = True
        search_file_mock.return_value = False
        radvd_mock = Radvd('mock_runner', 'wlan0')
        self.assertIsNone(radvd_mock._scan_for_errors(True))

    @patch(DELETE_FILE)
    @patch('acts.controllers.utils_lib.commands.shell.ShellCommand.write_file')
    def test_write_configs_simple(self, write_file, delete_file):
        delete_file.side_effect = delete_file_mock
        write_file.side_effect = write_configs_mock
        basic_radvd_config = RadvdConfig(
            prefix=RADVD_PREFIX,
            adv_send_advert=radvd_constants.ADV_SEND_ADVERT_ON,
            adv_on_link=radvd_constants.ADV_ON_LINK_ON,
            adv_autonomous=radvd_constants.ADV_AUTONOMOUS_ON)
        radvd_mock = Radvd('mock_runner', 'wlan0')
        radvd_mock._write_configs(basic_radvd_config)
        radvd_config = radvd_mock._config_file
        with open(radvd_config, 'r') as radvd_config_fileId:
            config_data = radvd_config_fileId.read()
            self.assertTrue(CORRECT_SIMPLE_RADVD_CONFIG == config_data)

    @patch(DELETE_FILE)
    @patch('acts.controllers.utils_lib.commands.shell.ShellCommand.write_file')
    def test_write_configs_complex(self, write_file, delete_file):
        delete_file.side_effect = delete_file_mock
        write_file.side_effect = write_configs_mock
        complex_radvd_config = RadvdConfig(
            prefix=RADVD_PREFIX,
            clients=['fe80::c66d:3c75:2cec:1d72', 'fe80::c66d:3c75:2cec:1d73'],
            route=RADVD_PREFIX,
            rdnss=[
                '2401:fa00:480:7a00:4d56:5373:4549:1e29',
                '2401:fa00:480:7a00:4d56:5373:4549:1e30',
            ],
            ignore_if_missing=radvd_constants.IGNORE_IF_MISSING_ON,
            adv_send_advert=radvd_constants.ADV_SEND_ADVERT_OFF,
            unicast_only=radvd_constants.UNICAST_ONLY_ON,
            max_rtr_adv_interval=60,
            min_rtr_adv_interval=5,
            min_delay_between_ras=5,
            adv_managed_flag=radvd_constants.ADV_MANAGED_FLAG_OFF,
            adv_other_config_flag=radvd_constants.ADV_OTHER_CONFIG_FLAG_ON,
            adv_link_mtu=1400,
            adv_reachable_time=3600000,
            adv_retrans_timer=10,
            adv_cur_hop_limit=50,
            adv_default_lifetime=8000,
            adv_default_preference=radvd_constants.ADV_DEFAULT_PREFERENCE_OFF,
            adv_source_ll_address=radvd_constants.ADV_SOURCE_LL_ADDRESS_ON,
            adv_home_agent_flag=radvd_constants.ADV_HOME_AGENT_FLAG_OFF,
            adv_home_agent_info=radvd_constants.ADV_HOME_AGENT_INFO_ON,
            home_agent_lifetime=100,
            home_agent_preference=100,
            adv_mob_rtr_support_flag=radvd_constants.
            ADV_MOB_RTR_SUPPORT_FLAG_OFF,
            adv_interval_opt=radvd_constants.ADV_INTERVAL_OPT_ON,
            adv_on_link=radvd_constants.ADV_ON_LINK_OFF,
            adv_autonomous=radvd_constants.ADV_AUTONOMOUS_ON,
            adv_router_addr=radvd_constants.ADV_ROUTER_ADDR_OFF,
            adv_valid_lifetime=86400,
            adv_preferred_lifetime=14400,
            base_6to4_interface='NA',
            adv_route_lifetime=1024,
            adv_route_preference=radvd_constants.ADV_ROUTE_PREFERENCE_HIGH,
            adv_rdnss_preference=8,
            adv_rdnss_open=radvd_constants.ADV_RDNSS_OPEN_ON,
            adv_rdnss_lifetime=1025)
        radvd_mock = Radvd('mock_runner', 'wlan0')
        radvd_mock._write_configs(complex_radvd_config)
        radvd_config = radvd_mock._config_file
        with open(radvd_config, 'r') as radvd_config_fileId:
            config_data = radvd_config_fileId.read()
            self.assertTrue(CORRECT_COMPLEX_RADVD_CONFIG == config_data)
