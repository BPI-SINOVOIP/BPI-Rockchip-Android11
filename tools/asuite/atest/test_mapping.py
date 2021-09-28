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

"""Classes for test mapping related objects."""


import copy
import fnmatch
import os
import re

import atest_utils
import constants

TEST_MAPPING = 'TEST_MAPPING'


class TestDetail:
    """Stores the test details set in a TEST_MAPPING file."""

    def __init__(self, details):
        """TestDetail constructor

        Parse test detail from a dictionary, e.g.,
        {
          "name": "SettingsUnitTests",
          "host": true,
          "options": [
            {
              "instrumentation-arg":
                  "annotation=android.platform.test.annotations.Presubmit"
            },
          "file_patterns": ["(/|^)Window[^/]*\\.java",
                           "(/|^)Activity[^/]*\\.java"]
        }

        Args:
            details: A dictionary of test detail.
        """
        self.name = details['name']
        self.options = []
        # True if the test should run on host and require no device.
        self.host = details.get('host', False)
        assert isinstance(self.host, bool), 'host can only have boolean value.'
        options = details.get('options', [])
        for option in options:
            assert len(option) == 1, 'Each option can only have one key.'
            self.options.append(copy.deepcopy(option).popitem())
        self.options.sort(key=lambda o: o[0])
        self.file_patterns = details.get('file_patterns', [])

    def __str__(self):
        """String value of the TestDetail object."""
        host_info = (', runs on host without device required.' if self.host
                     else '')
        if not self.options:
            return self.name + host_info
        options = ''
        for option in self.options:
            options += '%s: %s, ' % option

        return '%s (%s)%s' % (self.name, options.strip(', '), host_info)

    def __hash__(self):
        """Get the hash of TestDetail based on the details"""
        return hash(str(self))

    def __eq__(self, other):
        return str(self) == str(other)


class Import:
    """Store test mapping import details."""

    def __init__(self, test_mapping_file, details):
        """Import constructor

        Parse import details from a dictionary, e.g.,
        {
            "path": "..\folder1"
        }
        in which, project is the name of the project, by default it's the
        current project of the containing TEST_MAPPING file.

        Args:
            test_mapping_file: Path to the TEST_MAPPING file that contains the
                import.
            details: A dictionary of details about importing another
                TEST_MAPPING file.
        """
        self.test_mapping_file = test_mapping_file
        self.path = details['path']

    def __str__(self):
        """String value of the Import object."""
        return 'Source: %s, path: %s' % (self.test_mapping_file, self.path)

    def get_path(self):
        """Get the path to TEST_MAPPING import directory."""
        path = os.path.realpath(os.path.join(
            os.path.dirname(self.test_mapping_file), self.path))
        if os.path.exists(path):
            return path
        root_dir = os.environ.get(constants.ANDROID_BUILD_TOP, os.sep)
        path = os.path.realpath(os.path.join(root_dir, self.path))
        if os.path.exists(path):
            return path
        # The import path can't be located.
        return None


def is_match_file_patterns(test_mapping_file, test_detail):
    """Check if the changed file names match the regex pattern defined in
    file_patterns of TEST_MAPPING files.

    Args:
        test_mapping_file: Path to a TEST_MAPPING file.
        test_detail: A TestDetail object.

    Returns:
        True if the test's file_patterns setting is not set or contains a
        pattern matches any of the modified files.
    """
    # Only check if the altered files are located in the same or sub directory
    # of the TEST_MAPPING file. Extract the relative path of the modified files
    # which match file patterns.
    file_patterns = test_detail.get('file_patterns', [])
    if not file_patterns:
        return True
    test_mapping_dir = os.path.dirname(test_mapping_file)
    modified_files = atest_utils.get_modified_files(test_mapping_dir)
    if not modified_files:
        return False
    modified_files_in_source_dir = [
        os.path.relpath(filepath, test_mapping_dir)
        for filepath in fnmatch.filter(modified_files,
                                       os.path.join(test_mapping_dir, '*'))
    ]
    for modified_file in modified_files_in_source_dir:
        # Force to run the test if it's in a TEST_MAPPING file included in the
        # changesets.
        if modified_file == constants.TEST_MAPPING:
            return True
        for pattern in file_patterns:
            if re.search(pattern, modified_file):
                return True
    return False
