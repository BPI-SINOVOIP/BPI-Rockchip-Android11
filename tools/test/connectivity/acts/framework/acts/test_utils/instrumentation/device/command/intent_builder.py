#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import collections

TYPE_TO_FLAG = collections.defaultdict(lambda: '--es')
TYPE_TO_FLAG.update({bool: '--ez', int: '--ei', float: '--ef', str: '--es'})


class IntentBuilder(object):
    """Helper class to build am broadcast <INTENT> commands."""

    def __init__(self, base_cmd=''):
        """Initializes the intent command builder.

        Args:
            base_cmd: The base am command, e.g. am broadcast, am start
        """
        self._base_cmd = base_cmd
        self._action = None
        self._component = None
        self._data_uri = None
        self._flags = []
        self._key_value_params = collections.OrderedDict()

    def set_action(self, action):
        """Set the intent action, as marked by the -a flag"""
        self._action = action

    def set_component(self, package, component=None):
        """Set the package and/or component, as marked by the -n flag.
        Only the package name will be used if no component is specified.
        """
        if component:
            self._component = '%s/%s' % (package, component)
        else:
            self._component = package

    def set_data_uri(self, data_uri):
        """Set the data URI, as marked by the -d flag"""
        self._data_uri = data_uri

    def add_flag(self, flag):
        """Add any additional flags to the intent argument"""
        self._flags.append(flag)

    def add_key_value_param(self, key, value=None):
        """Add any extra data as a key-value pair"""
        self._key_value_params[key] = value

    def build(self):
        """Returns the full intent command string."""
        cmd = [self._base_cmd]
        if self._action:
            cmd.append('-a %s' % self._action)
        if self._component:
            cmd.append('-n %s' % self._component)
        if self._data_uri:
            cmd.append('-d %s' % self._data_uri)
        cmd += self._flags
        for key, value in self._key_value_params.items():
            if value is None:
                cmd.append('--esn %s' % key)
            else:
                str_value = str(value)
                if isinstance(value, bool):
                    str_value = str_value.lower()
                cmd.append(' '.join((TYPE_TO_FLAG[type(value)], key,
                                     str_value)))
        return ' '.join(cmd).strip()
