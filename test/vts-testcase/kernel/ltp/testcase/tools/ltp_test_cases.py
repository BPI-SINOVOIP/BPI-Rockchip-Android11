#
# Copyright (C) 2020 The Android Open Source Project
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

import os
import logging

import ltp_configs
import ltp_enums
import test_case
from configs import stable_tests
from configs import disabled_tests
from common import filter_utils

ltp_test_template = '        <option name="test-command-line" key="%s" value="&env_setup_cmd; ;' \
                    ' cd &ltp_bin_dir; ; %s" />'

class LtpTestCases(object):
    """Load a ltp vts testcase definition file and parse it into a generator.

    Attributes:
        _data_path: string, the vts data path on host side
        _filter_func: function, a filter method that will emit exception if a test is filtered
        _ltp_tests_filter: list of string, filter for tests that are stable and disabled
        _ltp_binaries: list of string, All ltp binaries that generate in build time
        _ltp_config_lines: list of string: the context of the generated config
    """

    def __init__(self, android_build_top, filter_func):
        self._android_build_top = android_build_top
        self._filter_func = filter_func
        self._ltp_tests_filter = filter_utils.Filter(
            [ i[0] for i in stable_tests.STABLE_TESTS ],
            disabled_tests.DISABLED_TESTS,
            enable_regex=True)
        self._ltp_tests_filter.ExpandBitness()
        self._ltp_binaries = []
        self._ltp_config_lines = []

    def ValidateDefinition(self, line):
        """Validate a tab delimited test case definition.

        Will check whether the given line of definition has three parts
        separated by tabs.
        It will also trim leading and ending white spaces for each part
        in returned tuple (if valid).

        Returns:
            A tuple in format (test suite, test name, test command) if
            definition is valid. None otherwise.
        """
        items = [
            item.strip()
            for item in line.split(ltp_enums.Delimiters.TESTCASE_DEFINITION)
        ]
        if not len(items) == 3 or not items:
            return None
        else:
            return items

    def ReadConfigTemplateFile(self):
        """Read the template of the config file and return the context.

        Returns:
            String.
        """
        file_name = ltp_configs.LTP_CONFIG_TEMPLATE_FILE_NAME
        file_path = os.path.join(self._android_build_top, ltp_configs.LTP_CONFIG_TEMPLATE_DIR, file_name)
        with open(file_path, 'r') as f:
            return f.read()

    def GetKernelModuleControllerOption(self, arch, n_bit, is_low_mem=False, is_hwasan=False):
        """Get the Option of KernelModuleController.

        Args:
            arch: String, arch
            n_bit: int, bitness
            run_staging: bool, whether to use staging configuration
            is_low_mem: bool, whether to use low memory device configuration

        Returns:
            String.
        """
        arch_template = '        <option name="arch" value="{}"/>\n'
        is_low_mem_template = '        <option name="is-low-mem" value="{}"/>\n'
        is_hwasan_template = '        <option name="is-hwasan" value="{}"/>'
        option_lines = arch_template + is_low_mem_template + is_hwasan_template
        if n_bit == '64':
            n_bit_string = str(n_bit) if arch == 'arm' else ('_'+str(n_bit))
        else:
            n_bit_string = ''
        arch_name = arch + n_bit_string
        is_low_mem = 'true' if is_low_mem else 'false'
        is_hwasan = 'true' if is_hwasan else 'false'
        option_lines = option_lines.format(arch_name,
                                           str(is_low_mem).lower(),
                                           str(is_hwasan).lower())
        return option_lines

    def GetLtpBinaries(self):
        """Check the binary exist in the command.

        Args:
            command: String, the test command

        Returns:
            bool: True if the binary in the gen.bp
        """
        gen_bp_path = os.path.join(self._android_build_top, ltp_configs.LTP_GEN_BINARY_BP)
        for line in open(gen_bp_path, 'r'):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith("stem:") or line.startswith('filename:'):
                ltp_binary = line.split('"')[1]
                self._ltp_binaries.append(ltp_binary)

    def IsLtpBinaryExist(self, commands):
        """Check the binary exist in the command.

        Args:
            command: String, the test command

        Returns:
            bool: True if the binary in the gen.bp
        """
        all_commands = commands.split(';')
        for cmd in all_commands:
            cmd = cmd.strip()
            binary_name = cmd.split(' ')[0]
            if binary_name in self._ltp_binaries:
                return True
        logging.info("Ltp binary not exist in cmd of '%s'", commands)
        return False

    def GenConfig(self,
             arch,
             n_bit,
             test_filter,
             output_file,
             run_staging=False,
             is_low_mem=False,
             is_hwasan=False):
        """Read the definition file and generate the test config.

        Args:
            arch: String, arch
            n_bit: int, bitness
            test_filter: Filter object, test name filter from base_test
            output_file: String, the file path of the generating config
            run_staging: bool, whether to use staging configuration
            is_low_mem: bool, whether to use low memory device configuration
        """
        self.GetLtpBinaries()
        scenario_groups = (ltp_configs.TEST_SUITES_LOW_MEM
                           if is_low_mem else ltp_configs.TEST_SUITES)
        logging.info('LTP scenario groups: %s', scenario_groups)
        start_append_test_keyword = 'option name="per-binary-timeout"'
        config_lines = self.ReadConfigTemplateFile()
        module_controller_option = self.GetKernelModuleControllerOption(arch, n_bit,
                                                                        is_low_mem,
                                                                        is_hwasan)
        test_case_string = ''
        run_scritp = self.GenerateLtpRunScript(scenario_groups, is_hwasan=is_hwasan)
        for line in run_scritp:
            items = self.ValidateDefinition(line)
            if not items:
                continue

            testsuite, testname, command = items
            if is_low_mem and testsuite.endswith(
                    ltp_configs.LOW_MEMORY_SCENARIO_GROUP_SUFFIX):
                testsuite = testsuite[:-len(
                    ltp_configs.LOW_MEMORY_SCENARIO_GROUP_SUFFIX)]

            # Tests failed to build will have prefix "DISABLED_"
            if testname.startswith("DISABLED_"):
                logging.info("[Parser] Skipping test case {}-{}. Reason: "
                             "not built".format(testsuite, testname))
                continue

            # Some test cases have hardcoded "/tmp" in the command
            # we replace that with ltp_configs.TMPDIR
            command = command.replace('/tmp', ltp_configs.TMPDIR)

            testcase = test_case.TestCase(
                testsuite=testsuite, testname=testname, command=command)
            test_display_name = "{}_{}bit".format(str(testcase), n_bit)

            # Check runner's base_test filtering method
            try:
                self._filter_func(test_display_name)
            except:
                logging.info("[Parser] Skipping test case %s. Reason: "
                             "filtered" % testcase.fullname)
                testcase.is_filtered = True
                testcase.note = "filtered"

            logging.info('ltp_test_cases Load(): test_display_name = %s\n'
                         'cmd = %s', test_display_name, command)

            # For skipping tests that are not designed or ready for Android,
            # check for bit specific test in disabled list as well as non-bit specific
            if ((self._ltp_tests_filter.IsInExcludeFilter(str(testcase)) or
                 self._ltp_tests_filter.IsInExcludeFilter(test_display_name)) and
                    not test_filter.IsInIncludeFilter(test_display_name)):
                logging.info("[Parser] Skipping test case %s. Reason: "
                             "disabled" % testcase.fullname)
                continue

            # For separating staging tests from stable tests
            if not self._ltp_tests_filter.IsInIncludeFilter(test_display_name):
                if not run_staging and not test_filter.IsInIncludeFilter(
                        test_display_name):
                    # Skip staging tests in stable run
                    continue
                else:
                    testcase.is_staging = True
                    testcase.note = "staging"
            else:
                if run_staging:
                    # Skip stable tests in staging run
                    continue

            if not testcase.is_staging:
                for x in stable_tests.STABLE_TESTS:
                    if x[0] == test_display_name and x[1]:
                        testcase.is_mandatory = True
                        break
            if self.IsLtpBinaryExist(command):
                logging.info("[Parser] Adding test case %s." % testcase.fullname)
                # Some test cases contain semicolons in their commands,
                # and we replace them with &&
                command = command.replace(';', '&amp;&amp;')
                # Replace the original command with '/data/local/tmp/ltp'
                # e.g. mm.mmapstress07
                command = command.replace(ltp_configs.LTPDIR, '&ltp_dir;')
                ltp_test_line = ltp_test_template % (test_display_name, command)
                test_case_string += (ltp_test_line + '\n')
        nativetest_bit_path = '64' if n_bit == '64' else ''
        config_lines = config_lines.format(nativetest_bit_path, module_controller_option,
                                           test_case_string)
        with open(output_file, 'w') as f:
            f.write(config_lines)

    def ReadCommentedTxt(self, filepath):
        '''Read a lines of a file that are not commented by #.

        Args:
            filepath: string, path of file to read

        Returns:
            A set of string representing non-commented lines in given file
        '''
        if not filepath:
            logging.error('Invalid file path')
            return None

        with open(filepath, 'r') as f:
            lines_gen = (line.strip() for line in f)
            return set(
                line for line in lines_gen
                if line and not line.startswith('#'))

    def GenerateLtpTestCases(self, testsuite, disabled_tests_list):
        '''Generate test cases for each ltp test suite.

        Args:
            testsuite: string, test suite name

        Returns:
            A list of string
        '''
        testsuite_script = os.path.join(self._android_build_top,
                                        ltp_configs.LTP_RUNTEST_DIR, testsuite)

        result = []
        for line in open(testsuite_script, 'r'):
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            testname = line.split()[0]
            testname_prefix = ('DISABLED_'
                               if testname in disabled_tests_list else '')
            testname_modified = testname_prefix + testname

            result.append("\t".join(
                [testsuite, testname_modified, line[len(testname):].strip()]))
        return result

    def GenerateLtpRunScript(self, scenario_groups, is_hwasan=False):
        '''Given a scenario group generate test case script.

        Args:
            scenario_groups: list of string, name of test scenario groups to use

        Returns:
            A list of string
        '''
        disabled_tests_path = os.path.join(
            self._android_build_top, ltp_configs.LTP_DISABLED_BUILD_TESTS_CONFIG_PATH)
        disabled_tests_list = self.ReadCommentedTxt(disabled_tests_path)
        if is_hwasan:
          disabled_tests_list = disabled_tests_list.union(disabled_tests.DISABLED_TESTS_HWASAN)

        result = []
        for testsuite in scenario_groups:
            result.extend(
                self.GenerateLtpTestCases(testsuite, disabled_tests_list))
        return result
