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
from acts.test_utils.power.tel_simulations.LteSimulation import LteSimulation
from unittest import mock
from unittest.mock import mock_open


class SaveSummaryToFileTest(unittest.TestCase):
    """ Unit tests for testing the save summary functionality for
        instances of PowerCellularLabBaseTest
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

    def test_save_summary_to_file(self):
        """ Ensure that a new file is written when saving
            the test summary
        """
        self.test.unpack_userparams(simulation=mock.Mock(spec=LteSimulation))
        m = mock_open()
        with mock.patch('builtins.open', m, create=False):
            self.test.save_summary_to_file()
        self.assertTrue(m.called, 'Test summary was not written to output')


if __name__ == '__main__':
    unittest.main()
