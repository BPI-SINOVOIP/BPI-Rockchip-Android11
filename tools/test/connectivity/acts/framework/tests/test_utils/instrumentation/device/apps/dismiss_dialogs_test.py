#!/usr/bin/env python3
#
# Copyright 2020 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import unittest

import mock
from acts.test_utils.instrumentation.device.apps.dismiss_dialogs import \
    DialogDismissalUtil


class MockDialogDismissalUtil(DialogDismissalUtil):
    """Mock DialogDismissalUtil for unit testing"""
    def __init__(self):
        self._dut = mock.MagicMock()
        self._dismiss_dialogs_apk = mock.MagicMock()
        self._dismiss_dialogs_apk.pkg_name = 'dismiss.dialogs'


class DialogDismissalUtilTest(unittest.TestCase):
    def setUp(self):
        self._dismiss_dialogs_util = MockDialogDismissalUtil()

    def test_dismiss_dialog_zero_apps(self):
        """Test that no command is run if the apps arg is empty."""
        apps = []
        self._dismiss_dialogs_util.dismiss_dialogs(apps)
        self._dismiss_dialogs_util._dut.adb.shell.assert_not_called()

    def test_dismiss_dialog_single_app(self):
        """
        Test that the correct command is run when a single app is specified.
        """
        apps = ['sample.app.one']
        self._dismiss_dialogs_util.dismiss_dialogs(apps)
        expected_cmd = (
            'am instrument -w -f -e apps sample.app.one '
            'dismiss.dialogs/.DismissDialogsInstrumentation '
            '-e screenshots true -e quitOnError true'
        )
        self._dismiss_dialogs_util._dut.adb.shell.assert_called_with(
            expected_cmd)

    def test_dismiss_dialog_multiple_apps(self):
        """
        Test that the correct command is run when multiple apps are specified.
        """
        apps = ['sample.app.one', 'sample.app.two']
        self._dismiss_dialogs_util.dismiss_dialogs(apps)
        expected_cmd = (
            'am instrument -w -f -e apps sample.app.one,sample.app.two '
            'dismiss.dialogs/.DismissDialogsInstrumentation '
            '-e screenshots true -e quitOnError true'
        )
        self._dismiss_dialogs_util._dut.adb.shell.assert_called_with(
            expected_cmd)


if __name__ == '__main__':
    unittest.main()
