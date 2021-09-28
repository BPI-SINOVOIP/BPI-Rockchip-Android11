# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.cr50_test import Cr50Test


class firmware_Cr50CCDServoCap(Cr50Test):
    """Verify Cr50 CCD output enable/disable when servo is connected.

    Verify Cr50 will enable/disable the CCD servo output capabilities when servo
    is attached/detached.
    """
    version = 1

    # Time used to wait for Cr50 to detect the servo state. Cr50 updates the ccd
    # state once a second. Wait 2 seconds to be conservative.
    SLEEP = 2

    # A list of the actions we should verify
    TEST_CASES = [
        'fake_servo on, cr50_run reboot',
        'fake_servo on, rdd attach, cr50_run reboot',

        'rdd attach, fake_servo on, cr50_run reboot, fake_servo off',
        'rdd attach, fake_servo on, rdd detach',
        'rdd attach, fake_servo off, rdd detach',
    ]

    ON = 0
    OFF = 1
    UNDETECTABLE = 2
    STATUS_MAP = [ 'on', 'off', 'unknown' ]
    # Create maps for the different ccd states. Mapping each state to 'on',
    # 'off', and 'unknown'. These lists map to the acceptable [ on values, off
    # values, and unknown state values]
    ON_MAP = [ 'on', 'off', '' ]
    ENABLED_MAP = [ 'enabled', 'disabled', '' ]
    CONNECTED_MAP = [ 'connected', 'disconnected', 'undetectable' ]
    VALID_STATES = {
        'AP' : ON_MAP,
        'EC' : ON_MAP,
        'AP UART' : ON_MAP,
        'Rdd' : CONNECTED_MAP,
        'Servo' : CONNECTED_MAP,
        'CCD EXT' : ENABLED_MAP,
    }
    # RESULT_ORDER is a list of the CCD state strings. The order corresponds
    # with the order of the key states in EXPECTED_RESULTS.
    RESULT_ORDER = ['Rdd', 'CCD EXT', 'Servo']
    # A dictionary containing an order of steps to verify and the expected ccd
    # states as the value.
    #
    # The keys are a list of strings with the order of steps to run.
    #
    # The values are the expected state of [rdd, ccd ext, servo]. The ccdstate
    # strings are in RESULT_ORDER. The order of the EXPECTED_RESULTS key states
    # must match the order in RESULT_ORDER.
    #
    # There are three valid states: UNDETECTABLE, ON, or OFF. Undetectable only
    # describes the servo state when EC uart is enabled. If the ec uart is
    # enabled, cr50 cannot detect servo and the state becomes undetectable. All
    # other ccdstates can only be off or on. Cr50 has a lot of different words
    # for off off and on. So VALID_STATES can be used to convert off, on, and
    # undetectable to the actual state strings.
    EXPECTED_RESULTS = {
        # The state all tests will start with. Servo and the ccd cable are
        # disconnected.
        'reset_ccd state' : [OFF, OFF, OFF],

        # If rdd is attached all ccd functionality will be enabled, and servo
        # will be undetectable.
        'rdd attach' : [ON, ON, UNDETECTABLE],

        # Cr50 cannot detect servo if ccd has been enabled first
        'rdd attach, fake_servo off' : [ON, ON, UNDETECTABLE],
        'rdd attach, fake_servo off, rdd detach' : [OFF, OFF, OFF],
        'rdd attach, fake_servo on' : [ON, ON, UNDETECTABLE],
        'rdd attach, fake_servo on, rdd detach' : [OFF, OFF, ON],
        # Cr50 can detect servo after a reboot even if rdd was attached before
        # servo.
        'rdd attach, fake_servo on, cr50_run reboot' : [ON, ON, ON],
        # Once servo is detached, Cr50 will immediately reenable the EC uart.
        'rdd attach, fake_servo on, cr50_run reboot, fake_servo off' :
            [ON, ON, UNDETECTABLE],

        # Cr50 can detect a servo attach
        'fake_servo on' : [OFF, OFF, ON],
        # Cr50 knows servo is attached when ccd is enabled, so it wont enable
        # uart.
        'fake_servo on, rdd attach' : [ON, ON, ON],
        'fake_servo on, rdd attach, cr50_run reboot' : [ON, ON, ON],
        'fake_servo on, cr50_run reboot' : [OFF, OFF, ON],
    }


    def initialize(self, host, cmdline_args, full_args):
        super(firmware_Cr50CCDServoCap, self).initialize(host, cmdline_args,
                full_args)
        if not hasattr(self, 'cr50'):
            raise error.TestNAError('Test can only be run on devices with '
                                    'access to the Cr50 console')

        if (self.servo.get_servo_version(active=True) !=
            'servo_v4_with_servo_micro'):
            raise error.TestNAError('Must use servo v4 with servo micro')

        if not self.cr50.servo_dts_mode_is_valid():
            raise error.TestNAError('Need working servo v4 DTS control')

        self.check_servo_monitor()
        # Make sure cr50 is open with testlab enabled.
        self.fast_open(enable_testlab=True)
        if not self.cr50.testlab_is_on():
            raise error.TestNAError('Cr50 testlab mode needs to be enabled')
        logging.info('Cr50 is %s', self.servo.get('cr50_ccd_level'))
        self.cr50.set_cap('UartGscTxECRx', 'Always')


    def cleanup(self):
        """Reenable the EC uart"""
        try:
            self.fake_servo('on')
            self.rdd('detach')
            self.rdd('attach')
        finally:
            super(firmware_Cr50CCDServoCap, self).cleanup()


    def state_matches(self, state_dict, state_name, expected_value):
        """Check the current state. Make sure it matches expected value"""
        valid_state = self.VALID_STATES[state_name][expected_value]
        # I2C isn't a reliable flag, because the hardware often doesn't support
        # it. Remove any I2C flags from the ccdstate output.
        current_state = state_dict[state_name].replace(' I2C', '')
        if isinstance(valid_state, list):
            return current_state in valid_state
        return current_state == valid_state


    def check_servo_monitor(self):
        """Make sure cr50 can detect servo connect and disconnect"""
        # Detach ccd so EC uart won't interfere with servo detection
        self.rdd('detach')
        servo_detect_error = error.TestNAError("Cannot run on device that does "
                "not support servo dectection with ec_uart_en:off/on")
        self.fake_servo('off')
        if not self.state_matches(self.cr50.get_ccdstate(), 'Servo', self.OFF):
            raise servo_detect_error
        self.fake_servo('on')
        if not self.state_matches(self.cr50.get_ccdstate(), 'Servo', self.ON):
            raise servo_detect_error


    def state_is_on(self, ccdstate, state_name):
        """Returns true if the state is on"""
        return self.state_matches(ccdstate, state_name, self.ON)


    def check_state_flags(self, ccdstate):
        """Check the state flags against the reset of the device state

        If there is any mismatch between the device state and state flags,
        return a list of errors.
        """
        flags = ccdstate['State flags']
        ap_uart_enabled = 'UARTAP' in flags
        ec_uart_enabled = 'UARTEC' in flags
        output_enabled = '+TX' in flags
        ccd_enabled = ap_uart_enabled or ec_uart_enabled or output_enabled
        ccd_ext_is_enabled = ccdstate['CCD EXT'] == 'enabled'
        mismatch = []
        if ccd_enabled and not ccd_ext_is_enabled:
            mismatch.append('CCD functionality enabled without CCD EXT')
        if ccd_ext_is_enabled:
            if output_enabled and self.state_is_on(ccdstate, 'Servo'):
                mismatch.append('CCD output is enabled with servo attached')
            if ap_uart_enabled != self.state_is_on(ccdstate, 'AP UART'):
                mismatch.append('AP UART enabled without AP UART on')
            if ec_uart_enabled != self.state_is_on(ccdstate, 'EC'):
                mismatch.append('EC UART enabled without EC on')
        return mismatch



    def verify_ccdstate(self, run):
        """Verify the current state matches the expected result from the run.

        Args:
            run: the string representing the actions that have been run.

        Raises:
            TestError if any of the states are not correct
        """
        if run not in self.EXPECTED_RESULTS:
            raise error.TestError('Add results for %s to EXPECTED_RESULTS', run)
        expected_states = self.EXPECTED_RESULTS[run]

        # Wait a short time for the ccd state to settle
        time.sleep(self.SLEEP)

        ccdstate = self.cr50.get_ccdstate()
        # Check the state flags. Make sure they're in line with the rest of
        # ccdstate
        mismatch = self.check_state_flags(ccdstate)
        for i, expected_state in enumerate(expected_states):
            name = self.RESULT_ORDER[i]
            if expected_state == None:
                logging.info('No expected %s state skipping check', name)
                continue
            # Check that the current state matches the expected state
            if not self.state_matches(ccdstate, name, expected_state):
                mismatch.append('%s is %r not %r' % (name, ccdstate[name],
                                self.STATUS_MAP[expected_state]))
        if mismatch:
            logging.info(ccdstate)
            raise error.TestFail('Unexpected states after %s: %s' % (run,
                mismatch))


    def cr50_run(self, action):
        """Reboot cr50

        @param action: string 'reboot'
        """
        if action == 'reboot':
            self.cr50.reboot()
            self.cr50.send_command('ccd testlab open')
            time.sleep(self.SLEEP)


    def reset_ccd(self, state=None):
        """detach the ccd cable and disconnect servo.

        State is ignored. It just exists to be consistent with the other action
        functions.

        @param state: a var that is ignored
        """
        self.rdd('detach')
        self.fake_servo('off')


    def rdd(self, state):
        """Attach or detach the ccd cable.

        @param state: string 'attach' or 'detach'
        """
        self.servo.set_dts_mode('on' if state == 'attach' else 'off')
        time.sleep(self.SLEEP)


    def fake_servo(self, state):
        """Mimic servo on/off

        Cr50 monitors the servo EC uart tx signal to detect servo. If the signal
        is pulled up, then Cr50 will think servo is connnected. Enable the ec
        uart to enable the pullup. Disable the it to remove the pullup.

        It takes some time for Cr50 to detect the servo state so wait 2 seconds
        before returning.
        """
        self.servo.set('ec_uart_en', state)

        # Cr50 needs time to detect the servo state
        time.sleep(self.SLEEP)


    def run_steps(self, steps):
        """Do each step in steps and then verify the uart state.

        The uart state is order dependent, so we need to know all of the
        previous steps to verify the state. This will do all of the steps in
        the string and verify the Cr50 CCD uart state after each step.

        @param steps: a comma separated string with the steps to run
        """
        # The order of steps is separated by ', '. Remove the last step and
        # run all of the steps before it.
        separated_steps = steps.rsplit(', ', 1)
        if len(separated_steps) > 1:
            self.run_steps(separated_steps[0])

        step = separated_steps[-1]
        # The func and state are separated by ' '
        func, state = step.split(' ')
        logging.info('running %s', step)
        getattr(self, func)(state)

        # Verify the ccd state is correct
        self.verify_ccdstate(steps)


    def run_once(self):
        """Run through TEST_CASES and verify that Cr50 enables/disables uart"""
        for steps in self.TEST_CASES:
            self.run_steps('reset_ccd state')
            logging.info('TESTING: %s', steps)
            self.run_steps(steps)
            logging.info('VERIFIED: %s', steps)
