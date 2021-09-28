#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import os
import re
import sys
import uuid

if sys.version_info < (3, ):
    import warnings

    with warnings.catch_warnings():
        warnings.filterwarnings('ignore', category=PendingDeprecationWarning)
        import imp

    import importlib
    import unittest2 as unittest

    def import_module(name, path):
        return imp.load_source(name, path)

    def import_acts():
        return importlib.import_module('acts')
else:
    import importlib.machinery
    import unittest

    def import_module(name, path):
        return importlib.machinery.SourceFileLoader(name, path).load_module()

    def import_acts():
        return importlib.import_module('acts')


PY_FILE_REGEX = re.compile('.+\.py$')

BLACKLIST = [
    'acts/controllers/rohdeschwarz_lib/contest.py',
    'acts/controllers/native.py',
    'acts/controllers/native_android_device.py',
    'acts/controllers/packet_sender.py',
    'acts/test_utils/wifi/ota_chamber.py',
    'acts/controllers/buds_lib/dev_utils/proto/gen/nanopb_pb2.py',
    'acts/test_utils/wifi/wifi_performance_test_utils.py',
    'acts/test_utils/wifi/wifi_power_test_utils.py',
    'acts/test_utils/wifi/wifi_retail_ap.py',
    'acts/test_utils/bt/bt_power_test_utils.py',
    'acts/test_utils/coex/coex_test_utils.py',
    'acts/test_utils/tel/twilio_client.py',
    'acts/test_utils/bt/A2dpBaseTest.py',
    'acts/test_utils/bt/BtSarBaseTest.py',
    'tests/google/ble/beacon_tests/BeaconSwarmTest.py',
    'tests/google/bt/pts/BtCmdLineTest.py',
    'tests/google/bt/headphone_automation/SineWaveQualityTest.py',
    'tests/google/bt/audio_lab/BtChameleonTest.py',
    'tests/google/native/bt/BtNativeTest.py',
    'tests/google/wifi/WifiRvrTest.py',
    'tests/google/wifi/WifiStaApConcurrencyStressTest.py',
    'tests/google/wifi/WifiPasspointTest.py',
    'tests/google/wifi/WifiOtaTest.py',
    'tests/google/wifi/WifiRoamingPerformanceTest.py',
    'tests/google/wifi/WifiRssiTest.py',
    'tests/google/wifi/WifiPingTest.py',
    'tests/google/wifi/WifiThroughputStabilityTest.py',
    'tests/google/wifi/WifiSensitivityTest.py',
    'tests/google/wifi/WifiSoftApPerformanceTest.py',
    'tests/google/tel/live/TelLiveMobilityStressTest.py',
    'tests/google/tel/live/TelLiveNoSimTest.py',
    'tests/google/tel/live/TelLiveLockedSimTest.py',
    'tests/google/tel/live/TelLiveEmergencyTest.py',
    'tests/google/tel/live/TelLiveConnectivityMonitorTest.py',
    'tests/google/tel/live/TelLiveConnectivityMonitorMobilityTest.py',
    'tests/google/fuchsia/bt/FuchsiaCmdLineTest.py',
    'tests/google/fuchsia/bt/gatt/GattServerSetupTest.py',
    'tests/google/fuchsia/wlan/RebootStressTest.py',
    'acts/test_utils/gnss/gnss_testlog_utils.py',
    'tests/google/fuchsia/bt/BluetoothCmdLineTest.py',
]

BLACKLIST_DIRECTORIES = [
    'acts/controllers/buds_lib',
    'acts/test_utils/audio_analysis_lib/',
    'acts/test_utils/coex/',
    'acts/test_utils/power/',
    'tests/google/coex/',
    'tests/google/gnss/',
    'tests/google/power/',
    'tests/google/bt/performance/',
    'tests/google/bt/sar/',
]

BANNED_IMPORTS = ['mobly.controllers']


class ActsImportUnitTest(unittest.TestCase):
    """Test that all acts framework imports work."""

    def test_import_acts_successful(self):
        """Test that importing ACTS works."""
        acts = import_acts()
        self.assertIsNotNone(acts)

    def test_import_framework_and_tests_successful(self):
        """Dynamically test all imports from the framework and ACTS tests.
        Ensure that no imports of banned packages/modules took place."""
        acts = import_acts()
        if hasattr(acts, '__path__') and len(acts.__path__) > 0:
            acts_path = acts.__path__[0]
        else:
            acts_path = os.path.dirname(acts.__file__)
        tests_path = os.path.normpath(os.path.join(acts_path, '../../tests'))

        for base_dir in [acts_path, tests_path]:
            for root, _, files in os.walk(base_dir):
                for f in files:
                    full_path = os.path.join(root, f)
                    if (any(full_path.endswith(e) for e in BLACKLIST)
                            or any(e in full_path
                                   for e in BLACKLIST_DIRECTORIES)):
                        continue

                    path = os.path.relpath(os.path.join(root, f), os.getcwd())

                    if PY_FILE_REGEX.match(full_path):
                        with self.subTest(msg='import %s' % path):
                            fake_module_name = str(uuid.uuid4())
                            module = import_module(fake_module_name, path)
                            self.assertIsNotNone(module)

        # Suppress verbose output on assertion failure.
        self.longMessage = False

        for banned_import in BANNED_IMPORTS:
            self.assertNotIn(
                banned_import, sys.modules,
                'Attempted to import the banned package/module '
                '%s.' % banned_import)


if __name__ == '__main__':
    unittest.main()
