#!/usr/bin/env python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

# The dashboard suppresses upload to APFE for GS directories (based on autotest
# tag) that contain 'tradefed-run-collect-tests'. b/119640440
# Do not change the name/tag without adjusting the dashboard.
_COLLECT = 'tradefed-run-collect-tests-only-internal'
_PUBLIC_COLLECT = 'tradefed-run-collect-tests-only'
_ALL = 'all'

CONFIG = {}

CONFIG['TEST_NAME'] = 'cheets_GTS_N'
CONFIG['DOC_TITLE'] = 'Android Google Test Suite (GTS)'
CONFIG['MOBLAB_SUITE_NAME'] = 'suite:gts'
CONFIG['COPYRIGHT_YEAR'] = 2016

CONFIG['AUTHKEY'] = 'gs://chromeos-arc-images/cts/bundle/gts-arc.json'
CONFIG['TRADEFED_IGNORE_BUSINESS_LOGIC_FAILURE'] = True

CONFIG['LARGE_MAX_RESULT_SIZE'] = 500 * 1024
CONFIG['NORMAL_MAX_RESULT_SIZE'] = 300 * 1024

CONFIG['TRADEFED_CTS_COMMAND'] ='gts'
CONFIG['TRADEFED_RETRY_COMMAND'] = 'retry'
CONFIG['TRADEFED_DISABLE_REBOOT'] = False
CONFIG['TRADEFED_DISABLE_REBOOT_ON_COLLECTION'] = True
CONFIG['TRADEFED_MAY_SKIP_DEVICE_INFO'] = False
CONFIG['NEEDS_DEVICE_INFO'] = []
CONFIG['TRADEFED_EXECUTABLE_PATH'] = 'android-gts/tools/gts-tradefed'

CONFIG['INTERNAL_SUITE_NAMES'] = ['suite:arc-gts']
CONFIG['QUAL_SUITE_NAMES'] = ['suite:arc-gts-qual']

CONFIG['CONTROLFILE_TEST_FUNCTION_NAME'] = 'run_TS'
CONFIG['CONTROLFILE_WRITE_SIMPLE_QUAL_AND_REGRESS'] = False
CONFIG['CONTROLFILE_WRITE_CAMERA'] = False
CONFIG['CONTROLFILE_WRITE_EXTRA'] = False

CONFIG['CTS_JOB_RETRIES_IN_PUBLIC'] = 2
CONFIG['CTS_QUAL_RETRIES'] = 9
CONFIG['CTS_MAX_RETRIES'] = {}

# Timeout in hours.
# Modules that run very long are encoded here.
CONFIG['CTS_TIMEOUT_DEFAULT'] = 0.2
CONFIG['CTS_TIMEOUT'] = {
    'GtsExoPlayerTestCases': 1.5,
    'GtsGmscoreHostTestCases': 1.0,
    'GtsMediaTestCases': 4,
    'GtsYouTubeTestCases': 1.0,
    _ALL: 24,
    _COLLECT: 0.5,
    _PUBLIC_COLLECT: 0.5,
}

# Any test that runs as part as blocking BVT needs to be stable and fast. For
# this reason we enforce a tight timeout on these modules/jobs.
# Timeout in hours. (0.1h = 6 minutes)
CONFIG['BVT_TIMEOUT'] = 0.1
# We allow a very long runtime for qualification (1 day).
CONFIG['QUAL_TIMEOUT'] = 24

CONFIG['QUAL_BOOKMARKS'] = sorted([
    'A',  # A bookend to simplify partition algorithm.
    'zzzzz'  # A bookend to simplify algorithm.
])

CONFIG['SMOKE'] = []

CONFIG['BVT_ARC'] = [
    'GtsContactsTest',
]

CONFIG['BVT_PERBUILD'] = [
    'GtsAdminTestCases',
    'GtsMemoryHostTestCases',
    'GtsMemoryTestCases',
    'GtsNetTestCases',
    'GtsOsTestCases',
    'GtsPlacementTestCases',
    'GtsPrivacyTestCases',
]

CONFIG['NEEDS_POWER_CYCLE'] = []

CONFIG['HARDWARE_DEPENDENT_MODULES'] = []

CONFIG['VMTEST_INFO_SUITES'] = collections.OrderedDict()

# Modules that are known to download and/or push media file assets.
CONFIG['MEDIA_MODULES'] = ['GtsYouTubeTestCases']
CONFIG['NEEDS_PUSH_MEDIA'] = CONFIG['MEDIA_MODULES'] + [_ALL]
CONFIG['ENABLE_DEFAULT_APPS'] = [
    # TODO(kinaba): See b/143740808. Can be droped after GTS7.0r3 deployed.
    'GtsAssistantHostTestCases'
]

# Preconditions applicable to public and internal tests.
CONFIG['PRECONDITION'] = {}
CONFIG['LOGIN_PRECONDITION'] = {}

CONFIG['LAB_DEPENDENCY'] = {}

# Preconditions applicable to public tests.
CONFIG['PUBLIC_PRECONDITION'] = {}
CONFIG['PUBLIC_DEPENDENCIES'] = {}

# This information is changed based on regular analysis of the failure rate on
# partner moblabs.
CONFIG['PUBLIC_MODULE_RETRY_COUNT'] = {
  _ALL: 2,
  'GtsMediaTestCases': 5   # TODO(b/140841434)
}

# This information is changed based on regular analysis of the job run time on
# partner moblabs.

CONFIG['OVERRIDE_TEST_LENGTH'] = {
    'GtsMediaTestCases': 4,
    _ALL: 4,
    # Even though collect tests doesn't run very long, it must be the very first
    # job executed inside of the suite. Hence it is the only 'LENGTHY' test.
    _COLLECT: 5,  # LENGTHY
}

# Enabling --logcat-on-failure can extend total run time significantly if
# individual tests finish in the order of 10ms or less (b/118836700). Specify
# modules here to not enable the flag.
CONFIG['DISABLE_LOGCAT_ON_FAILURE'] = set([])
CONFIG['EXTRA_MODULES'] = {}
CONFIG['PUBLIC_EXTRA_MODULES'] = {}
CONFIG['EXTRA_SUBMODULE_OVERRIDE'] = {}
CONFIG['EXTRA_COMMANDLINE'] = {}
CONFIG['EXTRA_ATTRIBUTES'] = {
    'tradefed-run-collect-tests-only-internal': ['suite:arc-gts'],
}
CONFIG['EXTRA_ARTIFACTS'] = {}
CONFIG['PREREQUISITES'] = {}

from generate_controlfiles_common import main

if __name__ == '__main__':
    main(CONFIG)

