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

import unittest
import mobly.config_parser as mobly_config_parser
import tests.test_utils.power.tel.lab.mock_bokeh
from acts.controllers.anritsu_lib import md8475_cellular_simulator as anritsu
from acts.controllers.rohdeschwarz_lib import cmw500_cellular_simulator as cmw
from unittest import mock


class InitializeSimulatorTest(unittest.TestCase):
    """ Unit tests for ensuring the simulator is correctly
        initialized for instances of PowerCellularLabBaseTest
    """
    @classmethod
    def setUpClass(self):
        from acts.test_utils.power.cellular.cellular_power_base_test import PowerCellularLabBaseTest as PCBT
        self.PCBT = PCBT
        PCBT.log = mock.Mock()
        PCBT.log_path = ''

    def setUp(self):
        self.tb_key = 'testbed_configs'
        test_run_config = mobly_config_parser.TestRunConfig()
        test_run_config.testbed_name = 'MockTestBed'
        test_run_config.log_path = '/tmp'
        test_run_config.summary_writer = mock.MagicMock()
        test = self.PCBT(test_run_config)
        self.test = test

    def test_initialize_simulator_md8475_A(self):
        """ Ensure that an instance of MD8475CellularSimulator
            is returned when requesting md8475_version A
        """
        self.test.unpack_userparams(md8475_version='A', md8475a_ip_address='12345')
        try:
            with mock.patch.object(anritsu.MD8475CellularSimulator,
                                   '__init__',
                                   return_value=None):
                result = self.test.initialize_simulator()
                self.assertTrue(
                    isinstance(result, anritsu.MD8475CellularSimulator),
                    'Incorrect simulator type returned for md8475_version A')
        except ValueError as e:
            self.fail('Error thrown: {}'.format(e))

    def test_initialize_simulator_md8475_B(self):
        """ Ensure that an instance of MD8475BCellularSimulator
            is returned when requesting md8475_version B
        """
        self.test.unpack_userparams(md8475_version='B', md8475a_ip_address='12345')
        try:
            with mock.patch.object(anritsu.MD8475BCellularSimulator,
                                   '__init__',
                                   return_value=None):
                result = self.test.initialize_simulator()
                self.assertTrue(
                    isinstance(result, anritsu.MD8475BCellularSimulator),
                    'Incorrect simulator type returned for md8475_version B')
        except ValueError as e:
            self.fail('Error thrown: {}'.format(e))

    def test_initialize_simulator_cmw500(self):
        """ Ensure that an instance of CMW500CellularSimulator
            is returned when requesting cmw500
        """
        self.test.unpack_userparams(md8475_version=None,
                               md8475a_ip_address=None,
                               cmw500_ip='12345',
                               cmw500_port='12345')
        try:
            with mock.patch.object(cmw.CMW500CellularSimulator,
                                   '__init__',
                                   return_value=None):
                result = self.test.initialize_simulator()
                self.assertTrue(
                    isinstance(result, cmw.CMW500CellularSimulator),
                    'Incorrect simulator type returned for cmw500')
        except ValueError as e:
            self.fail('Error thrown: {}'.format(e))

    def test_initialize_simulator_throws_with_missing_configs(self):
        """ Ensure that an error is raised when initialize_simulator
            is called with missing configs
        """
        self.test.unpack_userparams(md8475_version=None,
                               md8475a_ip_address=None,
                               cmw500_ip='12345')
        with self.assertRaises(RuntimeError), mock.patch.object(
                cmw.CMW500CellularSimulator, '__init__', return_value=None):
            self.test.initialize_simulator()


if __name__ == '__main__':
    unittest.main()
