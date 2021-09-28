# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
from mock import patch
import unittest


from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import policy_group
from autotest_lib.client.cros.enterprise import policy_manager

"""
This is the unittest file for policy_manager.py.
If you modify that file, you should be at minimum re-running this file.

Add and correct tests as changes are made to the utils file.

To run the tests, use the following command from your DEV box (outside_chroot):

src/third_party/autotest/files/utils$ python unittest_suite.py \
autotest_lib.client.cros.enterprise.policy_manager_unittest --debug

"""
FX_NAME = '_get_pol_from_api'
PATCH_BASE = 'autotest_lib.client.cros.enterprise.enterprise_policy_utils'
PATCH_PATH = '{}.{}'.format(PATCH_BASE, FX_NAME)


class TestPolicyManager(unittest.TestCase):

    def configPolicyManager(self, username=None):
        self.policy_manager = policy_manager.Policy_Manager(username)
        self.policy_manager.autotest_ext = 'Demo'
        self.maxDiff = None
        self.expected = {'deviceLocalAccountPolicies': {},
                         'extensionPolicies': {
                             'ExtID1':
                                 {'Extension Policy 1':
                                     {'scope': 'user',
                                      'level': 'mandatory',
                                      'value': 'EP1',
                                      'source': 'cloud'}}},
                         'chromePolicies':
                             {'Suggested Policy 1':
                                 {'scope': 'user',
                                  'level': 'recommended',
                                  'value': 'Suggested 1',
                                  'source': 'cloud'},
                              'Test Policy1':
                                 {'scope': 'user',
                                  'level': 'mandatory',
                                  'value': 'Policy1',
                                  'source': 'cloud'},
                               'Device Policy1':
                                 {'scope': 'machine',
                                  'level': 'mandatory',
                                  'value': 'Device 1', 'source': 'cloud'}}}

    def test_Defaults(self):
        self.configPolicyManager()
        self.assertEqual(type(self.policy_manager._configured),
                         policy_group.AllPolicies)
        self.assertEqual(type(self.policy_manager._obtained),
                         policy_group.AllPolicies)

    def test_configure_policies_and_get_configured_as_dict(self):
        self.configPolicyManager()
        self.policy_manager.configure_policies(
            user={'Test Policy1': 'Policy1'},
            device={'Device Policy1': 'Device 1'},
            suggested_user={'Suggested Policy 1': 'Suggested 1'},
            extension={'ExtID1': {'Extension Policy 1': 'EP1'}})
        configured = self.policy_manager.get_configured_policies_as_dict()
        self.assertEqual(configured, self.expected)

    @patch(PATCH_PATH)
    def test_obtain_policies_from_device_and_get_obtained_policies_as_dict(
            self, get_pol_mock):
        self.configPolicyManager()
        get_pol_mock.return_value = self.expected
        self.policy_manager.obtain_policies_from_device()
        received = self.policy_manager.get_obtained_policies_as_dict()
        self.assertEqual(received, self.expected)

    def test_changing_policy(self):
        """Test setting a policy to True, then changing the value"""
        self.policy_manager = policy_manager.Policy_Manager()

        self.policy_manager.configure_policies(user={'TP1': False})
        configured = self.policy_manager.get_configured_policies_as_dict()
        self.assertEqual(configured['chromePolicies']['TP1']['value'], False)

        # Now set the policy to False
        self.policy_manager.configure_policies(user={'TP1': True}, new=False)
        configured = self.policy_manager.get_configured_policies_as_dict()
        self.assertEqual(configured['chromePolicies']['TP1']['value'], True)

    def test_remove_policy(self):
        """
        Will test remove_policy and the private methods:
            _removeChromePolicy
            _removeExtensionPolicy

        """
        # Setup the policy Manager
        self.policy_manager = policy_manager.Policy_Manager()
        self.policy_manager.configure_policies(
            user={'Test Policy1': 'Policy1', 'Test Policy2': 'Policy2'})

        # Remove one policy. Verify it is gone, but the other is there.
        self.policy_manager.remove_policy('Test Policy2', 'user')
        self.assertNotIn('Test Policy2', self.policy_manager._configured.chrome)
        self.assertIn('Test Policy1', self.policy_manager._configured.chrome)

        # Add an extension Policy. Verify it is there, then remove and verify.
        self.policy_manager.configure_policies(extension={'ID1': {'P1': 'V1'}},
                                               new=False)

        self.assertIn('ID1', self.policy_manager._configured.extension_configured_data)
        self.assertIn('P1', self.policy_manager._configured.extension_configured_data['ID1'])
        self.policy_manager.remove_policy('P1', 'extension', extID='ID1')
        self.assertNotIn('P1', self.policy_manager._configured.extension_configured_data['ID1'])

        # Attempt to remove non-existant policies. Verify an error is raised
        with self.assertRaises(error.TestError) as context:
            self.policy_manager.remove_policy('Test Policy2', 'user')
        self.assertEqual(str(context.exception),
                         'Policy Test Policy2 missing from chrome policies.')

        # Attempt to remove an extension policy without an ID and non-existant
        # policy in the valid extension
        with self.assertRaises(error.TestError) as context:
            self.policy_manager.remove_policy('P1', 'extension')
        self.assertEqual(str(context.exception),
                         'Cannot delete extension policy without extension ID')

        with self.assertRaises(error.TestError) as context:
            self.policy_manager.remove_policy('P2', 'extension', 'ID1')
        self.assertEqual(str(context.exception),
                         'Policy P2 missing from extension policies.')

    @patch(PATCH_PATH)
    def test_verify_policies(self, get_pol_mock):
        """Test the verify_policies method."""
        self.configPolicyManager()
        self.policy_manager.configure_policies(
            user={'Test Policy1': 'Policy1'},
            device={'Device Policy1': 'Device 1'},
            suggested_user={'Suggested Policy 1': 'Suggested 1'},
            extension={'ExtID1': {'Extension Policy 1': 'EP1'}})
        get_pol_mock.return_value = self.expected
        self.policy_manager.obtain_policies_from_device()
        self.policy_manager.verify_policies()

        # Add another policy to the configured, and test verify_policies fails.
        self.policy_manager.configure_policies(extension={'ID1': {'P1': 'V1'}},
                                               new=False)
        with self.assertRaises(error.TestError) as context:
            self.policy_manager.verify_policies()
        self.assertEqual(
            str(context.exception),
            'Configured policies did not match policies received from DUT.')

    @patch(PATCH_PATH)
    def test_verify_special_extension(self, get_pol_mock):
        """Test the configure_extension_visual_policy and the verify_policies
        methods."""
        self.configPolicyManager()
        self.policy_manager.configure_policies(
            user={'Test Policy1': 'Policy1'},
            device={'Device Policy1': 'Device 1'},
            suggested_user={'Suggested Policy 1': 'Suggested 1'},
            extension={'ExtID1': {'Afile': 'Adirectory'}})
        get_pol_mock.return_value = self.expected
        self.policy_manager.obtain_policies_from_device()
        with self.assertRaises(error.TestError) as context:
            self.policy_manager.verify_policies()
        self.assertEqual(
            str(context.exception),
            'Configured policies did not match policies received from DUT.')

        # Add the "visual" policy (aka how the policy should be reported).
        self.policy_manager.configure_extension_visual_policy(
            {'ExtID1': {'Extension Policy 1': 'EP1'}})
        self.policy_manager.verify_policies()

        # Test the configured shows actual policy value by default
        configured_policies = (self.policy_manager.
                               get_configured_policies_as_dict())
        expected_extension = {'ExtID1':
                                {'Afile':
                                    {'scope': 'user',
                                     'level': 'mandatory',
                                     'value': 'Adirectory',
                                     'source': 'cloud'}}}
        self.assertEqual(expected_extension,
                         configured_policies['extensionPolicies'])

        # Finally, test the configured can also get the 'visual' policy.
        visual_configured_policies = (self.policy_manager.
                                      get_configured_policies_as_dict(True))
        expected_extension = {'ExtID1':
                                {'Extension Policy 1':
                                    {'scope': 'user',
                                     'level': 'mandatory',
                                     'value': 'EP1',
                                     'source': 'cloud'}}}
        self.assertEqual(expected_extension,
                         visual_configured_policies['extensionPolicies'])

    @patch(PATCH_PATH)
    def test_get_policy_value_from_DUT(self, get_pol_mock):
        """Test obtaining a single policy value from the DUT:
            Getting a policy prior to obtaining (returns None)
            Getting a normal chrome policy, using the Refresh flag
            Getting an ExtensionPolicy
            """
        self.configPolicyManager()
        policy_value = self.policy_manager.get_policy_value_from_DUT(
            'Test Policy1')
        self.assertEqual(policy_value, None)
        get_pol_mock.return_value = self.expected
        policy_value = self.policy_manager.get_policy_value_from_DUT(
            'Test Policy1', refresh=True)
        self.assertEqual(policy_value, 'Policy1')
        extension_policy_value = self.policy_manager.get_policy_value_from_DUT(
            'Extension Policy 1', 'ExtID1', False)
        self.assertEqual(extension_policy_value, 'EP1')

    def test_DMServerConfig(self):
        """
        Test the getDMConfig method via the following:
            Test the default DM json
            Test the DM Json when policies are configured.

        """
        self.configPolicyManager('Test_UserName')
        base_config = json.loads(self.policy_manager.getDMConfig())
        default_dm = (
            {'invalidation_name': 'test_policy',
             'invalidation_source': 16,
             'google/chromeos/device': {},
             'current_key_index': 0,
             'google/chrome/extension': {},
             'managed_users': ['*'],
             'google/chromeos/user': {'recommended': {}, 'mandatory': {}},
             'policy_user': 'Test_UserName'})
        self.assertEqual(base_config, default_dm)

        self.policy_manager.configure_policies(
            user={'Test Policy1': 'Policy1'},
            device={'SystemTimezone': 'Device 1'},
            suggested_user={'Suggested Policy 1': 'Suggested 1'},
            extension={'ExtID1': {'Extension Policy 1': 'EP1'}})

        expected_DMJson = (
            {"invalidation_name": "test_policy",
             "invalidation_source": 16,
             "google/chromeos/device":
                {"system_timezone.timezone": "Device 1"},
             "current_key_index": 0,
             "google/chrome/extension":
                {"ExtID1": {"Extension Policy 1": "EP1"}},
             "managed_users": ["*"],
             "google/chromeos/user":
                {"recommended": {"Suggested Policy 1": "Suggested 1"},
                 "mandatory": {"Test Policy1": "Policy1"}},
             "policy_user": "Test_UserName"})
        Dm_with_policies = json.loads(self.policy_manager.getDMConfig())
        self.assertEqual(Dm_with_policies, expected_DMJson)

    def test_getCloudDpc(self):
        """
        Test getCloudDpc and the following private methods:
            _arc_certs
            _add_shared_arc_policy
            _add_shared_policies
            _add_arc_certs

        """
        self.configPolicyManager()
        self.policy_manager.configure_policies(
            user={'Test Policy1': 'Policy1',
                  'ArcCertificatesSyncMode': 'TestValue1',
                  'OpenNetworkConfiguration': 'Apolicy',
                  'VideoCaptureAllowed': True,
                  'ArcPolicy':
                    {'applications': 'SomeApplication',
                     'OtherPolicy': 'OtherValue'}
                  })
        expected = {'applications': 'SomeApplication',
                    'cameraDisabled': True,
                    'caCerts': 'Apolicy'}
        cloudDpcPolicies = self.policy_manager.getCloudDpc()
        self.assertEqual(expected, cloudDpcPolicies)


if __name__ == '__main__':
    unittest.main()
