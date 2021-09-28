#
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os

import ltp_enums

VTS_LTP_OUTPUT = os.path.join('DATA', 'nativetest', 'ltp')
LTP_RUNTEST_DIR = 'external/ltp/runtest'
# The bp file that contains all binaries of ltp
LTP_GEN_BINARY_BP = 'external/ltp/gen.bp'

LTP_DISABLED_BUILD_TESTS_CONFIG_PATH = 'external/ltp/android/tools/disabled_tests.txt'
# Directory for the template of the test config.
LTP_CONFIG_TEMPLATE_DIR = 'test/vts-testcase/kernel/ltp/testcase/tools/template'
# The file name of the config template file
LTP_CONFIG_TEMPLATE_FILE_NAME = 'template.xml'

# Environment paths for ltp test cases
# string, ltp build root directory on target
LTPDIR = '/data/local/tmp/ltp'
# Directory for environment variable 'TMP' needed by some test cases
TMP = os.path.join(LTPDIR, 'tmp')
# Directory for environment variable 'TMPBASE' needed by some test cases
TMPBASE = os.path.join(TMP, 'tmpbase')
# Directory for environment variable 'LTPTMP' needed by some test cases
LTPTMP = os.path.join(TMP, 'ltptemp')
# Directory for environment variable 'TMPDIR' needed by some test cases
TMPDIR = os.path.join(TMP, 'tmpdir')

# File name suffix for low memory scenario group scripts
LOW_MEMORY_SCENARIO_GROUP_SUFFIX = '_low_mem'

# Requirement to testcase dictionary.
REQUIREMENTS_TO_TESTCASE = {
    ltp_enums.Requirements.LOOP_DEVICE_SUPPORT: [
        'syscalls.mount01',
        'syscalls.fchmod06',
        'syscalls.ftruncate04',
        'syscalls.ftruncate04_64',
        'syscalls.inotify03',
        'syscalls.link08',
        'syscalls.linkat02',
        'syscalls.mkdir03',
        'syscalls.mkdirat02',
        'syscalls.mknod07',
        'syscalls.mknodat02',
        'syscalls.mmap16',
        'syscalls.mount01',
        'syscalls.mount02',
        'syscalls.mount03',
        'syscalls.mount04',
        'syscalls.mount06',
        'syscalls.rename11',
        'syscalls.renameat01',
        'syscalls.rmdir02',
        'syscalls.umount01',
        'syscalls.umount02',
        'syscalls.umount03',
        'syscalls.umount2_01',
        'syscalls.umount2_02',
        'syscalls.umount2_03',
        'syscalls.utime06',
        'syscalls.utimes01',
        'syscalls.mkfs01',
        'fs.quota_remount_test01',
    ],
    ltp_enums.Requirements.BIN_IN_PATH_LDD: ['commands.ldd'],
}

# Requirement for all test cases
REQUIREMENT_FOR_ALL = [ltp_enums.Requirements.LTP_TMP_DIR]

# Requirement to test suite dictionary
REQUIREMENT_TO_TESTSUITE = {}

# List of LTP test suites to run
TEST_SUITES = [
    'can',
    'cap_bounds',
    'commands',
    'connectors',
    'containers',
    'controllers',
    'cpuhotplug',
    'cve',
    'dio',
    'fcntl-locktests_android',
    'filecaps',
    'fs',
    'fs_bind',
    'fs_perms_simple',
    'fsx',
    'hugetlb',
    'hyperthreading',
    'input',
    'io',
    'ipc',
    'kernel_misc',
    'math',
    'mm',
    'nptl',
    'power_management_tests',
    'pty',
    'sched',
    'securebits',
    'syscalls',
    'tracing',
]

# List of LTP test suites to run
TEST_SUITES_LOW_MEM = [
    'can',
    'cap_bounds',
    'commands',
    'connectors',
    'containers',
    'cpuhotplug',
    'cve',
    'dio',
    'fcntl-locktests_android',
    'filecaps',
    'fs',
    'fs_bind',
    'fs_perms_simple',
    'fsx',
    'hugetlb',
    'hyperthreading',
    'input',
    'io',
    'ipc',
    'kernel_misc',
    'math',
    'mm',
    'nptl',
    'power_management_tests',
    'pty',
    'sched_low_mem',
    'securebits',
    'syscalls',
    'tracing',
]
