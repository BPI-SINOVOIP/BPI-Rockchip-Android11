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

import os

from acts.test_utils.instrumentation.device.apps.app_installer import \
    AppInstaller
from acts.test_utils.instrumentation.device.command.instrumentation_command_builder \
    import InstrumentationCommandBuilder

DISMISS_DIALOGS_RUNNER = '.DismissDialogsInstrumentation'
SCREENSHOTS_DIR = 'dialog-dismissal'
DISMISS_DIALOGS_TIMEOUT = 300


class DialogDismissalUtil(object):
    """Utility for dismissing app dialogs."""
    def __init__(self, dut, util_apk):
        self._dut = dut
        self._dismiss_dialogs_apk = AppInstaller(dut, util_apk)
        self._dismiss_dialogs_apk.install('-g')

    def dismiss_dialogs(self, apps, screenshots=True, quit_on_error=True):
        """Dismiss dialogs for the given apps.

        Args:
            apps: List of apps to dismiss dialogs
            screenshots: Boolean to enable screenshots upon dialog dismissal
            quit_on_error: Boolean to indicate if tool should quit on failure
        """
        if not apps:
            return
        if not isinstance(apps, list):
            apps = [apps]
        self._dut.log.info('Dismissing app dialogs for %s' % apps)
        cmd_builder = InstrumentationCommandBuilder()
        cmd_builder.set_manifest_package(self._dismiss_dialogs_apk.pkg_name)
        cmd_builder.set_runner(DISMISS_DIALOGS_RUNNER)
        cmd_builder.add_flag('-w')
        cmd_builder.add_key_value_param('apps', ','.join(apps))
        cmd_builder.add_key_value_param('screenshots', screenshots)
        cmd_builder.add_key_value_param('quitOnError', quit_on_error)
        self._dut.adb.shell(cmd_builder.build(),
                            timeout=DISMISS_DIALOGS_TIMEOUT)

        # Pull screenshots if screenshots=True
        if screenshots:
            self._dut.pull_files(
                os.path.join(self._dut.external_storage_path, SCREENSHOTS_DIR),
                self._dut.device_log_path
            )

    def close(self):
        """Clean up util by uninstalling the dialog dismissal APK."""
        self._dismiss_dialogs_apk.uninstall()
