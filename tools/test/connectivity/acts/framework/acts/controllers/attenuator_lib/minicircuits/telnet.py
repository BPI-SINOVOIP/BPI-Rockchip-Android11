#!/usr/bin/env python3

#   Copyright 2016- The Android Open Source Project
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
"""
Class for Telnet control of Mini-Circuits RCDAT series attenuators

This class provides a wrapper to the MC-RCDAT attenuator modules for purposes
of simplifying and abstracting control down to the basic necessities. It is
not the intention of the module to expose all functionality, but to allow
interchangeable HW to be used.

See http://www.minicircuits.com/softwaredownload/Prog_Manual-6-Programmable_Attenuator.pdf
"""

from acts.controllers import attenuator
from acts.controllers.attenuator_lib import _tnhelper


class AttenuatorInstrument(attenuator.AttenuatorInstrument):
    """A specific telnet-controlled implementation of AttenuatorInstrument for
    Mini-Circuits RC-DAT attenuators.

    With the exception of telnet-specific commands, all functionality is defined
    by the AttenuatorInstrument class. Because telnet is a stateful protocol,
    the functionality of AttenuatorInstrument is contingent upon a telnet
    connection being established.
    """
    def __init__(self, num_atten=0):
        super(AttenuatorInstrument, self).__init__(num_atten)
        self._tnhelper = _tnhelper._TNHelper(tx_cmd_separator='\r\n',
                                             rx_cmd_separator='\r\n',
                                             prompt='')
        self.address = None

    def __del__(self):
        if self.is_open():
            self.close()

    def open(self, host, port=23):
        """Opens a telnet connection to the desired AttenuatorInstrument and
        queries basic information.

        Args:
            host: A valid hostname (IP address or DNS-resolvable name) to an
            MC-DAT attenuator instrument.
            port: An optional port number (defaults to telnet default 23)
        """
        self._tnhelper.open(host, port)
        self.address = host

        if self.num_atten == 0:
            self.num_atten = 1

        config_str = self._tnhelper.cmd('MN?')

        if config_str.startswith('MN='):
            config_str = config_str[len('MN='):]

        self.properties = dict(
            zip(['model', 'max_freq', 'max_atten'], config_str.split('-', 2)))
        self.max_atten = float(self.properties['max_atten'])

    def is_open(self):
        """Returns True if the AttenuatorInstrument has an open connection."""
        return bool(self._tnhelper.is_open())

    def close(self):
        """Closes the telnet connection.

        This should be called as part of any teardown procedure prior to the
        attenuator instrument leaving scope.
        """
        self._tnhelper.close()

    def set_atten(self, idx, value, strict_flag=True):
        """This function sets the attenuation of an attenuator given its index
        in the instrument.

        Args:
            idx: A zero-based index that identifies a particular attenuator in
                an instrument. For instruments that only have one channel, this
                is ignored by the device.
            value: A floating point value for nominal attenuation to be set.
            strict_flag: if True, function raises an error when given out of
                bounds attenuation values, if false, the function sets out of
                bounds values to 0 or max_atten.

        Raises:
            InvalidOperationError if the telnet connection is not open.
            IndexError if the index is not valid for this instrument.
            ValueError if the requested set value is greater than the maximum
                attenuation value.
        """

        if not self.is_open():
            raise attenuator.InvalidOperationError('Connection not open!')

        if idx >= self.num_atten:
            raise IndexError('Attenuator index out of range!', self.num_atten,
                             idx)

        if value > self.max_atten and strict_flag:
            raise ValueError('Attenuator value out of range!', self.max_atten,
                             value)
        # The actual device uses one-based index for channel numbers.
        self._tnhelper.cmd('CHAN:%s:SETATT:%s' % (idx + 1, value))

    def get_atten(self, idx):
        """Returns the current attenuation of the attenuator at the given index.

        Args:
            idx: The index of the attenuator.

        Raises:
            InvalidOperationError if the telnet connection is not open.

        Returns:
            the current attenuation value as a float
        """
        if not self.is_open():
            raise attenuator.InvalidOperationError('Connection not open!')

        if idx >= self.num_atten or idx < 0:
            raise IndexError('Attenuator index out of range!', self.num_atten,
                             idx)

        if self.num_atten == 1:
            atten_val_str = self._tnhelper.cmd(':ATT?')
        else:
            atten_val_str = self._tnhelper.cmd('CHAN:%s:ATT?' % (idx + 1))
        atten_val = float(atten_val_str)
        return atten_val
