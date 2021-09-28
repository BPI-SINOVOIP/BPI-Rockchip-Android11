# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import time

from autotest_lib.client.bin import utils
from autotest_lib.server.cros.servo import chrome_ec


class PDTesterError(Exception):
    """Error object for PDTester"""
    pass


class PDTester(chrome_ec.ChromeEC):
    """Manages control of a PDTester hardware.

    PDTester is a general term for hardware developed to aid in USB type-C
    debug and control of various type C host devices. It can be either a
    Plankton board or a Servo v4 board.

    We control the PDTester board via the UART and the Servod interfaces.
    PDTester provides many interfaces that access the hardware. It can
    also be passed into the PDConsoleUtils as a console which then
    provides methods to access the pd console.

    This class is to abstract these interfaces.
    """
    # USB charging command delays in seconds.
    USBC_COMMAND_DELAY = 0.5
    # PDTester USBC commands.
    USBC_ROLE = 'usbc_role'
    USBC_MUX = 'usbc_mux'
    RE_USBC_ROLE_VOLTAGE = r'src(\d+)v'
    USBC_CHARGING_VOLTAGES = {
        0: 'sink',
        5: 'src5v',
        12: 'src12v',
        20: 'src20v'}
    USBC_MAX_VOLTAGE = 20
    VBUS_VOLTAGE_MV = 'vbus_voltage'
    VBUS_CURRENT_MA = 'vbus_current'
    VBUS_POWER_MW = 'vbus_power'
    # USBC PD states.
    USBC_PD_STATES = {
        'sink': 'SNK_READY',
        'source': 'SRC_READY'}
    POLL_STATE_SECS = 2

    def __init__(self, servo, servod_proxy):
        """Initialize and keep the servo object.

        @param servo: A Servo object
        @param servod_proxy: Servod proxy for pdtester host
        """
        self.servo_type = servo.get_servo_version()
        if 'servo_v4' in self.servo_type:
            uart_prefix = 'servo_v4_uart'
        else:
            uart_prefix = 'ec_uart'

        super(PDTester, self).__init__(servo, uart_prefix)
        # save servod proxy for methods that access PDTester servod
        self._server = servod_proxy
        self.init_hardware()


    def init_hardware(self):
        """Initializes PDTester hardware."""
        if self.servo_type == 'plankton':
            if not int(self.get('debug_usb_sel')):
                raise PDTesterError('debug_usb_sel (SW3) should be ON!! '
                                    'Please use CN15 to connect Plankton.')
            self.set('typec_to_hub_sw', '0')
            self.set('usb2_mux_sw', '1')
            self.set('usb_dn_pwren', 'on')


    def set(self, control_name, value):
        """Sets the value of a control using servod.

        @param control_name: pdtester servo control item
        @param value: value to set pdtester servo control item
        """
        assert control_name
        self._server.set(control_name, value)


    def get(self, control_name):
        """Gets the value of a control from servod.

        @param control_name: pdtester servo control item
        """
        assert control_name
        return self._server.get(control_name)


    @property
    def vbus_voltage(self):
        """Gets PDTester VBUS voltage in volts."""
        return float(self.get(self.VBUS_VOLTAGE_MV)) / 1000.0


    @property
    def vbus_current(self):
        """Gets PDTester VBUS current in amps."""
        return float(self.get(self.VBUS_CURRENT_MA)) / 1000.0


    @property
    def vbus_power(self):
        """Gets PDTester charging power in watts."""
        return float(self.get(self.VBUS_POWER_MW)) / 1000.0


    def get_charging_voltages(self):
        """Gets the lists of available charging voltages."""
        return self.USBC_CHARGING_VOLTAGES.keys()


    def charge(self, voltage):
        """Sets PDTester to provide power at specific voltage.

        @param voltage: Specified charging voltage in volts.
        """
        if voltage not in self.USBC_CHARGING_VOLTAGES:
            raise PDTesterError('Invalid charging voltage: %s' % voltage)

        self.set(self.USBC_ROLE, self.USBC_CHARGING_VOLTAGES[voltage])
        time.sleep(self.USBC_COMMAND_DELAY)


    @property
    def charging_voltage(self):
        """Gets current charging voltage."""
        usbc_role = self.get(self.USBC_ROLE)
        m = re.match(self.RE_USBC_ROLE_VOLTAGE, usbc_role)
        if m:
            return int(m.group(1))

        if usbc_role == self.USBC_CHARGING_VOLTAGES[0]:
            return 0

        raise PDTesterError('Invalid USBC role: %s' % usbc_role)


    def poll_pd_state(self, state):
        """Polls until PDTester pd goes to the specific state.

        @param state: Specified pd state name.
        """
        if state not in self.USBC_PD_STATES:
            raise PDTesterError('Invalid state name: %s' % state)
        utils.poll_for_condition(
            lambda: self.get('pd_state') == self.USBC_PD_STATES[state],
            exception=utils.TimeoutError('PDTester not in %s state '
                                         'after %s seconds.' %
                                         (self.USBC_PD_STATES[state],
                                          self.POLL_STATE_SECS)),
            timeout=self.POLL_STATE_SECS)


    def set_usbc_mux(self, mux):
        """Sets PDTester usbc_mux.

        @param mux: Specified mux state name.
        """
        if mux not in ['dp', 'usb']:
            raise PDTesterError('Invalid mux name: %s, '
                                'should be either \'dp\' or \'usb\'.' % mux)
        self.set(self.USBC_MUX, mux)
        time.sleep(self.USBC_COMMAND_DELAY)
