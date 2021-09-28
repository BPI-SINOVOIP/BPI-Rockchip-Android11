#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import time

from acts import base_test
from acts import asserts
from acts.controllers.rohdeschwarz_lib import contest
from acts.test_utils.tel import tel_test_utils
import json


class AGNSSPerformanceTest(base_test.BaseTestClass):

    # User parameters defined in the ACTS config file

    TESTPLAN_KEY = '{}_testplan'
    CONTEST_IP_KEY = 'contest_ip'
    REMOTE_SERVER_PORT_KEY = 'remote_server_port'
    AUTOMATION_PORT_KEY = 'automation_port'
    CUSTOM_FILES_KEY = 'custom_files'
    AUTOMATION_LISTEN_IP = 'automation_listen_ip'
    FTP_USER_KEY = 'ftp_user'
    FTP_PASSWORD_KEY = 'ftp_password'

    def __init__(self, controllers):
        """ Initializes class attributes. """

        super().__init__(controllers)

        self.dut = None
        self.contest = None
        self.testplan = None
        self.thresholds_file = None

    def setup_class(self):
        """ Executed before any test case is started. Initializes the Contest
        controller and prepares the DUT for testing. """

        req_params = [
            self.CONTEST_IP_KEY, self.REMOTE_SERVER_PORT_KEY,
            self.AUTOMATION_PORT_KEY, self.AUTOMATION_LISTEN_IP,
            self.FTP_USER_KEY, self.FTP_PASSWORD_KEY
        ]

        for param in req_params:
            if param not in self.user_params:
                self.log.error('Required parameter {} is missing in config '
                               'file.'.format(param))
                return False

        contest_ip = self.user_params[self.CONTEST_IP_KEY]
        remote_port = self.user_params[self.REMOTE_SERVER_PORT_KEY]
        automation_port = self.user_params[self.AUTOMATION_PORT_KEY]
        listen_ip = self.user_params[self.AUTOMATION_LISTEN_IP]
        ftp_user = self.user_params[self.FTP_USER_KEY]
        ftp_password = self.user_params[self.FTP_PASSWORD_KEY]
        custom_files = self.user_params.get(self.CUSTOM_FILES_KEY, [])

        self.dut = self.android_devices[0]

        self.contest = contest.Contest(logger=self.log,
                                       remote_ip=contest_ip,
                                       remote_port=remote_port,
                                       automation_listen_ip=listen_ip,
                                       automation_port=automation_port,
                                       dut_on_func=self.set_apm_off,
                                       dut_off_func=self.set_apm_on,
                                       ftp_usr=ftp_user,
                                       ftp_pwd=ftp_password)

        # Look for the threshold files
        for file in custom_files:
            if 'pass_fail_threshold_' + self.dut.model in file:
                self.thresholds_file = file
                self.log.debug('Threshold file loaded: ' + file)
                break
        else:
            self.log.warning('No threshold files found in custom files.')

    def teardown_class(self):
        """ Executed after completing all selected test cases."""
        if self.contest:
            self.contest.destroy()

    def setup_test(self):
        """ Executed before every test case.

        Returns:
            False if the setup failed.
        """

        testplan_formatted_key = self.TESTPLAN_KEY.format(self.test_name)

        if testplan_formatted_key not in self.user_params:
            self.log.error('Test plan not indicated in the config file. Use '
                           'the {} key to set the testplan filename.'.format(
                               testplan_formatted_key))
            return False

        self.testplan = self.user_params[testplan_formatted_key]

    def agnss_performance_test(self):
        """ Executes the aGNSS performance test and verifies that the results
        are within the expected values if a thresholds file is available.

        The thresholds file is in json format and contains the metrics keys
        defined in the Contest object with 'min' and 'max' values. """

        results = self.contest.execute_testplan(self.testplan)

        asserts.assert_true(
            results, 'No results were obtained from the test execution.')

        if not self.thresholds_file:
            self.log.info('Skipping pass / fail check because no thresholds '
                          'file was provided.')
            return

        passed = True

        with open(self.thresholds_file, 'r') as file:

            thresholds = json.load(file)

            for key, val in results.items():

                asserts.assert_true(
                    key in thresholds, 'Key {} is missing in '
                    'the thresholds file.'.format(key))

                # If the result is provided as a dictionary, obtain the value
                # from the 'avg' key.
                if isinstance(val, dict):
                    metric = val['avg']
                else:
                    metric = val

                if thresholds[key]['min'] < metric < thresholds[key]['max']:
                    self.log.info('Metric {} = {} is within the expected '
                                  'values.'.format(key, metric))
                else:
                    self.log.error('Metric {} = {} is not within ({}, '
                                   '{}).'.format(key, metric,
                                                 thresholds[key]['min'],
                                                 thresholds[key]['max']))
                    passed = False

        asserts.assert_true(
            passed, 'At least one of the metrics was not '
            'within the expected values.')

    def set_apm_on(self):
        """ Wrapper method to turn airplane mode on.

        This is passed to the Contest object so it can be executed when the
        automation system requires the DUT to be set to 'off' state.
        """

        tel_test_utils.toggle_airplane_mode(self.log, self.dut, True)

    def set_apm_off(self):
        """ Wrapper method to turn airplane mode off.

        This is passed to the Contest object so it can be executed when the
        automation system requires the DUT to be set to 'on' state.
        """
        # Wait for the Contest system to initialize the base stations before
        # actually setting APM off.
        time.sleep(5)

        tel_test_utils.toggle_airplane_mode(self.log, self.dut, False)

    def test_agnss_performance(self):
        self.agnss_performance_test()
