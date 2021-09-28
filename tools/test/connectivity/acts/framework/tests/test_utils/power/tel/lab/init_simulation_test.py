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
from acts.test_utils.power.tel_simulations.UmtsSimulation import UmtsSimulation
from unittest import mock


class InitSimulationTest(unittest.TestCase):
    """ Unit tests for ensuring the simulation is correctly
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

    def test_init_simulation_reuses_simulation_if_same_type(self):
        """ Ensure that a new simulation is not instantiated if
            the type is the same as the last simulation
        """
        mock_lte_sim = mock.Mock(spec=LteSimulation)
        self.test.unpack_userparams(simulation=mock_lte_sim)
        try:
            self.test.init_simulation(self.PCBT.PARAM_SIM_TYPE_LTE)
        except ValueError as e:
            self.fail('Error thrown: {}'.format(e))
        self.assertTrue(self.test.simulation is mock_lte_sim,
                        'A new simulation was instantiated')

    def test_init_simulation_does_not_reuse_simulation_if_different_type(self):
        """ Ensure that a new simulation is instantiated if
            the type is different from the last simulation
        """
        self.test.unpack_userparams(simulation=mock.Mock(spec=LteSimulation),
                               test_params=mock.Mock())
        try:
            with mock.patch.object(UmtsSimulation,
                                   '__init__',
                                   return_value=None) as mock_init:
                self.test.init_simulation(self.PCBT.PARAM_SIM_TYPE_UMTS)
        except Exception as e:
            self.fail('Error thrown: {}'.format(e))
        self.assertTrue(mock_init.called,
                        'A new simulation was not instantiated')

    def test_init_simulation_throws_error_with_invalid_simulation_type(self):
        """ Ensure that a new simulation is not instantiated if
            the type is invalid
        """
        self.test.unpack_userparams(simulation=mock.Mock(spec=LteSimulation))
        with self.assertRaises(ValueError):
            self.test.init_simulation('Invalid simulation type')


if __name__ == '__main__':
    unittest.main()
