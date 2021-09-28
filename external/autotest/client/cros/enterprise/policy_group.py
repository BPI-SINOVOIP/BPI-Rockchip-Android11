# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json

from autotest_lib.client.cros.enterprise.policy import Policy as Policy
from autotest_lib.client.cros.enterprise.device_policy_lookup import DEVICE_POLICY_DICT

CHROMEPOLICIES = 'chromePolicies'
DEVICELOCALACCOUNT = 'deviceLocalAccountPolicies'
EXTENSIONPOLICIES = 'extensionPolicies'


class AllPolicies(object):

    def __init__(self, isConfiguredPolicies=False):
        self.extension_configured_data = {}
        # Because Extensions have a different "displayed" value than the
        # configured, the extension_displayed_values will be used when the
        # policy group is "configured" to represent what the "displayed" policy
        # values SHOULD be.
        self.extension_displayed_values = {}
        self.local = {}
        self.chrome = {}

        self.policy_dict = {CHROMEPOLICIES: {},
                            EXTENSIONPOLICIES: {},
                            DEVICELOCALACCOUNT: {}}
        self.isConfiguredPolicies = isConfiguredPolicies
        if self.isConfiguredPolicies:
            self.createNewFakeDMServerJson()

    def createNewFakeDMServerJson(self):
        """Creates a fresh DM blob that will be used by the Fake DM server."""

        self._DMJSON = {
            'managed_users': ['*'],
            'policy_user': None,
            'current_key_index': 0,
            'invalidation_source': 16,
            'invalidation_name': 'test_policy',
            'google/chromeos/user': {'mandatory': {}, 'recommended': {}},
            'google/chromeos/device': {},
            'google/chrome/extension': {}
        }
        self._DM_MANDATORY = self._DMJSON['google/chromeos/user']['mandatory']
        self._DM_RECOMMENDED = (
            self._DMJSON['google/chromeos/user']['recommended'])
        self._DM_DEVICE = self._DMJSON['google/chromeos/device']
        self._DM_EXTENSION = self._DMJSON['google/chrome/extension']

    def get_policy_as_dict(self, visual=False):
        """Returns the policies as a dictionary."""
        self._update_policy_dict(visual)
        return self.policy_dict

    def set_extension_policy(self, policies, visual=False):
        """
        Sets the extension policies

        @param policies: Dict formatted as follows:
            {'extID': {pol1: v1}, extid2: {p2:v2}}

        @param visual: bool, If the extension policy provided is what should be
            displayed via the api/chrome page. If False, then the 'policies'
            should be the actual value that will be provided to the DMServer.

        """
        # If the policy is configured (ie this policy group object represents)
        # the policies being SET for testing) add the 'policy_group' value.
        policy_group = 'extension' if self.isConfiguredPolicies else None
        extension_policies = copy.deepcopy(policies)

        for extension_ID, extension_policy in extension_policies.items():

            # Adding the extension ID key into the extension dict.
            if visual:
                self.extension_displayed_values[extension_ID] = {}
                key = 'displayed_ext_values'
            else:
                self.extension_configured_data[extension_ID] = {}
                key = 'ext_values'
            self.set_policy(key, extension_policy, policy_group, extension_ID)

    def set_policy(self,
                   policy_type,
                   policies,
                   group=None,
                   extension_key=None):
        """
        Create and the policy object, and set it in the corresponding group.

        @param policy_type: str of the policy type. Must be:
            'chrome', 'ext_values', 'displayed_ext_values', or 'local'.
        @param policies: dict of policy values.
        @param group: str, group key for the Policy object setter.
        @param extension_key: optional, key for exentsionID.

        """
        policy_group = self._get_policy_group(policy_type, extension_key)
        for name, value in policies.items():
            policy_group[name] = self._create_pol_obj(name, value, group)

    def _get_policy_group(self, policy_type, extension_key=None):
        """Simple lookup to put the policies in the correct bucket."""
        if policy_type == 'chrome':
            policy_group = self.chrome
        elif policy_type == 'ext_values':
            policy_group = self.extension_configured_data[extension_key]
        elif policy_type == 'displayed_ext_values':
            policy_group = self.extension_displayed_values[extension_key]
        elif policy_type == 'local':
            policy_group = self.local
        return policy_group

    def updateDMJson(self):
        """
        Update the ._DM_JSON with the values currently set in
        self.chrome, self.extension_configured_data, and self.local.

        """

        self._populateChromeData()
        self._populateExtensionData()

    def _populateChromeData(self):
        """Update the DM_JSON's chrome values."""
        for policy_name, policy_object in self.chrome.items():
            if policy_object.scope == 'machine':
                dm_name = DEVICE_POLICY_DICT[policy_name]
                self._DM_DEVICE.update(
                    self._clean_pol({dm_name: policy_object.value}))

            elif policy_object.level == 'recommended':
                self._DM_RECOMMENDED.update(
                    self._clean_pol({policy_name: policy_object.value}))

            elif (policy_object.level == 'mandatory' and
                    policy_object.scope == 'user'):
                self._DM_MANDATORY.update(
                    self._clean_pol({policy_name: policy_object.value}))

    def _populateExtensionData(self):
        """Updates the DM_JSON's extension values."""
        for extension, ext_pol in self.extension_configured_data.items():
            extension_policies = {}
            for polname, polItem in ext_pol.items():
                extension_policies[polname] = polItem.value
            self._DM_EXTENSION.update({extension: extension_policies})

    def _clean_pol(self, policies):
        """Cleans the policies to be set on the fake DM server."""
        cleaned = {}
        for policy, value in policies.items():
            if value is None:
                continue
            cleaned[policy] = self._jsonify(policy, value)
        return cleaned

    def _jsonify(self, policy, value):
        """Jsonify policy if its a dict or list that is not kiosk policy."""
        if isinstance(value, dict):
            return json.dumps(value)
        # Kiosk Policy, aka "account", is the only policy not formatted.
        elif (
                isinstance(value, list) and
                (policy != 'device_local_accounts.account')):
            if value and isinstance(value[0], dict):
                return json.dumps(value)
        return value

    def _create_pol_obj(self, name, data, group=None):
        """
        Create a policy object from Policy.Policy().

        @param name: str, name of the policy
        @param data: data value of the policy
        @param group: optional, group of the policy.

        @returns: Policy object, reperesenting the policy args provided.
        """
        policy_obj = Policy()
        policy_obj.name = name
        if policy_obj.is_formatted_value(data):
            policy_obj.set_policy_from_dict(data)
        else:
            policy_obj.value = data
        policy_obj.group = group
        return policy_obj

    def _update_policy_dict(self, secondary_ext_policies):
        """Update the local .policy_dict with the most current values."""
        for policy in self.chrome:
            self.policy_dict[CHROMEPOLICIES].update(
                self.chrome[policy].get_policy_as_dict())

        ext_item = self._select_ext_group(secondary_ext_policies)

        for ext_name, ext_group in ext_item.items():
            ext_dict = {ext_name: {}}
            for policy in ext_group:
                pol_as_dict = ext_group[policy].get_policy_as_dict()

                ext_dict[ext_name].update(pol_as_dict)
            self.policy_dict[EXTENSIONPOLICIES].update(ext_dict)
        for policy in self.local:
            self.policy_dict[DEVICELOCALACCOUNT].update(
                self.local[policy].get_policy_as_dict())

    def _select_ext_group(self, secondary_ext_policies):
        """Determine which extension group to use for the configured dictionary
        formatting. If the secondary_ext_policies flag has been set, and
        the self.extension_displayed_values is not None, use
        self.extension_displayed_values,
        else: use the original configured

        @param secondary_ext_policies: bool

        """
        if secondary_ext_policies and self.extension_displayed_values:
            return self.extension_displayed_values
        else:
            return self.extension_configured_data

    def __ne__(self, other):
        return not self.__eq__(other)

    def __eq__(self, other):
        """
        Override the == to check a policy group object vs another.

        Will return False if:
            A policy is missing from self is missing in other,
                when the policy is not None.
            An Extension from self is missing in other.
            If the policy valus in self are are not equal to the other
                (less obfuscation).

        Else: True
        """
        own_ext = self.extension_configured_data
        if self.extension_displayed_values:
            own_ext = self.extension_displayed_values
        for ext_name, ext_group in own_ext.items():
            if ext_name not in other.extension_configured_data:
                return False
            if not self._check(own_ext[ext_name],
                               other.extension_configured_data[ext_name]):
                return False
        if (
                not self._check(self.chrome, other.chrome) or
                not self._check(self.local, other.local)):
            return False
        return True

    def _check(self, policy_group, other_policy_group):
        """
        Check if the policy_group is ==.

        Will return False if:
            policy is missing from other policy object
            policy objects != (per the Policy object __eq__ override)
        Will return True if:
            There is no policies
            if the policy value is None
            If no other conditions are violated

        """
        if not policy_group:  # No object
            return True
        for policy_name, policy_group in policy_group.items():
            if policy_group.value is None:
                return True
            if policy_name not in other_policy_group:
                return False
            if policy_group != other_policy_group[policy_name]:
                return False
        return True
