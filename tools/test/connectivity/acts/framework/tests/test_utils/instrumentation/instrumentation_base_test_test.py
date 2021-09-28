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

import copy
import unittest

from acts.test_utils.instrumentation.config_wrapper import ConfigWrapper
from acts.test_utils.instrumentation.instrumentation_base_test import \
    InstrumentationBaseTest

MOCK_INSTRUMENTATION_CONFIG = {
    'MockController': {
        'param1': 1,
        'param2': 4
    },
    'MockInstrumentationBaseTest': {
        'MockController': {
            'param2': 2,
            'param3': 5
        },
        'test_case': {
            'MockController': {
                'param3': 3
            }
        }
    }
}


class MockInstrumentationBaseTest(InstrumentationBaseTest):
    """Mock test class to initialize required attributes."""
    def __init__(self):
        self.current_test_name = None
        self._instrumentation_config = ConfigWrapper(
            MOCK_INSTRUMENTATION_CONFIG)
        self._class_config = self._instrumentation_config.get_config(
            self.__class__.__name__)


class InstrumentationBaseTestTest(unittest.TestCase):
    def setUp(self):
        self.instrumentation_test = MockInstrumentationBaseTest()

    def test_get_controller_config_for_test_case(self):
        """Test that _get_controller_config returns the corresponding
        controller config for the current test case.
        """
        self.instrumentation_test.current_test_name = 'test_case'
        config = self.instrumentation_test._get_merged_config(
            'MockController')
        self.assertEqual(config.get('param1'), 1)
        self.assertEqual(config.get('param2'), 2)
        self.assertEqual(config.get('param3'), 3)

    def test_get_controller_config_for_test_class(self):
        """Test that _get_controller_config returns the controller config for
        the current test class (while no test case is running).
        """
        config = self.instrumentation_test._get_merged_config(
            'MockController')
        self.assertEqual(config.get('param1'), 1)
        self.assertEqual(config.get('param2'), 2)
        self.assertEqual(config.get('param3'), 5)


if __name__ == '__main__':
    unittest.main()
