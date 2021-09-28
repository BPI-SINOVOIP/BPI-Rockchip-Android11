# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import math
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest
from autotest_lib.server.cros.servo import pd_console


class firmware_PDVbusRequest(FirmwareTest):
    """
    Servo based USB PD VBUS level test. This test is written to use both
    the DUT and PDTester test board. It requires that the DUT support
    dualrole (SRC or SNK) operation. VBUS change requests occur in two
    methods.

    The 1st test initiates the VBUS change by using special PDTester
    feature to send new SRC CAP message. This causes the DUT to request
    a new VBUS voltage matching what's in the SRC CAP message.

    The 2nd test configures the DUT in SNK mode and uses the pd console
    command 'pd 0/1 dev V' command where V is the desired voltage
    5/12/20. This test is more risky and won't be executed if the 1st
    test is failed. If the DUT max input voltage is not 20V, like 12V,
    and the FAFT config is set wrong, it may negotiate to a voltage
    higher than it can support, that may damage the DUT.

    Pass critera is all voltage transitions are successful.

    """
    version = 1

    PD_SETTLE_DELAY = 4
    USBC_SINK_VOLTAGE = 5
    VBUS_TOLERANCE = 0.12

    VOLTAGE_SEQUENCE = [5, 12, 20, 12, 5, 20, 5, 5, 12, 12, 20]

    def _compare_vbus(self, expected_vbus_voltage, ok_to_fail):
        """Check VBUS using pdtester

        @param expected_vbus_voltage: nominal VBUS level (in volts)
        @param ok_to_fail: True to not treat voltage-not-matched as failure.

        @returns: a tuple containing pass/fail indication and logging string
        """
        # Get Vbus voltage and current
        vbus_voltage = self.pdtester.vbus_voltage
        # Compute voltage tolerance range
        tolerance = self.VBUS_TOLERANCE * expected_vbus_voltage
        voltage_difference = math.fabs(expected_vbus_voltage - vbus_voltage)
        result_str = 'Target = %02dV:\tAct = %.2f\tDelta = %.2f' % \
                     (expected_vbus_voltage, vbus_voltage, voltage_difference)
        # Verify that measured Vbus voltage is within expected range
        voltage_difference = math.fabs(expected_vbus_voltage - vbus_voltage)
        if voltage_difference > tolerance:
            result = 'ALLOWED_FAIL' if ok_to_fail else 'FAIL'
        else:
            result = 'PASS'
        return result, result_str

    def initialize(self, host, cmdline_args, flip_cc=False):
        super(firmware_PDVbusRequest, self).initialize(host, cmdline_args)
        self.setup_pdtester(flip_cc)
        # Only run in normal mode
        self.switcher.setup_mode('normal')
        self.usbpd.send_command('chan 0')

    def cleanup(self):
        # Set back to the max 20V SRC mode at the end.
        self.pdtester.charge(self.pdtester.USBC_MAX_VOLTAGE)

        self.usbpd.send_command('chan 0xffffffff')
        super(firmware_PDVbusRequest, self).cleanup()

    def run_once(self):
        """Exectue VBUS request test.

        """
        # TODO(b/35573842): Refactor to use PDPortPartner to probe the port
        self.pdtester_port = 1 if 'servo_v4' in self.pdtester.servo_type else 0

        # create objects for pd utilities
        pd_dut_utils = pd_console.PDConsoleUtils(self.usbpd)
        pd_pdtester_utils = pd_console.PDConsoleUtils(self.pdtester)

        # Make sure PD support exists in the UART console
        if pd_dut_utils.verify_pd_console() == False:
            raise error.TestFail("pd command not present on console!")

        # Type C connection (PD contract) should exist at this point
        dut_state = pd_dut_utils.query_pd_connection()
        logging.info('DUT PD connection state: %r', dut_state)
        if dut_state['connect'] == False:
            raise error.TestFail("pd connection not found")

        dut_voltage_limit = self.faft_config.usbc_input_voltage_limit
        is_override = self.faft_config.charger_profile_override
        if is_override:
            logging.info('*** Custom charger profile takes over, which may '
                         'cause voltage-not-matched. It is OK to fail. *** ')

        pdtester_failures = []
        logging.info('Start PDTester initiated tests')
        for voltage in self.pdtester.get_charging_voltages():
            logging.info('********* %r *********', voltage)
            # Set charging voltage
            self.pdtester.charge(voltage)
            # Wait for new PD contract to be established
            time.sleep(self.PD_SETTLE_DELAY)
            # Get current PDTester PD state
            pdtester_state = pd_pdtester_utils.get_pd_state(self.pdtester_port)
            # If PDTester is sink, then Vbus_exp = 5v, not skip failure even
            # using charger profile override.
            if pdtester_state == pd_pdtester_utils.SNK_CONNECT:
                expected_vbus_voltage = self.USBC_SINK_VOLTAGE
                ok_to_fail = False
            else:
                expected_vbus_voltage = min(voltage, dut_voltage_limit)
                ok_to_fail = is_override

            result, result_str = self._compare_vbus(expected_vbus_voltage,
                                                    ok_to_fail)
            logging.info('%s, %s', result_str, result)
            if result == 'FAIL':
                pdtester_failures.append(result_str)

        # PDTester is set back to 20V SRC mode.
        self.pdtester.charge(self.pdtester.USBC_MAX_VOLTAGE)
        time.sleep(self.PD_SETTLE_DELAY)

        if pdtester_failures:
            logging.error('PDTester voltage source cap failures')
            for fail in pdtester_failures:
                logging.error('%s', fail)
            number = len(pdtester_failures)
            raise error.TestFail('PDTester failed %d times' % number)

        # The DUT must be in SNK mode for the pd <port> dev <voltage>
        # command to have an effect.
        if dut_state['role'] != pd_dut_utils.SNK_CONNECT:
            # DUT needs to be in SINK Mode, attempt to force change
            pd_dut_utils.set_pd_dualrole(dut_state['port'], 'snk')
            time.sleep(self.PD_SETTLE_DELAY)
            if (pd_dut_utils.get_pd_state(dut_state['port']) !=
                pd_dut_utils.SNK_CONNECT):
                raise error.TestFail("DUT not able to connect in SINK mode")

        logging.info('Start of DUT initiated tests')
        dut_failures = []
        for v in self.VOLTAGE_SEQUENCE:
            if v > dut_voltage_limit:
                logging.info('Target = %02dV: skipped, over the limit %0dV',
                             v, dut_voltage_limit)
                continue
            # Build 'pd <port> dev <voltage> command
            cmd = 'pd %d dev %d' % (dut_state['port'], v)
            pd_dut_utils.send_pd_command(cmd)
            time.sleep(self.PD_SETTLE_DELAY)
            result, result_str = self._compare_vbus(v, ok_to_fail=is_override)
            logging.info('%s, %s', result_str, result)
            if result == 'FAIL':
                dut_failures.append(result_str)

        # Make sure DUT is set back to its max voltage so DUT will accept all
        # options
        cmd = 'pd %d dev %d' % (dut_state['port'], dut_voltage_limit)
        pd_dut_utils.send_pd_command(cmd)
        time.sleep(self.PD_SETTLE_DELAY)
        # The next group of tests need DUT to connect in SNK and SRC modes
        pd_dut_utils.set_pd_dualrole(dut_state['port'], 'on')

        if dut_failures:
            logging.error('DUT voltage request failures')
            for fail in dut_failures:
                logging.error('%s', fail)
            number = len(dut_failures)
            raise error.TestFail('DUT failed %d times' % number)
