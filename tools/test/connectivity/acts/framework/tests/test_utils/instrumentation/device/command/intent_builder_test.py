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

import unittest

from acts.test_utils.instrumentation.device.command.intent_builder import \
    IntentBuilder


class IntentBuilderTest(unittest.TestCase):
    """Unit tests for IntentBuilder"""

    def test_set_action(self):
        """Test that a set action yields the correct intent call"""
        builder = IntentBuilder('am start')
        builder.set_action('android.intent.action.SOME_ACTION')
        self.assertEqual(builder.build(),
                         'am start -a android.intent.action.SOME_ACTION')

    def test_set_component_with_package_only(self):
        """Test that the intent call is built properly with only the package
        name specified.
        """
        builder = IntentBuilder('am broadcast')
        builder.set_component('android.package.name')
        self.assertEqual(builder.build(),
                         'am broadcast -n android.package.name')

    def test_set_component_with_package_and_component(self):
        """Test that the intent call is built properly with both the package
        and component name specified.
        """
        builder = IntentBuilder('am start')
        builder.set_component('android.package.name', '.AndroidComponent')
        self.assertEqual(
            builder.build(),
            'am start -n android.package.name/.AndroidComponent')

    def test_set_data_uri(self):
        """Test that a set data URI yields the correct intent call"""
        builder = IntentBuilder()
        builder.set_data_uri('file://path/to/file')
        self.assertEqual(builder.build(), '-d file://path/to/file')

    def test_add_flag(self):
        """Test that additional flags are added properly"""
        builder = IntentBuilder('am start')
        builder.add_flag('--flag-numero-uno')
        builder.add_flag('--flag-numero-dos')
        self.assertEqual(
            builder.build(), 'am start --flag-numero-uno --flag-numero-dos')

    def test_add_key_value_with_empty_value(self):
        """Test that a param with an empty value is added properly."""
        builder = IntentBuilder('am broadcast')
        builder.add_key_value_param('empty_param')
        self.assertEqual(builder.build(), 'am broadcast --esn empty_param')

    def test_add_key_value_with_nonempty_values(self):
        """Test that a param with various non-empty values is added properly."""
        builder = IntentBuilder('am start')
        builder.add_key_value_param('bool_param', False)
        builder.add_key_value_param('string_param', 'enabled')
        builder.add_key_value_param('int_param', 5)
        builder.add_key_value_param('float_param', 12.1)
        self.assertEqual(
            builder.build(),
            'am start --ez bool_param false --es string_param enabled '
            '--ei int_param 5 --ef float_param 12.1')

    def test_full_intent_command(self):
        """Test a full intent command with all possible components."""
        builder = IntentBuilder('am broadcast')
        builder.set_action('android.intent.action.TEST_ACTION')
        builder.set_component('package.name', '.ComponentName')
        builder.set_data_uri('file://path/to/file')
        builder.add_key_value_param('empty')
        builder.add_key_value_param('numeric_param', 11.6)
        builder.add_key_value_param('bool_param', True)
        builder.add_flag('--unit-test')
        self.assertEqual(
            builder.build(),
            'am broadcast -a android.intent.action.TEST_ACTION '
            '-n package.name/.ComponentName -d file://path/to/file --unit-test '
            '--esn empty --ef numeric_param 11.6 --ez bool_param true')


if __name__ == '__main__':
    unittest.main()
