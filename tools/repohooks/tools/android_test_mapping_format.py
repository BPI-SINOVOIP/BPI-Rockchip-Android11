#!/usr/bin/env python3
# Copyright 2018 The Android Open Source Project
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

"""Validate TEST_MAPPING files in Android source code.

The goal of this script is to validate the format of TEST_MAPPING files:
1. It must be a valid json file.
2. Each test group must have a list of test that containing name and options.
3. Each import must have only one key `path` and one value for the path to
   import TEST_MAPPING files.
"""

import argparse
import json
import os
import re
import sys

_path = os.path.realpath(__file__ + '/../..')
if sys.path[0] != _path:
    sys.path.insert(0, _path)
del _path

# We have to import our local modules after the sys.path tweak.  We can't use
# relative imports because this is an executable program, not a module.
# pylint: disable=wrong-import-position
import rh.git

IMPORTS = 'imports'
NAME = 'name'
OPTIONS = 'options'
PATH = 'path'
HOST = 'host'
PREFERRED_TARGETS = 'preferred_targets'
FILE_PATTERNS = 'file_patterns'
TEST_MAPPING_URL = (
    'https://source.android.com/compatibility/tests/development/'
    'test-mapping')

# Pattern used to identify line-level '//'-format comment in TEST_MAPPING file.
_COMMENTS_RE = re.compile(r'^\s*//')


class Error(Exception):
    """Base exception for all custom exceptions in this module."""


class InvalidTestMappingError(Error):
    """Exception to raise when detecting an invalid TEST_MAPPING file."""


def filter_comments(json_data):
    """Remove '//'-format comments in TEST_MAPPING file to valid format.

    Args:
        json_data: TEST_MAPPING file content (as a string).

    Returns:
        Valid json string without comments.
    """
    return ''.join('\n' if _COMMENTS_RE.match(x) else x for x in
                   json_data.splitlines())


def _validate_import(entry, test_mapping_file):
    """Validate an import setting.

    Args:
        entry: A dictionary of an import setting.
        test_mapping_file: Path to the TEST_MAPPING file to be validated.

    Raises:
        InvalidTestMappingError: if the import setting is invalid.
    """
    if len(entry) != 1:
        raise InvalidTestMappingError(
            'Invalid import config in test mapping file %s. each import can '
            'only have one `path` setting. Failed entry: %s' %
            (test_mapping_file, entry))
    if list(entry.keys())[0] != PATH:
        raise InvalidTestMappingError(
            'Invalid import config in test mapping file %s. import can only '
            'have one `path` setting. Failed entry: %s' %
            (test_mapping_file, entry))


def _validate_test(test, test_mapping_file):
    """Validate a test declaration.

    Args:
        entry: A dictionary of a test declaration.
        test_mapping_file: Path to the TEST_MAPPING file to be validated.

    Raises:
        InvalidTestMappingError: if the a test declaration is invalid.
    """
    if NAME not in test:
        raise InvalidTestMappingError(
            'Invalid test config in test mapping file %s. test config must '
            'a `name` setting. Failed test config: %s' %
            (test_mapping_file, test))
    if not isinstance(test.get(HOST, False), bool):
        raise InvalidTestMappingError(
            'Invalid test config in test mapping file %s. `host` setting in '
            'test config can only have boolean value of `true` or `false`. '
            'Failed test config: %s' % (test_mapping_file, test))
    preferred_targets = test.get(PREFERRED_TARGETS, [])
    if (not isinstance(preferred_targets, list) or
            any(not isinstance(t, str) for t in preferred_targets)):
        raise InvalidTestMappingError(
            'Invalid test config in test mapping file %s. `preferred_targets` '
            'setting in test config can only be a list of strings. Failed test '
            'config: %s' % (test_mapping_file, test))
    file_patterns = test.get(FILE_PATTERNS, [])
    if (not isinstance(file_patterns, list) or
            any(not isinstance(p, str) for p in file_patterns)):
        raise InvalidTestMappingError(
            'Invalid test config in test mapping file %s. `file_patterns` '
            'setting in test config can only be a list of strings. Failed test '
            'config: %s' % (test_mapping_file, test))
    for option in test.get(OPTIONS, []):
        if len(option) != 1:
            raise InvalidTestMappingError(
                'Invalid option setting in test mapping file %s. each option '
                'setting can only have one key-val setting. Failed entry: %s' %
                (test_mapping_file, option))


def _load_file(test_mapping_file):
    """Load a TEST_MAPPING file as a json file."""
    try:
        return json.loads(filter_comments(test_mapping_file))
    except ValueError as e:
        # The file is not a valid JSON file.
        print(
            'Failed to parse JSON file %s, error: %s' % (test_mapping_file, e),
            file=sys.stderr)
        raise


def process_file(test_mapping_file):
    """Validate a TEST_MAPPING file."""
    test_mapping = _load_file(test_mapping_file)
    # Validate imports.
    for import_entry in test_mapping.get(IMPORTS, []):
        _validate_import(import_entry, test_mapping_file)
    # Validate tests.
    all_tests = [test for group, tests in test_mapping.items()
                 if group != IMPORTS for test in tests]
    for test in all_tests:
        _validate_test(test, test_mapping_file)


def get_parser():
    """Return a command line parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', type=str,
                        help='Specify the commit to validate.')
    parser.add_argument('project_dir')
    parser.add_argument('files', nargs='+')
    return parser


def main(argv):
    parser = get_parser()
    opts = parser.parse_args(argv)
    try:
        for filename in opts.files:
            if opts.commit:
                json_data = rh.git.get_file_content(opts.commit, filename)
            else:
                with open(os.path.join(opts.project_dir, filename)) as f:
                    json_data = f.read()
            process_file(json_data)
    except:
        print('Visit %s for details about the format of TEST_MAPPING '
              'file.' % TEST_MAPPING_URL, file=sys.stderr)
        raise


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
