#
# Copyright (C) 2020 The Android Open Source Project
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

from proc_tests import KernelProcFileTestBase
from proc_tests.KernelProcFileTestBase import repeat_rule, literal_token


class ProcUidConcurrentActiveTimeTest(
    KernelProcFileTestBase.KernelProcFileTestBase):
    '''/proc/uid_concurrent_active_time provides the time each UID's processes spend
    executing concurrently with processes on other CPUs.

    This is an Android specific file.
    '''

    start = 'uid_active_time_table'

    t_CPU = literal_token(r'cpus')

    p_uid_active_times = repeat_rule('uid_active_time')
    p_numbers = repeat_rule('NUMBER')

    t_ignore = ' '

    def p_uid_active_time_table(self, p):
        'uid_active_time_table : cpus uid_active_times'
        p[0] = p[1:]

    def p_cpus(self, p):
        'cpus : CPU COLON NUMBER NEWLINE'
        p[0] = p[3]

    def p_uid_active_time(self, p):
        'uid_active_time : NUMBER COLON NUMBERs NEWLINE'
        p[0] = [p[1], p[3]]

    def get_path(self):
        return "/proc/uid_concurrent_active_time"

    def file_optional(self, shell=None, dut=None):
        return True

    def result_correct(self, result):
        cpus, times = result
        no_repeated_uids = len(set(x[0] for x in times)) == len(times)
        row_lengths_match = all(len(time[1]) == int(cpus) for time in times)
        return no_repeated_uids and row_lengths_match

class ProcUidConcurrentPolicyTimeTest(
    KernelProcFileTestBase.KernelProcFileTestBase):
    '''/proc/uid_concurrent_policy_time provides the time each UID's processes spend
    executing concurrently with processes on the same cluster.

    This is an Android specific file.
    '''

    start = 'uid_policy_time_table'

    t_POLICY = literal_token(r'policy')

    p_policy_infos = repeat_rule('policy_info')
    p_uid_policy_times = repeat_rule('uid_policy_time')
    p_numbers = repeat_rule('NUMBER')

    t_ignore = ' '

    def p_uid_policy_time_table(self, p):
        'uid_policy_time_table : header_row uid_policy_times'
        p[0] = p[1:]

    def p_header_row(self, p):
        'header_row : policy_infos NEWLINE'
        p[0] = sum(int(x) for x in p[1])

    def p_policy_info(self, p):
        'policy_info : POLICY NUMBER COLON NUMBER'
        p[0] = p[4]

    def p_uid_policy_time(self, p):
        'uid_policy_time : NUMBER COLON NUMBERs NEWLINE'
        p[0] = [p[1], p[3]]

    def get_path(self):
        return "/proc/uid_concurrent_policy_time"

    def file_optional(self, shell=None, dut=None):
        return True

    def result_correct(self, result):
        cpus, times = result
        no_repeated_uids = len(set(x[0] for x in times)) == len(times)
        row_lengths_match = all(len(time[1]) == int(cpus) for time in times)
        return no_repeated_uids and row_lengths_match
