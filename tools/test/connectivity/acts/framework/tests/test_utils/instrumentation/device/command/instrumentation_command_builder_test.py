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

from acts.test_utils.instrumentation.device.command.instrumentation_command_builder \
    import InstrumentationCommandBuilder
from acts.test_utils.instrumentation.device.command.instrumentation_command_builder \
    import InstrumentationTestCommandBuilder


class InstrumentationCommandBuilderTest(unittest.TestCase):

    def test__runner_and_manifest_package_definition(self):
        builder = InstrumentationCommandBuilder()
        builder.set_manifest_package('package')
        builder.set_runner('runner')
        call = builder.build()
        self.assertIn('package/runner', call)

    def test__manifest_package_must_be_defined(self):
        builder = InstrumentationCommandBuilder()

        with self.assertRaisesRegex(Exception, '.*package cannot be none.*'):
            builder.build()

    def test__runner_must_be_defined(self):
        builder = InstrumentationCommandBuilder()

        with self.assertRaisesRegex(Exception, '.*runner cannot be none.*'):
            builder.build()

    def test_proto_flag_without_set_proto_path(self):
        builder = InstrumentationCommandBuilder()
        builder.set_runner('runner')
        builder.set_manifest_package('some.manifest.package')

        call = builder.build()
        self.assertIn('-f', call)

    def test_proto_flag_with_set_proto_path(self):
        builder = InstrumentationCommandBuilder()
        builder.set_runner('runner')
        builder.set_manifest_package('some.manifest.package')
        builder.set_proto_path('/some/proto/path')

        call = builder.build()
        self.assertIn('-f /some/proto/path', call)

    def test_set_nohup(self):
        builder = InstrumentationCommandBuilder()
        builder.set_runner('runner')
        builder.set_manifest_package('some.manifest.package')
        builder.set_nohup()

        call = builder.build()
        self.assertEqual(
            call, 'nohup am instrument -f some.manifest.package/runner >> '
                  '$EXTERNAL_STORAGE/nohup.log 2>&1')

    def test__key_value_param_definition(self):
        builder = InstrumentationCommandBuilder()
        builder.set_runner('runner')
        builder.set_manifest_package('some.manifest.package')

        builder.add_key_value_param('my_key_1', 'my_value_1')
        builder.add_key_value_param('my_key_2', 'my_value_2')

        call = builder.build()
        self.assertIn('-e my_key_1 my_value_1', call)
        self.assertIn('-e my_key_2 my_value_2', call)

    def test__flags(self):
        builder = InstrumentationCommandBuilder()
        builder.set_runner('runner')
        builder.set_manifest_package('some.manifest.package')

        builder.add_flag('--flag1')
        builder.add_flag('--flag2')

        call = builder.build()
        self.assertIn('--flag1', call)
        self.assertIn('--flag2', call)


class InstrumentationTestCommandBuilderTest(unittest.TestCase):
    """Test class for
    acts/test_utils/instrumentation/instrumentation_call_builder.py
    """

    def test__test_packages_can_not_be_added_if_classes_were_added_first(self):
        builder = InstrumentationTestCommandBuilder()
        builder.add_test_class('some.tests.Class')

        with self.assertRaisesRegex(Exception, '.*only a list of classes.*'):
            builder.add_test_package('some.tests.package')

    def test__test_classes_can_not_be_added_if_packages_were_added_first(self):
        builder = InstrumentationTestCommandBuilder()
        builder.add_test_package('some.tests.package')

        with self.assertRaisesRegex(Exception, '.*only a list of classes.*'):
            builder.add_test_class('some.tests.Class')

    def test__test_classes_and_test_methods_can_be_combined(self):
        builder = InstrumentationTestCommandBuilder()
        builder.set_runner('runner')
        builder.set_manifest_package('some.manifest.package')
        builder.add_test_class('some.tests.Class1')
        builder.add_test_method('some.tests.Class2', 'favoriteTestMethod')

        call = builder.build()
        self.assertIn('some.tests.Class1', call)
        self.assertIn('some.tests.Class2', call)
        self.assertIn('favoriteTestMethod', call)


if __name__ == '__main__':
    unittest.main()
