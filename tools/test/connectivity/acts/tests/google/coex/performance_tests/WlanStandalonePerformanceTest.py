#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

import itertools

from acts.test_utils.coex.CoexPerformanceBaseTest import CoexPerformanceBaseTest
from acts.test_utils.bt.bt_test_utils import disable_bluetooth


class WlanStandalonePerformanceTest(CoexPerformanceBaseTest):

    def __init__(self, controllers):
        super().__init__(controllers)
        req_params = ['standalone_params']
        self.unpack_userparams(req_params)
        self.tests = self.generate_test_cases()

    def setup_class(self):
        super().setup_class()
        if not disable_bluetooth(self.pri_ad.droid):
            self.log.info('Failed to disable bluetooth')
            return False

    def setup_test(self):
        super().setup_test()

    def generate_test_cases(self):
        test_cases = []
        for protocol, stream in itertools.product(
                self.standalone_params['protocol'],
                self.standalone_params['stream']):
            test_case_name = (
                'test_wlan_standalone_{}_{}'.format(protocol, stream)
            )
            setattr(
                self, test_case_name, self.set_attenuation_and_run_iperf
            )
            test_cases.append(test_case_name)
        return test_cases
