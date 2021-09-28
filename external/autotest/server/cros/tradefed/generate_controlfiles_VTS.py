#!/usr/bin/env python2
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from generate_controlfiles_common import main


_ALL = 'all'

CONFIG = {}

CONFIG['TEST_NAME'] = 'cheets_VTS'
CONFIG['DOC_TITLE'] = \
    'Vendor Test Suite (VTS)'
CONFIG['MOBLAB_SUITE_NAME'] = 'suite:android-vts'
CONFIG['COPYRIGHT_YEAR'] = 2020
CONFIG['AUTHKEY'] = ''

CONFIG['LARGE_MAX_RESULT_SIZE'] = 1000 * 1024
CONFIG['NORMAL_MAX_RESULT_SIZE'] = 500 * 1024

CONFIG['TRADEFED_CTS_COMMAND'] = 'vts'
CONFIG['TRADEFED_RETRY_COMMAND'] = 'retry'
CONFIG['TRADEFED_DISABLE_REBOOT'] = False
CONFIG['TRADEFED_DISABLE_REBOOT_ON_COLLECTION'] = True
CONFIG['TRADEFED_MAY_SKIP_DEVICE_INFO'] = False
CONFIG['TRADEFED_EXECUTABLE_PATH'] = 'android-vts/tools/vts-tradefed'

CONFIG['TRADEFED_IGNORE_BUSINESS_LOGIC_FAILURE'] = False

CONFIG['INTERNAL_SUITE_NAMES'] = ['suite:arc-vts']

CONFIG['CONTROLFILE_TEST_FUNCTION_NAME'] = 'run_TS'
CONFIG['CONTROLFILE_WRITE_SIMPLE_QUAL_AND_REGRESS'] = False # True
CONFIG['CONTROLFILE_WRITE_CAMERA'] = False
CONFIG['CONTROLFILE_WRITE_EXTRA'] = False

# Do not change the name/tag without adjusting the dashboard.
_COLLECT = 'tradefed-run-collect-tests-only-internal'
_PUBLIC_COLLECT = 'tradefed-run-collect-tests-only'

CONFIG['LAB_DEPENDENCY'] = {}

CONFIG['CTS_JOB_RETRIES_IN_PUBLIC'] = 1
CONFIG['CTS_QUAL_RETRIES'] = 9
CONFIG['CTS_MAX_RETRIES'] = {}

# Timeout in hours.
CONFIG['CTS_TIMEOUT_DEFAULT'] = 1.0
CONFIG['CTS_TIMEOUT'] = {
    _ALL: 5.0,
    _COLLECT: 2.0,
    _PUBLIC_COLLECT: 2.0,
}

# Any test that runs as part as blocking BVT needs to be stable and fast. For
# this reason we enforce a tight timeout on these modules/jobs.
# Timeout in hours. (0.1h = 6 minutes)
CONFIG['BVT_TIMEOUT'] = 0.1

CONFIG['QUAL_TIMEOUT'] = 5

CONFIG['QUAL_BOOKMARKS'] = []

CONFIG['SMOKE'] = []

CONFIG['BVT_ARC'] = []

CONFIG['BVT_PERBUILD'] = []

CONFIG['NEEDS_POWER_CYCLE'] = []

CONFIG['HARDWARE_DEPENDENT_MODULES'] = []

# The suite is divided based on the run-time hint in the *.config file.
CONFIG['VMTEST_INFO_SUITES'] = collections.OrderedDict()

# Modules that are known to download and/or push media file assets.
CONFIG['MEDIA_MODULES'] = []
CONFIG['NEEDS_PUSH_MEDIA'] = []

CONFIG['ENABLE_DEFAULT_APPS'] = []

# Preconditions applicable to public and internal tests.
CONFIG['PRECONDITION'] = {}
CONFIG['LOGIN_PRECONDITION'] = {}

# Preconditions applicable to public tests.
CONFIG['PUBLIC_PRECONDITION'] = {}

CONFIG['PUBLIC_DEPENDENCIES'] = {}

# This information is changed based on regular analysis of the failure rate on
# partner moblabs.
CONFIG['PUBLIC_MODULE_RETRY_COUNT'] = {
    _PUBLIC_COLLECT: 0,
}

CONFIG['PUBLIC_OVERRIDE_TEST_PRIORITY'] = {
    _PUBLIC_COLLECT: 70,
}

# This information is changed based on regular analysis of the job run time on
# partner moblabs.

CONFIG['OVERRIDE_TEST_LENGTH'] = {
    # Even though collect tests doesn't run very long, it must be the very first
    # job executed inside of the suite. Hence it is the only 'LENGTHY' test.
    _COLLECT: 5,  # LENGTHY
}

CONFIG['DISABLE_LOGCAT_ON_FAILURE'] = set()
CONFIG['EXTRA_MODULES'] = {}
CONFIG['PUBLIC_EXTRA_MODULES'] = {}
CONFIG['EXTRA_SUBMODULE_OVERRIDE'] = {}

CONFIG['EXTRA_COMMANDLINE'] = {}

CONFIG['EXTRA_ATTRIBUTES'] = {
    'tradefed-run-collect-tests-only-internal': ['suite:arc-vts'],
}

CONFIG['EXTRA_ARTIFACTS'] = {}
CONFIG['PREREQUISITES'] = {}

if __name__ == '__main__':
        main(CONFIG)

