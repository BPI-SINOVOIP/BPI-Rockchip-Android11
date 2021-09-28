# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import ast
import copy
from mock import patch
import os
import unittest

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_utils as epu


"""

This is the unittest file for enterprise_policy_utils.
If you modify that file, you should be at minimum re-running this file.

Add and correct tests as changes are made to the utils file.

To run the tests, use the following command from your DEV box (outside_chroot):

src/third_party/autotest/files/utils$ python unittest_suite.py \
autotest_lib.client.cros.enterprise.enterprise_policy_utils_unittest --debug

Most of the test data are large dictionaries mocking real data. They are stored
in the ent_policy_unittest_data file (located in this directory).

"""

# Load the test data
TEST_DATA = {}
folderDir = os.path.join(os.path.dirname(__file__))
fileName = 'ent_policy_unittest_data'
fullPath = os.path.join(folderDir, fileName)

with open(fullPath) as t:
    f = (t.readlines())
    for variable in f:
        name, data = variable.split('=')
        TEST_DATA[name] = ast.literal_eval(data)

# Set the base path for the Mock
PATCH_BASE = 'autotest_lib.client.cros.enterprise.enterprise_policy_utils'


class TestPolicyUtils_get_all_policies(unittest.TestCase):
    """
    Test the "get_all_policies" function.

    Mock the reply from the API with an example policy.

    """
    FX_NAME = '_get_pol_from_api'
    PATCH_PATH = '{}.{}'.format(PATCH_BASE, FX_NAME)

    @patch(PATCH_PATH)
    def test_Normal(self, get_pol_mock):
        get_pol_mock.return_value = TEST_DATA['TEST_RAW']
        self.assertEqual(epu.get_all_policies(None), TEST_DATA['TEST_RAW'])

    @patch(PATCH_PATH)
    def test_NoAPIResponse(self, get_pol_mock):
        get_pol_mock.return_value = None
        self.assertRaises(error.TestError, epu.get_all_policies, None)


class TestPolicyUtils_reformat_policies(unittest.TestCase):
    """Test the _reformat_policies function and the following private
        methods:
            _remove_visual_formatting

    """

    def test_NormalData(self):
        # Tests a policy with a "chromePolicies" and 1 "extensionPolicies".
        subtest_data = copy.deepcopy(TEST_DATA['TEST_RAW'])
        epu._reformat_policies(subtest_data)
        self.assertEqual(subtest_data, TEST_DATA['TEST_RAW'])

    def test_no_data(self):
        subtest_data = {}
        epu._reformat_policies(subtest_data)
        self.assertEqual(subtest_data, {})

        subtest_data2 = {'deviceLocalAccountPolicies': {},
                         'extensionPolicies': {},
                         'chromePolicies': {}}
        expected = copy.deepcopy(subtest_data2)
        epu._reformat_policies(subtest_data2)
        self.assertEqual(subtest_data2, expected)

    def test_partial_data(self):
        # "chromePolicies" contains data, "extensionPolicies" has an extension
        # with no policies.
        subtest_data = copy.deepcopy(TEST_DATA['PARTIAL_RAW'])
        epu._reformat_policies(subtest_data)
        self.assertEqual(subtest_data, TEST_DATA['PARTIAL_RAW'])

    def test_mult_extension(self):
        subtest_data = copy.deepcopy(TEST_DATA['TEST_MULTI_EXT'])
        epu._reformat_policies(subtest_data)
        self.assertEqual(subtest_data, TEST_DATA['TEST_MULTI_EXT'])

    def test_unicode_dict(self):
        # Specifically will check if the _remove_visual_formatting
        # function will remove visual formatting. e.g. "\n        "
        subtest_data = copy.deepcopy(TEST_DATA['POL_WITH_UNICODE'])
        epu._reformat_policies(subtest_data)
        self.assertEqual(subtest_data, TEST_DATA['POL_WITH_UNICODE_PARSED'])

    def test_string_value(self):
        # Checks that a unicode string is not modified.
        subtest_data = copy.deepcopy(TEST_DATA['POL_WITH_STRING'])
        epu._reformat_policies(subtest_data)
        self.assertEqual(subtest_data, TEST_DATA['POL_WITH_STRING'])


if __name__ == '__main__':
    unittest.main()
