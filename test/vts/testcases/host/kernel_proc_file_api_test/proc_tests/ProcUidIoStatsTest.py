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
from proc_tests.KernelProcFileTestBase import repeat_rule


class ProcUidIoStatsTest(KernelProcFileTestBase.KernelProcFileTestBase):
    '''/proc/uid_io/stats returns a list of I/O stats for each UID in the system.

    This is an Android specific file.
    '''

    t_ignore = ' '
    start = 'lines'
    p_lines = repeat_rule('line')

    def p_line(self, p):
        '''line : NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NEWLINE'''
        p[0] = p[1:]

    def get_path(self):
        return "/proc/uid_io/stats"
