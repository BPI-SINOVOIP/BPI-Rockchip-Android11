#!/usr/bin/env python3
#
# Copyright 2016 - The Android Open Source Project
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
"""This module is where all the record definitions and record containers live.
"""

import collections
import copy
import io
import json

from acts import logger
from acts.libs import yaml_writer

from mobly.records import ExceptionRecord
from mobly.records import OUTPUT_FILE_SUMMARY
from mobly.records import TestResultEnums as MoblyTestResultEnums
from mobly.records import TestResultRecord as MoblyTestResultRecord
from mobly.records import TestResult as MoblyTestResult
from mobly.records import TestSummaryEntryType
from mobly.records import TestSummaryWriter as MoblyTestSummaryWriter


class TestSummaryWriter(MoblyTestSummaryWriter):
    """Writes test results to a summary file in real time. Inherits from Mobly's
    TestSummaryWriter.
    """

    def dump(self, content, entry_type):
        """Update Mobly's implementation of dump to work on OrderedDict.

        See MoblyTestSummaryWriter.dump for documentation.
        """
        new_content = collections.OrderedDict(copy.deepcopy(content))
        new_content['Type'] = entry_type.value
        new_content.move_to_end('Type', last=False)
        # Both user code and Mobly code can trigger this dump, hence the lock.
        with self._lock:
            # For Python3, setting the encoding on yaml.safe_dump does not work
            # because Python3 file descriptors set an encoding by default, which
            # PyYAML uses instead of the encoding on yaml.safe_dump. So, the
            # encoding has to be set on the open call instead.
            with io.open(self._path, 'a', encoding='utf-8') as f:
                # Use safe_dump here to avoid language-specific tags in final
                # output.
                yaml_writer.safe_dump(new_content, f)


class TestResultEnums(MoblyTestResultEnums):
    """Enums used for TestResultRecord class. Inherits from Mobly's
    TestResultEnums.

    Includes the tokens to mark test result with, and the string names for each
    field in TestResultRecord.
    """

    RECORD_LOG_BEGIN_TIME = "Log Begin Time"
    RECORD_LOG_END_TIME = "Log End Time"


class TestResultRecord(MoblyTestResultRecord):
    """A record that holds the information of a test case execution. This class
    inherits from Mobly's TestResultRecord class.

    Attributes:
        test_name: A string representing the name of the test case.
        begin_time: Epoch timestamp of when the test case started.
        end_time: Epoch timestamp of when the test case ended.
        self.uid: Unique identifier of a test case.
        self.result: Test result, PASS/FAIL/SKIP.
        self.extras: User defined extra information of the test result.
        self.details: A string explaining the details of the test case.
    """

    def __init__(self, t_name, t_class=None):
        super().__init__(t_name, t_class)
        self.log_begin_time = None
        self.log_end_time = None

    def test_begin(self):
        """Call this when the test case it records begins execution.

        Sets the begin_time of this record.
        """
        super().test_begin()
        self.log_begin_time = logger.epoch_to_log_line_timestamp(
            self.begin_time)

    def _test_end(self, result, e):
        """Class internal function to signal the end of a test case execution.

        Args:
            result: One of the TEST_RESULT enums in TestResultEnums.
            e: A test termination signal (usually an exception object). It can
                be any exception instance or of any subclass of
                acts.signals.TestSignal.
        """
        super()._test_end(result, e)
        if self.end_time:
            self.log_end_time = logger.epoch_to_log_line_timestamp(
                self.end_time)

    def to_dict(self):
        """Gets a dictionary representing the content of this class.

        Returns:
            A dictionary representing the content of this class.
        """
        d = collections.OrderedDict()
        d[TestResultEnums.RECORD_NAME] = self.test_name
        d[TestResultEnums.RECORD_CLASS] = self.test_class
        d[TestResultEnums.RECORD_BEGIN_TIME] = self.begin_time
        d[TestResultEnums.RECORD_END_TIME] = self.end_time
        d[TestResultEnums.RECORD_LOG_BEGIN_TIME] = self.log_begin_time
        d[TestResultEnums.RECORD_LOG_END_TIME] = self.log_end_time
        d[TestResultEnums.RECORD_RESULT] = self.result
        d[TestResultEnums.RECORD_UID] = self.uid
        d[TestResultEnums.RECORD_EXTRAS] = self.extras
        d[TestResultEnums.RECORD_DETAILS] = self.details
        d[TestResultEnums.RECORD_EXTRA_ERRORS] = {
            key: value.to_dict()
            for (key, value) in self.extra_errors.items()
        }
        d[TestResultEnums.RECORD_STACKTRACE] = self.stacktrace
        return d

    def json_str(self):
        """Converts this test record to a string in json format.

        Format of the json string is:
            {
                'Test Name': <test name>,
                'Begin Time': <epoch timestamp>,
                'Details': <details>,
                ...
            }

        Returns:
            A json-format string representing the test record.
        """
        return json.dumps(self.to_dict())


class TestResult(MoblyTestResult):
    """A class that contains metrics of a test run. This class inherits from
    Mobly's TestResult class.

    This class is essentially a container of TestResultRecord objects.

    Attributes:
        self.requested: A list of strings, each is the name of a test requested
            by user.
        self.failed: A list of records for tests failed.
        self.executed: A list of records for tests that were actually executed.
        self.passed: A list of records for tests passed.
        self.skipped: A list of records for tests skipped.
    """

    def __add__(self, r):
        """Overrides '+' operator for TestResult class.

        The add operator merges two TestResult objects by concatenating all of
        their lists together.

        Args:
            r: another instance of TestResult to be added

        Returns:
            A TestResult instance that's the sum of two TestResult instances.
        """
        if not isinstance(r, MoblyTestResult):
            raise TypeError("Operand %s of type %s is not a TestResult." %
                            (r, type(r)))
        sum_result = TestResult()
        for name in sum_result.__dict__:
            r_value = getattr(r, name)
            l_value = getattr(self, name)
            if isinstance(r_value, list):
                setattr(sum_result, name, l_value + r_value)
        return sum_result

    def json_str(self):
        """Converts this test result to a string in json format.

        Format of the json string is:
            {
                "Results": [
                    {<executed test record 1>},
                    {<executed test record 2>},
                    ...
                ],
                "Summary": <summary dict>
            }

        Returns:
            A json-format string representing the test results.
        """
        d = collections.OrderedDict()
        d["ControllerInfo"] = {record.controller_name: record.controller_info
                               for record in self.controller_info}
        d["Results"] = [record.to_dict() for record in self.executed]
        d["Summary"] = self.summary_dict()
        d["Error"] = self.errors_list()
        json_str = json.dumps(d, indent=4)
        return json_str

    def summary_str(self):
        """Gets a string that summarizes the stats of this test result.

        The summary provides the counts of how many test cases fall into each
        category, like "Passed", "Failed" etc.

        Format of the string is:
            Requested <int>, Executed <int>, ...

        Returns:
            A summary string of this test result.
        """
        l = ["%s %s" % (k, v) for k, v in self.summary_dict().items()]
        msg = ", ".join(l)
        return msg

    def errors_list(self):
        l = list()
        for record in self.error:
            if isinstance(record, TestResultRecord):
                keys = [TestResultEnums.RECORD_NAME,
                        TestResultEnums.RECORD_DETAILS,
                        TestResultEnums.RECORD_EXTRA_ERRORS]
            elif isinstance(record, ExceptionRecord):
                keys = [TestResultEnums.RECORD_DETAILS,
                        TestResultEnums.RECORD_POSITION]
            else:
                return []
            l.append({k: record.to_dict()[k] for k in keys})
        return l
