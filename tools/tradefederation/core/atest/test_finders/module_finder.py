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
Module Finder class.
"""

import logging
import os

# pylint: disable=import-error
import atest_error
import atest_utils
import constants
from test_finders import test_info
from test_finders import test_finder_base
from test_finders import test_finder_utils
from test_runners import atest_tf_test_runner
from test_runners import robolectric_test_runner
from test_runners import vts_tf_test_runner

_MODULES_IN = 'MODULES-IN-%s'
_ANDROID_MK = 'Android.mk'

# These are suites in LOCAL_COMPATIBILITY_SUITE that aren't really suites so
# we can ignore them.
_SUITES_TO_IGNORE = frozenset({'general-tests', 'device-tests', 'tests'})

class ModuleFinder(test_finder_base.TestFinderBase):
    """Module finder class."""
    NAME = 'MODULE'
    _TEST_RUNNER = atest_tf_test_runner.AtestTradefedTestRunner.NAME
    _ROBOLECTRIC_RUNNER = robolectric_test_runner.RobolectricTestRunner.NAME
    _VTS_TEST_RUNNER = vts_tf_test_runner.VtsTradefedTestRunner.NAME

    def __init__(self, module_info=None):
        super(ModuleFinder, self).__init__()
        self.root_dir = os.environ.get(constants.ANDROID_BUILD_TOP)
        self.module_info = module_info

    def _determine_testable_module(self, path):
        """Determine which module the user is trying to test.

        Returns the module to test. If there are multiple possibilities, will
        ask the user. Otherwise will return the only module found.

        Args:
            path: String path of module to look for.

        Returns:
            A list of the module names.
        """
        testable_modules = []
        for mod in self.module_info.get_module_names(path):
            mod_info = self.module_info.get_module_info(mod)
            # Robolectric tests always exist in pairs of 2, one module to build
            # the test and another to run it. For now, we are assuming they are
            # isolated in their own folders and will return if we find one.
            if self.module_info.is_robolectric_test(mod):
                # return a list with one module name if it is robolectric.
                return [mod]
            if self.module_info.is_testable_module(mod_info):
                testable_modules.append(mod_info.get(constants.MODULE_NAME))
        return test_finder_utils.extract_test_from_tests(testable_modules)

    def _is_vts_module(self, module_name):
        """Returns True if the module is a vts10 module, else False."""
        mod_info = self.module_info.get_module_info(module_name)
        suites = []
        if mod_info:
            suites = mod_info.get('compatibility_suites', [])
        # Pull out all *ts (cts, tvts, etc) suites.
        suites = [suite for suite in suites if suite not in _SUITES_TO_IGNORE]
        return len(suites) == 1 and 'vts10' in suites

    def _update_to_vts_test_info(self, test):
        """Fill in the fields with vts10 specific info.

        We need to update the runner to use the vts10 runner and also find the
        test specific dependencies.

        Args:
            test: TestInfo to update with vts10 specific details.

        Return:
            TestInfo that is ready for the vts10 test runner.
        """
        test.test_runner = self._VTS_TEST_RUNNER
        config_file = os.path.join(self.root_dir,
                                   test.data[constants.TI_REL_CONFIG])
        # Need to get out dir (special logic is to account for custom out dirs).
        # The out dir is used to construct the build targets for the test deps.
        out_dir = os.environ.get(constants.ANDROID_HOST_OUT)
        custom_out_dir = os.environ.get(constants.ANDROID_OUT_DIR)
        # If we're not an absolute custom out dir, get relative out dir path.
        if custom_out_dir is None or not os.path.isabs(custom_out_dir):
            out_dir = os.path.relpath(out_dir, self.root_dir)
        vts_out_dir = os.path.join(out_dir, 'vts10', 'android-vts10', 'testcases')
        # Parse dependency of default staging plans.
        xml_paths = test_finder_utils.search_integration_dirs(
            constants.VTS_STAGING_PLAN,
            self.module_info.get_paths(constants.VTS_TF_MODULE))
        vts_xmls = set()
        vts_xmls.add(config_file)
        for xml_path in xml_paths:
            vts_xmls |= test_finder_utils.get_plans_from_vts_xml(xml_path)
        for config_file in vts_xmls:
            # Add in vts10 test build targets.
            test.build_targets |= test_finder_utils.get_targets_from_vts_xml(
                config_file, vts_out_dir, self.module_info)
        test.build_targets.add('vts-test-core')
        test.build_targets.add(test.test_name)
        return test

    def _update_to_robolectric_test_info(self, test):
        """Update the fields for a robolectric test.

        Args:
          test: TestInfo to be updated with robolectric fields.

        Returns:
          TestInfo with robolectric fields.
        """
        test.test_runner = self._ROBOLECTRIC_RUNNER
        test.test_name = self.module_info.get_robolectric_test_name(test.test_name)
        return test

    def _process_test_info(self, test):
        """Process the test info and return some fields updated/changed.

        We need to check if the test found is a special module (like vts10) and
        update the test_info fields (like test_runner) appropriately.

        Args:
            test: TestInfo that has been filled out by a find method.

        Return:
            TestInfo that has been modified as needed and return None if
            this module can't be found in the module_info.
        """
        module_name = test.test_name
        mod_info = self.module_info.get_module_info(module_name)
        if not mod_info:
            return None
        test.module_class = mod_info['class']
        test.install_locations = test_finder_utils.get_install_locations(
            mod_info['installed'])
        # Check if this is only a vts10 module.
        if self._is_vts_module(test.test_name):
            return self._update_to_vts_test_info(test)
        elif self.module_info.is_robolectric_test(test.test_name):
            return self._update_to_robolectric_test_info(test)
        rel_config = test.data[constants.TI_REL_CONFIG]
        test.build_targets = self._get_build_targets(module_name, rel_config)
        return test

    def _get_build_targets(self, module_name, rel_config):
        """Get the test deps.

        Args:
            module_name: name of the test.
            rel_config: XML for the given test.

        Returns:
            Set of build targets.
        """
        targets = set()
        if not self.module_info.is_auto_gen_test_config(module_name):
            config_file = os.path.join(self.root_dir, rel_config)
            targets = test_finder_utils.get_targets_from_xml(config_file,
                                                             self.module_info)
        if constants.VTS_CORE_SUITE in self.module_info.get_module_info(
                module_name).get(constants.MODULE_COMPATIBILITY_SUITES, []):
            targets.add(constants.VTS_CORE_TF_MODULE)
        for module_path in self.module_info.get_paths(module_name):
            mod_dir = module_path.replace('/', '-')
            targets.add(_MODULES_IN % mod_dir)
        # (b/156457698) Force add vts_kernel_tests as build target if our test
        # belong to REQUIRED_KERNEL_TEST_MODULES due to required_module option
        # not working for sh_test in soong.
        if module_name in constants.REQUIRED_KERNEL_TEST_MODULES:
            targets.add('vts_kernel_tests')
        return targets

    def _get_module_test_config(self, module_name, rel_config=None):
        """Get the value of test_config in module_info.

        Get the value of 'test_config' in module_info if its
        auto_test_config is not true.
        In this case, the test_config is specified by user.
        If not, return rel_config.

        Args:
            module_name: A string of the test's module name.
            rel_config: XML for the given test.

        Returns:
            A string of test_config path if found, else return rel_config.
        """
        mod_info = self.module_info.get_module_info(module_name)
        if mod_info:
            test_config = ''
            test_config_list = mod_info.get(constants.MODULE_TEST_CONFIG, [])
            if test_config_list:
                test_config = test_config_list[0]
            if not self.module_info.is_auto_gen_test_config(module_name) and test_config != '':
                return test_config
        return rel_config

    def _get_test_info_filter(self, path, methods, **kwargs):
        """Get test info filter.

        Args:
            path: A string of the test's path.
            methods: A set of method name strings.
            rel_module_dir: Optional. A string of the module dir relative to
                root.
            class_name: Optional. A string of the class name.
            is_native_test: Optional. A boolean variable of whether to search
                for a native test or not.

        Returns:
            A set of test info filter.
        """
        _, file_name = test_finder_utils.get_dir_path_and_filename(path)
        ti_filter = frozenset()
        if kwargs.get('is_native_test', None):
            ti_filter = frozenset([test_info.TestFilter(
                test_finder_utils.get_cc_filter(
                    kwargs.get('class_name', '*'), methods), frozenset())])
        # Path to java file.
        elif file_name and constants.JAVA_EXT_RE.match(file_name):
            full_class_name = test_finder_utils.get_fully_qualified_class_name(
                path)
            ti_filter = frozenset(
                [test_info.TestFilter(full_class_name, methods)])
        # Path to cc file.
        elif file_name and constants.CC_EXT_RE.match(file_name):
            if not test_finder_utils.has_cc_class(path):
                raise atest_error.MissingCCTestCaseError(
                    "Can't find CC class in %s" % path)
            if methods:
                ti_filter = frozenset(
                    [test_info.TestFilter(test_finder_utils.get_cc_filter(
                        kwargs.get('class_name', '*'), methods), frozenset())])
        # Path to non-module dir, treat as package.
        elif (not file_name
              and kwargs.get('rel_module_dir', None) !=
              os.path.relpath(path, self.root_dir)):
            dir_items = [os.path.join(path, f) for f in os.listdir(path)]
            for dir_item in dir_items:
                if constants.JAVA_EXT_RE.match(dir_item):
                    package_name = test_finder_utils.get_package_name(dir_item)
                    if package_name:
                        # methods should be empty frozenset for package.
                        if methods:
                            raise atest_error.MethodWithoutClassError(
                                '%s: Method filtering requires class'
                                % str(methods))
                        ti_filter = frozenset(
                            [test_info.TestFilter(package_name, methods)])
                        break
        return ti_filter

    def _get_rel_config(self, test_path):
        """Get config file's relative path.

        Args:
            test_path: A string of the test absolute path.

        Returns:
            A string of config's relative path, else None.
        """
        test_dir = os.path.dirname(test_path)
        rel_module_dir = test_finder_utils.find_parent_module_dir(
            self.root_dir, test_dir, self.module_info)
        if rel_module_dir:
            return os.path.join(rel_module_dir, constants.MODULE_CONFIG)
        return None

    def _get_test_infos(self, test_path, rel_config, module_name, test_filter):
        """Get test_info for test_path.

        Args:
            test_path: A string of the test path.
            rel_config: A string of rel path of config.
            module_name: A string of the module name to use.
            test_filter: A test info filter.

        Returns:
            A list of TestInfo namedtuple if found, else None.
        """
        if not rel_config:
            rel_config = self._get_rel_config(test_path)
            if not rel_config:
                return None
        if module_name:
            module_names = [module_name]
        else:
            module_names = self._determine_testable_module(
                os.path.dirname(rel_config))
        test_infos = []
        if module_names:
            for mname in module_names:
                # The real test config might be record in module-info.
                rel_config = self._get_module_test_config(mname,
                                                          rel_config=rel_config)
                mod_info = self.module_info.get_module_info(mname)
                tinfo = self._process_test_info(test_info.TestInfo(
                    test_name=mname,
                    test_runner=self._TEST_RUNNER,
                    build_targets=set(),
                    data={constants.TI_FILTER: test_filter,
                          constants.TI_REL_CONFIG: rel_config},
                    compatibility_suites=mod_info.get(
                        constants.MODULE_COMPATIBILITY_SUITES, [])))
                if tinfo:
                    test_infos.append(tinfo)
        return test_infos

    def find_test_by_module_name(self, module_name):
        """Find test for the given module name.

        Args:
            module_name: A string of the test's module name.

        Returns:
            A list that includes only 1 populated TestInfo namedtuple
            if found, otherwise None.
        """
        mod_info = self.module_info.get_module_info(module_name)
        if self.module_info.is_testable_module(mod_info):
            # path is a list with only 1 element.
            rel_config = os.path.join(mod_info['path'][0],
                                      constants.MODULE_CONFIG)
            rel_config = self._get_module_test_config(module_name, rel_config=rel_config)
            tinfo = self._process_test_info(test_info.TestInfo(
                test_name=module_name,
                test_runner=self._TEST_RUNNER,
                build_targets=set(),
                data={constants.TI_REL_CONFIG: rel_config,
                      constants.TI_FILTER: frozenset()},
                compatibility_suites=mod_info.get(
                    constants.MODULE_COMPATIBILITY_SUITES, [])))
            if tinfo:
                return [tinfo]
        return None

    def find_test_by_kernel_class_name(self, module_name, class_name):
        """Find kernel test for the given class name.

        Args:
            module_name: A string of the module name to use.
            class_name: A string of the test's class name.

        Returns:
            A list of populated TestInfo namedtuple if test found, else None.
        """
        class_name, methods = test_finder_utils.split_methods(class_name)
        test_config = self._get_module_test_config(module_name)
        test_config_path = os.path.join(self.root_dir, test_config)
        mod_info = self.module_info.get_module_info(module_name)
        ti_filter = frozenset(
            [test_info.TestFilter(class_name, methods)])
        if test_finder_utils.is_test_from_kernel_xml(test_config_path, class_name):
            tinfo = self._process_test_info(test_info.TestInfo(
                test_name=module_name,
                test_runner=self._TEST_RUNNER,
                build_targets=set(),
                data={constants.TI_REL_CONFIG: test_config,
                      constants.TI_FILTER: ti_filter},
                compatibility_suites=mod_info.get(
                    constants.MODULE_COMPATIBILITY_SUITES, [])))
            if tinfo:
                return [tinfo]
        return None

    def find_test_by_class_name(self, class_name, module_name=None,
                                rel_config=None, is_native_test=False):
        """Find test files given a class name.

        If module_name and rel_config not given it will calculate it determine
        it by looking up the tree from the class file.

        Args:
            class_name: A string of the test's class name.
            module_name: Optional. A string of the module name to use.
            rel_config: Optional. A string of module dir relative to repo root.
            is_native_test: A boolean variable of whether to search for a
            native test or not.

        Returns:
            A list of populated TestInfo namedtuple if test found, else None.
        """
        class_name, methods = test_finder_utils.split_methods(class_name)
        if rel_config:
            search_dir = os.path.join(self.root_dir,
                                      os.path.dirname(rel_config))
        else:
            search_dir = self.root_dir
        test_paths = test_finder_utils.find_class_file(search_dir, class_name,
                                                       is_native_test, methods)
        if not test_paths and rel_config:
            logging.info('Did not find class (%s) under module path (%s), '
                         'researching from repo root.', class_name, rel_config)
            test_paths = test_finder_utils.find_class_file(self.root_dir,
                                                           class_name,
                                                           is_native_test,
                                                           methods)
        if not test_paths:
            return None
        tinfos = []
        for test_path in test_paths:
            test_filter = self._get_test_info_filter(
                test_path, methods, class_name=class_name,
                is_native_test=is_native_test)
            tinfo = self._get_test_infos(test_path, rel_config,
                                         module_name, test_filter)
            if tinfo:
                tinfos.extend(tinfo)
        return tinfos

    def find_test_by_module_and_class(self, module_class):
        """Find the test info given a MODULE:CLASS string.

        Args:
            module_class: A string of form MODULE:CLASS or MODULE:CLASS#METHOD.

        Returns:
            A list of populated TestInfo namedtuple if found, else None.
        """
        if ':' not in module_class:
            return None
        module_name, class_name = module_class.split(':')
        # module_infos is a list with at most 1 element.
        module_infos = self.find_test_by_module_name(module_name)
        module_info = module_infos[0] if module_infos else None
        if not module_info:
            return None
        find_result = None
        # If the target module is NATIVE_TEST, search CC classes only.
        if not self.module_info.is_native_test(module_name):
            # Find by java class.
            find_result = self.find_test_by_class_name(
                class_name, module_info.test_name,
                module_info.data.get(constants.TI_REL_CONFIG))
        # kernel target test is also define as NATIVE_TEST in build system.
        # TODO (b/157210083) Update find_test_by_kernel_class_name method to
        # support gen_rule use case.
        if not find_result:
            find_result = self.find_test_by_kernel_class_name(
                module_name, class_name)
        # Find by cc class.
        if not find_result:
            find_result = self.find_test_by_cc_class_name(
                class_name, module_info.test_name,
                module_info.data.get(constants.TI_REL_CONFIG))
        return find_result

    def find_test_by_package_name(self, package, module_name=None,
                                  rel_config=None):
        """Find the test info given a PACKAGE string.

        Args:
            package: A string of the package name.
            module_name: Optional. A string of the module name.
            ref_config: Optional. A string of rel path of config.

        Returns:
            A list of populated TestInfo namedtuple if found, else None.
        """
        _, methods = test_finder_utils.split_methods(package)
        if methods:
            raise atest_error.MethodWithoutClassError('%s: Method filtering '
                                                      'requires class' % (
                                                          methods))
        # Confirm that packages exists and get user input for multiples.
        if rel_config:
            search_dir = os.path.join(self.root_dir,
                                      os.path.dirname(rel_config))
        else:
            search_dir = self.root_dir
        package_paths = test_finder_utils.run_find_cmd(
            test_finder_utils.FIND_REFERENCE_TYPE.PACKAGE, search_dir, package)
        # Package path will be the full path to the dir represented by package.
        if not package_paths:
            return None
        test_filter = frozenset([test_info.TestFilter(package, frozenset())])
        test_infos = []
        for package_path in package_paths:
            tinfo = self._get_test_infos(package_path, rel_config,
                                         module_name, test_filter)
            if tinfo:
                test_infos.extend(tinfo)
        return test_infos

    def find_test_by_module_and_package(self, module_package):
        """Find the test info given a MODULE:PACKAGE string.

        Args:
            module_package: A string of form MODULE:PACKAGE

        Returns:
            A list of populated TestInfo namedtuple if found, else None.
        """
        module_name, package = module_package.split(':')
        # module_infos is a list with at most 1 element.
        module_infos = self.find_test_by_module_name(module_name)
        module_info = module_infos[0] if module_infos else None
        if not module_info:
            return None
        return self.find_test_by_package_name(
            package, module_info.test_name,
            module_info.data.get(constants.TI_REL_CONFIG))

    def find_test_by_path(self, path):
        """Find the first test info matching the given path.

        Strategy:
            path_to_java_file --> Resolve to CLASS
            path_to_cc_file --> Resolve to CC CLASS
            path_to_module_file -> Resolve to MODULE
            path_to_module_dir -> Resolve to MODULE
            path_to_dir_with_class_files--> Resolve to PACKAGE
            path_to_any_other_dir --> Resolve as MODULE

        Args:
            path: A string of the test's path.

        Returns:
            A list of populated TestInfo namedtuple if test found, else None
        """
        logging.debug('Finding test by path: %s', path)
        path, methods = test_finder_utils.split_methods(path)
        # TODO: See if this can be generalized and shared with methods above
        # create absolute path from cwd and remove symbolic links
        path = os.path.realpath(path)
        if not os.path.exists(path):
            return None
        if (methods and
                not test_finder_utils.has_method_in_file(path, methods)):
            return None
        dir_path, _ = test_finder_utils.get_dir_path_and_filename(path)
        # Module/Class
        rel_module_dir = test_finder_utils.find_parent_module_dir(
            self.root_dir, dir_path, self.module_info)
        if not rel_module_dir:
            return None
        rel_config = os.path.join(rel_module_dir, constants.MODULE_CONFIG)
        test_filter = self._get_test_info_filter(path, methods,
                                                 rel_module_dir=rel_module_dir)
        return self._get_test_infos(path, rel_config, None, test_filter)

    def find_test_by_cc_class_name(self, class_name, module_name=None,
                                   rel_config=None):
        """Find test files given a cc class name.

        If module_name and rel_config not given, test will be determined
        by looking up the tree for files which has input class.

        Args:
            class_name: A string of the test's class name.
            module_name: Optional. A string of the module name to use.
            rel_config: Optional. A string of module dir relative to repo root.

        Returns:
            A list of populated TestInfo namedtuple if test found, else None.
        """
        # Check if class_name is prepended with file name. If so, trim the
        # prefix and keep only the class_name.
        if '.' in class_name:
            # Assume the class name has a format of file_name.class_name
            class_name = class_name[class_name.rindex('.')+1:]
            logging.info('Search with updated class name: %s', class_name)
        return self.find_test_by_class_name(
            class_name, module_name, rel_config, is_native_test=True)

    def get_testable_modules_with_ld(self, user_input, ld_range=0):
        """Calculate the edit distances of the input and testable modules.

        The user input will be calculated across all testable modules and
        results in integers generated by Levenshtein Distance algorithm.
        To increase the speed of the calculation, a bound can be applied to
        this method to prevent from calculating every testable modules.

        Guessing from typos, e.g. atest atest_unitests, implies a tangible range
        of length that Atest only needs to search within it, and the default of
        the bound is 2.

        Guessing from keywords however, e.g. atest --search Camera, means that
        the uncertainty of the module name is way higher, and Atest should walk
        through all testable modules and return the highest possibilities.

        Args:
            user_input: A string of the user input.
            ld_range: An integer that range the searching scope. If the length of
                      user_input is 10, then Atest will calculate modules of which
                      length is between 8 and 12. 0 is equivalent to unlimited.

        Returns:
            A List of LDs and possible module names. If the user_input is "fax",
            the output will be like:
            [[2, "fog"], [2, "Fix"], [4, "duck"], [7, "Duckies"]]

            Which means the most lilely names of "fax" are fog and Fix(LD=2),
            while Dickies is the most unlikely one(LD=7).
        """
        atest_utils.colorful_print('\nSearching for similar module names using '
                                   'fuzzy search...', constants.CYAN)
        testable_modules = sorted(self.module_info.get_testable_modules(), key=len)
        lower_bound = len(user_input) - ld_range
        upper_bound = len(user_input) + ld_range
        testable_modules_with_ld = []
        for module_name in testable_modules:
            # Dispose those too short or too lengthy.
            if ld_range != 0:
                if len(module_name) < lower_bound:
                    continue
                elif len(module_name) > upper_bound:
                    break
            testable_modules_with_ld.append(
                [test_finder_utils.get_levenshtein_distance(
                    user_input, module_name), module_name])
        return testable_modules_with_ld

    def get_fuzzy_searching_results(self, user_input):
        """Give results which have no more than allowance of edit distances.

        Args:
            user_input: the target module name for fuzzy searching.

        Return:
            A list of guessed modules.
        """
        modules_with_ld = self.get_testable_modules_with_ld(user_input,
                                                            ld_range=constants.LD_RANGE)
        guessed_modules = []
        for _distance, _module in modules_with_ld:
            if _distance <= abs(constants.LD_RANGE):
                guessed_modules.append(_module)
        return guessed_modules
