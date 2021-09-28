# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Status dict updated from HC.
DEVICE_STATUS_DICT = {
    # default state, currently not in use.
    "unknown": 0,
    # for devices detected via "fastboot devices" shell command.
    "fastboot": 1,
    # for devices detected via "adb devices" shell command.
    "online": 2,
    # currently not in use.
    "ready": 3,
    # currently not in use.
    "use": 4,
    # for devices in error state.
    "error": 5,
    # for devices which timed out (not detected either via fastboot or adb).
    "no-response": 6
}

# Scheduling status dict based on the status of each jobs in job queue.
DEVICE_SCHEDULING_STATUS_DICT = {
    # for devices detected but not scheduled.
    "free": 0,
    # for devices scheduled but not running.
    "reserved": 1,
    # for devices scheduled for currently leased job(s).
    "use": 2
}

# Job status dict
JOB_STATUS_DICT = {
    # scheduled but not leased yet
    "ready": 0,
    # scheduled and in running
    "leased": 1,
    # completed job
    "complete": 2,
    # unexpected error during running
    "infra-err": 3,
    # never leased within schedule period
    "expired": 4,
    # device boot error after flashing the given img sets
    "bootup-err": 5
}

JOB_PRIORITY_DICT = {
    "top": 3,
    "high": 6,
    "medium": 9,
    "low": 12,
    "other": 15
}


STORAGE_TYPE_DICT = {
    "unknown": 0,
    "PAB": 1,
    "GCS": 2
}


TEST_TYPE_UNKNOWN = "unknown"
TEST_TYPE_TOT = "ToT"
TEST_TYPE_OTA = "OTA"
TEST_TYPE_SIGNED = "signed"
TEST_TYPE_PRESUBMIT = "presubmit"
TEST_TYPE_MANUAL = "manual"

# a dict, where keys indicate test type and values have bitwise values.
# bit 0-1  : version related test type
#            00 - Unknown
#            01 - ToT
#            10 - OTA
# bit 2    : device signed build
# bit 3-4  : reserved for gerrit related test type
#            01 - pre-submit
# bit 5    : manually created test job
TEST_TYPE_DICT = {
    TEST_TYPE_UNKNOWN: 0,
    TEST_TYPE_TOT: 1,
    TEST_TYPE_OTA: 1 << 1,
    TEST_TYPE_SIGNED: 1 << 2,
    TEST_TYPE_PRESUBMIT: 1 << 3,
    TEST_TYPE_MANUAL: 1 << 5
}

# # of errors in a row to suspend a schedule
NUM_ERRORS_FOR_SUSPENSION = 3

# filter methods
FILTER_EqualTo = "EqualTo"
FILTER_LessThan = "LessThan"
FILTER_GreaterThan = "GreaterThan"
FILTER_LessThanOrEqualTo = "LessThanOrEqualTo"
FILTER_GreaterThanOrEqualTo = "GreaterThanOrEqualTo"
FILTER_NotEqualTo = "NotEqualTo"
FILTER_Has = "Has"

FILTER_METHOD = {
    FILTER_EqualTo: 1,
    FILTER_LessThan: 2,
    FILTER_GreaterThan: 3,
    FILTER_LessThanOrEqualTo: 4,
    FILTER_GreaterThanOrEqualTo: 5,
    FILTER_NotEqualTo: 6,
    FILTER_Has: 7,
}


def GetPriorityValue(priority):
    """Helper function to sort jobs based on priority.

    Args:
        priority: string, the job priority.

    Returns:
        int, priority order (the lower, the higher)
    """
    if priority:
        priority = priority.lower()
        if priority in JOB_PRIORITY_DICT:
            return JOB_PRIORITY_DICT[priority]
    return JOB_PRIORITY_DICT["other"]
