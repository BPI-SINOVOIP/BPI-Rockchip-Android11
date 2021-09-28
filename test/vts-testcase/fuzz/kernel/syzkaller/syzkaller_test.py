#
# Copyright (C) 2018 The Android Open Source Project
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
import shutil
import tempfile

import environment_variables as env
import syzkaller_test_case

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import config_parser
from vts.runners.host import keys
from vts.runners.host import test_runner
from vts.utils.python.common import cmd_utils
from vts.utils.python.controllers import adb
from vts.utils.python.controllers import android_device
from vts.utils.python.gcs import gcs_api_utils


class SyzkallerTest(base_test.BaseTestClass):
    """Runs Syzkaller tests on target."""

    start_vts_agents = False

    def setUpClass(self):
        """Creates a remote shell instance, and copies data files.

        Attributes:
            _env: dict, a mapping from key to environment path of this test.
            _gcs_api_utils: a GcsApiUtils object used for uploading logs.
            _dut: an android device controller object used for getting device info.
        """

        required_params = [
            keys.ConfigKeys.IKEY_SERVICE_JSON_PATH,
            keys.ConfigKeys.IKEY_FUZZING_GCS_BUCKET_NAME,
            keys.ConfigKeys.IKEY_SYZKALLER_PACKAGES_PATH,
            keys.ConfigKeys.IKEY_SYZKALLER_TEMPLATE_PATH
        ]
        self.getUserParams(required_params)

        _temp_dir = tempfile.mkdtemp()
        self._env = dict()
        self._env['temp_dir'] = _temp_dir
        self._env['syzkaller_dir'] = os.path.join(_temp_dir, env.syzkaller_dir)
        self._env['syzkaller_bin_dir'] = os.path.join(_temp_dir,
                                                      env.syzkaller_bin_dir)
        self._env['syzkaller_work_dir'] = os.path.join(_temp_dir,
                                                       env.syzkaller_work_dir)
        self._env['template_cfg'] = os.path.join(_temp_dir, env.template_cfg)

        self._gcs_api_utils = gcs_api_utils.GcsApiUtils(
            self.service_key_json_path, self.fuzzing_gcs_bucket_name)
        self._dut = self.android_devices[0]

    def FetchSyzkaller(self):
        """Fetch Syzkaller program from x20 and make sure files are executable."""
        try:
            logging.info('Fetching Syzkaller program.')
            shutil.copytree(self.syzkaller_packages_path,
                            self._env['syzkaller_dir'])
            logging.info('Fetching Syzkaller template configuration.')
            shutil.copy(self.syzkaller_template_path, self._env['temp_dir'])
        except IOError, e:
            logging.exception(e)
            self.skipAllTests(
                'Syzkaller program is not available. Skipping all tests.')

        for root, dirs, files in os.walk(self._env['syzkaller_bin_dir']):
            for filepath in files:
                os.chmod(os.path.join(root, filepath), 0755)

    def CreateTestCases(self):
        """Create syzkaller test cases.

        Returns:
            test_cases, list, the list of test_cases for this test.
        """
        test_cases = []
        test_cases.append(
            syzkaller_test_case.SyzkallerTestCase(self._env, 'linux/arm64',
                                                  self._dut.serial, 'adb',
                                                  'false', 12))
        return test_cases

    def RunTestCase(self, test_case):
        """Run a syzkaller test case.

        Args:
            test_case: SyzkallerTestCase object, the test case to run.
        """
        test_command = test_case.GetRunCommand()
        stdout, stderr, err_code = cmd_utils.ExecuteOneShellCommand(
            test_command, timeout=18000)
        if err_code:
            logging.error(stderr)
        else:
            logging.info(stdout)
        self.ReportTestCase(test_case)

    def ReportTestCase(self, test_case):
        """Asserts the result of the test case and uploads report to GCS.

        Args:
            test_case: SyzkallerTestCase object, the test case to report.
        """
        self.AssertTestCaseResult(test_case)
        self._gcs_api_utils.UploadDir(test_case._work_dir_path,
                                      'kernelfuzz_reports')

    def AssertTestCaseResult(self, test_case):
        """Asserts that test case finished as expected.

        If crash reports were generated during the test, reports test as failure.
        If crash reports were not generated during the test, reports test as success.

        Args:
            test_case: SyzkallerTestCase object, the test case to assert
                       as failure or success.
        """
        logging.info('Test case results.')
        crash_dir = os.path.join(test_case._work_dir_path, 'crashes')
        if os.listdir(crash_dir) == []:
            logging.info('%s did not cause crash in our device.',
                         test_case._test_name)
        else:
            asserts.fail('%s caused crash in our device.',
                         test_case._test_name)

    def tearDownClass(self):
        """Removes the temporary directory used for Syzkaller."""
        logging.debug('Temporary directory %s is being deleted',
                      self._env['temp_dir'])
        try:
            shutil.rmtree(self._env['temp_dir'])
        except OSError as e:
            logging.exception(e)

    def generateKernelFuzzerTests(self):
        """Runs kernel fuzzer tests."""
        self.FetchSyzkaller()
        self.runGeneratedTests(
            test_func=self.RunTestCase,
            settings=self.CreateTestCases(),
            name_func=lambda x: x._test_name)


if __name__ == '__main__':
    test_runner.main()
