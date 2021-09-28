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
from unittest import mock


class ConsumeParameterTest(unittest.TestCase):
    """ Unit tests for testing the consumption of test name parameters
      for instances of PowerCellularLabBaseTest
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

    def test_consume_parameter_typical_case(self):
        """ Tests the typical case: The parameter is available
            for consumption and it has enough values
        """
        parameters = ['param1', 1, 2, 'param2', 3, 'param3', 'value']
        expected = ['param2', 3]
        self.test.unpack_userparams(parameters=parameters)
        try:
            result = self.test.consume_parameter('param2', 1)
            self.assertTrue(
                result == expected,
                'Consume parameter did not return the expected result')
        except ValueError as e:
            self.fail('Error thrown: {}'.format(e))

    def test_consume_parameter_returns_empty_when_parameter_unavailabe(self):
        """ Tests the case where the requested parameter is unavailable
            for consumption. In this case, a ValueError should be raised
        """
        parameters = ['param1', 1, 2]
        expected = []
        self.test.unpack_userparams(parameters=parameters)
        try:
            result = self.test.consume_parameter('param2', 1)
            self.assertTrue(
                result == expected,
                'Consume parameter should return empty list for an invalid key'
            )
        except ValueError as e:
            self.fail('Error thrown: {}'.format(e))

    def test_consume_parameter_throws_when_requesting_too_many_parameters(
            self):
        """ Tests the case where the requested parameter is available
            for consumption, but too many values are requested
        """
        parameters = ['param1', 1, 2]
        self.test.unpack_userparams(parameters=parameters)
        with self.assertRaises(ValueError):
            self.test.consume_parameter('param1', 3)


if __name__ == '__main__':
    unittest.main()
