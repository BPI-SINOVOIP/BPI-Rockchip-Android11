# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.enterprise import enterprise_policy_base

from telemetry.core import exceptions


class policy_ReportUploadFrequency(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Tests the ReportUploadFrequency policy in Chrome OS.

    """

    version = 1


    def initialize(self, **kwargs):
        super(policy_ReportUploadFrequency, self).initialize(**kwargs)

        self.POLICY_NAME = 'ReportUploadFrequency'
        self.POLICIES = {}
        self.TEST_CASES = {
            '60s': 60000,
        }


    def _check_report_upload_frequency(self):
        """
        Grep syslog for "Starting status upload: have_device_status = 1" line

        @param case_value: policy value in milliseconds

        """

        def is_log_present():
            """
            Checks to see if logs have been written.

            @returns True if written False if not.

            """
            try:
                if 'Starting status upload: has_device_status = 1' in open(
                    '/var/log/messages').read():
                        return True
            except exceptions.EvaluateException:
                return False

        utils.poll_for_condition(
            lambda: is_log_present(),
            exception=error.TestFail('No status upload sent.'),
            timeout=60,
            sleep_interval=5,
            desc='Polling for logs to be written.')


    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """

        case_value = self.TEST_CASES[case]
        self.POLICIES[self.POLICY_NAME] = case_value

        self.setup_case(device_policies=self.POLICIES, enroll=True)
        self._check_report_upload_frequency()
