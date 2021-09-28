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

import os

DEFAULT_NOHUP_LOG = 'nohup.log'


class InstrumentationCommandBuilder(object):
    """Helper class to build instrumentation commands."""

    def __init__(self):
        self._manifest_package_name = None
        self._flags = []
        self._key_value_params = {}
        self._runner = None
        self._nohup = False
        self._proto_path = None
        self._nohup_log_path = None

    def set_manifest_package(self, test_package):
        self._manifest_package_name = test_package

    def set_runner(self, runner):
        self._runner = runner

    def add_flag(self, param):
        self._flags.append(param)

    def add_key_value_param(self, key, value):
        if isinstance(value, bool):
            value = str(value).lower()
        self._key_value_params[key] = str(value)

    def set_proto_path(self, path):
        """Sets a custom path to store result proto. Note that this path will
        be relative to $EXTERNAL_STORAGE on device.
        """
        self._proto_path = path

    def set_nohup(self, log_path=DEFAULT_NOHUP_LOG):
        """Enables nohup mode. This enables the instrumentation command to
        continue running after a USB disconnect.

        Args:
            log_path: Path to store stdout of the process. Relative to
                $EXTERNAL_STORAGE
        """
        self._nohup = True
        self._nohup_log_path = log_path

    def build(self):
        call = self._instrument_call_with_arguments()
        call.append('{}/{}'.format(self._manifest_package_name, self._runner))
        if self._nohup:
            call = ['nohup'] + call
            call.append('>>')
            call.append(os.path.join('$EXTERNAL_STORAGE', self._nohup_log_path))
            call.append('2>&1')
        return " ".join(call)

    def _instrument_call_with_arguments(self):
        errors = []
        if self._manifest_package_name is None:
            errors.append('manifest package cannot be none')
        if self._runner is None:
            errors.append('instrumentation runner cannot be none')
        if len(errors) > 0:
            raise Exception('instrumentation call build errors: {}'
                            .format(','.join(errors)))
        call = ['am instrument']
        for flag in self._flags:
            call.append(flag)
        call.append('-f')
        if self._proto_path:
            call.append(self._proto_path)
        for key, value in self._key_value_params.items():
            call.append('-e')
            call.append(key)
            call.append(value)
        return call


class InstrumentationTestCommandBuilder(InstrumentationCommandBuilder):

    def __init__(self):
        super().__init__()
        self._packages = []
        self._classes = []

    @staticmethod
    def default():
        """Default instrumentation call builder.

        The flags -w, -r and --no-isolated-storage are enabled.

           -w  Forces am instrument to wait until the instrumentation terminates
           (needed for logging)
           -r  Outputs results in raw format.
           --no-isolated-storage  Disables the isolated storage feature
           introduced in Q.
           https://developer.android.com/studio/test/command-line#AMSyntax

        The default test runner is androidx.test.runner.AndroidJUnitRunner.
        """
        builder = InstrumentationTestCommandBuilder()
        builder.add_flag('-w')
        builder.add_flag('-r')
        builder.add_flag('--no-isolated-storage')
        builder.set_runner('androidx.test.runner.AndroidJUnitRunner')
        return builder

    CONFLICTING_PARAMS_MESSAGE = ('only a list of classes and test methods or '
                                  'a list of test packages are allowed.')

    def add_test_package(self, package):
        if len(self._classes) != 0:
            raise Exception(self.CONFLICTING_PARAMS_MESSAGE)
        self._packages.append(package)

    def add_test_method(self, class_name, test_method):
        if len(self._packages) != 0:
            raise Exception(self.CONFLICTING_PARAMS_MESSAGE)
        self._classes.append('{}#{}'.format(class_name, test_method))

    def add_test_class(self, class_name):
        if len(self._packages) != 0:
            raise Exception(self.CONFLICTING_PARAMS_MESSAGE)
        self._classes.append(class_name)

    def build(self):
        errors = []
        if len(self._packages) == 0 and len(self._classes) == 0:
            errors.append('at least one of package, class or test method need '
                          'to be defined')

        if len(errors) > 0:
            raise Exception('instrumentation call build errors: {}'
                            .format(','.join(errors)))

        call = self._instrument_call_with_arguments()

        if len(self._packages) > 0:
            call.append('-e')
            call.append('package')
            call.append(','.join(self._packages))
        elif len(self._classes) > 0:
            call.append('-e')
            call.append('class')
            call.append(','.join(self._classes))

        call.append('{}/{}'.format(self._manifest_package_name, self._runner))
        if self._nohup:
            call = ['nohup'] + call
            call.append('>>')
            call.append(os.path.join('$EXTERNAL_STORAGE', self._nohup_log_path))
            call.append('2>&1')
        return ' '.join(call)
