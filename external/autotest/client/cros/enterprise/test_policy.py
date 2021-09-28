# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import copy
import unittest

from autotest_lib.client.common_lib import error

from autotest_lib.client.cros.enterprise import policy

"""
This is the unittest file for policy.py.
If you modify that file, you should be at minimum re-running this file.

Add and correct tests as changes are made to the utils file.

To run the tests, use the following command from your DEV box (outside_chroot):

src/third_party/autotest/files/utils$ python unittest_suite.py \
autotest_lib.client.cros.enterprise.test_policy --debug

"""


class test_policyManager(unittest.TestCase):

    dict_value = {'scope': 'testScope', 'source': 'testSource',
                  'level': 'testLevel', 'value': 'test_value'}
    dict_value2 = {'scope': 'testScope', 'source': 'testSource',
                   'level': 'testLevel', 'value': 'test_value2'}

    def test_setting_params(self):
        test_policy = policy.Policy()
        test_policy.name = 'Test'
        test_policy.level = 1
        test_policy.value = 'TheValue'
        test_policy.scope = 'test_value'
        test_policy.source = 'Cloud'
        self.assertEqual(test_policy.name, 'Test')
        self.assertEqual(test_policy.level, 1)
        self.assertEqual(test_policy.scope, 'test_value')
        self.assertEqual(test_policy.source, 'Cloud')
        self.assertEqual(test_policy.value, 'TheValue')

    def test_setting_group_user(self):
        test_policy = policy.Policy()
        test_policy.name = 'Test'
        test_policy.group = 'user'
        self.assertEqual(test_policy.name, 'Test')
        self.assertEqual(test_policy.source, 'cloud')
        self.assertEqual(test_policy.level, 'mandatory')
        self.assertEqual(test_policy.scope, 'user')

    def test_setting_group_device(self):
        test_policy = policy.Policy()
        test_policy.name = 'Test'
        test_policy.group = 'device'
        self.assertEqual(test_policy.name, 'Test')
        self.assertEqual(test_policy.source, 'cloud')
        self.assertEqual(test_policy.level, 'mandatory')
        self.assertEqual(test_policy.scope, 'machine')

    def test_setting_group_suggested_user(self):
        test_policy = policy.Policy()
        test_policy.name = 'Test'
        test_policy.group = 'suggested_user'
        self.assertEqual(test_policy.name, 'Test')
        self.assertEqual(test_policy.source, 'cloud')
        self.assertEqual(test_policy.level, 'recommended')
        self.assertEqual(test_policy.scope, 'user')

    def test_setting_value(self):
        test_policy = policy.Policy()
        test_policy.name = 'Test'
        test_policydict = copy.deepcopy(self.dict_value)
        test_policydict['error'] = 'An error'
        test_policy.set_policy_from_dict(test_policydict)

        self.assertEqual(test_policy.name, 'Test')
        self.assertEqual(test_policy.level, 'testLevel')
        self.assertEqual(test_policy.scope, 'testScope')
        self.assertEqual(test_policy.source, 'testSource')
        self.assertEqual(test_policy.value, 'test_value')

    def test_get_policy_as_dict(self):
        test_policy = policy.Policy()
        test_policy.name = 'Test'
        test_policy.level = 1
        test_policy.value = 'TheValue'
        test_policy.scope = 'test_value'
        test_policy.source = 'Cloud'

        expected_dict = {
            'Test': {
                'scope': 'test_value',
                'level': 1,
                'value': 'TheValue',
                'source': 'Cloud'}}
        self.assertEqual(expected_dict, test_policy.get_policy_as_dict())

    def test_policy_eq_ne(self):
        test_policy = policy.Policy()
        test_policy.name = 'Test'
        test_policy.set_policy_from_dict(self.dict_value)

        test_policy2 = policy.Policy()
        test_policy2.name = 'Test2'
        test_policy2.set_policy_from_dict(self.dict_value)
        self.assertTrue(test_policy == test_policy2)
        self.assertFalse(test_policy != test_policy2)

        test_policy3 = policy.Policy()
        test_policy3.name = 'Test3'
        test_policy3.set_policy_from_dict(self.dict_value2)
        self.assertFalse(test_policy == test_policy3)

    def test_policy_eq_obfuscated(self):
        test_policy = policy.Policy()
        test_policy.name = 'Test'
        test_value = {
            'scope': 'testScope', 'source': 'testSource',
            'level': 'testLevel',
            'value': {'NetworkConfigurations': [
                {'WiFi': {'Passphrase': 'Secret'}}]}}
        test_policy.set_policy_from_dict(test_value)

        test_policy2 = policy.Policy()
        test_policy2.name = 'TestOpenNetworkConfiguration'
        obfuscated_value = {
            'scope': 'testScope', 'source': 'testSource',
            'level': 'testLevel',
            'value': {'NetworkConfigurations': [
                {'WiFi': {'Passphrase': '********'}}]}}
        test_policy2.set_policy_from_dict(obfuscated_value)
        self.assertTrue(test_policy == test_policy2)
        self.assertFalse(test_policy != test_policy2)

    def test_is_network_policy(self):
        self.assertTrue(
            policy.is_network_policy('TestOpenNetworkConfiguration'))

        self.assertFalse(policy.is_network_policy('Test'))

    def test_check_obfuscation(self):
        test_value = {
            'NetworkConfigurations': [
                {'WiFi': {'Passphrase': '********'}}]}
        self.assertTrue(policy.check_obfuscation(test_value))

        test_value2 = {
            'NetworkConfigurations': [
                {'WiFi': {'Passphrase': 'Bad'}}]}
        self.assertFalse(policy.check_obfuscation(test_value2))

        test_value3 = {
            'NetworkConfigurations': [
                {'WiFi': {'EAP': {'Password': '********'}}}]}
        self.assertTrue(policy.check_obfuscation(test_value3))

        test_value4 = {
            'NetworkConfigurations': [
                {'WiFi': {'EAP': {'Password': 'Bad'}}}]}
        self.assertFalse(policy.check_obfuscation(test_value4))

        test_value5 = {
            'Certificates': [
                {'PKCS12': '********'}]}
        self.assertTrue(policy.check_obfuscation(test_value5))

        test_value6 = {
            'Certificates': [
                {'PKCS12': 'BAD'}]}
        self.assertFalse(policy.check_obfuscation(test_value6))

    def test_invalid_policy_dict(self):
        with self.assertRaises(error.TestError) as context:
            test_policy = policy.Policy()
            test_policy.name = 'Bad Policy'
            test_policy.set_policy_from_dict({'Invalid Keys': 'Invalid Value'})
        self.assertIn('Incorrect input data provided', str(context.exception))


if __name__ == '__main__':
    unittest.main()
