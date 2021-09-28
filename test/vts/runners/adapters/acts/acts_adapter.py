#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import importlib
import json
import logging
import os
import subprocess
import sys
import time

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import config_parser
from vts.runners.host import records
from vts.runners.host import test_runner
from vts.utils.python.io import capture_printout
from vts.utils.python.io import file_util

ACTS_TEST_MODULE = 'ACTS_TEST_MODULE'
LIST_TEST_OUTPUT_START = '==========> '
LIST_TEST_OUTPUT_END = ' <=========='
# Temp directory inside python log path. The name is required to be
# 'temp' for the Java framework to skip reading contents as regular test logs.
TEMP_DIR_NAME = 'temp'
CONFIG_FILE_NAME = 'acts_config.txt'
RESULT_FILE_NAME = 'test_run_summary.json'

CONFIG_TEXT = '''{{
    "_description": "VTS acts tests",
    "testbed":
    [
        {{
            "_description": "ACTS test bed",
            "name": "{module_name}",
            "AndroidDevice":
            [
              {serials}
            ]
        }}
    ],
    "logpath": "{log_path}",
    "testpaths":
    [
        "{src_path}"
    ]
}}
'''


class ActsAdapter(base_test.BaseTestClass):
    '''Template class for running acts test cases.

    Attributes:
        test_type: string, name of test type this adapter is for
        result_path: string, test result directory for the adaptor
        config_path: string, test config file path
        module_name: string, ACTS module name
        test_path: string, ACTS module source directory
    '''
    test_type = 'ACTS'

    def setUpClass(self):
        '''Set up result directory, generate configuration file, and list tests.'''
        self.result_path = os.path.join(logging.log_path, TEMP_DIR_NAME,
                                        self.test_type, str(time.time()))
        file_util.Makedirs(self.result_path)
        logging.debug('Result path for %s: %s' % (self.test_type,
                                                 self.result_path))
        self.test_path, self.module_name = self.getUserParam(
            ACTS_TEST_MODULE).rsplit('/', 1)

        self.config_path = os.path.join(self.result_path, CONFIG_FILE_NAME)
        self.GenerateConfigFile()

        testcases = self.ListTestCases()
        logging.debug('ACTS Test cases: %s', testcases)

    def tearDownClass(self):
        '''Clear the result path.'''
        file_util.Rmdirs(self.result_path, ignore_errors=True)

    def GenerateConfigFile(self):
        '''Generate test configuration file.'''
        serials = []
        for ad in self.android_devices:
            serials.append('{"serial":"%s"}' % ad.serial)

        config_text = CONFIG_TEXT.format(
            module_name=self.module_name,
            serials=','.join(serials),
            log_path=self.result_path,
            src_path=self.test_path)

        with open(self.config_path, 'w') as f:
            f.write(config_text)

    def ListTestCases(self):
        '''List test cases.

        Returns:
            List of string, test names.
        '''
        # TODO use ACTS runner to list test cases and add requested record.
        # This step is optional but desired. To be implemented later

    def Run(self):
        '''Execute test cases.'''
        # acts.py is installed to user bin by ACTS setup script.
        # In the future, it is preferred to use the source code
        # from repo directory.
        bin = 'acts/bin/act.py'

        cmd = '{bin} -c {config} -tb {module_name} -tc {module_name}'.format(
            bin=bin, config=self.config_path, module_name=self.module_name)
        logging.debug('cmd is: %s', cmd)

        # Calling through subprocess is required because ACTS requires python3
        # while VTS is currently using python2. In the future, ACTS runner
        # can be invoked through importing when VTS upgrades to python3.

        # A "hack" to call python3 outside of python2 virtualenv created by
        # VTS framework
        environ = {
            key: val
            for key, val in os.environ.iteritems() if 'virtualenv' not in val
        }

        # TODO(yuexima): disable buffer
        p = subprocess.Popen(
            cmd,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=environ)

        for line in iter(p.stdout.readline, b''):
            print line.rstrip()

        p.communicate()
        if p.returncode:
            asserts.fail('Subprocess of ACTS command failed. Return code: %s' %
                         p.returncode)

    def ParseResults(self):
        '''Get module run results and put in vts results.'''
        file_path = file_util.FindFile(self.result_path, RESULT_FILE_NAME)

        if file_path:
            logging.debug('ACTS test result path: %s', file_path)
            self.ParseJsonResults(file_path)
        else:
            logging.error('Cannot find result file name %s in %s',
                          RESULT_FILE_NAME, self.result_path)

    def generateAllTests(self):
        '''Run the test module and parse results.'''
        self.Run()
        self.ParseResults()

    def ParseJsonResults(self, result_path):
        '''Parse test json result.

        Args:
            result_path: string, result json file path.
        '''
        with open(result_path, 'r') as f:
            summary = json.load(f)

        results = summary['Results']
        for result in results:
            logging.debug('Adding result for %s' %
                         result[records.TestResultEnums.RECORD_NAME])
            record = records.TestResultRecord(
                result[records.TestResultEnums.RECORD_NAME])
            record.test_class = result[records.TestResultEnums.RECORD_CLASS]
            record.begin_time = result[
                records.TestResultEnums.RECORD_BEGIN_TIME]
            record.end_time = result[records.TestResultEnums.RECORD_END_TIME]
            record.result = result[records.TestResultEnums.RECORD_RESULT]
            record.uid = result[records.TestResultEnums.RECORD_UID]
            record.extras = result[records.TestResultEnums.RECORD_EXTRAS]
            record.details = result[records.TestResultEnums.RECORD_DETAILS]
            record.extra_errors = result[
                records.TestResultEnums.RECORD_EXTRA_ERRORS]

            self.results.addRecord(record)

        # TODO(yuexima): parse new result types

if __name__ == "__main__":
    test_runner.main()
