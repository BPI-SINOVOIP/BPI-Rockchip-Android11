#!/usr/bin/env python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections


CONFIG = {}

CONFIG['TEST_NAME'] = 'cheets_CTS_N'
CONFIG['DOC_TITLE'] = 'Android Compatibility Test Suite (CTS)'
CONFIG['MOBLAB_SUITE_NAME'] = 'suite:cts_N'
CONFIG['COPYRIGHT_YEAR'] = 2016
CONFIG['AUTHKEY'] = ''

# Both arm, x86 tests results normally is below 100MB.
# 500MB should be sufficient for CTS tests and dump logs for android-cts.
CONFIG['LARGE_MAX_RESULT_SIZE'] = 500 * 1024

# Individual module normal produces less results than all modules, which is
# ranging from 4MB to 50MB. 500MB should be sufficient to handle all the cases.
CONFIG['NORMAL_MAX_RESULT_SIZE'] = 300 * 1024

CONFIG['TRADEFED_CTS_COMMAND'] = 'cts'
CONFIG['TRADEFED_RETRY_COMMAND'] = 'cts'

# TODO(yoshiki, kinaba): Flip this to false (and remove the flag itself). On N,
# this flag is not working and the tradefed never reboots regardless of the
# flag.
CONFIG['TRADEFED_DISABLE_REBOOT'] = True

CONFIG['TRADEFED_DISABLE_REBOOT_ON_COLLECTION'] = False

# TODO(yoshiki, kinaba): Flip this to False (and remove the flag itself).
CONFIG['TRADEFED_MAY_SKIP_DEVICE_INFO'] = True

CONFIG['TRADEFED_EXECUTABLE_PATH'] = 'android-cts/tools/cts-tradefed'
CONFIG['TRADEFED_IGNORE_BUSINESS_LOGIC_FAILURE'] = False

# As this is not called for the "all" runs we can safely assume that each module
# runs in suite:arc-cts.
CONFIG['INTERNAL_SUITE_NAMES'] = ['suite:arc-cts']
CONFIG['QUAL_SUITE_NAMES'] = ['suite:arc-cts-qual']

CONFIG['CONTROLFILE_TEST_FUNCTION_NAME'] = 'run_TS'
CONFIG['CONTROLFILE_WRITE_SIMPLE_QUAL_AND_REGRESS'] = False
CONFIG['CONTROLFILE_WRITE_CAMERA'] = True
CONFIG['CONTROLFILE_WRITE_EXTRA'] = True

# The dashboard suppresses upload to APFE for GS directories (based on autotest
# tag) that contain 'tradefed-run-collect-tests'. b/119640440
# Do not change the name/tag without adjusting the dashboard.
_COLLECT = 'tradefed-run-collect-tests-only-internal'
_PUBLIC_COLLECT = 'tradefed-run-collect-tests-only'

CONFIG['LAB_DEPENDENCY'] = {
    'x86': ['cts_abi_x86']
}

CONFIG['CTS_JOB_RETRIES_IN_PUBLIC'] = 2
CONFIG['CTS_QUAL_RETRIES'] = 9
CONFIG['CTS_MAX_RETRIES'] = {}

# TODO(ihf): Update from wmatrix data.
# Times guessed by looking for times > 20m in
# grep runtime-hint android-cts/testcases/*.config
# Timeout in hours.
CONFIG['CTS_TIMEOUT_DEFAULT'] = 1.0
CONFIG['CTS_TIMEOUT'] = {
    'CtsAppSecurityHostTestCases':      1.5,
    'CtsDeqpTestCases':                 12.0,
    'CtsDeqpTestCases.dEQP-EGL'  :       2.0,
    'CtsDeqpTestCases.dEQP-GLES2':       2.0,
    'CtsDeqpTestCases.dEQP-GLES3':       6.0,
    'CtsDeqpTestCases.dEQP-GLES31':     12.0,
    'CtsDeqpTestCases.dEQP-VK':         12.0,
    'CtsDevicePolicyManagerTestCases':   2.0,
    'CtsFileSystemTestCases':            2.5,
    'CtsHardwareTestCases':              3.0,
    'CtsIcuTestCases':                   2.0,
    'CtsLibcoreOjTestCases':             2.5,
    'CtsLibcoreTestCases':               1.5,
    'CtsMediaStressTestCases':           6.0,
    'CtsMediaTestCases':                 6.0,
    'CtsPrintTestCases':                 3.0,
    'CtsSecurityHostTestCases':          1.5,
    'CtsSecurityTestCases':              1.5,
    'CtsShortcutHostTestCases':          1.5,
    'CtsThemeHostTestCases':             6.0,
    'CtsVmTestCases':                    1.5,
    'vm-tests-tf':                       2.0,
    _COLLECT:                            0.6667,
    _PUBLIC_COLLECT:                     0.6667,
}

# Any test that runs as part as blocking BVT needs to be stable and fast. For
# this reason we enforce a tight timeout on these modules/jobs.
# Timeout in hours. (0.1h = 6 minutes)
CONFIG['BVT_TIMEOUT'] = 0.1
# We allow a very long runtime for qualification (2 days).
CONFIG['QUAL_TIMEOUT'] = 48

CONFIG['QUAL_BOOKMARKS'] = sorted([
    'A',  # A bookend to simplify partition algorithm.
    'CtsActivityManagerDevice',  # Runs long enough. (3h)
    'CtsActivityManagerDevicez',
    'CtsDeqpTestCases',
    'CtsDeqpTestCasesz',  # Put Deqp in one control file. Long enough, fairly stable.
    'CtsFileSystemTestCases',  # Runs long enough. (3h)
    'CtsFileSystemTestCasesz',
    'CtsMedia',  # Put Media module in its own control file. Long enough.
    'CtsMediaz',
    'CtsSecurityHostTestCasesz',  # Split SecurityHost and Simpleperf preconditions
    'zzzzz'  # A bookend to simplify algorithm.
])

CONFIG['SMOKE'] = [
    'CtsDramTestCases',
]

CONFIG['BVT_ARC'] = [
    'CtsAccelerationTestCases',
    'CtsAccountManagerTestCases',
]

CONFIG['BVT_PERBUILD'] = [
    'CtsAccountManagerTestCases',
    'CtsAppUsageHostTestCases',
    'CtsDeviceAdminUninstallerTestCases',
    'CtsDramTestCases',
    'CtsGraphicsTestCases',
    'CtsJankDeviceTestCases',
    'CtsOpenGLTestCases',
    'CtsOpenGlPerf2TestCases',
    'CtsPermission2TestCases',
    'CtsSignatureTestCases',
    'CtsSimpleperfTestCases',
    'CtsSpeechTestCases',
    'CtsTelecomTestCases',
    'CtsTelephonyTestCases',
    'CtsThemeDeviceTestCases',
    'CtsTransitionTestCases',
    'CtsTvTestCases',
    'CtsUiAutomationTestCases',
    'CtsUsbTests',
    'CtsVoiceSettingsTestCases',
]

CONFIG['NEEDS_POWER_CYCLE'] = [
    'CtsBluetoothTestCases',
]

CONFIG['HARDWARE_DEPENDENT_MODULES'] = []

# The suite is divided based on the run-time hint in the *.config file.
CONFIG['VMTEST_INFO_SUITES'] = collections.OrderedDict()
# This is the default suite for all the modules that are not specified below.
CONFIG['VMTEST_INFO_SUITES']['vmtest-informational1'] = []
CONFIG['VMTEST_INFO_SUITES']['vmtest-informational2'] = [
    'CtsMediaTestCases', 'CtsMediaStressTestCases', 'CtsHardwareTestCases'
]
CONFIG['VMTEST_INFO_SUITES']['vmtest-informational3'] = [
    'CtsThemeHostTestCases', 'CtsHardwareTestCases', 'CtsLibcoreTestCases'
]
CONFIG['VMTEST_INFO_SUITES']['vmtest-informational4'] = ['']

# Modules that are known to download and/or push media file assets.
# TODO(ihf): Check if the media modules are not needed in N?
CONFIG['MEDIA_MODULES'] = [
]

# TODO(yoshiki, kinaba): merge it with MEDIA_MODULES.
CONFIG['NEEDS_PUSH_MEDIA'] = [
    'CtsMediaStressTestCases',
]

# Modules that are known to need the default apps of Chrome (eg. Files.app).
CONFIG['ENABLE_DEFAULT_APPS'] = [
    'CtsAppSecurityHostTestCases',
    'CtsContentTestCases',
]

# TODO(kinaba, b/110869932): remove this.
# The tests do not really require the device-info collection step to run,
# but they are greatly stabilized if the step is inserted before running the
# actual test content (presumably because the test needs some warm-up time
# after boot to collect the tested stats?)
#
# For now as a workaround, we avoid adding --skip-device-info flag.
# This is only for N.
CONFIG['NEEDS_DEVICE_INFO'] = [
    'CtsDumpsysHostTestCases',
]

# Run `eject` for (and only for) each device with RM=1 in lsblk output.
_EJECT_REMOVABLE_DISK_COMMAND = (
    "\'lsblk -do NAME,RM | sed -n s/1$//p | xargs -n1 eject\'")
# Behave more like in the verififed mode.
_SECURITY_PARANOID_COMMAND = (
    "\'echo 3 > /proc/sys/kernel/perf_event_paranoid\'")
# TODO(kinaba): Come up with a less hacky way to handle the situation.
# {0} is replaced with the retry count. Writes either 1 (required by
# CtsSimpleperfTestCases) or 3 (CtsSecurityHostTestCases).
_ALTERNATING_PARANOID_COMMAND = (
    "\'echo $(({0} % 2 * 2 + 1)) > /proc/sys/kernel/perf_event_paranoid\'")
# Expose /proc/config.gz
_CONFIG_MODULE_COMMAND = "\'modprobe configs\'"

# Preconditions applicable to public and internal tests.
CONFIG['PRECONDITION'] = {
    'CtsSecurityHostTestCases': [
        _SECURITY_PARANOID_COMMAND, # _CONFIG_MODULE_COMMAND
    ],
}
CONFIG['LOGIN_PRECONDITION'] = {
    'CtsAppSecurityHostTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
    'CtsJobSchedulerTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
    'CtsMediaTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
    'CtsOsTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
    'CtsProviderTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
}

_WIFI_CONNECT_COMMANDS = [
    # These needs to be in order.
    "'/usr/local/autotest/cros/scripts/wifi connect %s %s\' % (ssid, wifipass)",
    "'/usr/local/autotest/cros/scripts/reorder-services-moblab.sh wifi'"
]

# Preconditions applicable to public tests.
CONFIG['PUBLIC_PRECONDITION'] = {
    'CtsSecurityHostTestCases': [
        _SECURITY_PARANOID_COMMAND, # _CONFIG_MODULE_COMMAND
    ],
    'CtsUsageStatsTestCases': _WIFI_CONNECT_COMMANDS,
    'CtsNetTestCases': _WIFI_CONNECT_COMMANDS,
    'CtsLibcoreTestCases': _WIFI_CONNECT_COMMANDS,
}

CONFIG['PUBLIC_DEPENDENCIES'] = {
    'CtsCameraTestCases': ['lighting'],
    'CtsMediaTestCases': ['noloopback'],
}

# This information is changed based on regular analysis of the failure rate on
# partner moblabs.
CONFIG['PUBLIC_MODULE_RETRY_COUNT'] = {
    'CtsNetTestCases': 10,
    'CtsSecurityHostTestCases': 10,
    'CtsUsageStatsTestCases': 10,
    'CtsFileSystemTestCases': 10,
    'CtsBluetoothTestCases': 10,
}

CONFIG['PUBLIC_OVERRIDE_TEST_PRIORITY'] = {
    _PUBLIC_COLLECT: 70,
}

# This information is changed based on regular analysis of the job run time on
# partner moblabs.

CONFIG['OVERRIDE_TEST_LENGTH'] = {
    'CtsDeqpTestCases': 4,  # LONG
    'CtsMediaTestCases': 4,
    'CtsMediaStressTestCases': 4,
    'CtsSecurityTestCases': 4,
    'CtsCameraTestCases': 4,
    # Even though collect tests doesn't run very long, it must be the very first
    # job executed inside of the suite. Hence it is the only 'LENGTHY' test.
    _COLLECT: 5,  # LENGTHY
}

# Enabling --logcat-on-failure can extend total run time significantly if
# individual tests finish in the order of 10ms or less (b/118836700). Specify
# modules here to not enable the flag.
CONFIG['DISABLE_LOGCAT_ON_FAILURE'] = set([
    'CtsDeqpTestCases',
    'CtsDeqpTestCases.dEQP-EGL',
    'CtsDeqpTestCases.dEQP-GLES2',
    'CtsDeqpTestCases.dEQP-GLES3',
    'CtsDeqpTestCases.dEQP-GLES31',
    'CtsDeqpTestCases.dEQP-VK',
])

CONFIG['EXTRA_MODULES'] = {
    'CtsDeqpTestCases': {
        'SUBMODULES': set([
            'CtsDeqpTestCases.dEQP-EGL',
            'CtsDeqpTestCases.dEQP-GLES2',
            'CtsDeqpTestCases.dEQP-GLES3',
            'CtsDeqpTestCases.dEQP-GLES31',
            'CtsDeqpTestCases.dEQP-VK'
        ]),
        'SUITES': ['suite:arc-cts-deqp', 'suite:graphics_per-week'],
    },
}

CONFIG['PUBLIC_EXTRA_MODULES'] = {}

CONFIG['EXTRA_SUBMODULE_OVERRIDE'] = {}

CONFIG['EXTRA_COMMANDLINE'] = {
    'CtsDeqpTestCases.dEQP-EGL': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-EGL.*'
    ],
    'CtsDeqpTestCases.dEQP-GLES2': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-GLES2.*'
    ],
    'CtsDeqpTestCases.dEQP-GLES3': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-GLES3.*'
    ],
    'CtsDeqpTestCases.dEQP-GLES31': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-GLES31.*'
    ],
    'CtsDeqpTestCases.dEQP-VK': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.*'
    ],
}

CONFIG['EXTRA_ATTRIBUTES'] = {
    'CtsDeqpTestCases': ['suite:arc-cts', 'suite:arc-cts-deqp'],
    'CtsDeqpTestCases.dEQP-EGL': [
        'suite:arc-cts-deqp', 'suite:graphics_per-week'
    ],
    'CtsDeqpTestCases.dEQP-GLES2': [
        'suite:arc-cts-deqp', 'suite:graphics_per-week'
    ],
    'CtsDeqpTestCases.dEQP-GLES3': [
        'suite:arc-cts-deqp', 'suite:graphics_per-week'
    ],
    'CtsDeqpTestCases.dEQP-GLES31': [
        'suite:arc-cts-deqp', 'suite:graphics_per-week'
    ],
    'CtsDeqpTestCases.dEQP-VK': [
        'suite:arc-cts-deqp', 'suite:graphics_per-week'
    ],
    _COLLECT: ['suite:arc-cts-qual', 'suite:arc-cts'],
}

CONFIG['EXTRA_ARTIFACTS'] = {
}

CONFIG['PREREQUISITES'] = {
}

from generate_controlfiles_common import main

if __name__ == '__main__':
    main(CONFIG)
