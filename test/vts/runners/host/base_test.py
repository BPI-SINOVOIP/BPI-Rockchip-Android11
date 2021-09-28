#
# Copyright (C) 2016 The Android Open Source Project
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

import logging
import os
import re
import signal
import sys
import threading
import time

from vts.proto import VtsReportMessage_pb2 as ReportMsg
from vts.runners.host import asserts
from vts.runners.host import const
from vts.runners.host import errors
from vts.runners.host import keys
from vts.runners.host import logger
from vts.runners.host import records
from vts.runners.host import signals
from vts.runners.host import utils
from vts.utils.python.controllers import adb
from vts.utils.python.controllers import android_device
from vts.utils.python.controllers.adb import AdbError
from vts.utils.python.common import cmd_utils
from vts.utils.python.common import filter_utils
from vts.utils.python.common import list_utils
from vts.utils.python.common import timeout_utils
from vts.utils.python.coverage import coverage_utils
from vts.utils.python.coverage import sancov_utils
from vts.utils.python.instrumentation import test_framework_instrumentation as tfi
from vts.utils.python.io import file_util
from vts.utils.python.precondition import precondition_utils
from vts.utils.python.profiling import profiling_utils
from vts.utils.python.reporting import log_uploading_utils
from vts.utils.python.systrace import systrace_utils
from vts.utils.python.web import feature_utils
from vts.utils.python.web import web_utils


# Macro strings for test result reporting
TEST_CASE_TEMPLATE = "[Test Case] %s %s"
RESULT_LINE_TEMPLATE = TEST_CASE_TEMPLATE + " %s"
STR_TEST = "test"
STR_GENERATE = "generate"
TIMEOUT_SECS_LOG_UPLOADING = 60
TIMEOUT_SECS_TEARDOWN_CLASS = 120
_REPORT_MESSAGE_FILE_NAME = "report_proto.msg"
_BUG_REPORT_FILE_PREFIX = "bugreport_"
_BUG_REPORT_FILE_EXTENSION = ".zip"
_DEFAULT_TEST_TIMEOUT_SECS = 60 * 3
_LOGCAT_FILE_PREFIX = "logcat_"
_LOGCAT_FILE_EXTENSION = ".txt"
_ANDROID_DEVICES = '_android_devices'
_REASON_TO_SKIP_ALL_TESTS = '_reason_to_skip_all_tests'
_SETUP_RETRY_NUMBER = 5
# the name of a system property which tells whether to stop properly configured
# native servers where properly configured means a server's init.rc is
# configured to stop when that property's value is 1.
SYSPROP_VTS_NATIVE_SERVER = "vts.native_server.on"

LOGCAT_BUFFERS = [
    'radio',
    'events',
    'main',
    'system',
    'crash'
]


class BaseTestClass(object):
    """Base class for all test classes to inherit from.

    This class gets all the controller objects from test_runner and executes
    the test cases requested within itself.

    Most attributes of this class are set at runtime based on the configuration
    provided.

    Attributes:
        android_devices: A list of AndroidDevice object, representing android
                         devices.
        test_module_name: A string representing the test module name.
        tests: A list of strings, each representing a test case name.
        log: A logger object used for logging.
        results: A records.TestResult object for aggregating test results from
                 the execution of test cases.
        _current_record: A records.TestResultRecord object for the test case
                         currently being executed. If no test is running, this
                         should be None.
        _interrupted: Whether the test execution has been interrupted.
        _interrupt_lock: The threading.Lock object that protects _interrupted.
        _timer: The threading.Timer object that interrupts main thread when
                timeout.
        timeout: A float, the timeout, in seconds, configured for this object.
        include_filer: A list of string, each representing a test case name to
                       include.
        exclude_filer: A list of string, each representing a test case name to
                       exclude. Has no effect if include_filer is not empty.
        abi_name: String, name of abi in use
        abi_bitness: String, bitness of abi in use
        web: WebFeature, object storing web feature util for test run
        coverage: CoverageFeature, object storing coverage feature util for test run
        sancov: SancovFeature, object storing sancov feature util for test run
        start_time_sec: int, time value in seconds when the module execution starts.
        start_vts_agents: whether to start vts agents when registering new
                          android devices.
        profiling: ProfilingFeature, object storing profiling feature util for test run
        _bug_report_on_failure: bool, whether to catch bug report at the end
                                of failed test cases. Default is False
        _logcat_on_failure: bool, whether to dump logcat at the end
                                of failed test cases. Default is True
        _is_final_run: bool, whether the current test run is the final run during retry.
        test_filter: Filter object to filter test names.
        _test_filter_retry: Filter object for retry filtering.
        max_retry_count: int, max number of retries.
    """
    _current_record = None
    start_vts_agents = True

    def __init__(self, configs):
        self.start_time_sec = time.time()
        self.tests = []
        # Set all the controller objects and params.
        for name, value in configs.items():
            setattr(self, name, value)
        self.results = records.TestResult()
        self.log = logger.LoggerProxy()

        # Timeout
        self._interrupted = False
        self._interrupt_lock = threading.Lock()
        self._timer = None

        timeout_milli = self.getUserParam(keys.ConfigKeys.KEY_TEST_TIMEOUT, 0)
        self.timeout = timeout_milli / 1000 if timeout_milli > 0 else _DEFAULT_TEST_TIMEOUT_SECS

        # Setup test filters
        # TODO(yuexima): remove include_filter and exclude_filter from class attributes
        # after confirming all modules no longer have reference to them
        self.include_filter = self.getUserParam(
            [
                keys.ConfigKeys.KEY_TEST_SUITE,
                keys.ConfigKeys.KEY_INCLUDE_FILTER
            ],
            default_value=[])
        self.exclude_filter = self.getUserParam(
            [
                keys.ConfigKeys.KEY_TEST_SUITE,
                keys.ConfigKeys.KEY_EXCLUDE_FILTER
            ],
            default_value=[])

        self.test_module_name = self.getUserParam(
            keys.ConfigKeys.KEY_TESTBED_NAME,
            warn_if_not_found=True,
            default_value=self.__class__.__name__)

        self.updateTestFilter()

        logging.debug('Test filter: %s' % self.test_filter)

        # TODO: get abi information differently for multi-device support.
        # Set other optional parameters
        self.abi_name = self.getUserParam(
            keys.ConfigKeys.IKEY_ABI_NAME, default_value=None)
        self.abi_bitness = self.getUserParam(
            keys.ConfigKeys.IKEY_ABI_BITNESS, default_value=None)
        self.skip_on_32bit_abi = self.getUserParam(
            keys.ConfigKeys.IKEY_SKIP_ON_32BIT_ABI, default_value=False)
        self.skip_on_64bit_abi = self.getUserParam(
            keys.ConfigKeys.IKEY_SKIP_ON_64BIT_ABI, default_value=False)
        self.run_32bit_on_64bit_abi = self.getUserParam(
            keys.ConfigKeys.IKEY_RUN_32BIT_ON_64BIT_ABI, default_value=False)
        self.max_retry_count = self.getUserParam(
            keys.ConfigKeys.IKEY_MAX_RETRY_COUNT, default_value=0)

        self.web = web_utils.WebFeature(self.user_params)
        self.coverage = coverage_utils.CoverageFeature(
            self.user_params, web=self.web)
        self.sancov = sancov_utils.SancovFeature(
            self.user_params, web=self.web)
        self.profiling = profiling_utils.ProfilingFeature(
            self.user_params, web=self.web)
        self.systrace = systrace_utils.SystraceFeature(
            self.user_params, web=self.web)
        self.log_uploading = log_uploading_utils.LogUploadingFeature(
            self.user_params, web=self.web)
        self.collect_tests_only = self.getUserParam(
            keys.ConfigKeys.IKEY_COLLECT_TESTS_ONLY, default_value=False)
        self.run_as_vts_self_test = self.getUserParam(
            keys.ConfigKeys.RUN_AS_VTS_SELFTEST, default_value=False)
        self.run_as_compliance_test = self.getUserParam(
            keys.ConfigKeys.RUN_AS_COMPLIANCE_TEST, default_value=False)
        self._bug_report_on_failure = self.getUserParam(
            keys.ConfigKeys.IKEY_BUG_REPORT_ON_FAILURE, default_value=False)
        self._logcat_on_failure = self.getUserParam(
            keys.ConfigKeys.IKEY_LOGCAT_ON_FAILURE, default_value=True)
        self._test_filter_retry = None

    @property
    def android_devices(self):
        """Returns a list of AndroidDevice objects"""
        if not hasattr(self, _ANDROID_DEVICES):
            event = tfi.Begin('base_test registering android_device. '
                              'Start agents: %s' % self.start_vts_agents,
                              tfi.categories.FRAMEWORK_SETUP)
            setattr(self, _ANDROID_DEVICES,
                    self.registerController(android_device,
                                            start_services=self.start_vts_agents))
            event.End()

            for device in getattr(self, _ANDROID_DEVICES):
                device.shell_default_nohup = self.getUserParam(
                    keys.ConfigKeys.SHELL_DEFAULT_NOHUP, default_value=True)
        return getattr(self, _ANDROID_DEVICES)

    @android_devices.setter
    def android_devices(self, devices):
        """Set the list of AndroidDevice objects"""
        setattr(self, _ANDROID_DEVICES, devices)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self._exec_func(self.cleanUp)

    def updateTestFilter(self):
        """Updates test filter using include and exclude filters."""
        self.include_filter = list_utils.ExpandItemDelimiters(
            list_utils.ItemsToStr(self.include_filter), ',')
        self.exclude_filter = list_utils.ExpandItemDelimiters(
            list_utils.ItemsToStr(self.exclude_filter), ',')

        exclude_over_include = self.getUserParam(
            keys.ConfigKeys.KEY_EXCLUDE_OVER_INCLUDE, default_value=None)

        self.test_filter = filter_utils.Filter(
            self.include_filter,
            self.exclude_filter,
            enable_regex=True,
            exclude_over_include=exclude_over_include,
            enable_negative_pattern=True,
            enable_module_name_prefix_matching=True,
            module_name=self.test_module_name,
            expand_bitness=True)

    def unpack_userparams(self, req_param_names=[], opt_param_names=[], **kwargs):
        """Wrapper for test cases using ACTS runner API."""
        return self.getUserParams(req_param_names, opt_param_names, **kwargs)

    def getUserParams(self, req_param_names=[], opt_param_names=[], **kwargs):
        """Unpacks user defined parameters in test config into individual
        variables.

        Instead of accessing the user param with self.user_params["xxx"], the
        variable can be directly accessed with self.xxx.

        A missing required param will raise an exception. If an optional param
        is missing, an INFO line will be logged.

        Args:
            req_param_names: A list of names of the required user params.
            opt_param_names: A list of names of the optional user params.
            **kwargs: Arguments that provide default values.
                e.g. getUserParams(required_list, opt_list, arg_a="hello")
                     self.arg_a will be "hello" unless it is specified again in
                     required_list or opt_list.

        Raises:
            BaseTestError is raised if a required user params is missing from
            test config.
        """
        for k, v in kwargs.items():
            setattr(self, k, v)
        for name in req_param_names:
            if name not in self.user_params:
                raise errors.BaseTestError(("Missing required user param '%s' "
                                            "in test configuration.") % name)
            setattr(self, name, self.user_params[name])
        for name in opt_param_names:
            if name not in self.user_params:
                logging.debug(("Missing optional user param '%s' in "
                              "configuration, continue."), name)
            else:
                setattr(self, name, self.user_params[name])

    def getUserParam(self,
                     param_name,
                     error_if_not_found=False,
                     warn_if_not_found=False,
                     default_value=None,
                     to_str=False):
        """Get the value of a single user parameter.

        This method returns the value of specified user parameter.

        Note: unlike getUserParams(), this method will not automatically set
              attribute using the parameter name and value.

        Args:
            param_name: string or list of string, denoting user parameter names. If provided
                        a single string, self.user_params["<param_name>"] will be accessed.
                        If provided multiple strings,
                        self.user_params["<param_name1>"]["<param_name2>"]["<param_name3>"]...
                        will be accessed.
            error_if_not_found: bool, whether to raise error if parameter not
                                exists. Default: False
            warn_if_not_found: bool, log a warning message if parameter value
                               not found. Default: False
            default_value: object, default value to return if not found.
                           If error_if_not_found is true, this parameter has no
                           effect. Default: None
            to_str: boolean, whether to convert the result object to string if
                    not None.
                    Note, strings passing in from java json config are often
                    unicode.

        Returns:
            object, value of the specified parameter name chain if exists;
            <default_value> otherwise.
        """

        def ToStr(return_value):
            """Check to_str option and convert to string if not None"""
            if to_str and return_value is not None:
                return str(return_value)
            return return_value

        if not param_name:
            if error_if_not_found:
                raise errors.BaseTestError("empty param_name provided")
            logging.error("empty param_name")
            return ToStr(default_value)

        if not isinstance(param_name, list):
            param_name = [param_name]

        curr_obj = self.user_params
        for param in param_name:
            if param not in curr_obj:
                msg = ("Missing user param '%s' in test configuration.\n"
                       "User params: %s") % (param_name, self.user_params)
                if error_if_not_found:
                    raise errors.BaseTestError(msg)
                elif warn_if_not_found:
                    logging.warn(msg)
                return ToStr(default_value)
            curr_obj = curr_obj[param]

        return ToStr(curr_obj)

    def _getUserConfig(self,
                       config_type,
                       key,
                       default_value=None,
                       error_if_not_found=False,
                       warn_if_not_found=False,
                       to_str=False):
        """Get the value of a user config given the key.

        This method returns the value of specified user config type.

        Args:
            config_type: string, type of user config
            key: string, key of the value string in string config map.
            default_value: object, default value to return if not found.
                           If error_if_not_found is true, this parameter has no
                           effect. Default: None
            error_if_not_found: bool, whether to raise error if parameter not
                                exists. Default: False
            warn_if_not_found: bool, log a warning message if parameter value
                               not found. Default: False
            to_str: boolean, whether to apply str() method to result value
                    if result is not None.
                    Note, strings passing in from java json config are ofen
                    unicode.

        Returns:
            Value in config matching the given key and type if exists;
            <default_value> otherwise.
        """
        dic = self.getUserParam(config_type,
                                error_if_not_found=False,
                                warn_if_not_found=False,
                                default_value=None,
                                to_str=False)

        if dic is None or key not in dic:
            msg = ("Config key %s not found in user config type %s.\n"
                   "User params: %s") % (key, config_type, self.user_params)
            if error_if_not_found:
                raise errors.BaseTestError(msg)
            elif warn_if_not_found:
                logging.warn(msg)

            return default_value

        return dic[key] if not to_str else str(dic[key])

    def getUserConfigStr(self, key, **kwargs):
        """Get the value of a user config string given the key.

        See _getUserConfig method for more details.
        """
        kwargs["to_str"] = True
        return self._getUserConfig(keys.ConfigKeys.IKEY_USER_CONFIG_STR,
                                   key,
                                   **kwargs)

    def getUserConfigInt(self, key, **kwargs):
        """Get the value of a user config int given the key.

        See _getUserConfig method for more details.
        """
        return self._getUserConfig(keys.ConfigKeys.IKEY_USER_CONFIG_INT,
                                   key,
                                   **kwargs)

    def getUserConfigBool(self, key, **kwargs):
        """Get the value of a user config bool given the key.

        See _getUserConfig method for more details.
        """
        return self._getUserConfig(keys.ConfigKeys.IKEY_USER_CONFIG_BOOL,
                                   key,
                                   **kwargs)

    def _setUpClass(self):
        """Proxy function to guarantee the base implementation of setUpClass
        is called.
        """
        event = tfi.Begin('_setUpClass method for test class',
                          tfi.categories.TEST_CLASS_SETUP)
        timeout = self.timeout - time.time() + self.start_time_sec
        if timeout < 0:
            timeout = 1
        self.resetTimeout(timeout)
        if not precondition_utils.MeetFirstApiLevelPrecondition(self):
            self.skipAllTests("The device's first API level doesn't meet the "
                              "precondition.")
        for device in self.android_devices:
            if not precondition_utils.CheckFeaturePrecondition(self, device):
                self.skipAllTests("Precondition feature check fail.")

        if (self.getUserParam(
                keys.ConfigKeys.IKEY_DISABLE_FRAMEWORK, default_value=False) or
                # @Deprecated Legacy configuration option name.
                self.getUserParam(
                    keys.ConfigKeys.IKEY_BINARY_TEST_DISABLE_FRAMEWORK,
                    default_value=False)):
            stop_native_server = (
                self.getUserParam(
                    keys.ConfigKeys.IKEY_STOP_NATIVE_SERVERS,
                    default_value=False) or
                # @Deprecated Legacy configuration option name.
                self.getUserParam(
                    keys.ConfigKeys.IKEY_BINARY_TEST_STOP_NATIVE_SERVERS,
                    default_value=False))
            # Disable the framework if requested.
            for device in self.android_devices:
                device.stop(stop_native_server)
        else:
            # Enable the framework if requested.
            for device in self.android_devices:
                device.start()

        # Wait for the native service process to stop.
        native_server_process_names = self.getUserParam(
                    keys.ConfigKeys.IKEY_NATIVE_SERVER_PROCESS_NAME,
                    default_value=[])
        for device in self.android_devices:
            device.waitForProcessStop(native_server_process_names)


        event_sub = tfi.Begin('setUpClass method from test script',
                              tfi.categories.TEST_CLASS_SETUP,
                              enable_logging=False)
        result = self.setUpClass()
        event_sub.End()
        event.End()
        return result

    def setUpClass(self):
        """Setup function that will be called before executing any test case in
        the test class.

        To signal setup failure, return False or raise an exception. If
        exceptions were raised, the stack trace would appear in log, but the
        exceptions would not propagate to upper levels.

        Implementation is optional.
        """
        pass

    def _tearDownClass(self):
        """Proxy function to guarantee the base implementation of tearDownClass
        is called.
        """
        event = tfi.Begin('_tearDownClass method for test class',
                          tfi.categories.TEST_CLASS_TEARDOWN)
        self.cancelTimeout()

        event_sub = tfi.Begin('tearDownClass method from test script',
                              tfi.categories.TEST_CLASS_TEARDOWN,
                              enable_logging=False)

        @timeout_utils.timeout(TIMEOUT_SECS_TEARDOWN_CLASS,
                               message='tearDownClass method timed out.',
                               no_exception=True)
        def _executeTearDownClass(baseTest):
            baseTest._exec_func(baseTest.tearDownClass)
        _executeTearDownClass(self)

        event_sub.End()

        if self.web.enabled:
            if self.results.class_errors:
                # Create a result to make the module shown as failure.
                self.web.AddTestReport("setup_class")
                self.web.SetTestResult(ReportMsg.TEST_CASE_RESULT_FAIL)

            # Attach log destination urls to proto message so the urls will be
            # recorded and uploaded to dashboard. The actual log uploading is postponed
            # after generating report message to prevent failure due to timeout of log uploading.
            self.log_uploading.UploadLogs(dryrun=True)

            message_b = self.web.GenerateReportMessage(self.results.requested,
                                                       self.results.executed)
        else:
            message_b = ''

        report_proto_path = os.path.join(logging.log_path,
                                         _REPORT_MESSAGE_FILE_NAME)

        if message_b:
            logging.debug('Result proto message path: %s', report_proto_path)

        with open(report_proto_path, "wb") as f:
            f.write(message_b)

        if self.log_uploading.enabled:
            @timeout_utils.timeout(TIMEOUT_SECS_LOG_UPLOADING,
                                   message='_tearDownClass method in base_test timed out.',
                                   no_exception=True)
            def _executeLogUpload(_log_uploading):
                _log_uploading.UploadLogs()

            event_upload = tfi.Begin('Log upload',
                                     tfi.categories.RESULT_PROCESSING)
            _executeLogUpload(self.log_uploading)
            event_upload.End()

        event.End()

    def tearDownClass(self):
        """Teardown function that will be called after all the selected test
        cases in the test class have been executed.

        Implementation is optional.
        """
        pass

    def interrupt(self):
        """Interrupts test execution and terminates process."""
        with self._interrupt_lock:
            if self._interrupted:
                logging.warning("Cannot interrupt more than once.")
                return
            self._interrupted = True
        logging.info("Test timed out, interrupt")
        utils.stop_current_process(TIMEOUT_SECS_TEARDOWN_CLASS)

    def cancelTimeout(self):
        """Cancels main thread timer."""
        if hasattr(signal, "SIGALRM"):
            signal.alarm(0)
        else:
            with self._interrupt_lock:
                if self._interrupted:
                    logging.warning("Test execution has been interrupted. "
                                    "Cannot cancel or reset timeout.")
                    return

            if self._timer:
                self._timer.cancel()

    def resetTimeout(self, timeout):
        """Restarts the timer that will interrupt the main thread.

        This class starts the timer before setUpClass. As the timeout depends
        on number of generated tests, the subclass can restart the timer.

        Args:
            timeout: A float, wait time in seconds before interrupt.
        """
        logging.debug("Start timer with timeout=%ssec.", timeout)
        if hasattr(signal, "SIGALRM"):
            signal.signal(signal.SIGALRM, utils._timeout_handler)
            signal.alarm(int(timeout))
        else:
            self.cancelTimeout()

            self._timer = threading.Timer(timeout, self.interrupt)
            self._timer.daemon = True
            self._timer.start()

    def _testEntry(self, test_record):
        """Internal function to be called upon entry of a test case.

        Args:
            test_record: The TestResultRecord object for the test case going to
                         be executed.
        """
        self._current_record = test_record
        if self.web.enabled:
            self.web.AddTestReport(test_record.test_name)

    def _setUp(self, test_name):
        """Proxy function to guarantee the base implementation of setUp is
        called.
        """
        event = tfi.Begin('_setUp method for test case',
                          tfi.categories.TEST_CASE_SETUP,)
        if not self.Heal(passive=True):
            msg = 'Framework self diagnose didn\'t pass for %s. Marking test as fail.' % test_name
            logging.error(msg)
            event.Remove(msg)
            asserts.fail(msg)

        if self.systrace.enabled:
            self.systrace.StartSystrace()

        event_sub = tfi.Begin('_setUp method from test script',
                              tfi.categories.TEST_CASE_SETUP,
                              enable_logging=False)
        result = self.setUp()
        event_sub.End()
        event.End()

    def setUp(self):
        """Setup function that will be called every time before executing each
        test case in the test class.

        To signal setup failure, return False or raise an exception. If
        exceptions were raised, the stack trace would appear in log, but the
        exceptions would not propagate to upper levels.

        Implementation is optional.
        """

    def _testExit(self):
        """Internal function to be called upon exit of a test."""
        self._current_record = None

    def _tearDown(self, test_name):
        """Proxy function to guarantee the base implementation of tearDown
        is called.
        """
        event = tfi.Begin('_tearDown method from base_test',
                          tfi.categories.TEST_CASE_TEARDOWN)
        if self.systrace.enabled:
            self._exec_func(self.systrace.ProcessAndUploadSystrace, test_name)
        event_sub = tfi.Begin('_tearDown method from test script',
                              tfi.categories.TEST_CASE_TEARDOWN,
                              enable_logging=False)
        self._exec_func(self.tearDown)
        event_sub.End()
        event.End()

    def tearDown(self):
        """Teardown function that will be called every time a test case has
        been executed.

        Implementation is optional.
        """

    def _onFail(self):
        """Proxy function to guarantee the base implementation of onFail is
        called.
        """
        record = self._current_record
        logging.error(record.details)
        begin_time = logger.epochToLogLineTimestamp(record.begin_time)
        logging.error(RESULT_LINE_TEMPLATE, self.results.progressStr,
                      record.test_name, record.result)
        if self.web.enabled:
            self.web.SetTestResult(ReportMsg.TEST_CASE_RESULT_FAIL)

        event = tfi.Begin('_onFail method from BaseTest',
                          tfi.categories.FAILED_TEST_CASE_PROCESSING,
                          enable_logging=False)
        self.onFail(record.test_name, begin_time)
        if self._bug_report_on_failure:
            self.DumpBugReport(record.test_name)
        if self._logcat_on_failure:
            self.DumpLogcat(record.test_name)
        event.End()

    def onFail(self, test_name, begin_time):
        """A function that is executed upon a test case failure.

        User implementation is optional.

        Args:
            test_name: Name of the test that triggered this function.
            begin_time: Logline format timestamp taken when the test started.
        """

    def _onPass(self):
        """Proxy function to guarantee the base implementation of onPass is
        called.
        """
        record = self._current_record
        begin_time = logger.epochToLogLineTimestamp(record.begin_time)
        msg = record.details
        if msg:
            logging.debug(msg)
        logging.info(RESULT_LINE_TEMPLATE, self.results.progressStr,
                     record.test_name, record.result)
        if self.web.enabled:
            self.web.SetTestResult(ReportMsg.TEST_CASE_RESULT_PASS)
        self.onPass(record.test_name, begin_time)

    def onPass(self, test_name, begin_time):
        """A function that is executed upon a test case passing.

        Implementation is optional.

        Args:
            test_name: Name of the test that triggered this function.
            begin_time: Logline format timestamp taken when the test started.
        """

    def _onSkip(self):
        """Proxy function to guarantee the base implementation of onSkip is
        called.
        """
        record = self._current_record
        begin_time = logger.epochToLogLineTimestamp(record.begin_time)
        logging.info(RESULT_LINE_TEMPLATE, self.results.progressStr,
                     record.test_name, record.result)
        logging.debug("Reason to skip: %s", record.details)
        if self.web.enabled:
            self.web.SetTestResult(ReportMsg.TEST_CASE_RESULT_SKIP)
        self.onSkip(record.test_name, begin_time)

    def onSkip(self, test_name, begin_time):
        """A function that is executed upon a test case being skipped.

        Implementation is optional.

        Args:
            test_name: Name of the test that triggered this function.
            begin_time: Logline format timestamp taken when the test started.
        """

    def _onSilent(self):
        """Proxy function to guarantee the base implementation of onSilent is
        called.
        """
        record = self._current_record
        begin_time = logger.epochToLogLineTimestamp(record.begin_time)
        if self.web.enabled:
            self.web.SetTestResult(None)
        self.onSilent(record.test_name, begin_time)

    def onSilent(self, test_name, begin_time):
        """A function that is executed upon a test case being marked as silent.

        Implementation is optional.

        Args:
            test_name: Name of the test that triggered this function.
            begin_time: Logline format timestamp taken when the test started.
        """

    def _onException(self):
        """Proxy function to guarantee the base implementation of onException
        is called.
        """
        record = self._current_record
        logging.exception(record.details)
        begin_time = logger.epochToLogLineTimestamp(record.begin_time)
        if self.web.enabled:
            self.web.SetTestResult(ReportMsg.TEST_CASE_RESULT_EXCEPTION)

        event = tfi.Begin('_onFail method from BaseTest',
                          tfi.categories.FAILED_TEST_CASE_PROCESSING,
                          enable_logging=False)
        self.onException(record.test_name, begin_time)
        if self._bug_report_on_failure:
            self.DumpBugReport(ecord.test_name)
        if self._logcat_on_failure:
            self.DumpLogcat(record.test_name)
        event.End()

    def onException(self, test_name, begin_time):
        """A function that is executed upon an unhandled exception from a test
        case.

        Implementation is optional.

        Args:
            test_name: Name of the test that triggered this function.
            begin_time: Logline format timestamp taken when the test started.
        """

    def _exec_procedure_func(self, func):
        """Executes a procedure function like onPass, onFail etc.

        This function will alternate the 'Result' of the test's record if
        exceptions happened when executing the procedure function.

        This will let signals.TestAbortAll through so abortAll works in all
        procedure functions.

        Args:
            func: The procedure function to be executed.
        """
        record = self._current_record

        if (func not in (self._onPass, self._onSilent, self._onSkip)
            and not self._is_final_run):
            logging.debug('Skipping test failure procedure function during retry')
            logging.info(RESULT_LINE_TEMPLATE, self.results.progressStr,
                         record.test_name, 'RETRY')
            return

        if record is None:
            logging.error("Cannot execute %s. No record for current test.",
                          func.__name__)
            return
        try:
            func()
        except signals.TestAbortAll as e:
            raise signals.TestAbortAll, e, sys.exc_info()[2]
        except Exception as e:
            logging.exception("Exception happened when executing %s for %s.",
                              func.__name__, record.test_name)
            record.addError(func.__name__, e)

    def addTableToResult(self, name, rows):
        """Adds a table to current test record.

        A subclass can call this method to add a table to _current_record when
        running test cases.

        Args:
            name: String, the table name.
            rows: A 2-dimensional list which contains the data.
        """
        self._current_record.addTable(name, rows)

    def filterOneTest(self, test_name, test_filter=None):
        """Check test filters for a test name.

        The first layer of filter is user defined test filters:
        if a include filter is not empty, only tests in include filter will
        be executed regardless whether they are also in exclude filter. Else
        if include filter is empty, only tests not in exclude filter will be
        executed.

        The second layer of filter is checking whether skipAllTests method is
        called. If the flag is set, this method raises signals.TestSkip.

        The third layer of filter is checking abi bitness:
        if a test has a suffix indicating the intended architecture bitness,
        and the current abi bitness information is available, non matching tests
        will be skipped. By our convention, this function will look for bitness in suffix
        formated as "32bit", "32Bit", "32BIT", or 64 bit equivalents.

        This method assumes const.SUFFIX_32BIT and const.SUFFIX_64BIT are in lower cases.

        Args:
            test_name: string, name of a test
            test_filter: Filter object, test filter

        Raises:
            signals.TestSilent if a test should not be executed
            signals.TestSkip if a test should be logged but not be executed
        """
        if self._test_filter_retry and not self._test_filter_retry.Filter(test_name):
            # TODO: TestSilent will remove test case from record,
            #       TestSkip will record test skip with a reason.
            #       skip during retry is neither of these, as the test should be skipped,
            #       not being recorded and not being deleted.
            #       Create a new signal type if callers outside this class wants to distinguish
            #       between these skip types.
            raise signals.TestSkip('Skipping completed tests in retry run attempt.')

        self._filterOneTestThroughTestFilter(test_name, test_filter)
        self._filterOneTestThroughAbiBitness(test_name)

    def _filterOneTestThroughTestFilter(self, test_name, test_filter=None):
        """Check test filter for the given test name.

        Args:
            test_name: string, name of a test

        Raises:
            signals.TestSilent if a test should not be executed
            signals.TestSkip if a test should be logged but not be executed
        """
        if not test_filter:
            test_filter = self.test_filter

        if not test_filter.Filter(test_name):
            raise signals.TestSilent("Test case '%s' did not pass filters.")

        if self.isSkipAllTests():
            raise signals.TestSkip(self.getSkipAllTestsReason())

    def _filterOneTestThroughAbiBitness(self, test_name):
        """Check test filter for the given test name.

        Args:
            test_name: string, name of a test

        Raises:
            signals.TestSilent if a test should not be executed
        """
        asserts.skipIf(
            self.abi_bitness and
            ((self.skip_on_32bit_abi is True) and self.abi_bitness == "32") or
            ((self.skip_on_64bit_abi is True) and self.abi_bitness == "64") or
            (test_name.lower().endswith(const.SUFFIX_32BIT) and
             self.abi_bitness != "32") or
            (test_name.lower().endswith(const.SUFFIX_64BIT) and
             self.abi_bitness != "64" and not self.run_32bit_on_64bit_abi),
            "Test case '{}' excluded as ABI bitness is {}.".format(
                test_name, self.abi_bitness))

    def execOneTest(self, test_name, test_func, args, **kwargs):
        """Executes one test case and update test results.

        Executes one test case, create a records.TestResultRecord object with
        the execution information, and add the record to the test class's test
        results.

        Args:
            test_name: Name of the test.
            test_func: The test function.
            args: A tuple of params.
            kwargs: Extra kwargs.
        """
        if self._test_filter_retry and not self._test_filter_retry.Filter(test_name):
            return

        is_silenced = False
        tr_record = records.TestResultRecord(test_name, self.test_module_name)
        tr_record.testBegin()
        logging.info(TEST_CASE_TEMPLATE, self.results.progressStr, test_name)
        verdict = None
        finished = False
        try:
            ret = self._testEntry(tr_record)
            asserts.assertTrue(ret is not False,
                               "Setup test entry for %s failed." % test_name)
            self.filterOneTest(test_name)
            if self.collect_tests_only:
                asserts.explicitPass("Collect tests only.")

            try:
                ret = self._setUp(test_name)
                asserts.assertTrue(ret is not False,
                                   "Setup for %s failed." % test_name)

                event_test = tfi.Begin("test function",
                                       tfi.categories.TEST_CASE_EXECUTION)
                if args or kwargs:
                    verdict = test_func(*args, **kwargs)
                else:
                    verdict = test_func()
                event_test.End()
                finished = True
            finally:
                self._tearDown(test_name)
        except (signals.TestFailure, AssertionError) as e:
            tr_record.testFail(e)
            self._exec_procedure_func(self._onFail)
            finished = True
        except signals.TestSkip as e:
            # Test skipped.
            tr_record.testSkip(e)
            self._exec_procedure_func(self._onSkip)
            finished = True
        except signals.TestAbortClass as e:
            # Abort signals, pass along.
            tr_record.testFail(e)
            self._is_final_run = True
            finished = True
            raise signals.TestAbortClass, e, sys.exc_info()[2]
        except signals.TestAbortAll as e:
            # Abort signals, pass along.
            tr_record.testFail(e)
            self._is_final_run = True
            finished = True
            raise signals.TestAbortAll, e, sys.exc_info()[2]
        except utils.TimeoutError as e:
            logging.exception(e)
            # Mark current test case as failure and abort remaining tests.
            tr_record.testFail(e)
            self._is_final_run = True
            finished = True
            raise signals.TestAbortAll, e, sys.exc_info()[2]
        except KeyboardInterrupt as e:
            logging.error("Received KeyboardInterrupt signal")
            # Abort signals, pass along.
            tr_record.testFail(e)
            self._is_final_run = True
            finished = True
            raise
        except AdbError as e:
            logging.error(e)
            if not self.Heal():
                # Non-recoverable adb failure. Mark test failure and abort
                tr_record.testFail(e)
                self._is_final_run = True
                finished = True
                raise signals.TestAbortAll, e, sys.exc_info()[2]
            # error specific to the test case, mark test failure and continue with remaining test
            tr_record.testFail(e)
            self._exec_procedure_func(self._onFail)
            finished = True
        except signals.TestPass as e:
            # Explicit test pass.
            tr_record.testPass(e)
            self._exec_procedure_func(self._onPass)
            finished = True
        except signals.TestSilent as e:
            # Suppress test reporting.
            is_silenced = True
            self._exec_procedure_func(self._onSilent)
            self.results.removeRecord(tr_record)
            finished = True
        except Exception as e:
            # Exception happened during test.
            logging.exception(e)
            tr_record.testError(e)
            self._exec_procedure_func(self._onException)
            self._exec_procedure_func(self._onFail)
            finished = True
        else:
            # Keep supporting return False for now.
            # TODO(angli): Deprecate return False support.
            if verdict or (verdict is None):
                # Test passed.
                tr_record.testPass()
                self._exec_procedure_func(self._onPass)
                return
            # Test failed because it didn't return True.
            # This should be removed eventually.
            tr_record.testFail()
            self._exec_procedure_func(self._onFail)
            finished = True
        finally:
            if not finished:
                for device in self.android_devices:
                    # if shell has not been set up yet
                    if device.shell is not None:
                        device.shell.DisableShell()

                logging.error('Test timed out.')
                tr_record.testError()
                self._exec_procedure_func(self._onException)
                self._exec_procedure_func(self._onFail)

            if not is_silenced:
                self.results.addRecord(tr_record)
            self._testExit()

    def runGeneratedTests(self,
                          test_func,
                          settings,
                          args=None,
                          kwargs=None,
                          tag="",
                          name_func=None):
        """Runs generated test cases.

        Generated test cases are not written down as functions, but as a list
        of parameter sets. This way we reduce code repetition and improve
        test case scalability.

        Args:
            test_func: The common logic shared by all these generated test
                       cases. This function should take at least one argument,
                       which is a parameter set.
            settings: A list of strings representing parameter sets. These are
                      usually json strings that get loaded in the test_func.
            args: Iterable of additional position args to be passed to
                  test_func.
            kwargs: Dict of additional keyword args to be passed to test_func
            tag: Name of this group of generated test cases. Ignored if
                 name_func is provided and operates properly.
            name_func: A function that takes a test setting and generates a
                       proper test name. The test name should be shorter than
                       utils.MAX_FILENAME_LEN. Names over the limit will be
                       truncated.

        Returns:
            A list of settings that did not pass.
        """
        args = args or ()
        kwargs = kwargs or {}
        failed_settings = []

        def GenerateTestName(setting):
            test_name = "{} {}".format(tag, setting)
            if name_func:
                try:
                    test_name = name_func(setting, *args, **kwargs)
                except:
                    logging.exception(("Failed to get test name from "
                                       "test_func. Fall back to default %s"),
                                      test_name)

            if len(test_name) > utils.MAX_FILENAME_LEN:
                test_name = test_name[:utils.MAX_FILENAME_LEN]

            return test_name

        for setting in settings:
            test_name = GenerateTestName(setting)

            tr_record = records.TestResultRecord(test_name, self.test_module_name)
            self.results.requested.append(tr_record)

        for setting in settings:
            test_name = GenerateTestName(setting)
            previous_success_cnt = len(self.results.passed)

            event_exec = tfi.Begin('BaseTest execOneTest method for generated tests',
                                   enable_logging=False)
            self.execOneTest(test_name, test_func, (setting, ) + args, **kwargs)
            event_exec.End()
            if len(self.results.passed) - previous_success_cnt != 1:
                failed_settings.append(setting)

        return failed_settings

    def _exec_func(self, func, *args):
        """Executes a function with exception safeguard.

        This will let signals.TestAbortAll through so abortAll works in all
        procedure functions.

        Args:
            func: Function to be executed.
            args: Arguments to be passed to the function.

        Returns:
            Whatever the function returns, or False if non-caught exception
            occurred.
        """
        try:
            return func(*args)
        except signals.TestAbortAll as e:
            raise signals.TestAbortAll, e, sys.exc_info()[2]
        except:
            logging.exception("Exception happened when executing %s in %s.",
                              func.__name__, self.test_module_name)
            return False

    def _get_all_test_names(self):
        """Finds all the function names that match the test case naming
        convention in this class.

        Returns:
            A list of strings, each is a test case name.
        """
        test_names = []
        for name in dir(self):
            if name.startswith(STR_TEST) or name.startswith(STR_GENERATE):
                attr_func = getattr(self, name)
                if hasattr(attr_func, "__call__"):
                    test_names.append(name)
        return test_names

    def _get_test_funcs(self, test_names):
        """Obtain the actual functions of test cases based on test names.

        Args:
            test_names: A list of strings, each string is a test case name.

        Returns:
            A list of tuples of (string, function). String is the test case
            name, function is the actual test case function.

        Raises:
            errors.USERError is raised if the test name does not follow
            naming convention "test_*". This can only be caused by user input
            here.
        """
        test_funcs = []
        for test_name in test_names:
            if not hasattr(self, test_name):
                logging.warning("%s does not have test case %s.",
                                self.test_module_name, test_name)
            elif (test_name.startswith(STR_TEST) or
                  test_name.startswith(STR_GENERATE)):
                test_funcs.append((test_name, getattr(self, test_name)))
            else:
                msg = ("Test case name %s does not follow naming convention "
                       "test*, abort.") % test_name
                raise errors.USERError(msg)

        return test_funcs

    def getTests(self, test_names=None):
        """Get the test cases within a test class.

        Args:
            test_names: A list of string that are test case names requested in
                        cmd line.

        Returns:
            A list of tuples of (string, function). String is the test case
            name, function is the actual test case function.
        """
        if not test_names:
            if self.tests:
                # Specified by run list in class.
                test_names = list(self.tests)
            else:
                # No test case specified by user, execute all in the test class
                test_names = self._get_all_test_names()

        tests = self._get_test_funcs(test_names)
        return tests

    def _DiagnoseHost(self):
        """Runs diagnosis commands on host and logs the results."""
        commands = ['ps aux | grep adb',
                    'adb --version',
                    'adb devices']
        for cmd in commands:
            results = cmd_utils.ExecuteShellCommand(cmd)
            logging.debug('host diagnosis command %s', cmd)
            logging.debug('               output: %s', results[cmd_utils.STDOUT][0])

    def _DiagnoseDevice(self, device):
        """Runs diagnosis commands on device and logs the results."""
        commands = ['ps aux | grep vts',
                    'cat /proc/meminfo']
        for cmd in commands:
            results = device.adb.shell(cmd, no_except=True, timeout=adb.DEFAULT_ADB_SHORT_TIMEOUT)
            logging.debug('device diagnosis command %s', cmd)
            logging.debug('                 output: %s', results[const.STDOUT])

    def Heal(self, passive=False, timeout=900):
        """Performs a self healing.

        Includes self diagnosis that looks for any framework or device state errors.
        Includes self recovery that attempts to correct discovered errors.

        Args:
            passive: bool, whether to perform passive only self-check. A passive check means
                     only to check known status stored in memory. No command will be issued
                     to host or device - which makes the check fast.
            timeout: int, timeout in seconds.

        Returns:
            bool, True if everything is ok. Fales if some error is not recoverable.
        """
        start = time.time()

        if not passive:
            available_devices = android_device.list_adb_devices()
            self._DiagnoseHost()

            for device in self.android_devices:
                if device.serial not in available_devices:
                    device.log.warn(
                        "device become unavailable after tests. wait for device come back")
                    _timeout = timeout - time.time() + start
                    if _timeout < 0 or not device.waitForBootCompletion(timeout=_timeout):
                        device.log.error('failed to restore device %s')
                        return False
                    device.rootAdb()
                    device.stopServices()
                    device.startServices()
                    self._DiagnoseHost()
                else:
                    self._DiagnoseDevice(device)

        return all(map(lambda device: device.Heal(), self.android_devices))

    def runTestsWithRetry(self, tests):
        """Run tests with retry and collect test results.

        Args:
            tests: A list of tests to be run.
        """
        for count in range(self.max_retry_count + 1):
            if count:
                if not self.Heal():
                    logging.error('Self heal failed. '
                                  'Some error is not recoverable within time constraint.')
                    return

                include_filter = map(lambda item: item.test_name,
                                     self.results.getNonPassingRecords(skipped=False))
                self._test_filter_retry = filter_utils.Filter(include_filter=include_filter)
                logging.info('Automatically retrying %s test cases. Run attempt %s of %s',
                             len(include_filter),
                             count + 1,
                             self.max_retry_count + 1)
                msg = 'Retrying the following test cases: %s' % include_filter
                logging.debug(msg)

                path_retry_log = os.path.join(logging.log_path, 'retry_log.txt')
                with open(path_retry_log, 'a+') as f:
                    f.write(msg + '\n')

            self._is_final_run = count == self.max_retry_count

            try:
                self._runTests(tests)
            except Exception as e:
                if self._is_final_run:
                    raise e

            if self._is_final_run:
                break

    def _runTests(self, tests):
        """Run tests and collect test results.

        Args:
            tests: A list of tests to be run.
        """
        # Setup for the class with retry.
        for i in xrange(_SETUP_RETRY_NUMBER):
            setup_done = False
            caught_exception = None
            try:
                if self._setUpClass() is False:
                    raise signals.TestFailure(
                        "Failed to setup %s." % self.test_module_name)
                else:
                    setup_done = True
            except Exception as e:
                caught_exception = e
                logging.exception("Failed to setup %s.", self.test_module_name)
            finally:
                if setup_done:
                    break
                elif not caught_exception or i + 1 == _SETUP_RETRY_NUMBER:
                    self.results.failClass(self.test_module_name,
                                           caught_exception)
                    self._exec_func(self._tearDownClass)
                    self._is_final_run = True
                    return
                else:
                    # restart services before retry setup.
                    for device in self.android_devices:
                        logging.info("restarting service on device %s", device.serial)
                        device.stopServices()
                        device.startServices()

        class_error = None
        # Run tests in order.
        try:
            # Check if module is running in self test mode.
            if self.run_as_vts_self_test:
                logging.debug('setUpClass function was executed successfully.')
                self.results.passClass(self.test_module_name)
                return

            for test_name, test_func in tests:
                if test_name.startswith(STR_GENERATE):
                    logging.debug(
                        "Executing generated test trigger function '%s'",
                        test_name)
                    test_func()
                    logging.debug("Finished '%s'", test_name)
                else:
                    event_exec = tfi.Begin('BaseTest execOneTest method for individual tests',
                                           enable_logging=False)
                    self.execOneTest(test_name, test_func, None)
                    event_exec.End()
            if self.isSkipAllTests() and not self.results.executed:
                self.results.skipClass(
                    self.test_module_name,
                    "All test cases skipped; unable to find any test case.")
        except signals.TestAbortClass as e:
            logging.error("Received TestAbortClass signal")
            class_error = e
            self._is_final_run = True
        except signals.TestAbortAll as e:
            logging.info("Received TestAbortAll signal")
            class_error = e
            self._is_final_run = True
            # Piggy-back test results on this exception object so we don't lose
            # results from this test class.
            setattr(e, "results", self.results)
            raise signals.TestAbortAll, e, sys.exc_info()[2]
        except KeyboardInterrupt as e:
            class_error = e
            self._is_final_run = True
            # Piggy-back test results on this exception object so we don't lose
            # results from this test class.
            setattr(e, "results", self.results)
            raise
        except Exception as e:
            # Exception happened during test.
            logging.exception(e)
            class_error = e
            raise e
        finally:
            if not self.results.getNonPassingRecords(skipped=False):
                self._is_final_run = True

            if class_error and self._is_final_run:
                self.results.failClass(self.test_module_name, class_error)

            self._exec_func(self._tearDownClass)

            if self._is_final_run:
                if self.web.enabled:
                    name, timestamp = self.web.GetTestModuleKeys()
                    self.results.setTestModuleKeys(name, timestamp)

                logging.info("Summary for test class %s: %s",
                             self.test_module_name, self.results.summary())

    def run(self, test_names=None):
        """Runs test cases within a test class by the order they appear in the
        execution list.

        One of these test cases lists will be executed, shown here in priority
        order:
        1. The test_names list, which is passed from cmd line. Invalid names
           are guarded by cmd line arg parsing.
        2. The self.tests list defined in test class. Invalid names are
           ignored.
        3. All function that matches test case naming convention in the test
           class.

        Args:
            test_names: A list of string that are test case names requested in
                cmd line.

        Returns:
            The test results object of this class.
        """
        logging.info("==========> %s <==========", self.test_module_name)
        # Devise the actual test cases to run in the test class.
        tests = self.getTests(test_names)

        if not self.run_as_vts_self_test:
            self.results.requested = [
                records.TestResultRecord(test_name, self.test_module_name)
                for test_name,_ in tests if test_name.startswith(STR_TEST)
            ]

        self.runTestsWithRetry(tests)
        return self.results

    def cleanUp(self):
        """A function that is executed upon completion of all tests cases
        selected in the test class.

        This function should clean up objects initialized in the constructor by
        user.
        """

    def DumpBugReport(self, prefix=''):
        """Get device bugreport through adb command.

        Args:
            prefix: string, file name prefix. Usually in format of
                    <test_module>-<test_case>
        """
        event = tfi.Begin('dump Bugreport',
                          tfi.categories.FAILED_TEST_CASE_PROCESSING)
        if prefix:
            prefix = re.sub('[^\w\-_\. ]', '_', prefix) + '_'

        parent_dir = os.path.join(logging.log_path, 'bugreport')

        if not file_util.Mkdir(parent_dir):
            logging.error('Failed to create bugreport output directory %s', parent_dir)
            return

        for device in self.android_devices:
            if device.fatal_error: continue
            file_name = (_BUG_REPORT_FILE_PREFIX
                         + prefix
                         + '_%s' % device.serial
                         + _BUG_REPORT_FILE_EXTENSION)

            file_path = os.path.join(parent_dir, file_name)

            logging.info('Dumping bugreport %s...' % file_path)
            device.adb.bugreport(file_path)
        event.End()

    def skipAllTestsIf(self, condition, msg):
        """Skip all test cases if the given condition is true.

        This method is usually called in setup functions when a precondition
        to the test module is not met.

        Args:
            condition: object that can be evaluated by bool(), a condition that
                       will trigger skipAllTests if evaluated to be True.
            msg: string, reason why tests are skipped. If set to None or empty
            string, a default message will be used (not recommended)
        """
        if condition:
            self.skipAllTests(msg)

    def skipAllTests(self, msg):
        """Skip all test cases.

        This method is usually called in setup functions when a precondition
        to the test module is not met.

        Args:
            msg: string, reason why tests are skipped. If set to None or empty
            string, a default message will be used (not recommended)
        """
        if not msg:
            msg = "No reason provided."

        setattr(self, _REASON_TO_SKIP_ALL_TESTS, msg)

    def isSkipAllTests(self):
        """Returns whether all tests are set to be skipped.

        Note: If all tests are being skipped not due to skipAllTests
              being called, or there is no tests defined, this method will
              still return False (since skipAllTests is not called.)

        Returns:
            bool, True if skipAllTests has been called; False otherwise.
        """
        return self.getSkipAllTestsReason() is not None

    def getSkipAllTestsReason(self):
        """Returns the reason why all tests are skipped.

        Note: If all tests are being skipped not due to skipAllTests
              being called, or there is no tests defined, this method will
              still return None (since skipAllTests is not called.)

        Returns:
            String, reason why tests are skipped. None if skipAllTests
            is not called.
        """
        return getattr(self, _REASON_TO_SKIP_ALL_TESTS, None)

    def DumpLogcat(self, prefix=''):
        """Dumps device logcat outputs to log directory.

        Args:
            prefix: string, file name prefix. Usually in format of
                    <test_module>-<test_case>
        """
        event = tfi.Begin('dump logcat',
                          tfi.categories.FAILED_TEST_CASE_PROCESSING)
        if prefix:
            prefix = re.sub('[^\w\-_\. ]', '_', prefix) + '_'

        parent_dir = os.path.join(logging.log_path, 'logcat')

        if not file_util.Mkdir(parent_dir):
            logging.error('Failed to create bugreport output directory %s', parent_dir)
            return

        for device in self.android_devices:
            if (not device.isAdbLogcatOn) or device.fatal_error:
                continue
            for buffer in LOGCAT_BUFFERS:
                file_name = (_LOGCAT_FILE_PREFIX
                             + prefix
                             + '_%s_' % buffer
                             + device.serial
                             + _LOGCAT_FILE_EXTENSION)

                file_path = os.path.join(parent_dir, file_name)

                logging.info('Dumping logcat %s...' % file_path)
                device.adb.logcat('-b', buffer, '-d', '>', file_path)
        event.End()
