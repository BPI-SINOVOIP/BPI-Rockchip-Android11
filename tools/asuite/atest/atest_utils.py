# Copyright 2017, The Android Open Source Project
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
Utility functions for atest.
"""


# pylint: disable=import-outside-toplevel

from __future__ import print_function

import hashlib
import itertools
import json
import logging
import os
import pickle
import re
import shutil
import subprocess
import sys

import atest_decorator
import atest_error
import constants

# b/147562331 only occurs when running atest in source code. We don't encourge
# the users to manually "pip3 install protobuf", therefore when the exception
# occurs, we don't collect data and the tab completion is for args is silence.
try:
    from metrics import metrics_base
    from metrics import metrics_utils
except ModuleNotFoundError:
    # This exception occurs only when invoking atest in source code.
    print("You shouldn't see this message unless you ran 'atest-src'."
          "To resolve the issue, please run:\n\t{}\n"
          "and try again.".format('pip3 install protobuf'))
    sys.exit(constants.IMPORT_FAILURE)

_BASH_RESET_CODE = '\033[0m\n'
# Arbitrary number to limit stdout for failed runs in _run_limited_output.
# Reason for its use is that the make command itself has its own carriage
# return output mechanism that when collected line by line causes the streaming
# full_output list to be extremely large.
_FAILED_OUTPUT_LINE_LIMIT = 100
# Regular expression to match the start of a ninja compile:
# ex: [ 99% 39710/39711]
_BUILD_COMPILE_STATUS = re.compile(r'\[\s*(\d{1,3}%\s+)?\d+/\d+\]')
_BUILD_FAILURE = 'FAILED: '
CMD_RESULT_PATH = os.path.join(os.environ.get(constants.ANDROID_BUILD_TOP,
                                              os.getcwd()),
                               'tools/tradefederation/core/atest/test_data',
                               'test_commands.json')
BUILD_TOP_HASH = hashlib.md5(os.environ.get(constants.ANDROID_BUILD_TOP, '').
                             encode()).hexdigest()
TEST_INFO_CACHE_ROOT = os.path.join(os.path.expanduser('~'), '.atest',
                                    'info_cache', BUILD_TOP_HASH[:8])
_DEFAULT_TERMINAL_WIDTH = 80
_DEFAULT_TERMINAL_HEIGHT = 25
_BUILD_CMD = 'build/soong/soong_ui.bash'
_FIND_MODIFIED_FILES_CMDS = (
    "cd {};"
    "local_branch=$(git rev-parse --abbrev-ref HEAD);"
    "remote_branch=$(git branch -r | grep '\\->' | awk '{{print $1}}');"
    # Get the number of commits from local branch to remote branch.
    "ahead=$(git rev-list --left-right --count $local_branch...$remote_branch "
    "| awk '{{print $1}}');"
    # Get the list of modified files from HEAD to previous $ahead generation.
    "git diff HEAD~$ahead --name-only")


def get_build_cmd():
    """Compose build command with no-absolute path and flag "--make-mode".

    Returns:
        A list of soong build command.
    """
    make_cmd = ('%s/%s' %
                (os.path.relpath(os.environ.get(
                    constants.ANDROID_BUILD_TOP, os.getcwd()), os.getcwd()),
                 _BUILD_CMD))
    return [make_cmd, '--make-mode']


def _capture_fail_section(full_log):
    """Return the error message from the build output.

    Args:
        full_log: List of strings representing full output of build.

    Returns:
        capture_output: List of strings that are build errors.
    """
    am_capturing = False
    capture_output = []
    for line in full_log:
        if am_capturing and _BUILD_COMPILE_STATUS.match(line):
            break
        if am_capturing or line.startswith(_BUILD_FAILURE):
            capture_output.append(line)
            am_capturing = True
            continue
    return capture_output


def _run_limited_output(cmd, env_vars=None):
    """Runs a given command and streams the output on a single line in stdout.

    Args:
        cmd: A list of strings representing the command to run.
        env_vars: Optional arg. Dict of env vars to set during build.

    Raises:
        subprocess.CalledProcessError: When the command exits with a non-0
            exitcode.
    """
    # Send stderr to stdout so we only have to deal with a single pipe.
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env=env_vars)
    sys.stdout.write('\n')
    term_width, _ = get_terminal_size()
    white_space = " " * int(term_width)
    full_output = []
    while proc.poll() is None:
        line = proc.stdout.readline().decode('utf-8')
        # Readline will often return empty strings.
        if not line:
            continue
        full_output.append(line)
        # Trim the line to the width of the terminal.
        # Note: Does not handle terminal resizing, which is probably not worth
        #       checking the width every loop.
        if len(line) >= term_width:
            line = line[:term_width - 1]
        # Clear the last line we outputted.
        sys.stdout.write('\r%s\r' % white_space)
        sys.stdout.write('%s' % line.strip())
        sys.stdout.flush()
    # Reset stdout (on bash) to remove any custom formatting and newline.
    sys.stdout.write(_BASH_RESET_CODE)
    sys.stdout.flush()
    # Wait for the Popen to finish completely before checking the returncode.
    proc.wait()
    if proc.returncode != 0:
        # Parse out the build error to output.
        output = _capture_fail_section(full_output)
        if not output:
            output = full_output
        if len(output) >= _FAILED_OUTPUT_LINE_LIMIT:
            output = output[-_FAILED_OUTPUT_LINE_LIMIT:]
        output = 'Output (may be trimmed):\n%s' % ''.join(output)
        raise subprocess.CalledProcessError(proc.returncode, cmd, output)


def build(build_targets, verbose=False, env_vars=None):
    """Shell out and make build_targets.

    Args:
        build_targets: A set of strings of build targets to make.
        verbose: Optional arg. If True output is streamed to the console.
                 If False, only the last line of the build output is outputted.
        env_vars: Optional arg. Dict of env vars to set during build.

    Returns:
        Boolean of whether build command was successful, True if nothing to
        build.
    """
    if not build_targets:
        logging.debug('No build targets, skipping build.')
        return True
    full_env_vars = os.environ.copy()
    if env_vars:
        full_env_vars.update(env_vars)
    print('\n%s\n%s' % (colorize("Building Dependencies...", constants.CYAN),
                        ', '.join(build_targets)))
    logging.debug('Building Dependencies: %s', ' '.join(build_targets))
    cmd = get_build_cmd() + list(build_targets)
    logging.debug('Executing command: %s', cmd)
    try:
        if verbose:
            subprocess.check_call(cmd, stderr=subprocess.STDOUT,
                                  env=full_env_vars)
        else:
            # TODO: Save output to a log file.
            _run_limited_output(cmd, env_vars=full_env_vars)
        logging.info('Build successful')
        return True
    except subprocess.CalledProcessError as err:
        logging.error('Error building: %s', build_targets)
        if err.output:
            logging.error(err.output)
        return False


def _can_upload_to_result_server():
    """Return True if we can talk to result server."""
    # TODO: Also check if we have a slow connection to result server.
    if constants.RESULT_SERVER:
        try:
            from urllib.request import urlopen
            urlopen(constants.RESULT_SERVER,
                    timeout=constants.RESULT_SERVER_TIMEOUT).close()
            return True
        # pylint: disable=broad-except
        except Exception as err:
            logging.debug('Talking to result server raised exception: %s', err)
    return False


def get_result_server_args(for_test_mapping=False):
    """Return list of args for communication with result server.

    Args:
        for_test_mapping: True if the test run is for Test Mapping to include
            additional reporting args. Default is False.
    """
    # TODO (b/147644460) Temporarily disable Sponge V1 since it will be turned
    # down.
    if _can_upload_to_result_server():
        if for_test_mapping:
            return (constants.RESULT_SERVER_ARGS +
                    constants.TEST_MAPPING_RESULT_SERVER_ARGS)
        return constants.RESULT_SERVER_ARGS
    return []


def sort_and_group(iterable, key):
    """Sort and group helper function."""
    return itertools.groupby(sorted(iterable, key=key), key=key)


def is_test_mapping(args):
    """Check if the atest command intends to run tests in test mapping.

    When atest runs tests in test mapping, it must have at most one test
    specified. If a test is specified, it must be started with  `:`,
    which means the test value is a test group name in TEST_MAPPING file, e.g.,
    `:postsubmit`.

    If any test mapping options is specified, the atest command must also be
    set to run tests in test mapping files.

    Args:
        args: arg parsed object.

    Returns:
        True if the args indicates atest shall run tests in test mapping. False
        otherwise.
    """
    return (
        args.test_mapping or
        args.include_subdirs or
        not args.tests or
        (len(args.tests) == 1 and args.tests[0][0] == ':'))

@atest_decorator.static_var("cached_has_colors", {})
def _has_colors(stream):
    """Check the output stream is colorful.

    Args:
        stream: The standard file stream.

    Returns:
        True if the file stream can interpreter the ANSI color code.
    """
    cached_has_colors = _has_colors.cached_has_colors
    if stream in cached_has_colors:
        return cached_has_colors[stream]
    cached_has_colors[stream] = True
    # Following from Python cookbook, #475186
    if not hasattr(stream, "isatty"):
        cached_has_colors[stream] = False
        return False
    if not stream.isatty():
        # Auto color only on TTYs
        cached_has_colors[stream] = False
        return False
    try:
        import curses
        curses.setupterm()
        cached_has_colors[stream] = curses.tigetnum("colors") > 2
    # pylint: disable=broad-except
    except Exception as err:
        logging.debug('Checking colorful raised exception: %s', err)
        cached_has_colors[stream] = False
    return cached_has_colors[stream]


def colorize(text, color, highlight=False):
    """ Convert to colorful string with ANSI escape code.

    Args:
        text: A string to print.
        color: ANSI code shift for colorful print. They are defined
               in constants_default.py.
        highlight: True to print with highlight.

    Returns:
        Colorful string with ANSI escape code.
    """
    clr_pref = '\033[1;'
    clr_suff = '\033[0m'
    has_colors = _has_colors(sys.stdout)
    if has_colors:
        if highlight:
            ansi_shift = 40 + color
        else:
            ansi_shift = 30 + color
        clr_str = "%s%dm%s%s" % (clr_pref, ansi_shift, text, clr_suff)
    else:
        clr_str = text
    return clr_str


def colorful_print(text, color, highlight=False, auto_wrap=True):
    """Print out the text with color.

    Args:
        text: A string to print.
        color: ANSI code shift for colorful print. They are defined
               in constants_default.py.
        highlight: True to print with highlight.
        auto_wrap: If True, Text wraps while print.
    """
    output = colorize(text, color, highlight)
    if auto_wrap:
        print(output)
    else:
        print(output, end="")


def get_terminal_size():
    """Get terminal size and return a tuple.

    Returns:
        2 integers: the size of X(columns) and Y(lines/rows).
    """
    # Determine the width of the terminal. We'll need to clear this many
    # characters when carriage returning. Set default value as 80.
    columns, rows = shutil.get_terminal_size(
        fallback=(_DEFAULT_TERMINAL_WIDTH,
                  _DEFAULT_TERMINAL_HEIGHT))
    return columns, rows


def is_external_run():
    # TODO(b/133905312): remove this function after aidegen calling
    #       metrics_base.get_user_type directly.
    """Check is external run or not.

    Determine the internal user by passing at least one check:
      - whose git mail domain is from google
      - whose hostname is from google
    Otherwise is external user.

    Returns:
        True if this is an external run, False otherwise.
    """
    return metrics_base.get_user_type() == metrics_base.EXTERNAL_USER


def print_data_collection_notice():
    """Print the data collection notice."""
    anonymous = ''
    user_type = 'INTERNAL'
    if metrics_base.get_user_type() == metrics_base.EXTERNAL_USER:
        anonymous = ' anonymous'
        user_type = 'EXTERNAL'
    notice = ('  We collect%s usage statistics in accordance with our Content '
              'Licenses (%s), Contributor License Agreement (%s), Privacy '
              'Policy (%s) and Terms of Service (%s).'
             ) % (anonymous,
                  constants.CONTENT_LICENSES_URL,
                  constants.CONTRIBUTOR_AGREEMENT_URL[user_type],
                  constants.PRIVACY_POLICY_URL,
                  constants.TERMS_SERVICE_URL
                 )
    print(delimiter('=', 18, prenl=1))
    colorful_print("Notice:", constants.RED)
    colorful_print("%s" % notice, constants.GREEN)
    print(delimiter('=', 18, postnl=1))


def handle_test_runner_cmd(input_test, test_cmds, do_verification=False,
                           result_path=CMD_RESULT_PATH):
    """Handle the runner command of input tests.

    Args:
        input_test: A string of input tests pass to atest.
        test_cmds: A list of strings for running input tests.
        do_verification: A boolean to indicate the action of this method.
                         True: Do verification without updating result map and
                               raise DryRunVerificationError if verifying fails.
                         False: Update result map, if the former command is
                                different with current command, it will confirm
                                with user if they want to update or not.
        result_path: The file path for saving result.
    """
    full_result_content = {}
    if os.path.isfile(result_path):
        with open(result_path) as json_file:
            full_result_content = json.load(json_file)
    former_test_cmds = full_result_content.get(input_test, [])
    if not _are_identical_cmds(test_cmds, former_test_cmds):
        if do_verification:
            raise atest_error.DryRunVerificationError(
                'Dry run verification failed, former commands: {}'.format(
                    former_test_cmds))
        if former_test_cmds:
            # If former_test_cmds is different from test_cmds, ask users if they
            # are willing to update the result.
            print('Former cmds = %s' % former_test_cmds)
            print('Current cmds = %s' % test_cmds)
            try:
                from distutils import util
                if not util.strtobool(
                        input('Do you want to update former result '
                              'with the latest one?(Y/n)')):
                    print('SKIP updating result!!!')
                    return
            except ValueError:
                # Default action is updating the command result of the
                # input_test. If the user input is unrecognizable telling yes
                # or no, "Y" is implicitly applied.
                pass
    else:
        # If current commands are the same as the formers, no need to update
        # result.
        return
    full_result_content[input_test] = test_cmds
    with open(result_path, 'w') as outfile:
        json.dump(full_result_content, outfile, indent=0)
        print('Save result mapping to %s' % result_path)


def _are_identical_cmds(current_cmds, former_cmds):
    """Tell two commands are identical. Note that '--atest-log-file-path' is not
    considered a critical argument, therefore, it will be removed during
    the comparison. Also, atest can be ran in any place, so verifying relative
    path is regardless as well.

    Args:
        current_cmds: A list of strings for running input tests.
        former_cmds: A list of strings recorded from the previous run.

    Returns:
        True if both commands are identical, False otherwise.
    """
    def _normalize(cmd_list):
        """Method that normalize commands.

        Args:
            cmd_list: A list with one element. E.g. ['cmd arg1 arg2 True']

        Returns:
            A list with elements. E.g. ['cmd', 'arg1', 'arg2', 'True']
        """
        _cmd = ''.join(cmd_list).split()
        for cmd in _cmd:
            if cmd.startswith('--atest-log-file-path'):
                _cmd.remove(cmd)
                continue
            if _BUILD_CMD in cmd:
                _cmd.remove(cmd)
                _cmd.append(os.path.join('./', _BUILD_CMD))
                continue
        return _cmd

    _current_cmds = _normalize(current_cmds)
    _former_cmds = _normalize(former_cmds)
    # Always sort cmd list to make it comparable.
    _current_cmds.sort()
    _former_cmds.sort()
    return _current_cmds == _former_cmds

def _get_hashed_file_name(main_file_name):
    """Convert the input string to a md5-hashed string. If file_extension is
       given, returns $(hashed_string).$(file_extension), otherwise
       $(hashed_string).cache.

    Args:
        main_file_name: The input string need to be hashed.

    Returns:
        A string as hashed file name with .cache file extension.
    """
    hashed_fn = hashlib.md5(str(main_file_name).encode())
    hashed_name = hashed_fn.hexdigest()
    return hashed_name + '.cache'

def get_test_info_cache_path(test_reference, cache_root=TEST_INFO_CACHE_ROOT):
    """Get the cache path of the desired test_infos.

    Args:
        test_reference: A string of the test.
        cache_root: Folder path where stores caches.

    Returns:
        A string of the path of test_info cache.
    """
    return os.path.join(cache_root,
                        _get_hashed_file_name(test_reference))

def update_test_info_cache(test_reference, test_infos,
                           cache_root=TEST_INFO_CACHE_ROOT):
    """Update cache content which stores a set of test_info objects through
       pickle module, each test_reference will be saved as a cache file.

    Args:
        test_reference: A string referencing a test.
        test_infos: A set of TestInfos.
        cache_root: Folder path for saving caches.
    """
    if not os.path.isdir(cache_root):
        os.makedirs(cache_root)
    cache_path = get_test_info_cache_path(test_reference, cache_root)
    # Save test_info to files.
    try:
        with open(cache_path, 'wb') as test_info_cache_file:
            logging.debug('Saving cache %s.', cache_path)
            pickle.dump(test_infos, test_info_cache_file, protocol=2)
    except (pickle.PicklingError, TypeError, IOError) as err:
        # Won't break anything, just log this error, and collect the exception
        # by metrics.
        logging.debug('Exception raised: %s', err)
        metrics_utils.handle_exc_and_send_exit_event(
            constants.ACCESS_CACHE_FAILURE)


def load_test_info_cache(test_reference, cache_root=TEST_INFO_CACHE_ROOT):
    """Load cache by test_reference to a set of test_infos object.

    Args:
        test_reference: A string referencing a test.
        cache_root: Folder path for finding caches.

    Returns:
        A list of TestInfo namedtuple if cache found, else None.
    """
    cache_file = get_test_info_cache_path(test_reference, cache_root)
    if os.path.isfile(cache_file):
        logging.debug('Loading cache %s.', cache_file)
        try:
            with open(cache_file, 'rb') as config_dictionary_file:
                return pickle.load(config_dictionary_file, encoding='utf-8')
        except (pickle.UnpicklingError,
                ValueError,
                TypeError,
                EOFError,
                IOError) as err:
            # Won't break anything, just remove the old cache, log this error,
            # and collect the exception by metrics.
            logging.debug('Exception raised: %s', err)
            os.remove(cache_file)
            metrics_utils.handle_exc_and_send_exit_event(
                constants.ACCESS_CACHE_FAILURE)
    return None

def clean_test_info_caches(tests, cache_root=TEST_INFO_CACHE_ROOT):
    """Clean caches of input tests.

    Args:
        tests: A list of test references.
        cache_root: Folder path for finding caches.
    """
    for test in tests:
        cache_file = get_test_info_cache_path(test, cache_root)
        if os.path.isfile(cache_file):
            logging.debug('Removing cache: %s', cache_file)
            try:
                os.remove(cache_file)
            except IOError as err:
                logging.debug('Exception raised: %s', err)
                metrics_utils.handle_exc_and_send_exit_event(
                    constants.ACCESS_CACHE_FAILURE)

def get_modified_files(root_dir):
    """Get the git modified files. The git path here is git top level of
    the root_dir. It's inevitable to utilise different commands to fulfill
    2 scenario:
        1. locate unstaged/staged files
        2. locate committed files but not yet merged.
    the 'git_status_cmd' fulfils the former while the 'find_modified_files'
    fulfils the latter.

    Args:
        root_dir: the root where it starts finding.

    Returns:
        A set of modified files altered since last commit.
    """
    modified_files = set()
    try:
        find_git_cmd = 'cd {}; git rev-parse --show-toplevel'.format(root_dir)
        git_paths = subprocess.check_output(
            find_git_cmd, shell=True).decode().splitlines()
        for git_path in git_paths:
            # Find modified files from git working tree status.
            git_status_cmd = ("repo forall {} -c git status --short | "
                              "awk '{{print $NF}}'").format(git_path)
            modified_wo_commit = subprocess.check_output(
                git_status_cmd, shell=True).decode().rstrip().splitlines()
            for change in modified_wo_commit:
                modified_files.add(
                    os.path.normpath('{}/{}'.format(git_path, change)))
            # Find modified files that are committed but not yet merged.
            find_modified_files = _FIND_MODIFIED_FILES_CMDS.format(git_path)
            commit_modified_files = subprocess.check_output(
                find_modified_files, shell=True).decode().splitlines()
            for line in commit_modified_files:
                modified_files.add(os.path.normpath('{}/{}'.format(
                    git_path, line)))
    except (OSError, subprocess.CalledProcessError) as err:
        logging.debug('Exception raised: %s', err)
    return modified_files

def delimiter(char, length=_DEFAULT_TERMINAL_WIDTH, prenl=0, postnl=0):
    """A handy delimiter printer.

    Args:
        char: A string used for delimiter.
        length: An integer for the replication.
        prenl: An integer that insert '\n' before delimiter.
        postnl: An integer that insert '\n' after delimiter.

    Returns:
        A string of delimiter.
    """
    return prenl * '\n' + char * length + postnl * '\n'
