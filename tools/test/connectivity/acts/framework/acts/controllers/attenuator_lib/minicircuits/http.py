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
Class for HTTP control of Mini-Circuits RCDAT series attenuators

This class provides a wrapper to the MC-RCDAT attenuator modules for purposes
of simplifying and abstracting control down to the basic necessities. It is
not the intention of the module to expose all functionality, but to allow
interchangeable HW to be used.

See http://www.minicircuits.com/softwaredownload/Prog_Manual-6-Programmable_Attenuator.pdf
"""

import urllib
from acts.controllers import attenuator


class AttenuatorInstrument(attenuator.AttenuatorInstrument):
    """A specific HTTP-controlled implementation of AttenuatorInstrument for
    Mini-Circuits RC-DAT attenuators.

    With the exception of HTTP-specific commands, all functionality is defined
    by the AttenuatorInstrument class.
    """

    def __init__(self, num_atten=1):
        super(AttenuatorInstrument, self).__init__(num_atten)
        self._ip_address = None
        self._port = None
        self._timeout = None
        self.address = None

    def open(self, host, port=80, timeout=2):
        """Initializes the AttenuatorInstrument and queries basic information.

        Args:
            host: A valid hostname (IP address or DNS-resolvable name) to an
            MC-DAT attenuator instrument.
            port: An optional port number (defaults to http default 80)
            timeout: An optional timeout for http requests
        """
        self._ip_address = host
        self._port = port
        self._timeout = timeout
        self.address = host

        att_req = urllib.request.urlopen('http://{}:{}/MN?'.format(
            self._ip_address, self._port))
        config_str = att_req.read().decode('utf-8')
        if not config_str.startswith('MN='):
            raise attenuator.InvalidDataError(
                'Attenuator returned invalid data. Attenuator returned: {}'.
                format(config_str))

        config_str = config_str[len('MN='):]
        self.properties = dict(
            zip(['model', 'max_freq', 'max_atten'], config_str.split('-', 2)))
        self.max_atten = float(self.properties['max_atten'])

    def is_open(self):
        """Returns True if the AttenuatorInstrument has an open connection.

        Since this controller is based on HTTP requests, there is no connection
        required and the attenuator is always ready to accept requests.
        """
        return True

    def close(self):
        """Closes the connection to the attenuator.

        Since this controller is based on HTTP requests, there is no connection
        teardowns required.
        """
        pass

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
            InvalidDataError if the attenuator does not respond with the
            expected output.
        """
        if not (0 <= idx < self.num_atten):
            raise IndexError('Attenuator index out of range!', self.num_atten,
                             idx)

        if value > self.max_atten and strict_flag:
            raise ValueError('Attenuator value out of range!', self.max_atten,
                             value)
        # The actual device uses one-based index for channel numbers.
        att_req = urllib.request.urlopen(
            'http://{}:{}/CHAN:{}:SETATT:{}'.format(
                self._ip_address, self._port, idx + 1, value),
            timeout=self._timeout)
        att_resp = att_req.read().decode('utf-8')
        if att_resp != '1':
            raise attenuator.InvalidDataError(
                'Attenuator returned invalid data. Attenuator returned: {}'.
                format(att_resp))

    def get_atten(self, idx):
        """Returns the current attenuation of the attenuator at the given index.

        Args:
            idx: The index of the attenuator.

        Raises:
            InvalidDataError if the attenuator does not respond with the
            expected outpu

        Returns:
            the current attenuation value as a float
        """
        if not (0 <= idx < self.num_atten):
            raise IndexError('Attenuator index out of range!', self.num_atten,
                             idx)
        att_req = urllib.request.urlopen(
            'http://{}:{}/CHAN:{}:ATT?'.format(self._ip_address, self.port,
                                               idx + 1),
            timeout=self._timeout)
        att_resp = att_req.read().decode('utf-8')
        try:
            atten_val = float(att_resp)
        except:
            raise attenuator.InvalidDataError(
                'Attenuator returned invalid data. Attenuator returned: {}'.
                format(att_resp))
        return atten_val
