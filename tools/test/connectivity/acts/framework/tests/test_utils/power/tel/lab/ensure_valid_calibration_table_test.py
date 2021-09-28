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


class EnsureValidCalibrationTableTest(unittest.TestCase):
    """ Unit tests for exercising the logic of ensure_valid_calibration_table
        for instances of PowerCellularLabBaseTest
    """

    VALID_CALIBRATION_TABLE = {'1': {'2': {'3': 123, '4': 3.14}}, '2': 45.67}

    INVALID_CALIBRATION_TABLE = invalid = {'1': {'a': 'invalid'}, '2': 1234}

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


    def _assert_no_exception(self, func, *args, **kwargs):
        try:
            func(*args, **kwargs)
        except Exception as e:
            self.fail('Error thrown: {}'.format(e))

    def _assert_calibration_table_passes(self, table):
        self._assert_no_exception(self.test.ensure_valid_calibration_table, table)

    def _assert_calibration_table_fails(self, table):
        with self.assertRaises(TypeError):
            self.test.ensure_valid_calibration_table(table)

    def test_ensure_valid_calibration_table_passes_with_empty_table(self):
        """ Ensure that empty calibration tables are invalid """
        self._assert_calibration_table_passes({})

    def test_ensure_valid_calibration_table_passes_with_valid_table(self):
        """ Ensure that valid calibration tables throw no error """
        self._assert_calibration_table_passes(self.VALID_CALIBRATION_TABLE)

    def test_ensure_valid_calibration_table_fails_with_invalid_data(self):
        """ Ensure that calibration tables with invalid entries throw an error """
        self._assert_calibration_table_fails(self.INVALID_CALIBRATION_TABLE)

    def test_ensure_valid_calibration_table_fails_with_none(self):
        """ Ensure an exception is thrown if no calibration table is given """
        self._assert_calibration_table_fails(None)

    def test_ensure_valid_calibration_table_fails_with_invalid_type(self):
        """ Ensure an exception is thrown if no calibration table is given """
        self._assert_calibration_table_fails([])


if __name__ == '__main__':
    unittest.main()
