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

import os
import shutil
import tempfile
import unittest

import android_test_mapping_format


VALID_TEST_MAPPING = r"""
{
  "presubmit": [
    {
      "name": "CtsWindowManagerDeviceTestCases",
      "options": [
        {
          "include-annotation": "android.platform.test.annotations.Presubmit"
        }
      ]
    }
  ],
  "postsubmit": [
    {
      "name": "CtsWindowManagerDeviceTestCases",
      "host": true,
      "preferred_targets": ["a", "b"],
      "file_patterns": [".*\\.java"]
    }
  ],
  "imports": [
    {
      "path": "frameworks/base/services/core/java/com/android/server/am"
    },
    {
      "path": "frameworks/base/services/core/java/com/android/server/wm"
    }
  ]
}
"""

BAD_JSON = """
{wrong format}
"""

BAD_TEST_WRONG_KEY = """
{
  "presubmit": [
    {
      "bad_name": "CtsWindowManagerDeviceTestCases"
    }
  ]
}
"""

BAD_TEST_WRONG_HOST_VALUE = """
{
  "presubmit": [
    {
      "name": "CtsWindowManagerDeviceTestCases",
      "host": "bad_value"
    }
  ]
}
"""


BAD_TEST_WRONG_PREFERRED_TARGETS_VALUE_NONE_LIST = """
{
  "presubmit": [
    {
      "name": "CtsWindowManagerDeviceTestCases",
      "preferred_targets": "bad_value"
    }
  ]
}
"""

BAD_TEST_WRONG_PREFERRED_TARGETS_VALUE_WRONG_TYPE = """
{
  "presubmit": [
    {
      "name": "CtsWindowManagerDeviceTestCases",
      "preferred_targets": ["bad_value", 123]
    }
  ]
}
"""

BAD_TEST_WRONG_OPTION = """
{
  "presubmit": [
    {
      "name": "CtsWindowManagerDeviceTestCases",
      "options": [
        {
          "include-annotation": "android.platform.test.annotations.Presubmit",
          "bad_option": "some_name"
        }
      ]
    }
  ]
}
"""

BAD_IMPORT_WRONG_KEY = """
{
  "imports": [
    {
      "name": "frameworks/base/services/core/java/com/android/server/am"
    }
  ]
}
"""

BAD_IMPORT_WRONG_IMPORT_VALUE = """
{
  "imports": [
    {
      "path": "frameworks/base/services/core/java/com/android/server/am",
      "option": "something"
    }
  ]
}
"""

BAD_FILE_PATTERNS = """
{
  "presubmit": [
    {
      "name": "CtsWindowManagerDeviceTestCases",
      "file_patterns": ["pattern", 123]
    }
  ]
}
"""

TEST_MAPPING_WITH_SUPPORTED_COMMENTS = r"""
// supported comment
{
  // supported comment!@#$%^&*()_
  "presubmit": [
    {
      "name": "CtsWindowManagerDeviceTestCases\"foo//baz",
      "options": [
        // supported comment!@#$%^&*()_
        {
          "include-annotation": "android.platform.test.annotations.Presubmit"
        }
      ]
    }
  ],
  "imports": [
    {
      "path": "path1//path2//path3"
    }
  ]
}
"""

TEST_MAPPING_WITH_NON_SUPPORTED_COMMENTS = """
{ #non-supported comments
  // supported comments
  "presubmit": [#non-supported comments
    {  // non-supported comments
      "name": "CtsWindowManagerDeviceTestCases",
    }
  ]
}
"""


class AndroidTestMappingFormatTests(unittest.TestCase):
    """Unittest for android_test_mapping_format module."""

    def setUp(self):
        self.tempdir = tempfile.mkdtemp()
        self.test_mapping_file = os.path.join(self.tempdir, 'TEST_MAPPING')

    def tearDown(self):
        shutil.rmtree(self.tempdir)

    def test_valid_test_mapping(self):
        """Verify that the check doesn't raise any error for valid test mapping.
        """
        with open(self.test_mapping_file, 'w') as f:
            f.write(VALID_TEST_MAPPING)
        with open(self.test_mapping_file, 'r') as f:
            android_test_mapping_format.process_file(f.read())

    def test_invalid_test_mapping_bad_json(self):
        """Verify that TEST_MAPPING file with bad json can be detected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_JSON)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                ValueError, android_test_mapping_format.process_file,
                f.read())

    def test_invalid_test_mapping_wrong_test_key(self):
        """Verify that test config using wrong key can be detected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_TEST_WRONG_KEY)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                android_test_mapping_format.InvalidTestMappingError,
                android_test_mapping_format.process_file,
                f.read())

    def test_invalid_test_mapping_wrong_test_value(self):
        """Verify that test config using wrong host value can be detected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_TEST_WRONG_HOST_VALUE)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                android_test_mapping_format.InvalidTestMappingError,
                android_test_mapping_format.process_file,
                f.read())

    def test_invalid_test_mapping_wrong_preferred_targets_value(self):
        """Verify invalid preferred_targets are rejected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_TEST_WRONG_PREFERRED_TARGETS_VALUE_NONE_LIST)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                android_test_mapping_format.InvalidTestMappingError,
                android_test_mapping_format.process_file,
                f.read())
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_TEST_WRONG_PREFERRED_TARGETS_VALUE_WRONG_TYPE)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                android_test_mapping_format.InvalidTestMappingError,
                android_test_mapping_format.process_file,
                f.read())

    def test_invalid_test_mapping_wrong_test_option(self):
        """Verify that test config using wrong option can be detected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_TEST_WRONG_OPTION)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                android_test_mapping_format.InvalidTestMappingError,
                android_test_mapping_format.process_file,
                f.read())

    def test_invalid_test_mapping_wrong_import_key(self):
        """Verify that import setting using wrong key can be detected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_IMPORT_WRONG_KEY)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                android_test_mapping_format.InvalidTestMappingError,
                android_test_mapping_format.process_file,
                f.read())

    def test_invalid_test_mapping_wrong_import_value(self):
        """Verify that import setting using wrong value can be detected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_IMPORT_WRONG_IMPORT_VALUE)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                android_test_mapping_format.InvalidTestMappingError,
                android_test_mapping_format.process_file,
                f.read())

    def test_invalid_test_mapping_file_patterns_value(self):
        """Verify that file_patterns using wrong value can be detected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(BAD_FILE_PATTERNS)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                android_test_mapping_format.InvalidTestMappingError,
                android_test_mapping_format.process_file,
                f.read())

    def test_valid_test_mapping_file_with_supported_comments(self):
        """Verify that '//'-format comment can be filtered."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(TEST_MAPPING_WITH_SUPPORTED_COMMENTS)
        with open(self.test_mapping_file, 'r') as f:
            android_test_mapping_format.process_file(f.read())

    def test_valid_test_mapping_file_with_non_supported_comments(self):
        """Verify that non-supported comment can be detected."""
        with open(self.test_mapping_file, 'w') as f:
            f.write(TEST_MAPPING_WITH_NON_SUPPORTED_COMMENTS)
        with open(self.test_mapping_file, 'r') as f:
            self.assertRaises(
                ValueError, android_test_mapping_format.process_file,
                f.read())


if __name__ == '__main__':
    unittest.main()
