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

from acts.test_utils.bt.bt_test_utils import enable_bluetooth
from acts.test_utils.coex.CoexPerformanceBaseTest import CoexPerformanceBaseTest
from acts.test_utils.coex.coex_test_utils import perform_classic_discovery


class CoexBasicPerformanceTest(CoexPerformanceBaseTest):

    def __init__(self, controllers):
        super().__init__(controllers)
        req_params = [
            # A dict containing:
            #     protocol: A list containing TCP/UDP. Ex: protocol: ['tcp'].
            #     stream: A list containing ul/dl. Ex: stream: ['ul']
            'standalone_params'
        ]
        self.unpack_userparams(req_params)
        self.tests = self.generated_test_cases(['bt_on', 'perform_discovery'])

    def perform_discovery(self):
        """ Starts iperf client on host machine and bluetooth discovery
        simultaneously.

        Returns:
            True if successful, False otherwise.
        """
        tasks = [(perform_classic_discovery,
                  (self.pri_ad, self.iperf['duration'], self.json_file,
                   self.dev_list)),
                 (self.run_iperf_and_get_result, ())]
        return self.set_attenuation_and_run_iperf(tasks)

    def bt_on(self):
        """ Turns on bluetooth and runs iperf.

        Returns:
            True on success, False otherwise.
        """
        if not enable_bluetooth(self.pri_ad.droid, self.pri_ad.ed):
            return False
        return self.set_attenuation_and_run_iperf()

    def generated_test_cases(self, test_types):
        """ Auto generates tests for basic coex tests. """
        test_cases = []
        for protocol, stream, test_type in itertools.product(
                self.standalone_params['protocol'],
                self.standalone_params['stream'], test_types):

            test_name = 'test_performance_with_{}_{}_{}'.format(
                test_type, protocol, stream)

            test_function = getattr(self, test_type)
            setattr(self, test_name, test_function)
            test_cases.append(test_name)
        return test_cases
