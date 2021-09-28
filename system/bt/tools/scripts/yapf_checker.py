#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

import logging
import os
import subprocess
import sys

PYTHONPATH_KEY = 'PYTHONPATH'
COMMIT_ID_ENV_KEY = 'PREUPLOAD_COMMIT'
ANDROID_BUILD_TOP_KEY = 'ANDROID_BUILD_TOP'
DEFAULT_YAPF_DIR = 'external/yapf'
GIT_COMMAND = ['git', 'diff-tree', '--no-commit-id', '--name-only']


def main():
    """
    A Python commit formatter using YAPF

    Caveat: if you modify a file, the entire file will be formatted, instead of
            the diff
    :return:
    """
    if COMMIT_ID_ENV_KEY not in os.environ:
        logging.error('Missing PREUPLOAD_COMMIT in environment.')
        exit(1)

    if ANDROID_BUILD_TOP_KEY not in os.environ:
        logging.error('Missing ANDROID_BUILD_TOP in environment.')
        exit(1)

    # Gather changed Python files
    commit_id = os.environ[COMMIT_ID_ENV_KEY]
    full_git_command = GIT_COMMAND + ['-r', commit_id]
    files = subprocess.check_output(full_git_command).decode(
        'utf-8').splitlines()
    full_files = [os.path.abspath(f) for f in files if f.endswith('.py')]
    if not full_files:
        return

    # Find yapf in Android code tree
    yapf_dir = os.path.join(os.environ[ANDROID_BUILD_TOP_KEY], DEFAULT_YAPF_DIR)
    yapf_binary = os.path.join(yapf_dir, 'yapf')

    # Run YAPF
    full_yapf_command = [
        "%s=$%s:%s" % (PYTHONPATH_KEY, PYTHONPATH_KEY, yapf_dir), 'python3',
        yapf_binary, '-d', '-p'
    ] + full_files
    environment = os.environ.copy()
    environment[PYTHONPATH_KEY] = environment[PYTHONPATH_KEY] + ":" + yapf_dir
    result = subprocess.run(
        full_yapf_command[1:],
        env=environment,
        stderr=subprocess.STDOUT,
        stdout=subprocess.PIPE)

    if result.returncode != 0 or result.stdout:
        logging.error(result.stdout.decode('utf-8').strip())
        logging.error('INVALID FORMATTING, return code %d', result.returncode)
        logging.error('To re-run the format command:\n\n'
                      '    %s\n' % ' '.join(full_yapf_command))
        yapf_inplace_format = ' '.join([
            "%s=$%s:%s" % (PYTHONPATH_KEY, PYTHONPATH_KEY,
                           yapf_dir), 'python3', yapf_binary, '-p', '-i'
        ] + full_files)
        logging.error('If this is a legitimate format error, please run:\n\n'
                      '    %s\n' % yapf_inplace_format)
        logging.error(
            'CAVEAT: Currently, this format the entire Python file if you modify even part of it'
        )
        exit(1)


if __name__ == '__main__':
    main()
