# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
USAGE: python generate_storage_qual_control_files.py

Creates all the control files required to run the storage qual full two week
test. The test consists of 3 sub-tests, each runs continuosuly on one DUT, and
runs stress tests to wear out the DUT's SSD. Each sub-test is broken into 4
phases, a base test to collect baseline performance data, a soak test to
produce wear on the SSD over 1 week, a payload test which does other types
of stress over 1 week depending on the sub-test, and another base test to
collect performance data after the device has been stressed for 2 weeks.

The week long phases are broken into iterations, so that a single test can
fail without failing the entire 2 week suite. Each sub-test is assigned a
label, and all control files in that sub-test are given that label as a
dependency, so that the tests are forced to run on the correct DUT. Priority
is used to force the tests to run in the correct order. Note that iterations
within the same phase can run out of order with no issue, but the entire phase
must complete before the next phase begins.

The generated control files follow this naming convention
control.[Suite name]_[Test name]_[DUT label]_[test tag?]_[iteration?]
Examples
control.storage_qual_hardware_StorageQualBase_retention_before
control.storeage_qual_hardware_StorageStress_retention_soak_12
"""
import copy

STORAGE_QUAL_VERSION = 1
DAY_IN_HOURS = 24
MINUTE_IN_SECS = 60
HOUR_IN_SECS = MINUTE_IN_SECS * 60
DAY_IN_SECS = HOUR_IN_SECS * DAY_IN_HOURS

CHECK_SETUP = {
    'test': 'hardware_StorageQualCheckSetup',
    'args': {},
    'priority': 110,
    'length': 'lengthy'
}

BASE_BEFORE = {
    'test': 'hardware_StorageQualBase',
    'args': {'tag': 'before', 'client_tag': 'before'},
    'priority': 100,
    'length': 'lengthy'
}

SOAK = {
    'test': 'hardware_StorageStress',
    'args': {'tag': 'soak', 'power_command': 'wait',
        'storage_test_command': 'full_write',
        'suspend_duration': 5 * MINUTE_IN_SECS,
        'duration': 4 * HOUR_IN_SECS
    },
    'iterations': 7 * DAY_IN_HOURS / 4,
    'priority': 90,
    'length': 'long'
}

BASE_AFTER = {
    'test': 'hardware_StorageQualBase',
    'args': {'tag': 'after', 'client_tag': 'after'},
    'priority': 70,
    'length': 'long'
}

SOAK_QUICK = copy.deepcopy(SOAK)
SOAK_QUICK['iterations'] = 2
SOAK_QUICK['args']['duration'] = HOUR_IN_SECS

BASE_BEFORE_CQ = copy.deepcopy(BASE_BEFORE)
BASE_BEFORE_CQ['args']['cq'] = True
SOAK_CQ = copy.deepcopy(SOAK)
SOAK_CQ['args']['cq'] = True
SOAK_CQ['iterations'] = 2
BASE_AFTER_CQ = copy.deepcopy(BASE_AFTER)
BASE_AFTER_CQ['args']['cq'] = True

SUITES = {
    'storage_qual': [
        {
            'label': 'retention',
            'tests': [
                CHECK_SETUP,
                BASE_BEFORE,
                SOAK,
                {
                    'test': 'hardware_StorageStress',
                    'args': {'tag': 'suspend', 'power_command': 'suspend',
                        'storage_test_command': 'full_write',
                        'suspend_duration': 12 * HOUR_IN_SECS,
                        'duration': 7 * DAY_IN_SECS
                    },
                    'priority': 80,
                    'length': 'long'
                },
                BASE_AFTER
            ]
        },

        {
            'label': 'suspend',
            'tests': [
                BASE_BEFORE,
                SOAK,
                {
                    'test': 'hardware_StorageQualSuspendStress',
                    'args': {'tag': 'suspend', 'duration': 4 * HOUR_IN_SECS},
                    'iterations': 7 * DAY_IN_HOURS / 4,
                    'priority': 80,
                    'length': 'long'
                },
                BASE_AFTER
            ]
        },

        {
            'label': 'trim',
            'tests': [
                BASE_BEFORE,
                SOAK,
                {
                    'test': 'hardware_StorageQualTrimStress',
                    'args': {'duration': 4 * HOUR_IN_SECS},
                    'iterations': 7 * DAY_IN_HOURS / 4,
                    'priority': 80,
                    'length': 'long'
                },
                BASE_AFTER
            ]
        }

    ],
    'storage_qual_quick': [
        {
            'label': 'retention',
            'tests': [
                CHECK_SETUP,
                BASE_BEFORE,
                SOAK_QUICK,
                {
                    'test': 'hardware_StorageStress',
                    'args': {'tag': 'suspend', 'power_command': 'suspend',
                        'storage_test_command': 'full_write',
                        'suspend_duration': 120,
                        'duration': HOUR_IN_SECS / 2
                    },
                    'priority': 80,
                    'length': 'long'
                },
                BASE_AFTER
            ]
        },

        {
            'label': 'suspend',
            'tests': [
                BASE_BEFORE,
                SOAK_QUICK,
                {
                    'test': 'hardware_StorageQualSuspendStress',
                    'args': {'tag': 'suspend', 'duration': HOUR_IN_SECS / 2},
                    'iterations': 2,
                    'priority': 80,
                    'length': 'long'
                },
                BASE_AFTER
            ]
        },

        {
            'label': 'trim',
            'tests': [
                BASE_BEFORE,
                SOAK_QUICK,
                {
                    'test': 'hardware_StorageQualTrimStress',
                    'args': {'duration': HOUR_IN_SECS / 2},
                    'iterations': 2,
                    'priority': 80,
                    'length': 'long'
                },
                BASE_AFTER
            ]
        }
    ],
    'storage_qual_cq': [
        {
            'label': 'storage_qual_cq_1',
            'tests': [
                BASE_BEFORE_CQ,
                SOAK_CQ,
                {
                    'test': 'hardware_StorageStress',
                    'args': {'tag': 'suspend', 'power_command': 'suspend',
                        'storage_test_command': 'full_write',
                        'suspend_duration': 120,
                        'duration': HOUR_IN_SECS / 2,
                        'cq': True
                    },
                    'priority': 80,
                    'length': 'long'
                },
                BASE_AFTER_CQ
            ]
        },

        {
            'label': 'storage_qual_cq_2',
            'tests': [
                BASE_BEFORE_CQ,
                SOAK_CQ,
                {
                    'test': 'hardware_StorageQualTrimStress',
                    'args': {'duration': HOUR_IN_SECS / 2, 'cq': True},
                    'iterations': 2,
                    'priority': 80,
                    'length': 'long'
                },
                BASE_AFTER_CQ
            ]
        }
    ]
}

SUITE_ATTRIBUTES = {
    'storage_qual': 'suite:storage_qual',
    'storage_qual_quick': 'suite:storage_qual_quick',
    'storage_qual_cq': 'suite:storage_qual_cq'
}

TEMPLATE = """
# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This control file was auto-generated by generate_storage_qual_control_files.py
# Do not edit this file!

from autotest_lib.client.common_lib import utils

AUTHOR = "Chrome OS Team"
NAME = "{name}"
ATTRIBUTES = "{attributes}"
PURPOSE = "{name}"
TIME = "{length}"
TEST_CATEGORY = "Stress"
TEST_CLASS = "Hardware"
TEST_TYPE = "server"
REQUIRE_SSP = False
PRIORITY = {priority}
DEPENDENCIES = "{label}"
JOB_RETRIES = 0

DOC = "{name}"

keyval = dict()
keyval['storage_qual_version'] = {version}
keyval['bug_id'] = bug_id
keyval['part_id'] = part_id
utils.write_keyval(job.resultdir, keyval)

def run(machine):
    job.run_test("{test}", host=hosts.create_host(machine),
            client_ip=machine, {args})

parallel_simple(run, machines)
"""


def _get_name(label, test, i=None):
    parts = [test['test'], label]
    if 'tag' in test['args']:
        parts.append(test['args']['tag'])
    if i is not None:
        parts.append(str(i))
    return '_'.join(parts)


def _get_control_file_name(suite, label, test, i=None):
    return 'control.' + '_'.join([suite,  _get_name(label, test, i)])


def _get_args(test):
    args = []
    for key, value in test['args'].iteritems():
        args.append('%s=%s' % (key, repr(value)))
    return ', '.join(args)


def _write_control_file(name, contents):
    f = open(name, 'w')
    f.write(contents)
    f.close()


for suite in SUITES:
    for sub_test in SUITES[suite]:
        label = sub_test['label']
        for test in sub_test['tests']:
            if 'iterations' in test:
                for i in xrange(test['iterations']):
                    control_file = TEMPLATE.format(
                        label = label,
                        name = _get_name(label, test, i),
                        args = _get_args(test),
                        priority = test['priority'],
                        test = test['test'],
                        length = test['length'],
                        attributes = SUITE_ATTRIBUTES[suite],
                        version = STORAGE_QUAL_VERSION,
                    )
                    _write_control_file(_get_control_file_name(
                        suite, label, test, i), control_file)

            else:
                control_file = TEMPLATE.format(
                    label = label,
                    name = _get_name(label, test),
                    args = _get_args(test),
                    priority = test['priority'],
                    test = test['test'],
                    length = test['length'],
                    attributes = SUITE_ATTRIBUTES[suite],
                    version = STORAGE_QUAL_VERSION
                )
                _write_control_file(_get_control_file_name(suite, label, test),
                        control_file)

