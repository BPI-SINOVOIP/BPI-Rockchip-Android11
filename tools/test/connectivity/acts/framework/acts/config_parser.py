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
import copy
import itertools
import os
import sys
from builtins import str

import mobly.config_parser as mobly_config_parser

from acts import keys
from acts import utils

# An environment variable defining the base location for ACTS logs.
_ENV_ACTS_LOGPATH = 'ACTS_LOGPATH'
# An environment variable that enables test case failures to log stack traces.
_ENV_TEST_FAILURE_TRACEBACKS = 'ACTS_TEST_FAILURE_TRACEBACKS'
# An environment variable defining the test search paths for ACTS.
_ENV_ACTS_TESTPATHS = 'ACTS_TESTPATHS'
_PATH_SEPARATOR = ':'


class ActsConfigError(Exception):
    """Raised when there is a problem in test configuration file."""


def _validate_test_config(test_config):
    """Validates the raw configuration loaded from the config file.

    Making sure all the required fields exist.
    """
    for k in keys.Config.reserved_keys.value:
        # TODO(markdr): Remove this continue after merging this with the
        # validation done in Mobly's load_test_config_file.
        if (k == keys.Config.key_test_paths.value
                or k == keys.Config.key_log_path.value):
            continue

        if k not in test_config:
            raise ActsConfigError("Required key %s missing in test config." %
                                  k)


def _validate_testbed_name(name):
    """Validates the name of a test bed.

    Since test bed names are used as part of the test run id, it needs to meet
    certain requirements.

    Args:
        name: The test bed's name specified in config file.

    Raises:
        If the name does not meet any criteria, ActsConfigError is raised.
    """
    if not name:
        raise ActsConfigError("Test bed names can't be empty.")
    if not isinstance(name, str):
        raise ActsConfigError("Test bed names have to be string.")
    for l in name:
        if l not in utils.valid_filename_chars:
            raise ActsConfigError(
                "Char '%s' is not allowed in test bed names." % l)


def _update_file_paths(config, config_path):
    """ Checks if the path entries are valid.

    If the file path is invalid, assume it is a relative path and append
    that to the config file path.

    Args:
        config : the config object to verify.
        config_path : The path to the config file, which can be used to
                      generate absolute paths from relative paths in configs.

    Raises:
        If the file path is invalid, ActsConfigError is raised.
    """
    # Check the file_path_keys and update if it is a relative path.
    for file_path_key in keys.Config.file_path_keys.value:
        if file_path_key in config:
            config_file = config[file_path_key]
            if type(config_file) is str:
                if not os.path.isfile(config_file):
                    config_file = os.path.join(config_path, config_file)
                if not os.path.isfile(config_file):
                    raise ActsConfigError(
                        "Unable to load config %s from test "
                        "config file.", config_file)
                config[file_path_key] = config_file


def _validate_testbed_configs(testbed_configs, config_path):
    """Validates the testbed configurations.

    Args:
        testbed_configs: A list of testbed configuration json objects.
        config_path : The path to the config file, which can be used to
                      generate absolute paths from relative paths in configs.

    Raises:
        If any part of the configuration is invalid, ActsConfigError is raised.
    """
    # Cross checks testbed configs for resource conflicts.
    for name, config in testbed_configs.items():
        _update_file_paths(config, config_path)
        _validate_testbed_name(name)


def gen_term_signal_handler(test_runners):
    def termination_sig_handler(signal_num, frame):
        print('Received sigterm %s.' % signal_num)
        for t in test_runners:
            t.stop()
        sys.exit(1)

    return termination_sig_handler


def _parse_one_test_specifier(item):
    """Parse one test specifier from command line input.

    Args:
        item: A string that specifies a test class or test cases in one test
            class to run.

    Returns:
        A tuple of a string and a list of strings. The string is the test class
        name, the list of strings is a list of test case names. The list can be
        None.
    """
    tokens = item.split(':')
    if len(tokens) > 2:
        raise ActsConfigError("Syntax error in test specifier %s" % item)
    if len(tokens) == 1:
        # This should be considered a test class name
        test_cls_name = tokens[0]
        return test_cls_name, None
    elif len(tokens) == 2:
        # This should be considered a test class name followed by
        # a list of test case names.
        test_cls_name, test_case_names = tokens
        clean_names = [elem.strip() for elem in test_case_names.split(',')]
        return test_cls_name, clean_names


def parse_test_list(test_list):
    """Parse user provided test list into internal format for test_runner.

    Args:
        test_list: A list of test classes/cases.
    """
    result = []
    for elem in test_list:
        result.append(_parse_one_test_specifier(elem))
    return result


def load_test_config_file(test_config_path, tb_filters=None):
    """Processes the test configuration file provided by the user.

    Loads the configuration file into a json object, unpacks each testbed
    config into its own TestRunConfig object, and validate the configuration in
    the process.

    Args:
        test_config_path: Path to the test configuration file.
        tb_filters: A subset of test bed names to be pulled from the config
                    file. If None, then all test beds will be selected.

    Returns:
        A list of mobly.config_parser.TestRunConfig objects to be passed to
        test_runner.TestRunner.
    """
    configs = utils.load_config(test_config_path)

    testbeds = configs[keys.Config.key_testbed.value]
    if type(testbeds) is list:
        tb_dict = dict()
        for testbed in testbeds:
            tb_dict[testbed[keys.Config.key_testbed_name.value]] = testbed
        testbeds = tb_dict
    elif type(testbeds) is dict:
        # For compatibility, make sure the entry name is the same as
        # the testbed's "name" entry
        for name, testbed in testbeds.items():
            testbed[keys.Config.key_testbed_name.value] = name

    if tb_filters:
        tbs = {}
        for name in tb_filters:
            if name in testbeds:
                tbs[name] = testbeds[name]
            else:
                raise ActsConfigError(
                    'Expected testbed named "%s", but none was found. Check'
                    'if you have the correct testbed names.' % name)
        testbeds = tbs

    if (keys.Config.key_log_path.value not in configs
            and _ENV_ACTS_LOGPATH in os.environ):
        print('Using environment log path: %s' %
              (os.environ[_ENV_ACTS_LOGPATH]))
        configs[keys.Config.key_log_path.value] = os.environ[_ENV_ACTS_LOGPATH]
    if (keys.Config.key_test_paths.value not in configs
            and _ENV_ACTS_TESTPATHS in os.environ):
        print('Using environment test paths: %s' %
              (os.environ[_ENV_ACTS_TESTPATHS]))
        configs[keys.Config.key_test_paths.
                value] = os.environ[_ENV_ACTS_TESTPATHS].split(_PATH_SEPARATOR)
    if (keys.Config.key_test_failure_tracebacks not in configs
            and _ENV_TEST_FAILURE_TRACEBACKS in os.environ):
        configs[keys.Config.key_test_failure_tracebacks.
                value] = os.environ[_ENV_TEST_FAILURE_TRACEBACKS]

    # TODO: See if there is a better way to do this: b/29836695
    config_path, _ = os.path.split(utils.abs_path(test_config_path))
    configs[keys.Config.key_config_path.value] = config_path
    _validate_test_config(configs)
    _validate_testbed_configs(testbeds, config_path)
    # Unpack testbeds into separate json objects.
    configs.pop(keys.Config.key_testbed.value)
    test_run_configs = []

    for _, testbed in testbeds.items():
        test_run_config = mobly_config_parser.TestRunConfig()
        test_run_config.testbed_name = testbed[
            keys.Config.key_testbed_name.value]
        test_run_config.controller_configs = testbed
        test_run_config.controller_configs[
            keys.Config.key_test_paths.value] = configs.get(
                keys.Config.key_test_paths.value, None)
        test_run_config.log_path = configs.get(keys.Config.key_log_path.value,
                                               None)
        if test_run_config.log_path is not None:
            test_run_config.log_path = utils.abs_path(test_run_config.log_path)

        user_param_pairs = []
        for item in itertools.chain(configs.items(), testbed.items()):
            if item[0] not in keys.Config.reserved_keys.value:
                user_param_pairs.append(item)
        test_run_config.user_params = dict(user_param_pairs)

        test_run_configs.append(test_run_config)
    return test_run_configs


def parse_test_file(fpath):
    """Parses a test file that contains test specifiers.

    Args:
        fpath: A string that is the path to the test file to parse.

    Returns:
        A list of strings, each is a test specifier.
    """
    with open(fpath, 'r') as f:
        tf = []
        for line in f:
            line = line.strip()
            if not line:
                continue
            if len(tf) and (tf[-1].endswith(':') or tf[-1].endswith(',')):
                tf[-1] += line
            else:
                tf.append(line)
        return tf
