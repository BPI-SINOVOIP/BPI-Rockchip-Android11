#!/usr/bin/env python
#
# Copyright 2018, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Unittests for robolectric_test_runner."""

import json
import unittest
import subprocess
import tempfile
import mock

import event_handler
# pylint: disable=import-error
from test_finders import test_info
from test_runners import robolectric_test_runner

# pylint: disable=protected-access
class RobolectricTestRunnerUnittests(unittest.TestCase):
    """Unit tests for robolectric_test_runner.py"""

    def setUp(self):
        self.polling_time = robolectric_test_runner.POLL_FREQ_SECS
        self.suite_tr = robolectric_test_runner.RobolectricTestRunner(results_dir='')

    def tearDown(self):
        mock.patch.stopall()

    @mock.patch.object(robolectric_test_runner.RobolectricTestRunner, 'run')
    def test_run_tests_raw(self, mock_run):
        """Test run_tests_raw method."""
        test_infos = [test_info.TestInfo("Robo1",
                                         "RobolectricTestRunner",
                                         ["RoboTest"])]
        extra_args = []
        mock_subproc = mock.Mock()
        mock_run.return_value = mock_subproc
        mock_subproc.returncode = 0
        mock_reporter = mock.Mock()
        # Test Build Pass
        self.assertEqual(
            0,
            self.suite_tr.run_tests_raw(test_infos, extra_args, mock_reporter))
        # Test Build Fail
        mock_subproc.returncode = 1
        self.assertNotEqual(
            0,
            self.suite_tr.run_tests_raw(test_infos, extra_args, mock_reporter))

    @mock.patch.object(event_handler.EventHandler, 'process_event')
    def test_exec_with_robo_polling_complete_information(self, mock_pe):
        """Test _exec_with_robo_polling method."""
        event_name = 'TEST_STARTED'
        event_data = {'className':'SomeClass', 'testName':'SomeTestName'}

        json_event_data = json.dumps(event_data)
        data = '%s %s\n\n' %(event_name, json_event_data)
        event_file = tempfile.NamedTemporaryFile(mode='w+r', delete=True)
        subprocess.call("echo '%s' -n >> %s" %(data, event_file.name), shell=True)
        robo_proc = subprocess.Popen("sleep %s" %str(self.polling_time * 2), shell=True)
        self.suite_tr. _exec_with_robo_polling(event_file, robo_proc, mock_pe)
        calls = [mock.call.process_event(event_name, event_data)]
        mock_pe.assert_has_calls(calls)

    @mock.patch.object(event_handler.EventHandler, 'process_event')
    def test_exec_with_robo_polling_with_partial_info(self, mock_pe):
        """Test _exec_with_robo_polling method."""
        event_name = 'TEST_STARTED'
        event1 = '{"className":"SomeClass","test'
        event2 = 'Name":"SomeTestName"}\n\n'
        data1 = '%s %s'%(event_name, event1)
        data2 = event2
        event_file = tempfile.NamedTemporaryFile(mode='w+r', delete=True)
        subprocess.Popen("echo -n '%s' >> %s" %(data1, event_file.name), shell=True)
        robo_proc = subprocess.Popen("echo '%s' >> %s && sleep %s"
                                     %(data2,
                                       event_file.name,
                                       str(self.polling_time*5)),
                                     shell=True)
        self.suite_tr. _exec_with_robo_polling(event_file, robo_proc, mock_pe)
        calls = [mock.call.process_event(event_name,
                                         json.loads(event1 + event2))]
        mock_pe.assert_has_calls(calls)

    @mock.patch.object(event_handler.EventHandler, 'process_event')
    def test_exec_with_robo_polling_with_fail_stacktrace(self, mock_pe):
        """Test _exec_with_robo_polling method."""
        event_name = 'TEST_FAILED'
        event_data = {'className':'SomeClass', 'testName':'SomeTestName',
                      'trace':'{"trace":"AssertionError: <true> is equal to <false>\n'
                              'at FailureStrategy.fail(FailureStrategy.java:24)\n'
                              'at FailureStrategy.fail(FailureStrategy.java:20)\n'}
        data = '%s %s\n\n'%(event_name, json.dumps(event_data))
        event_file = tempfile.NamedTemporaryFile(mode='w+r', delete=True)
        subprocess.call("echo '%s' -n >> %s" %(data, event_file.name), shell=True)
        robo_proc = subprocess.Popen("sleep %s" %str(self.polling_time * 2), shell=True)
        self.suite_tr. _exec_with_robo_polling(event_file, robo_proc, mock_pe)
        calls = [mock.call.process_event(event_name, event_data)]
        mock_pe.assert_has_calls(calls)

    @mock.patch.object(event_handler.EventHandler, 'process_event')
    def test_exec_with_robo_polling_with_multi_event(self, mock_pe):
        """Test _exec_with_robo_polling method."""
        event_file = tempfile.NamedTemporaryFile(mode='w+r', delete=True)
        events = [
            ('TEST_MODULE_STARTED', {
                'moduleContextFileName':'serial-util1146216{974}2772610436.ser',
                'moduleName':'someTestModule'}),
            ('TEST_RUN_STARTED', {'testCount': 2}),
            ('TEST_STARTED', {'start_time':52, 'className':'someClassName',
                              'testName':'someTestName'}),
            ('TEST_ENDED', {'end_time':1048, 'className':'someClassName',
                            'testName':'someTestName'}),
            ('TEST_STARTED', {'start_time':48, 'className':'someClassName2',
                              'testName':'someTestName2'}),
            ('TEST_FAILED', {'className':'someClassName2', 'testName':'someTestName2',
                             'trace': 'someTrace'}),
            ('TEST_ENDED', {'end_time':9876450, 'className':'someClassName2',
                            'testName':'someTestName2'}),
            ('TEST_RUN_ENDED', {}),
            ('TEST_MODULE_ENDED', {'foo': 'bar'}),]
        data = ''
        for event in events:
            data += '%s %s\n\n'%(event[0], json.dumps(event[1]))

        subprocess.call("echo '%s' -n >> %s" %(data, event_file.name), shell=True)
        robo_proc = subprocess.Popen("sleep %s" %str(self.polling_time * 2), shell=True)
        self.suite_tr. _exec_with_robo_polling(event_file, robo_proc, mock_pe)
        calls = [mock.call.process_event(name, data) for name, data in events]
        mock_pe.assert_has_calls(calls)

if __name__ == '__main__':
    unittest.main()
