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
Class for Telnet control of Aeroflex 832X and 833X Series Attenuator Modules

This class provides a wrapper to the Aeroflex attenuator modules for purposes
of simplifying and abstracting control down to the basic necessities. It is
not the intention of the module to expose all functionality, but to allow
interchangeable HW to be used.

See http://www.aeroflex.com/ams/weinschel/PDFILES/IM-608-Models-8320-&-8321-preliminary.pdf
"""

from acts.controllers import attenuator
from acts.controllers.attenuator_lib import _tnhelper


class AttenuatorInstrument(attenuator.AttenuatorInstrument):

    def __init__(self, num_atten=0):
        super(AttenuatorInstrument, self).__init__(num_atten)

        self._tnhelper = _tnhelper._TNHelper(tx_cmd_separator='\r\n',
                                             rx_cmd_separator='\r\n',
                                             prompt='>')
        self.properties = None
        self.address = None

    def open(self, host, port=23):
        """Opens a telnet connection to the desired AttenuatorInstrument and
        queries basic information.

        Args:
            host: A valid hostname (IP address or DNS-resolvable name) to an
            MC-DAT attenuator instrument.
            port: An optional port number (defaults to telnet default 23)
        """
        self._tnhelper.open(host, port)

        # work around a bug in IO, but this is a good thing to do anyway
        self._tnhelper.cmd('*CLS', False)
        self.address = host

        if self.num_atten == 0:
            self.num_atten = int(self._tnhelper.cmd('RFCONFIG? CHAN'))

        configstr = self._tnhelper.cmd('RFCONFIG? ATTN 1')

        self.properties = dict(zip(['model', 'max_atten', 'min_step',
                                    'unknown', 'unknown2', 'cfg_str'],
                                   configstr.split(", ", 5)))

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

    def set_atten(self, idx, value):
        """This function sets the attenuation of an attenuator given its index
        in the instrument.

        Args:
            idx: A zero-based index that identifies a particular attenuator in
                an instrument. For instruments that only have one channel, this
                is ignored by the device.
            value: A floating point value for nominal attenuation to be set.

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

        if value > self.max_atten:
            raise ValueError('Attenuator value out of range!', self.max_atten,
                             value)

        self._tnhelper.cmd('ATTN ' + str(idx + 1) + ' ' + str(value), False)

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

        #       Potentially redundant safety check removed for the moment
        #       if idx >= self.num_atten:
        #           raise IndexError("Attenuator index out of range!", self.num_atten, idx)

        atten_val = self._tnhelper.cmd('ATTN? ' + str(idx + 1))

        return float(atten_val)
