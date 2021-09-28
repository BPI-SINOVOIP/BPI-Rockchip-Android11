# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50DeferredECReset(Cr50Test):
    """Verify EC_RST_L stays asserted only if all conditions below are True.
    (1) System got 'Power-On reset'.
    (2) RDD cable is connected.
    (3) The power button is held.

    After this, EC_RST_L should be deasserted as soon as the power button
    gets released.

    Attributes
        version: test version number
        CUTOFF_DELAY: duration in second to keep cr50 powered off,
        PD_SETTLE_TIME: duration in second to wait PD to be stable
        HAS_CR50_RESET_ODL: boolean if 'cr50_reset_odl' control is available
    """
    version = 1
    CUTOFF_DELAY = 10
    PD_SETTLE_TIME = 3
    HAS_CR50_RESET_ODL = False

    def cr50_power_on_reset(self):
        """Perform a power-on-reset on cr50.
           If cr50_reset_odl control is available, then use it.
           Otherwise, disconnect and reconnect battery and power source.
        """
        if self.HAS_CR50_RESET_ODL:
            self.servo.set('cr50_reset_odl', 'on')

            time.sleep(self.CUTOFF_DELAY)

            self.servo.set('cr50_reset_odl', 'off')
        else:
            # Stop power delivery to dut
            logging.info('Stop charging')
            self.servo.set('servo_v4_role', 'snk')

            # Battery Cutoff
            logging.info('Cut battery off')
            self.ec.send_command('cutoff')

            time.sleep(self.CUTOFF_DELAY)

            # Enable power delivery to dut
            logging.info('Start charging')
            self.servo.set('servo_v4_role', 'src')

        time.sleep(self.PD_SETTLE_TIME)

    def initialize(self, host, cmdline_args, full_args):
        """Initialize the test and check if cr50 exists, DTS is controllable,
           and power delivery mode and power button is adjustable.

        Raises:
            TestFail: If test initialization setup fails.
            TestNAError: If the dut is not proper for this test for its RDD
                         recognition problem.
        """
        super(firmware_Cr50DeferredECReset, self).initialize(host, cmdline_args,
                full_args)
        if not hasattr(self, 'cr50'):
            raise error.TestNAError('Test can only be run on devices with '
                                    'access to the Cr50 console')
        if not self.cr50.servo_dts_mode_is_valid():
            raise error.TestNAError('Need working servo v4 DTS control')
        self.dts_restore = self.servo.get_dts_mode()

        # Fast open cr50 and check if testlab is enabled.
        self.fast_open(enable_testlab=True)
        if not self.cr50.testlab_is_on():
            raise error.TestNAError('Cr50 testlab mode is not enabled')

        # Check 'rdd_leakage' is marked in cr50 capability.
        if self.check_cr50_capability(['rdd_leakage']):
            self.rdd_leakage = True
            logging.warn('RDD leakage is marked in cr50 cap config')
        else:
            self.rdd_leakage = False

        # Test if the power button is adjustable.
        self.servo.set('pwr_button', 'press')
        self.servo.set('pwr_button', 'release')

        # Check if 'cr50_reset_odl' controlis available.
        try:
            self.servo.get('cr50_reset_odl')
            self.HAS_CR50_RESET_ODL = True
        except error.TestFail:
            self.HAS_CR50_RESET_ODL = False

            # Test the external power delivery
            self.servo.set('servo_v4_role', 'snk')
            time.sleep(self.PD_SETTLE_TIME)

            rv = self.ec.send_command_get_output('chgstate',['.*>'])[0].strip()
            logging.info(rv)
            if 'ac = 0' not in rv:
                raise error.TestFail('Failed to set servo_v4_role sink')

            # Test stopping the external power delivery
            self.servo.set('servo_v4_role', 'src')
            time.sleep(self.PD_SETTLE_TIME)

            rv = self.ec.send_command_get_output('chgstate',['.*>'])[0].strip()
            logging.info(rv)
            if 'ac = 1' not in rv:
                raise error.TestFail('Failed to set servo_v4_role source')

        # Check if the dut has any RDD recognition issue.
        # First, cut off power source and hold the power button.
        #        disable RDD connection.
        self.servo.set_dts_mode('off')
        self.servo.set('pwr_button', 'press')
        self.cr50_power_on_reset()
        try:
            #  Third, check if the rdd status is disconnected.
            #         If not, terminate the test.
            ccdstate = self.cr50.get_ccdstate()

            if (ccdstate['Rdd'].lower() != 'disconnected') != self.rdd_leakage:
                raise error.TestError('RDD leakage does not match capability'
                                      ' configuration.')
        finally:
            self.servo.set_dts_mode(self.dts_restore)
            self.servo.set_nocheck('pwr_button', 'release')
            time.sleep(self.PD_SETTLE_TIME)

            self.servo.power_short_press()            # Wake up AP

        logging.info('Initialization is done')

    def check_ecrst_asserted(self, expect_assert):
        """Ask CR50 whether EC_RST_L is asserted or deasserted.

        Args:
            expect_assert: True if it is expected asserted.
                           False otherwise.

        Raises:
            TestFail: if ecrst value is not as expected.
        """

        # If the console is responsive, then the EC is awake.
        expecting_txt = ' asserted' if expect_assert else ' deasserted'
        logging.info('Checking if ecrst is %s', expecting_txt)

        try:
            rv = self.cr50.send_command_get_output('ecrst',
                    ['(?i)EC_RST_L is \w{0,2}asserted.*>'])[0].strip()
            logging.info(rv)
        except error.TestError as e:
            raise error.TestFail(str(e))

        if expecting_txt.lower() not in rv.lower():
            raise error.TestFail(rv)

    def ping_ec(self, expect_response):
        """Check if EC is running and responding.

        Args:
            expect_response: True if EC should respond
                             False otherwise.
        Raises:
            TestFail: if ec responsiveness is not as same as expected.
        """
        try:
            logging.info('Checking if ec is %sresponsive',
                         '' if expect_response else 'not ')
            rv = self.ec.send_command_get_output('help', ['.*>'])[0].strip()
        except error.TestFail as e:
            msg = str(e)
            logging.info(msg)
            if not expect_response:
                if ('Timeout waiting for response' in msg or
                    'No data was sent from the pty' in msg):
                    return
            raise e
        else:
            if not expect_response:
                logging.error('EC should not respond')
                raise error.TestFail(rv)

    def test_deferred_ec_reset(self, power_button_hold, rdd_enable):
        """Do a power-on reset, and check if EC responds.

        Args:
            power_button_hold: True if it should be pressed on a system reset.
                               False otherwise.
            rdd_enable: True if RDD should be detected on a system reset.
                        False otherwise.
        """

        # If the board has a rdd leakage issue, RDD shall be detected
        # always in G3. EC_RST will be asserted if the power_button is
        # being presed in this test.
        expect_ec_response = not (power_button_hold and
                                  (rdd_enable or self.rdd_leakage))
        logging.info('Test deferred_ec_reset starts')
        logging.info('Power button held    : %s', power_button_hold)
        logging.info('RDD connection       : %s', rdd_enable)
        logging.info('RDD leakage          : %s', self.rdd_leakage)
        logging.info('Expected EC response : %s', expect_ec_response)

        try:
            # enable RDD Connection (or disable) before power-on-reset
            self.servo.set_dts_mode('on' if rdd_enable else 'off')

            # Set power button before the dut loses power,
            # because the power button value cannot be changed during power-off.
            self.servo.set('pwr_button',
                           'press' if power_button_hold else 'release')

            # Do a power-on-reset on CR50.
            self.cr50_power_on_reset()

            # Wait for a while
            wait_sec = 30
            logging.info('waiting for %d seconds', wait_sec)
            time.sleep(wait_sec)

            # Check if EC_RST_L is asserted and EC is on.
            # (or EC_RST_L deasserted and EC off)
            self.check_ecrst_asserted(not expect_ec_response)
            self.ping_ec(expect_ec_response)

            # Release power button
            logging.info('Power button released')
            self.servo.set('pwr_button', 'release')
            time.sleep(self.PD_SETTLE_TIME)

            # Check if EC_RST_L is deasserted and EC is on.
            self.check_ecrst_asserted(False)
            self.ping_ec(True)

        finally:
            if self.HAS_CR50_RESET_ODL:
                self.servo.set_nocheck('cr50_reset_odl', 'off')
            else:
                self.servo.set_nocheck('servo_v4_role', 'src')

            self.servo.set_dts_mode(self.dts_restore)
            time.sleep(1)

            # Press power button to wake up AP, and releases it soon
            # in any cases.
            self.servo.power_short_press()

    def run_once(self):
        """Test deferred EC reset feature. """

        # Release power button and disable RDD on power-on reset.
        # EC should be running.
        self.test_deferred_ec_reset(power_button_hold=False, rdd_enable=False)

        # Release power button but enable RDD on power-on reset.
        # EC should be running.
        self.test_deferred_ec_reset(power_button_hold=False, rdd_enable=True)

        # Hold power button but disable RDD on power-on reset.
        # EC should be running.
        self.test_deferred_ec_reset(power_button_hold=True, rdd_enable=False)

        # Hold power button and enable RDD on power-on reset.
        # EC should not be running.
        self.test_deferred_ec_reset(power_button_hold=True, rdd_enable=True)

        logging.info('Test is done')
