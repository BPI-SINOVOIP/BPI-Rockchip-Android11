#!/usr/bin/env python3
#
#   Copyright 2017 - The Android Open Source Project
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
import shutil
import tempfile
import unittest

from mobly.config_parser import TestRunConfig
from mock import Mock
from mock import patch

from acts import test_runner


class TestRunnerTest(unittest.TestCase):
    def setUp(self):
        self.tmp_dir = tempfile.mkdtemp()
        self.base_mock_test_config = TestRunConfig()
        self.base_mock_test_config.testbed_name = 'SampleTestBed'
        self.base_mock_test_config.log_path = self.tmp_dir
        self.base_mock_test_config.controller_configs = {'testpaths': ['./']}
        self.base_mock_test_config.user_params = {
            'icecream': 42,
            'extra_param': 'haha'
        }

    def tearDown(self):
        shutil.rmtree(self.tmp_dir)

    @staticmethod
    def create_test_classes(class_names):
        return {class_name: Mock() for class_name in class_names}

    @patch('acts.records.TestResult')
    @patch.object(test_runner.TestRunner, '_write_results_to_file')
    def test_class_name_pattern_single(self, *_):
        class_names = ['test_class_1', 'test_class_2']
        pattern = 'test*1'
        tr = test_runner.TestRunner(self.base_mock_test_config,
                                    [(pattern, None)])

        test_classes = self.create_test_classes(class_names)
        tr.import_test_modules = Mock(return_value=test_classes)
        tr.run()
        self.assertTrue(test_classes[class_names[0]].called)
        self.assertFalse(test_classes[class_names[1]].called)

    @patch('acts.records.TestResult')
    @patch.object(test_runner.TestRunner, '_write_results_to_file')
    def test_class_name_pattern_multi(self, *_):
        class_names = ['test_class_1', 'test_class_2', 'other_name']
        pattern = 'test_class*'
        tr = test_runner.TestRunner(self.base_mock_test_config,
                                    [(pattern, None)])

        test_classes = self.create_test_classes(class_names)
        tr.import_test_modules = Mock(return_value=test_classes)
        tr.run()
        self.assertTrue(test_classes[class_names[0]].called)
        self.assertTrue(test_classes[class_names[1]].called)
        self.assertFalse(test_classes[class_names[2]].called)

    @patch('acts.records.TestResult')
    @patch.object(test_runner.TestRunner, '_write_results_to_file')
    def test_class_name_pattern_question_mark(self, *_):
        class_names = ['test_class_1', 'test_class_12']
        pattern = 'test_class_?'
        tr = test_runner.TestRunner(self.base_mock_test_config,
                                    [(pattern, None)])

        test_classes = self.create_test_classes(class_names)
        tr.import_test_modules = Mock(return_value=test_classes)
        tr.run()
        self.assertTrue(test_classes[class_names[0]].called)
        self.assertFalse(test_classes[class_names[1]].called)

    @patch('acts.records.TestResult')
    @patch.object(test_runner.TestRunner, '_write_results_to_file')
    def test_class_name_pattern_char_seq(self, *_):
        class_names = ['test_class_1', 'test_class_2', 'test_class_3']
        pattern = 'test_class_[1357]'
        tr = test_runner.TestRunner(self.base_mock_test_config,
                                    [(pattern, None)])

        test_classes = self.create_test_classes(class_names)
        tr.import_test_modules = Mock(return_value=test_classes)
        tr.run()
        self.assertTrue(test_classes[class_names[0]].called)
        self.assertFalse(test_classes[class_names[1]].called)
        self.assertTrue(test_classes[class_names[2]].called)

    @patch('acts.records.TestResult')
    @patch.object(test_runner.TestRunner, 'dump_config')
    @patch.object(test_runner.TestRunner, '_write_results_to_file')
    @patch('acts.test_runner.logger')
    def test_class_logpath_contains_proper_directory(self, logger_mock, *_):
        expected_timestamp = '1970-01-01_00-00-00-00-000000'
        logger_mock.get_log_file_timestamp.return_value = expected_timestamp
        tr = test_runner.TestRunner(self.base_mock_test_config,
                                    [('MockTest', None)])
        mock_class = Mock()
        tr.import_test_modules = Mock(return_value={'MockTest': mock_class})
        tr.run()

        self.assertEqual(
            mock_class.call_args_list[0][0][0].log_path,
            os.path.join(self.tmp_dir, self.base_mock_test_config.testbed_name,
                         expected_timestamp))


if __name__ == '__main__':
    unittest.main()
