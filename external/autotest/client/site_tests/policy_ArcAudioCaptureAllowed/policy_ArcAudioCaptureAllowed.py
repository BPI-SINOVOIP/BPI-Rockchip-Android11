# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error

from autotest_lib.client.common_lib.cros import arc
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ArcAudioCaptureAllowed(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of the ArcAudioCaptureAllowed ChromeOS policy on ARC.

    This test will launch the ARC container via the ArcEnabled policy, then
    will verify the status of the mic using dumpsys. If mic can't be unmuted
    then the policy has been set to False. If mic can be unmuted then it's
    set to True or None.

    """
    version = 1

    def _test_microphone_status(self, case):
        microphone_status = arc.adb_shell("dumpsys | grep microphone")

        if case or case is None:
            if "no_unmute_microphone" in microphone_status:
                raise error.TestFail(
                    "Microphone is muted and it shouldn't be.")
        else:
            if "no_unmute_microphone" not in microphone_status:
                raise error.TestFail(
                    "Micprophone isn't muted and it should be.")

    def policy_creator(self, case):
        pol = {'ArcEnabled': True, 'AudioCaptureAllowed': case}
        return pol

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        policies = self.policy_creator(case)

        self.setup_case(user_policies=policies,
                        arc_mode='enabled',
                        use_clouddpc_test=True)

        self._test_microphone_status(case)
