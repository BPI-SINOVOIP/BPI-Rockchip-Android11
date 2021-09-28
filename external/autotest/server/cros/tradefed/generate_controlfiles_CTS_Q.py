#!/usr/bin/env python2
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections


CONFIG = {}

CONFIG['TEST_NAME'] = 'cheets_CTS_Q'
CONFIG['DOC_TITLE'] = 'Android Compatibility Test Suite (CTS)'
CONFIG['MOBLAB_SUITE_NAME'] = 'suite:cts_Q'
CONFIG['COPYRIGHT_YEAR'] = 2019
CONFIG['AUTHKEY'] = ''

# Both arm, x86 tests results normally is below 200MB.
# 1000MB should be sufficient for CTS tests and dump logs for android-cts.
CONFIG['LARGE_MAX_RESULT_SIZE'] = 1000 * 1024

# Individual module normal produces less results than all modules, which is
# ranging from 4MB to 50MB. 500MB should be sufficient to handle all the cases.
CONFIG['NORMAL_MAX_RESULT_SIZE'] = 500 * 1024

CONFIG['TRADEFED_CTS_COMMAND'] = 'cts'
CONFIG['TRADEFED_RETRY_COMMAND'] = 'retry'
CONFIG['TRADEFED_DISABLE_REBOOT'] = False
CONFIG['TRADEFED_DISABLE_REBOOT_ON_COLLECTION'] = True
CONFIG['TRADEFED_MAY_SKIP_DEVICE_INFO'] = False
CONFIG['TRADEFED_EXECUTABLE_PATH'] = 'android-cts/tools/cts-tradefed'
CONFIG['TRADEFED_IGNORE_BUSINESS_LOGIC_FAILURE'] = False

# On moblab everything runs in the same suite.
CONFIG['INTERNAL_SUITE_NAMES'] = ['suite:arc-cts-q']
CONFIG['QUAL_SUITE_NAMES'] = []

CONFIG['CONTROLFILE_TEST_FUNCTION_NAME'] = 'run_TS'
CONFIG['CONTROLFILE_WRITE_SIMPLE_QUAL_AND_REGRESS'] = False
CONFIG['CONTROLFILE_WRITE_CAMERA'] = False
CONFIG['CONTROLFILE_WRITE_EXTRA'] = True

# The dashboard suppresses upload to APFE for GS directories (based on autotest
# tag) that contain 'tradefed-run-collect-tests'. b/119640440
# Do not change the name/tag without adjusting the dashboard.
_COLLECT = 'tradefed-run-collect-tests-only-internal'

CONFIG['LAB_DEPENDENCY'] = {
   'x86': ['cts_abi_x86']
}

CONFIG['CTS_JOB_RETRIES_IN_PUBLIC'] = 1
CONFIG['CTS_QUAL_RETRIES'] = 9
CONFIG['CTS_MAX_RETRIES'] = {
    'CtsDeqpTestCases':         15,  # TODO(b/126787654)
    'CtsIncidentHostTestCases': 30,  # TODO(b/128695132)
    'CtsSensorTestCases':       30,  # TODO(b/124528412)
}

# Timeout in hours.
CONFIG['CTS_TIMEOUT_DEFAULT'] = 1.0
CONFIG['CTS_TIMEOUT'] = {
    'CtsAutoFillServiceTestCases':       2.5,  # TODO(b/134662826)
    'CtsDeqpTestCases':                 30.0,
    'CtsDeqpTestCases.dEQP-EGL'  :       2.0,
    'CtsDeqpTestCases.dEQP-GLES2':       2.0,
    'CtsDeqpTestCases.dEQP-GLES3':       6.0,
    'CtsDeqpTestCases.dEQP-GLES31':      6.0,
    'CtsDeqpTestCases.dEQP-VK':         15.0,
    'CtsFileSystemTestCases':            3.0,
    'CtsHardwareTestCases':              2.0,
    'CtsIcuTestCases':                   2.0,
    'CtsLibcoreOjTestCases':             2.0,
    'CtsMediaStressTestCases':           5.0,
    'CtsMediaTestCases':                10.0,
    'CtsNNAPIBenchmarkTestCases':        2.0,
    'CtsPrintTestCases':                 1.5,
    'CtsSecurityTestCases':              2.0,
    'CtsSensorTestCases':                2.0,
    'CtsStatsdHostTestCases':            2.0,
    'CtsVideoTestCases':                 1.5,
    'CtsWidgetTestCases':                2.0,
    _COLLECT:                            2.5,
}

# Any test that runs as part as blocking BVT needs to be stable and fast. For
# this reason we enforce a tight timeout on these modules/jobs.
# Timeout in hours. (0.1h = 6 minutes)
CONFIG['BVT_TIMEOUT'] = 0.1
# We allow a very long runtime for qualification (2 days).
CONFIG['QUAL_TIMEOUT'] = 48

CONFIG['QUAL_BOOKMARKS'] = sorted([])

CONFIG['SMOKE'] = []

CONFIG['BVT_ARC'] = []

CONFIG['BVT_PERBUILD'] = []

CONFIG['NEEDS_POWER_CYCLE'] = []

CONFIG['HARDWARE_DEPENDENT_MODULES'] = []

# The suite is divided based on the run-time hint in the *.config file.
CONFIG['VMTEST_INFO_SUITES'] = collections.OrderedDict()

# Modules that are known to download and/or push media file assets.
CONFIG['MEDIA_MODULES'] = [
    'CtsMediaTestCases',
    'CtsMediaStressTestCases',
    'CtsMediaBitstreamsTestCases',
]

CONFIG['NEEDS_PUSH_MEDIA'] = CONFIG['MEDIA_MODULES']

# Modules that are known to need the default apps of Chrome (eg. Files.app).
CONFIG['ENABLE_DEFAULT_APPS'] = [
    'CtsAppSecurityHostTestCases',
]

# Run `eject` for (and only for) each device with RM=1 in lsblk output.
_EJECT_REMOVABLE_DISK_COMMAND = (
    "\'lsblk -do NAME,RM | sed -n s/1$//p | xargs -n1 eject\'")
# Behave more like in the verififed mode.
_SECURITY_PARANOID_COMMAND = (
    "\'echo 3 > /proc/sys/kernel/perf_event_paranoid\'")
# Expose /proc/config.gz
_CONFIG_MODULE_COMMAND = "\'modprobe configs\'"

# TODO(b/126741318): Fix performance regression and remove this.
_SLEEP_60_COMMAND = "\'sleep 60\'"

# Preconditions applicable to public and internal tests.
CONFIG['PRECONDITION'] = {
    'CtsSecurityHostTestCases': [
        _SECURITY_PARANOID_COMMAND, _CONFIG_MODULE_COMMAND
    ],
    # Tests are performance-sensitive, workaround to avoid CPU load on login.
    # TODO(b/126741318): Fix performance regression and remove this.
    'CtsViewTestCases': [_SLEEP_60_COMMAND],
}
CONFIG['LOGIN_PRECONDITION'] = {
    'CtsAppSecurityHostTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
    'CtsJobSchedulerTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
    'CtsMediaTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
    'CtsOsTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
    'CtsProviderTestCases': [_EJECT_REMOVABLE_DISK_COMMAND],
}

# Preconditions applicable to public tests.
CONFIG['PUBLIC_PRECONDITION'] = {
}

CONFIG['PUBLIC_DEPENDENCIES'] = {
}

# This information is changed based on regular analysis of the failure rate on
# partner moblabs.
CONFIG['PUBLIC_MODULE_RETRY_COUNT'] = {
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
    'all',
    'CtsDeqpTestCases',
    'CtsDeqpTestCases.dEQP-EGL',
    'CtsDeqpTestCases.dEQP-GLES2',
    'CtsDeqpTestCases.dEQP-GLES3',
    'CtsDeqpTestCases.dEQP-GLES31',
    'CtsDeqpTestCases.dEQP-VK',
    'CtsLibcoreTestCases',
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
        'SUITES': ['suite:arc-cts-q'],
    },
}

# Moblab wants to shard dEQP really finely. This isn't needed anymore as it got
# faster, but I guess better safe than sorry.
CONFIG['PUBLIC_EXTRA_MODULES'] = {
    'CtsDeqpTestCases' : [
        'CtsDeqpTestCases.dEQP-EGL',
        'CtsDeqpTestCases.dEQP-GLES2',
        'CtsDeqpTestCases.dEQP-GLES3',
        'CtsDeqpTestCases.dEQP-GLES31',
        'CtsDeqpTestCases.dEQP-VK.api',
        'CtsDeqpTestCases.dEQP-VK.binding_model',
        'CtsDeqpTestCases.dEQP-VK.clipping',
        'CtsDeqpTestCases.dEQP-VK.compute',
        'CtsDeqpTestCases.dEQP-VK.device_group',
        'CtsDeqpTestCases.dEQP-VK.draw',
        'CtsDeqpTestCases.dEQP-VK.dynamic_state',
        'CtsDeqpTestCases.dEQP-VK.fragment_operations',
        'CtsDeqpTestCases.dEQP-VK.geometry',
        'CtsDeqpTestCases.dEQP-VK.glsl',
        'CtsDeqpTestCases.dEQP-VK.image',
        'CtsDeqpTestCases.dEQP-VK.info',
        'CtsDeqpTestCases.dEQP-VK.memory',
        'CtsDeqpTestCases.dEQP-VK.multiview',
        'CtsDeqpTestCases.dEQP-VK.pipeline',
        'CtsDeqpTestCases.dEQP-VK.protected_memory',
        'CtsDeqpTestCases.dEQP-VK.query_pool',
        'CtsDeqpTestCases.dEQP-VK.rasterization',
        'CtsDeqpTestCases.dEQP-VK.renderpass',
        'CtsDeqpTestCases.dEQP-VK.renderpass2',
        'CtsDeqpTestCases.dEQP-VK.robustness',
        'CtsDeqpTestCases.dEQP-VK.sparse_resources',
        'CtsDeqpTestCases.dEQP-VK.spirv_assembly',
        'CtsDeqpTestCases.dEQP-VK.ssbo',
        'CtsDeqpTestCases.dEQP-VK.subgroups',
        'CtsDeqpTestCases.dEQP-VK.subgroups.b',
        'CtsDeqpTestCases.dEQP-VK.subgroups.s',
        'CtsDeqpTestCases.dEQP-VK.subgroups.vote',
        'CtsDeqpTestCases.dEQP-VK.subgroups.arithmetic',
        'CtsDeqpTestCases.dEQP-VK.subgroups.clustered',
        'CtsDeqpTestCases.dEQP-VK.subgroups.quad',
        'CtsDeqpTestCases.dEQP-VK.synchronization',
        'CtsDeqpTestCases.dEQP-VK.tessellation',
        'CtsDeqpTestCases.dEQP-VK.texture',
        'CtsDeqpTestCases.dEQP-VK.ubo',
        'CtsDeqpTestCases.dEQP-VK.wsi',
        'CtsDeqpTestCases.dEQP-VK.ycbcr'
    ]
}
# TODO(haddowk,kinaba): Hack for b/138622686. Clean up later.
CONFIG['EXTRA_SUBMODULE_OVERRIDE'] = {
    'x86': {
         'CtsDeqpTestCases.dEQP-VK.subgroups.arithmetic': [
             'CtsDeqpTestCases.dEQP-VK.subgroups.arithmetic.32',
             'CtsDeqpTestCases.dEQP-VK.subgroups.arithmetic.64',
         ]
    }
}

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
    'CtsDeqpTestCases.dEQP-VK.api': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.api.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.binding_model': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.binding_model.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.clipping': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.clipping.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.compute': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.compute.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.device_group': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.device_group*'  # Not ending on .* like most others!
    ],
    'CtsDeqpTestCases.dEQP-VK.draw': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.draw.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.dynamic_state': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.dynamic_state.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.fragment_operations': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.fragment_operations.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.geometry': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.geometry.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.glsl': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.glsl.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.image': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.image.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.info': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.info*'  # Not ending on .* like most others!
    ],
    'CtsDeqpTestCases.dEQP-VK.memory': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.memory.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.multiview': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.multiview.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.pipeline': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.pipeline.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.protected_memory': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.protected_memory.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.query_pool': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.query_pool.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.rasterization': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.rasterization.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.renderpass': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.renderpass.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.renderpass2': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.renderpass2.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.robustness': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.robustness.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.sparse_resources': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.sparse_resources.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.spirv_assembly': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.spirv_assembly.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.ssbo': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.ssbo.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.subgroups': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.*'
    ],
    # Splitting VK.subgroups to smaller pieces to workaround b/138622686.
    # TODO(kinaba,haddowk): remove them once the root cause is fixed, or
    # reconsider the sharding strategy.
    'CtsDeqpTestCases.dEQP-VK.subgroups.b': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.b*'
    ],
    'CtsDeqpTestCases.dEQP-VK.subgroups.s': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.s*'
    ],
    'CtsDeqpTestCases.dEQP-VK.subgroups.vote': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.vote#*'
    ],
    'CtsDeqpTestCases.dEQP-VK.subgroups.arithmetic': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.arithmetic#*'
    ],
    # TODO(haddowk,kinaba): Hack for b/138622686. Clean up later.
    'CtsDeqpTestCases.dEQP-VK.subgroups.arithmetic.32': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.arithmetic#*', '--abi', 'x86'
    ],
    # TODO(haddowk,kinaba): Hack for b/138622686. Clean up later.
    'CtsDeqpTestCases.dEQP-VK.subgroups.arithmetic.64': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.arithmetic#*', '--abi', 'x86_64'
    ],
    'CtsDeqpTestCases.dEQP-VK.subgroups.clustered': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.clustered#*'
    ],
    'CtsDeqpTestCases.dEQP-VK.subgroups.quad': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.subgroups.quad#*'
    ],
    'CtsDeqpTestCases.dEQP-VK.synchronization': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.synchronization.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.tessellation': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.tessellation.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.texture': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.texture.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.ubo': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.ubo.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.wsi': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.wsi.*'
    ],
    'CtsDeqpTestCases.dEQP-VK.ycbcr': [
        '--include-filter', 'CtsDeqpTestCases', '--module', 'CtsDeqpTestCases',
        '--test', 'dEQP-VK.ycbcr.*'
    ]
}

CONFIG['EXTRA_ATTRIBUTES'] = {}

CONFIG['EXTRA_ARTIFACTS'] = {}
CONFIG['PREREQUISITES'] = {}

from generate_controlfiles_common import main

if __name__ == '__main__':
    main(CONFIG)
