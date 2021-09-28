#!/usr/bin/env python3
# Copyright 2019, The Android Open Source Project
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


import argparse
import os
import shlex
import subprocess
import sys


PREBUILTS_DIR = os.path.dirname(__file__)
PROJECTS = {'acloud', 'aidegen', 'atest'}
ARCHS = {'linux-x86', 'darwin-x86'}
SMOKE_TEST = 'smoke_tests'
EXIT_TEST_PASS = 0
EXIT_TEST_FAIL = 1
EXIT_INVALID_BINS = 2


def _get_prebuilt_bins():
    """Get asuite prebuilt binaries.

    Returns:
        A set of prebuilt binaries.
    """
    return {os.path.join(prj, arch, prj) for prj in PROJECTS for arch in ARCHS}


def _get_prebuilt_dirs():
    """Get asuite prebuilt directories.

    Returns:
        A set of prebuilt paths of binaries.
    """
    return {os.path.dirname(bin) for bin in _get_prebuilt_bins()}


def _get_smoke_tests_bins():
    """Get asuite smoke test scripts.

    Returns:
        A dict of project and smoke test script paths.
    """
    return {prj: os.path.join(prj, SMOKE_TEST) for prj in PROJECTS}


def _is_executable(bin_path):
    """Check if the given file is executable.

    Args:
        bin_path: a string of a file path.

    Returns:
        True if it is executable, false otherwise.
    """
    return os.access(bin_path, os.X_OK)


def check_uploaded_bins(preupload_files):
    """This method validates the uploaded files.

    If the uploaded file is in prebuilt_bins, ensure:
        - it is executable.
        - only one at a time.
    If the uploaded file is a smoke_test script, ensure:
        - it is executable.
    If the uploaded file is placed in prebuilt_dirs, ensure:
        - it is not executable.
        (It is to ensure PATH is not contaminated.
         e.g. atest/linux-x86/atest-dev will override $OUT/bin/atest-dev, or
         atest/linux-x86/rm does fraud/harmful things.)

    Args:
        preupload_files: A list of preuploaded files.

    Returns:
        True is the above criteria are all fulfilled, otherwise None.
    """
    prebuilt_bins = _get_prebuilt_bins()
    prebuilt_dirs = _get_prebuilt_dirs()
    smoke_tests_bins = _get_smoke_tests_bins().values()
    # Store valid executables.
    target_bins = set()
    # Unexpected executable files which may cause issues(they are in $PATH).
    illegal_bins = set()
    # Store prebuilts or smoke test script that are inexecutable.
    insufficient_perm_bins = set()
    for f in preupload_files:
        # Ensure target_bins are executable.
        if f in prebuilt_bins:
            if _is_executable(f):
                target_bins.add(f)
            else:
                insufficient_perm_bins.add(f)
        # Ensure smoke_tests scripts are executable.
        elif f in smoke_tests_bins and not _is_executable(f):
            insufficient_perm_bins.add(f)
        # Avoid fraud commands in $PATH. e.g. atest/linux-x86/rm.
        # must not be executable.
        elif os.path.dirname(f) in prebuilt_dirs and _is_executable(f):
            illegal_bins.add(f)
    if len(target_bins) > 1:
        print('\nYou\'re uploading multiple binaries: %s'
              % ' '.join(target_bins))
        print('\nPlease upload one prebuilt at a time.')
        return False
    if insufficient_perm_bins:
        print('\nInsufficient permission found: %s'
              % ' '.join(insufficient_perm_bins))
        print('\nPlease run:\n\tchmod 0755 %s\nand try again.'
              % ' '.join(insufficient_perm_bins))
        return False
    if illegal_bins:
        illegal_dirs = {os.path.dirname(bin) for bin in illegal_bins}
        print('\nIt is forbidden to upload executable file: %s'
              % '\n - %s\n' % '\n - '.join(illegal_bins))
        print('Because they are in the project paths: %s'
              % '\n - %s\n' % '\n - '.join(illegal_dirs))
        print('Please remove the binaries or make the files non-executable.')
        return False
    return True


def run_smoke_tests_pass(files_to_check):
    """Run smoke tests.

    Args:
        files_to_check: A list of preuploaded files to check.

    Returns:
        True when test passed or no need to test.
        False when test failed.
    """
    for target in files_to_check:
        if target in _get_prebuilt_bins():
            project = os.path.basename(target)
            test_file = _get_smoke_tests_bins().get(project)
            if os.path.exists(test_file):
                try:
                    subprocess.check_output(test_file, encoding='utf-8',
                                            stderr=subprocess.STDOUT)
                except subprocess.CalledProcessError as error:
                    print('Smoke tests failed at:\n\n%s' % error.output)
                    return False
                except OSError as oserror:
                    print('%s: Missing the header of the script.' % oserror)
                    print('Please define shebang like:\n')
                    print('#!/usr/bin/env bash\nor')
                    print('#!/usr/bin/env python3\n')
                    return False
    return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--skip-smoke-test', '-s', action="store_true",
                        help='Disable smoke testing.')
    parser.add_argument('preupload_files', nargs='*', help='Files to be uploaded.')
    args = parser.parse_args()
    files_to_check = args.preupload_files
    # Pre-process files_to_check(run directly by users.)
    if not files_to_check:
        # Only consider added(A), renamed(R) and modified(M) files.
        cmd = "git status --short | egrep ^[ARM] | awk '{print $NF}'"
        preupload_files = subprocess.check_output(cmd, shell=True,
                                                 encoding='utf-8').splitlines()
        if preupload_files:
            print('validating: %s' % preupload_files)
            files_to_check = preupload_files
    # Validating uploaded files and run smoke test script(run by repohook).
    if not check_uploaded_bins(files_to_check):
        sys.exit(EXIT_INVALID_BINS)
    if not args.skip_smoke_test and not run_smoke_tests_pass(files_to_check):
        sys.exit(EXIT_TEST_FAIL)
    sys.exit(EXIT_TEST_PASS)
