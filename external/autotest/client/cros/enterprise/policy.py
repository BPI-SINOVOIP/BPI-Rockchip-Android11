# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error


class Policy(object):
    """
    Class structure for a single policy object.

    A policy has the following attributes:
        value
        source
        level
        scope
        name

    All of the attributes are protected in @setter's. To set a value simply
    create a policy object (e.g. "somePolicy = Policy()") and set the desired
    value (somePolicy.value=True).

    This class will be used by the policy_manager for configuring and checking
    policies for Enterprise tests.

    """

    def __init__(self):
        self._value = None
        self._source = None
        self._scope = None
        self._level = None
        self._name = None

    def is_formatted_value(self, data):
        """
        Checks if the received value is a a dict containing all policy stats.

        """
        if not isinstance(data, dict):
            return False
        received_keys = set(['scope', 'source', 'level', 'value'])
        return received_keys.issubset(set(data.keys()))

    def get_policy_as_dict(self):
        """
        Returns the policy as a dict, in the same format as in the
        chrome://policy json file.

        """
        return {self.name: {'scope': self._scope,
                            'source': self._source,
                            'level': self._level,
                            'value': self._value}}

    def set_policy_from_dict(self, data):
        """
        Sets the policy attributes from a provided dict matching the following
        format:

            {"scope": scopeValue, "source": sourceValue, "level": levelValue,
             "value": value}

        @param data: a dict representing the policy values, as specified above.

        """
        if not self.is_formatted_value(data):
            raise error.TestError("""Incorrect input data provided. Value
                provided must be dict with the keys: "scope", "source",
                "level", "value". Got {}""".format(data))
        self.scope = data['scope']
        self.source = data['source']
        self.level = data['level']
        self.value = data['value']
        if data.get('error', None):
            logging.warning('\n[Policy Error] error reported with policy:\n{}'
                            .format(data['error']))

    @property
    def value(self):
        return self._value

    @value.setter
    def value(self, value):
        self._value = value

    @property
    def name(self):
        return self._name

    @name.setter
    def name(self, name):
        self._name = name

    @property
    def level(self):
        return self._level

    @level.setter
    def level(self, level):
        self._level = level

    @property
    def scope(self):
        return self._scope

    @scope.setter
    def scope(self, scope):
        self._scope = scope

    @property
    def source(self):
        return self._source

    @source.setter
    def source(self, source):
        self._source = source

    @property
    def group(self):
        return None

    @group.setter
    def group(self, group):
        """
        If setting a policy, you can also set it from a group ("user",
        "suggested_user", "device"). Doing this will automatically set the other
        policy attributes.

        If the group is None, nothing will be set.

        @param group: None or value of the policy group.

        """
        if not group:
            return
        self._source = 'cloud'
        if group != 'suggested_user':
            self._level = 'mandatory'
        else:
            self._level = 'recommended'
        if group == 'device':
            self._scope = 'machine'
        else:
            self._scope = 'user'

    def __ne__(self, other):
        return not self.__eq__(other)

    def __eq__(self, other):
        """
        Override of the "==" statment. Verify one policy object verse another.

        Will return True if the policy value, source, scope, and level are
        equal, otherwise False.

        There is a special check for the Network Policy, due to a displayed
        policy having visual obfuscation (********).

        """
        if self.value != other.value:
            if is_network_policy(other.name):
                return check_obfuscation(other.value)
            else:
                logging.info('value configured {} != received {}'
                             .format(self.value, other.value))
                return False
        if self.source != other.source:
            logging.info('source configured {} != received {}'
                         .format(self.source, other.source))
            return False
        if self.scope != other.scope:
            logging.info('scope configured {} != received {}'
                         .format(self.scope, other.scope))
            return False
        if self.level != other.level:
            logging.info('level configured {} != received {}'
                         .format(self.level, other.level))
            return False
        return True

    def __repr__(self):
        policy_data = self.get_policy_as_dict()
        return "<POLICY DATA OBJECT> | {}".format(policy_data)


def is_network_policy(policy_name):
    """
    Returns true if the policy name is that of an OpenNetworkConfiguration.

    """
    if policy_name.endswith('OpenNetworkConfiguration'):
        return True


def check_obfuscation(other_value):
    """Checks if value is a network policy, and is obfuscated."""
    DISPLAY_PWORD = '*' * 8

    for network in other_value.get('NetworkConfigurations', []):
        wifi = network.get('WiFi', {})
        if 'Passphrase' in wifi and wifi['Passphrase'] != DISPLAY_PWORD:
            return False
        if ('EAP' in wifi and
            'Password' in wifi['EAP'] and
             wifi['EAP']['Password'] != DISPLAY_PWORD):
            return False
    for cert in other_value.get('Certificates', []):
        if 'PKCS12' in cert:
            if cert['PKCS12'] != DISPLAY_PWORD:
                return False
    return True
