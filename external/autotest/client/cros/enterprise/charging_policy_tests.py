# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.power import power_status


class ChargingPolicyTest(enterprise_policy_base.EnterprisePolicyTest):
    """
    A Client test that verifies that AC usage and battery charging is consistent
    with policy settings. As of this writing, these features are only present on
    the Wilco platform.
    """
    # The Wilco EC updates it's charging behavior every 60 seconds,
    # so give ourselves 120 seconds to notice a change in behavior.
    POLICY_CHANGE_TIMEOUT = 120

    def run_once(self, test_cases, min_battery_level, prep_policies):
        """
        Test a collection of cases.

        @param test_cases: Collection of (policies, expected_behavior) pairs,
                           where expected_behavior is one of values accepted by
                           power_status.poll_for_charging_behavior().
        @param min_battery_level: For the policy to affect the behavior
                                  correctly, the battery level may need to be
                                  above a certain percentage.
        @param prep_policies: To test that policies P1 cause behavior B1, we
                              need to start in a state P2 where behavior B2 is
                              not B1, so we can notice the change to B1.
                              prep_policies is a dict that maps B1 => (P2, B2),
                              so that we can look up how to prep for testing
                              P1.
        """
        self.setup_case(enroll=True)

        failures = []
        for policies, expected_behavior in test_cases:
            setup_policies, prep_behavior = prep_policies[expected_behavior]
            err = self._test_policies(setup_policies, prep_behavior,
                                      min_battery_level)
            if err is not None:
                failures.append(err)

            # Now that we are set up, test the actual test case.
            err = self._test_policies(policies, expected_behavior,
                                      min_battery_level)
            if err is not None:
                failures.append(err)
        if failures:
            raise error.TestFail('Failed the following cases: {}'.format(
                    str(failures)))

    def _test_policies(self, policies, expected_behavior, min_battery_level):
        self.update_policies(device_policies=policies)
        try:
            self._assert_battery_is_testable(min_battery_level)
            power_status.poll_for_charging_behavior(expected_behavior,
                                                    self.POLICY_CHANGE_TIMEOUT)
        except BaseException as e:
            msg = ('Expected to be {} using policies {}. Got this instead: {}'.
                   format(expected_behavior, policies, str(e)))
            return msg
        return None

    def _assert_battery_is_testable(self, min_battery_level):
        status = power_status.get_status()
        if status.battery_full():
            raise error.TestError('The battery is full, but should not be')
        status.assert_battery_in_range(min_battery_level, 100)
