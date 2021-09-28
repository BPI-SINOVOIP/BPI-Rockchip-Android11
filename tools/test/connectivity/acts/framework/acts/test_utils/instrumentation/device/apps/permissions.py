#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
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

from acts.test_utils.instrumentation.device.apps.app_installer import \
    AppInstaller
from acts.test_utils.instrumentation.device.command.instrumentation_command_builder \
    import InstrumentationCommandBuilder

PERMISSION_RUNNER = '.PermissionInstrumentation'


class PermissionsUtil(object):
    """Utility for granting all revoked runtime permissions."""
    def __init__(self, dut, util_apk):
        self._dut = dut
        self._permissions_apk = AppInstaller(dut, util_apk)
        self._permissions_apk.install()

    def grant_all(self):
        """Grant all runtime permissions with PermissionUtils."""
        self._dut.log.info('Granting all revoked runtime permissions.')
        cmd_builder = InstrumentationCommandBuilder()
        cmd_builder.set_manifest_package(self._permissions_apk.pkg_name)
        cmd_builder.set_runner(PERMISSION_RUNNER)
        cmd_builder.add_flag('-w')
        cmd_builder.add_flag('-r')
        cmd_builder.add_key_value_param('command', 'grant-all')
        self._dut.adb.shell(cmd_builder.build())

    def close(self):
        """Clean up util by uninstalling the permissions APK."""
        self._permissions_apk.uninstall()
