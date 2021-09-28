# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import cr50_utils
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50RMAOpen(Cr50Test):
    """Verify Cr50 RMA behavoior

    Verify a couple of things:
        - basic open from AP and command line
        - Rate limiting
        - Authcodes can't be reused once another challenge is generated.
        - if the image is prod signed with mp flags, it isn't using test keys

    Generate the challenge and calculate the response using rma_reset -c. Verify
    open works and enables all of the ccd features.

    If the generated challenge has the wrong version, make sure the challenge
    generated with the test key fails.
    """
    version = 1

    # Tuple representing WP state when it is force disabled
    WP_PERMANENTLY_DISABLED = (False, False, False, False)

    # Various Error Messages from the command line and AP RMA failures
    MISMATCH_CLI = 'Auth code does not match.'
    MISMATCH_AP = 'rma unlock failed, code 6'
    # Starting in 0.4.8 cr50 doesn't print "RMA Auth error 0x504". It doesn't
    # print anything. Once prod and prepvt versions do this remove the error
    # code from the test.
    LIMIT_CLI = '(RMA Auth error 0x504|rma_auth\s+>)'
    LIMIT_AP = 'error 4'
    ERR_DISABLE_AP = 'error 7'
    DISABLE_WARNING = ('mux_client_request_session: read from master failed: '
            'Broken pipe')
    # GSCTool exit statuses
    UPDATE_ERROR = 3
    SUCCESS = 0
    # Cr50 limits generating challenges to once every 10 seconds
    CHALLENGE_INTERVAL = 10
    SHORT_WAIT = 3
    # Cr50 RMA commands can be sent from the AP or command line. They should
    # behave the same and be interchangeable
    CMD_INTERFACES = ['ap', 'cli']

    def initialize(self, host, cmdline_args, full_args):
        """Initialize the test"""
        super(firmware_Cr50RMAOpen, self).initialize(host, cmdline_args,
                full_args)
        self.host = host

        if not hasattr(self, 'cr50'):
            raise error.TestNAError('Test can only be run on devices with '
                                    'access to the Cr50 console')

        if not self.cr50.has_command('rma_auth'):
            raise error.TestNAError('Cannot test on Cr50 without RMA support')

        if not self.cr50._servo.dts_mode_is_valid():
            raise error.TestNAError('This messes with ccd settings. Use flex '
                    'cable to run the test.')

        if self.host.run('rma_reset -h', ignore_status=True).exit_status == 127:
            raise error.TestNAError('Cannot test RMA open without rma_reset')

        # Disable all capabilities at the start of the test. Go ahead and enable
        # testlab mode if it isn't enabled.
        self.fast_open(enable_testlab=True)
        self.cr50.send_command('ccd reset')
        self.cr50.set_ccd_level('lock')
        # Make sure all capabilities are set to default.
        try:
            self.check_ccd_cap_settings(False)
        except error.TestFail:
            raise error.TestError('Could not disable rma mode')

        self.is_prod_mp = self.get_prod_mp_status()


    def get_prod_mp_status(self):
        """Returns True if Cr50 is running a prod signed mp flagged image"""
        # Determine if the running image is using premp flags
        bid = self.cr50.get_active_board_id_str()
        premp_flags = int(bid.split(':')[2], 16) & 0x10 if bid else False

        # Check if the running image is signed with prod keys
        prod_keys = self.cr50.using_prod_rw_keys()
        logging.info('%s keys with %s flags', 'prod' if prod_keys else 'dev',
                'premp' if premp_flags else 'mp')
        return not premp_flags and prod_keys


    def parse_challenge(self, challenge):
        """Remove the whitespace from the challenge"""
        return re.sub('\s', '', challenge.strip())


    def generate_response(self, challenge):
        """Generate the authcode from the challenge.

        Args:
            challenge: The Cr50 challenge string

        Returns:
            A tuple of the authcode and a bool True if the response should
            work False if it shouldn't
        """
        stdout = self.host.run('rma_reset -c ' + challenge).stdout
        logging.info(stdout)
        # rma_reset generates authcodes with the test key. MP images should use
        # prod keys. Make sure prod signed MP images aren't using the test key.
        self.prod_rma_key = 'Unsupported' in stdout
        if self.is_prod_mp and not self.prod_rma_key:
            raise error.TestFail('MP image cannot use test key')
        return re.search('Authcode: +(\S+)', stdout).group(1), self.prod_rma_key


    def rma_cli(self, authcode='', disable=False, expected_exit_status=SUCCESS):
        """Run RMA commands using the command line.

        Args:
            authcode: The authcode string
            disable: True if RMA open should be disabled.
            expected_exit_status: the expected exit status

        Returns:
            The entire stdout from the command or the RMA challenge
        """
        cmd = 'rma_auth ' + ('disable' if disable else authcode)
        get_challenge = not (authcode or disable)
        resp = 'rma_auth(.*generated challenge:)?(.*)>'
        if expected_exit_status:
            resp = self.LIMIT_CLI if get_challenge else self.MISMATCH_CLI

        result = self.cr50.send_command_get_output(cmd, [resp])
        logging.info(result)
        return (self.parse_challenge(result[0][-1]) if get_challenge else
                result[0])


    def rma_ap(self, authcode='', disable=False, expected_exit_status=SUCCESS):
        """Run RMA commands using vendor commands from the ap.

        Args:
            authcode: the authcode string.
            disable: True if RMA open should be disabled.
            expected_exit_status: the expected exit status

        Returns:
            The entire stdout from the command or the RMA challenge

        Raises:
            error.TestFail if there is an unexpected gsctool response
        """
        if disable:
            cmd = '-a -F disable'
        else:
            cmd = '-a -r ' + authcode
        get_challenge = not (authcode or disable)

        expected_stderr = ''
        if expected_exit_status:
            if authcode:
                expected_stderr = self.MISMATCH_AP
            elif disable:
                expected_stderr = self.ERR_DISABLE_AP
            else:
                expected_stderr = self.LIMIT_AP

        result = cr50_utils.GSCTool(self.host, cmd.split(),
                ignore_status=expected_stderr)
        logging.info(result)
        # Various connection issues result in warnings. If there is a real issue
        # the expected_exit_status will raise it. Ignore any warning messages in
        # stderr.
        ignore_stderr = 'WARNING' in result.stderr and not expected_stderr
        if not ignore_stderr and expected_stderr not in result.stderr.strip():
            raise error.TestFail('Unexpected stderr: expected %s got %s' %
                    (expected_stderr, result.stderr.strip()))
        if result.exit_status != expected_exit_status:
            raise error.TestFail('Unexpected exit_status: expected %s got %s' %
                    (expected_exit_status, result.exit_status))
        if get_challenge:
            return self.parse_challenge(result.stdout.split('Challenge:')[-1])
        return result.stdout


    def fake_rma_open(self):
        """Use individual commands to enter the same state as factory mode"""
        self.cr50.send_command('ccd testlab open')
        self.cr50.send_command('ccd reset factory')
        self.cr50.send_command('wp disable atboot')


    def check_ccd_cap_settings(self, rma_opened):
        """Verify the ccd capability permissions match the RMA state

        Args:
            rma_opened: True if we expect Cr50 to be RMA opened

        Raises:
            TestFail if Cr50 is opened when it should be closed or it is closed
            when it should be opened.
        """
        time.sleep(self.SHORT_WAIT)
        caps = self.cr50.get_cap_dict()
        in_factory_mode, reset = self.cr50.get_cap_overview(caps)

        if rma_opened and not in_factory_mode:
            raise error.TestFail('Not all capablities were set to Always')
        if not rma_opened and not reset:
            raise error.TestFail('Not all capablities were set to Default')


    def rma_open(self, challenge_func, auth_func):
        """Run the RMA open process

        Run the RMA open process with the given functions. Use challenge func
        to generate the challenge and auth func to verify the authcode. The
        commands can be sent from the command line or ap. Both should be able
        to be used as the challenge or auth function interchangeably.

        Args:
            challenge_func: The method used to generate the challenge
            auth_func: The method used to verify the authcode
        """
        time.sleep(self.CHALLENGE_INTERVAL)

        # Get the challenge
        challenge = challenge_func()
        logging.info(challenge)

        # Try using the challenge. If the Cr50 KeyId is not supported, make sure
        # RMA open fails.
        authcode, unsupported_key = self.generate_response(challenge)
        exit_status = self.UPDATE_ERROR if unsupported_key else self.SUCCESS

        # Attempt RMA open with the given authcode
        auth_func(authcode=authcode, expected_exit_status=exit_status)

        # Make sure ccd is in the proper state. If the RMA key is prod, the test
        # wont be able to generate the authcode and ccd should still be reset.
        # It should not be in factory mode.
        if unsupported_key:
            self.confirm_ccd_is_reset()
        else:
            self.confirm_ccd_is_in_factory_mode()

        self.host.reboot()

        if not self.tpm_is_responsive():
            raise error.TestFail('TPM was not reenabled after reboot')

        # Run RMA disable to reset the capabilities.
        self.rma_ap(disable=True, expected_exit_status=exit_status)

        self.confirm_ccd_is_reset()


    def confirm_ccd_is_in_factory_mode(self, check_tpm=True):
        """Check wp and capabilities to confirm cr50 is in factory mode"""
        # The open process takes some time to complete. Wait for it.
        time.sleep(self.CHALLENGE_INTERVAL)

        if check_tpm and self.tpm_is_responsive():
            raise error.TestFail('TPM was not disabled after RMA open')

        if self.cr50.get_wp_state() != self.WP_PERMANENTLY_DISABLED:
            raise error.TestFail('HW WP was not disabled after RMA open')

        # Make sure capabilities are all set to Always
        self.check_ccd_cap_settings(True)


    def confirm_ccd_is_reset(self):
        """Check wp and capabilities to confirm ccd has been reset"""
        # The open process takes some time to complete. Wait for it.
        time.sleep(self.CHALLENGE_INTERVAL)

        if not self.tpm_is_responsive():
            raise error.TestFail('TPM is disabled')

        # Confirm write protect has been reset to follow battery presence. The
        # WP state may be enabled or disabled. The state just can't be forced.
        if not self.cr50.wp_is_reset():
            raise error.TestFail('Factory mode disable did not reset HW WP')

        # Make sure capabilities have been reset
        self.check_ccd_cap_settings(False)


    def verify_basic_factory_disable(self):
        """Verify RMA disable works.

        The RMA open process may not be able to be automated, because it
        requires phyiscal presence and access to the server. This uses console
        commands to enter the same state as factory mode and then verifies
        rma disable resets all of that.
        """
        self.fake_rma_open()

        # There's no way to disable the TPM, so ignore that state for the fake
        # RMA mode check.
        self.confirm_ccd_is_in_factory_mode(check_tpm=False)

        self.host.reboot()

        # Run RMA disable to reset the capabilities.
        self.rma_ap(disable=True)

        self.confirm_ccd_is_reset()


    def rate_limit_check(self, rma_func1, rma_func2):
        """Verify that Cr50 ratelimits challenge generation from any interface

        Get the challenge from rma_func1. Try to generate a challenge with
        rma_func2 in a time less than challenge_interval. Make sure it fails.
        Wait a little bit longer and make sure the function then succeeds.

        Args:
            rma_func1: the method to generate the first challenge
            rma_func2: the method to generate the second challenge
        """
        time.sleep(self.CHALLENGE_INTERVAL)
        rma_func1()

        # Wait too short of a time. Verify challenge generation fails
        time.sleep(self.CHALLENGE_INTERVAL - self.SHORT_WAIT)
        rma_func2(expected_exit_status=self.UPDATE_ERROR)

        # Wait long enough for the timeout to have elapsed. Verify another
        # challenge is generated.
        time.sleep(self.SHORT_WAIT)
        rma_func2()


    def old_authcodes_are_invalid(self, rma_func1, rma_func2):
        """Verify a response for a previous challenge can't be used again

        Generate 2 challenges. Verify only the authcode from the second
        challenge can be used to open the device.

        Args:
            rma_func1: the method to generate the first challenge
            rma_func2: the method to generate the second challenge
        """
        time.sleep(self.CHALLENGE_INTERVAL)
        old_challenge = rma_func1()

        time.sleep(self.CHALLENGE_INTERVAL)
        active_challenge = rma_func2()

        invalid_authcode = self.generate_response(old_challenge)[0]
        valid_authcode = self.generate_response(active_challenge)[0]

        # Use the old authcode
        rma_func1(invalid_authcode, expected_exit_status=self.UPDATE_ERROR)
        # make sure factory mode is still disabled
        self.confirm_ccd_is_reset()

        # Use the authcode generated with the most recent challenge.
        rma_func1(valid_authcode)
        # Make sure factory mode has been enabled now that the test has used the
        # correct authcode.
        self.confirm_ccd_is_in_factory_mode()

        # Reboot the AP to reenable the TPM
        self.host.reboot()

        self.rma_ap(disable=True)

        # Verify rma disable disabled factory mode
        self.confirm_ccd_is_reset()


    def verify_interface_combinations(self, test_func):
        """Run through tests using ap and cli

        Cr50 can run RMA commands from the AP or command line. Test sending
        commands from both, so we know there aren't any weird interactions
        between the two.

        Args:
            test_func: The function to verify some RMA behavior
        """
        for rma_interface1 in self.CMD_INTERFACES:
            rma_func1 = getattr(self, 'rma_' + rma_interface1)
            for rma_interface2 in self.CMD_INTERFACES:
                rma_func2 = getattr(self, 'rma_' + rma_interface2)
                test_func(rma_func1, rma_func2)


    def run_once(self):
        """Verify Cr50 RMA behavior"""
        self.verify_basic_factory_disable()

        self.verify_interface_combinations(self.rate_limit_check)

        self.verify_interface_combinations(self.rma_open)

        # We can only do RMA unlock with test keys, so this won't be useful
        # to run unless the Cr50 image is using test keys.
        if not self.prod_rma_key:
            self.verify_interface_combinations(self.old_authcodes_are_invalid)
