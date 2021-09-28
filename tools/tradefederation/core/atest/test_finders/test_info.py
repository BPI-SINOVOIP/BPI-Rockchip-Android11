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

"""
TestInfo class.
"""

from collections import namedtuple

# pylint: disable=import-error
import constants


TestFilterBase = namedtuple('TestFilter', ['class_name', 'methods'])


class TestInfo(object):
    """Information needed to identify and run a test."""

    # pylint: disable=too-many-arguments
    def __init__(self, test_name, test_runner, build_targets, data=None,
                 suite=None, module_class=None, install_locations=None,
                 test_finder='', compatibility_suites=None):
        """Init for TestInfo.

        Args:
            test_name: String of test name.
            test_runner: String of test runner.
            build_targets: Set of build targets.
            data: Dict of data for test runners to use.
            suite: Suite for test runners to use.
            module_class: A list of test classes. It's a snippet of class
                        in module_info. e.g. ["EXECUTABLES",  "NATIVE_TESTS"]
            install_locations: Set of install locations.
                        e.g. set(['host', 'device'])
            test_finder: String of test finder.
            compatibility_suites: A list of compatibility_suites. It's a
                        snippet of compatibility_suites in module_info. e.g.
                        ["device-tests",  "vts10"]
        """
        self.test_name = test_name
        self.test_runner = test_runner
        self.build_targets = build_targets
        self.data = data if data else {}
        self.suite = suite
        self.module_class = module_class if module_class else []
        self.install_locations = (install_locations if install_locations
                                  else set())
        # True if the TestInfo is built from a test configured in TEST_MAPPING.
        self.from_test_mapping = False
        # True if the test should run on host and require no device. The
        # attribute is only set through TEST_MAPPING file.
        self.host = False
        self.test_finder = test_finder
        self.compatibility_suites = (map(str, compatibility_suites)
                                     if compatibility_suites else [])

    def __str__(self):
        host_info = (' - runs on host without device required.' if self.host
                     else '')
        return ('test_name: %s - test_runner:%s - build_targets:%s - data:%s - '
                'suite:%s - module_class: %s - install_locations:%s%s - '
                'test_finder: %s - compatibility_suites:%s' % (
                    self.test_name, self.test_runner, self.build_targets,
                    self.data, self.suite, self.module_class,
                    self.install_locations, host_info, self.test_finder,
                    self.compatibility_suites))

    def get_supported_exec_mode(self):
        """Get the supported execution mode of the test.

        Determine the test supports which execution mode by strategy:
        Robolectric/JAVA_LIBRARIES --> 'both'
        Not native tests or installed only in out/target --> 'device'
        Installed only in out/host --> 'both'
        Installed under host and target --> 'both'

        Return:
            String of execution mode.
        """
        install_path = self.install_locations
        if not self.module_class:
            return constants.DEVICE_TEST
        # Let Robolectric test support both.
        if constants.MODULE_CLASS_ROBOLECTRIC in self.module_class:
            return constants.BOTH_TEST
        # Let JAVA_LIBRARIES support both.
        if constants.MODULE_CLASS_JAVA_LIBRARIES in self.module_class:
            return constants.BOTH_TEST
        if not install_path:
            return constants.DEVICE_TEST
        # Non-Native test runs on device-only.
        if constants.MODULE_CLASS_NATIVE_TESTS not in self.module_class:
            return constants.DEVICE_TEST
        # Native test with install path as host should be treated as both.
        # Otherwise, return device test.
        if len(install_path) == 1 and constants.DEVICE_TEST in install_path:
            return constants.DEVICE_TEST
        return constants.BOTH_TEST


class TestFilter(TestFilterBase):
    """Information needed to filter a test in Tradefed"""

    def to_set_of_tf_strings(self):
        """Return TestFilter as set of strings in TradeFed filter format."""
        if self.methods:
            return {'%s#%s' % (self.class_name, m) for m in self.methods}
        return {self.class_name}
