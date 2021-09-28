#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
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

import mock
import os
import shutil
import tempfile
import unittest

from mobly.config_parser import TestRunConfig

from acts import keys
from acts import test_runner

import acts_android_device_test
import mock_controller
import IntegrationTest


class ActsTestRunnerTest(unittest.TestCase):
    """This test class has unit tests for the implementation of everything
    under acts.test_runner.
    """
    def setUp(self):
        self.tmp_dir = tempfile.mkdtemp()
        self.base_mock_test_config = TestRunConfig()
        self.base_mock_test_config.testbed_name = 'SampleTestBed'
        self.base_mock_test_config.log_path = self.tmp_dir
        self.base_mock_test_config.controller_configs = {
            'testpaths': [os.path.dirname(IntegrationTest.__file__)]
        }
        self.base_mock_test_config.user_params = {
            'icecream': 42,
            'extra_param': 'haha'
        }
        self.mock_run_list = [('SampleTest', None)]

    def tearDown(self):
        shutil.rmtree(self.tmp_dir)

    def test_run_twice(self):
        """Verifies that:
        1. Repeated run works properly.
        2. The original configuration is not altered if a test controller
           module modifies configuration.
        """
        mock_test_config = self.base_mock_test_config.copy()
        tb_key = keys.Config.key_testbed.value
        mock_ctrlr_config_name = mock_controller.ACTS_CONTROLLER_CONFIG_NAME
        my_config = [{
            'serial': 'xxxx',
            'magic': 'Magic1'
        }, {
            'serial': 'xxxx',
            'magic': 'Magic2'
        }]
        mock_test_config.controller_configs[mock_ctrlr_config_name] = my_config
        tr = test_runner.TestRunner(mock_test_config,
                                    [('IntegrationTest', None)])
        tr.run()
        tr.run()
        tr.stop()
        results = tr.results.summary_dict()
        self.assertEqual(results['Requested'], 2)
        self.assertEqual(results['Executed'], 2)
        self.assertEqual(results['Passed'], 2)

    @mock.patch('acts.controllers.adb.AdbProxy',
                return_value=acts_android_device_test.MockAdbProxy(
                    1, return_value=''))
    @mock.patch('acts.controllers.fastboot.FastbootProxy',
                return_value=acts_android_device_test.MockFastbootProxy(1))
    @mock.patch('acts.controllers.android_device.list_adb_devices',
                return_value=['1'])
    @mock.patch('acts.controllers.android_device.get_all_instances',
                return_value=acts_android_device_test.get_mock_ads(1))
    @mock.patch(
        'acts.controllers.android_device.AndroidDevice.ensure_screen_on',
        return_value=True)
    @mock.patch(
        'acts.controllers.android_device.AndroidDevice.exit_setup_wizard',
        return_value=True)
    def test_run_two_test_classes(self, *_):
        """Verifies that running more than one test class in one test run works
        properly.

        This requires using a built-in controller module. Using AndroidDevice
        module since it has all the mocks needed already.
        """
        mock_test_config = self.base_mock_test_config.copy()
        tb_key = keys.Config.key_testbed.value
        mock_ctrlr_config_name = mock_controller.ACTS_CONTROLLER_CONFIG_NAME
        my_config = [{
            'serial': 'xxxx',
            'magic': 'Magic1'
        }, {
            'serial': 'xxxx',
            'magic': 'Magic2'
        }]
        mock_test_config.controller_configs[mock_ctrlr_config_name] = my_config
        mock_test_config.controller_configs['AndroidDevice'] = [{
            'serial':
            '1',
            'skip_sl4a':
            True
        }]
        tr = test_runner.TestRunner(mock_test_config,
                                    [('IntegrationTest', None),
                                     ('IntegrationTest', None)])
        tr.run()
        tr.stop()
        results = tr.results.summary_dict()
        self.assertEqual(results['Requested'], 2)
        self.assertEqual(results['Executed'], 2)
        self.assertEqual(results['Passed'], 2)


if __name__ == '__main__':
    unittest.main()
