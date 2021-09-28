#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
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

import logging
import time
import unittest

import mock

from acts import utils
from acts import signals
from acts.controllers.adb import AdbError

PROVISIONED_STATE_GOOD = 1


class ByPassSetupWizardTests(unittest.TestCase):
    """This test class for unit testing acts.utils.bypass_setup_wizard."""

    def test_start_standing_subproc(self):
        with self.assertRaisesRegex(utils.ActsUtilsError,
                                    'Process .* has terminated'):
            utils.start_standing_subprocess('sleep 0', check_health_delay=0.1)

    def test_stop_standing_subproc(self):
        p = utils.start_standing_subprocess('sleep 0')
        time.sleep(0.1)
        with self.assertRaisesRegex(utils.ActsUtilsError,
                                    'Process .* has terminated'):
            utils.stop_standing_subprocess(p)

    @mock.patch('time.sleep')
    def test_bypass_setup_wizard_no_complications(self, _):
        ad = mock.Mock()
        ad.adb.shell.side_effect = [
            # Return value for SetupWizardExitActivity
            BypassSetupWizardReturn.NO_COMPLICATIONS,
            # Return value for device_provisioned
            PROVISIONED_STATE_GOOD,
        ]
        ad.adb.return_state = BypassSetupWizardReturn.NO_COMPLICATIONS
        self.assertTrue(utils.bypass_setup_wizard(ad))
        self.assertFalse(
            ad.adb.root_adb.called,
            'The root command should not be called if there are no '
            'complications.')

    @mock.patch('time.sleep')
    def test_bypass_setup_wizard_unrecognized_error(self, _):
        ad = mock.Mock()
        ad.adb.shell.side_effect = [
            # Return value for SetupWizardExitActivity
            BypassSetupWizardReturn.UNRECOGNIZED_ERR,
            # Return value for device_provisioned
            PROVISIONED_STATE_GOOD,
        ]
        with self.assertRaises(AdbError):
            utils.bypass_setup_wizard(ad)
        self.assertFalse(
            ad.adb.root_adb.called,
            'The root command should not be called if we do not have a '
            'codepath for recovering from the failure.')

    @mock.patch('time.sleep')
    def test_bypass_setup_wizard_need_root_access(self, _):
        ad = mock.Mock()
        ad.adb.shell.side_effect = [
            # Return value for SetupWizardExitActivity
            BypassSetupWizardReturn.ROOT_ADB_NO_COMP,
            # Return value for rooting the device
            BypassSetupWizardReturn.NO_COMPLICATIONS,
            # Return value for device_provisioned
            PROVISIONED_STATE_GOOD
        ]

        utils.bypass_setup_wizard(ad)

        self.assertTrue(
            ad.adb.root_adb_called,
            'The command required root access, but the device was never '
            'rooted.')

    @mock.patch('time.sleep')
    def test_bypass_setup_wizard_need_root_already_skipped(self, _):
        ad = mock.Mock()
        ad.adb.shell.side_effect = [
            # Return value for SetupWizardExitActivity
            BypassSetupWizardReturn.ROOT_ADB_SKIPPED,
            # Return value for SetupWizardExitActivity after root
            BypassSetupWizardReturn.ALREADY_BYPASSED,
            # Return value for device_provisioned
            PROVISIONED_STATE_GOOD
        ]
        self.assertTrue(utils.bypass_setup_wizard(ad))
        self.assertTrue(ad.adb.root_adb_called)

    @mock.patch('time.sleep')
    def test_bypass_setup_wizard_root_access_still_fails(self, _):
        ad = mock.Mock()
        ad.adb.shell.side_effect = [
            # Return value for SetupWizardExitActivity
            BypassSetupWizardReturn.ROOT_ADB_FAILS,
            # Return value for SetupWizardExitActivity after root
            BypassSetupWizardReturn.UNRECOGNIZED_ERR,
            # Return value for device_provisioned
            PROVISIONED_STATE_GOOD
        ]

        with self.assertRaises(AdbError):
            utils.bypass_setup_wizard(ad)
        self.assertTrue(ad.adb.root_adb_called)


class BypassSetupWizardReturn:
    # No complications. Bypass works the first time without issues.
    NO_COMPLICATIONS = ('Starting: Intent { cmp=com.google.android.setupwizard/'
                        '.SetupWizardExitActivity }')

    # Fail with doesn't need to be skipped/was skipped already.
    ALREADY_BYPASSED = AdbError('', 'ADB_CMD_OUTPUT:0', 'Error type 3\n'
                                                        'Error: Activity class',
                                1)
    # Fail with different error.
    UNRECOGNIZED_ERR = AdbError('', 'ADB_CMD_OUTPUT:0', 'Error type 4\n'
                                                        'Error: Activity class',
                                0)
    # Fail, get root access, then no complications arise.
    ROOT_ADB_NO_COMP = AdbError('', 'ADB_CMD_OUTPUT:255',
                                'Security exception: Permission Denial: '
                                'starting Intent { flg=0x10000000 '
                                'cmp=com.google.android.setupwizard/'
                                '.SetupWizardExitActivity } from null '
                                '(pid=5045, uid=2000) not exported from uid '
                                '10000', 0)
    # Even with root access, the bypass setup wizard doesn't need to be skipped.
    ROOT_ADB_SKIPPED = AdbError('', 'ADB_CMD_OUTPUT:255',
                                'Security exception: Permission Denial: '
                                'starting Intent { flg=0x10000000 '
                                'cmp=com.google.android.setupwizard/'
                                '.SetupWizardExitActivity } from null '
                                '(pid=5045, uid=2000) not exported from '
                                'uid 10000', 0)
    # Even with root access, the bypass setup wizard fails
    ROOT_ADB_FAILS = AdbError(
        '', 'ADB_CMD_OUTPUT:255',
        'Security exception: Permission Denial: starting Intent { '
        'flg=0x10000000 cmp=com.google.android.setupwizard/'
        '.SetupWizardExitActivity } from null (pid=5045, uid=2000) not '
        'exported from uid 10000', 0)


class ConcurrentActionsTest(unittest.TestCase):
    """Tests acts.utils.run_concurrent_actions and related functions."""

    @staticmethod
    def function_returns_passed_in_arg(arg):
        return arg

    @staticmethod
    def function_raises_passed_in_exception_type(exception_type):
        raise exception_type

    def test_run_concurrent_actions_no_raise_returns_proper_return_values(self):
        """Tests run_concurrent_actions_no_raise returns in the correct order.

        Each function passed into run_concurrent_actions_no_raise returns the
        values returned from each individual callable in the order passed in.
        """
        ret_values = utils.run_concurrent_actions_no_raise(
            lambda: self.function_returns_passed_in_arg('ARG1'),
            lambda: self.function_returns_passed_in_arg('ARG2'),
            lambda: self.function_returns_passed_in_arg('ARG3')
        )

        self.assertEqual(len(ret_values), 3)
        self.assertEqual(ret_values[0], 'ARG1')
        self.assertEqual(ret_values[1], 'ARG2')
        self.assertEqual(ret_values[2], 'ARG3')

    def test_run_concurrent_actions_no_raise_returns_raised_exceptions(self):
        """Tests run_concurrent_actions_no_raise returns raised exceptions.

        Instead of allowing raised exceptions to be raised in the main thread,
        this function should capture the exception and return them in the slot
        the return value should have been returned in.
        """
        ret_values = utils.run_concurrent_actions_no_raise(
            lambda: self.function_raises_passed_in_exception_type(IndexError),
            lambda: self.function_raises_passed_in_exception_type(KeyError)
        )

        self.assertEqual(len(ret_values), 2)
        self.assertEqual(ret_values[0].__class__, IndexError)
        self.assertEqual(ret_values[1].__class__, KeyError)

    def test_run_concurrent_actions_returns_proper_return_values(self):
        """Tests run_concurrent_actions returns in the correct order.

        Each function passed into run_concurrent_actions returns the values
        returned from each individual callable in the order passed in.
        """

        ret_values = utils.run_concurrent_actions(
            lambda: self.function_returns_passed_in_arg('ARG1'),
            lambda: self.function_returns_passed_in_arg('ARG2'),
            lambda: self.function_returns_passed_in_arg('ARG3')
        )

        self.assertEqual(len(ret_values), 3)
        self.assertEqual(ret_values[0], 'ARG1')
        self.assertEqual(ret_values[1], 'ARG2')
        self.assertEqual(ret_values[2], 'ARG3')

    def test_run_concurrent_actions_raises_exceptions(self):
        """Tests run_concurrent_actions raises exceptions from given actions."""
        with self.assertRaises(KeyError):
            utils.run_concurrent_actions(
                lambda: self.function_returns_passed_in_arg('ARG1'),
                lambda: self.function_raises_passed_in_exception_type(KeyError)
            )

    def test_test_concurrent_actions_raises_non_test_failure(self):
        """Tests test_concurrent_actions raises the given exception."""
        with self.assertRaises(KeyError):
            utils.test_concurrent_actions(
                lambda: self.function_raises_passed_in_exception_type(KeyError),
                failure_exceptions=signals.TestFailure
            )

    def test_test_concurrent_actions_raises_test_failure(self):
        """Tests test_concurrent_actions raises the given exception."""
        with self.assertRaises(signals.TestFailure):
            utils.test_concurrent_actions(
                lambda: self.function_raises_passed_in_exception_type(KeyError),
                failure_exceptions=KeyError
            )


class SuppressLogOutputTest(unittest.TestCase):
    """Tests SuppressLogOutput"""

    def test_suppress_log_output(self):
        """Tests that the SuppressLogOutput context manager removes handlers
        of the specified levels upon entry and re-adds handlers upon exit.
        """
        handlers = [logging.NullHandler(level=lvl) for lvl in
                    (logging.DEBUG, logging.INFO, logging.ERROR)]
        log = logging.getLogger('test_log')
        for handler in handlers:
            log.addHandler(handler)
        with utils.SuppressLogOutput(log, [logging.INFO, logging.ERROR]):
            self.assertTrue(
                any(handler.level == logging.DEBUG for handler in log.handlers))
            self.assertFalse(
                any(handler.level in (logging.INFO, logging.ERROR)
                    for handler in log.handlers))
        self.assertCountEqual(handlers, log.handlers)


class IpAddressUtilTest(unittest.TestCase):

    def test_positive_ipv4_normal_address(self):
        ip_address = "192.168.1.123"
        self.assertTrue(utils.is_valid_ipv4_address(ip_address))

    def test_positive_ipv4_any_address(self):
        ip_address = "0.0.0.0"
        self.assertTrue(utils.is_valid_ipv4_address(ip_address))

    def test_positive_ipv4_broadcast(self):
        ip_address = "255.255.255.0"
        self.assertTrue(utils.is_valid_ipv4_address(ip_address))

    def test_negative_ipv4_with_ipv6_address(self):
        ip_address = "fe80::f693:9fff:fef4:1ac"
        self.assertFalse(utils.is_valid_ipv4_address(ip_address))

    def test_negative_ipv4_with_invalid_string(self):
        ip_address = "fdsafdsafdsafdsf"
        self.assertFalse(utils.is_valid_ipv4_address(ip_address))

    def test_negative_ipv4_with_invalid_number(self):
        ip_address = "192.168.500.123"
        self.assertFalse(utils.is_valid_ipv4_address(ip_address))

    def test_positive_ipv6(self):
        ip_address = 'fe80::f693:9fff:fef4:1ac'
        self.assertTrue(utils.is_valid_ipv6_address(ip_address))

    def test_positive_ipv6_link_local(self):
        ip_address = 'fe80::'
        self.assertTrue(utils.is_valid_ipv6_address(ip_address))

    def test_negative_ipv6_with_ipv4_address(self):
        ip_address = '192.168.1.123'
        self.assertFalse(utils.is_valid_ipv6_address(ip_address))

    def test_negative_ipv6_invalid_characters(self):
        ip_address = 'fe80:jkyr:f693:9fff:fef4:1ac'
        self.assertFalse(utils.is_valid_ipv6_address(ip_address))

    def test_negative_ipv6_invalid_string(self):
        ip_address = 'fdsafdsafdsafdsf'
        self.assertFalse(utils.is_valid_ipv6_address(ip_address))


if __name__ == '__main__':
    unittest.main()
