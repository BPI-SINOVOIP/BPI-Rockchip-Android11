#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#           http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
"""Python module for Spectracom/Orolia GSG-6 GNSS simulator."""

from acts.controllers import abstract_inst


class GSG6Error(abstract_inst.SocketInstrumentError):
    """GSG-6 Instrument Error Class."""


class GSG6(abstract_inst.SocketInstrument):
    """GSG-6 Class, inherted from abstract_inst SocketInstrument."""

    def __init__(self, ip_addr, ip_port):
        """Init method for GSG-6.

        Args:
            ip_addr: IP Address.
                Type, str.
            ip_port: TCPIP Port.
                Type, str.
        """
        super(GSG6, self).__init__(ip_addr, ip_port)

        self.idn = ''

    def connect(self):
        """Init and Connect to GSG-6."""
        self._connect_socket()

        self.get_idn()

        infmsg = 'Connected to GSG-6, with ID: {}'.format(self.idn)
        self._logger.debug(infmsg)

    def close(self):
        """Close GSG-6."""
        self._close_socket()

        self._logger.debug('Closed connection to GSG-6')

    def get_idn(self):
        """Get the Idenification of GSG-6.

        Returns:
            GSG-6 Identifier
        """
        self.idn = self._query('*IDN?')

        return self.idn

    def start_scenario(self, scenario=''):
        """Start to run scenario.

        Args:
            scenario: Scenario to run.
                Type, str.
                Default, '', which will run current selected one.
        """
        if scenario:
            cmd = 'SOUR:SCEN:LOAD ' + scenario
            self._send(cmd)

        self._send('SOUR:SCEN:CONT START')

        if scenario:
            infmsg = 'Started running scenario {}'.format(scenario)
        else:
            infmsg = 'Started running current scenario'

        self._logger.debug(infmsg)

    def stop_scenario(self):
        """Stop the running scenario."""

        self._send('SOUR:SCEN:CONT STOP')

        self._logger.debug('Stopped running scenario')

    def preset(self):
        """Preset GSG-6 to default status."""
        self._send('*RST')

        self._logger.debug('Reset GSG-6')

    def set_power(self, power_level):
        """set GSG-6 transmit power on all bands.

        Args:
            power_level: transmit power level
                Type, float.
                Decimal, unit [dBm]

        Raises:
            GSG6Error: raise when power level is not in [-160, -65] range.
        """
        if not -160 <= power_level <= -65:
            errmsg = ('"power_level" must be within [-160, -65], '
                      'current input is {}').format(str(power_level))
            raise GSG6Error(error=errmsg, command='set_power')

        self._send(':SOUR:POW ' + str(round(power_level, 1)))

        infmsg = 'Set GSG-6 transmit power to "{}"'.format(
            round(power_level, 1))
        self._logger.debug(infmsg)
