# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.network import telnet_helper


class Attenuator(object):
    """Represents a minicircuits telnet-controlled 4-channel variable
    attenuator."""

    def __init__(self, host, num_atten=0):
        self._tnhelper = telnet_helper.TelnetHelper(
                tx_cmd_separator="\r\n", rx_cmd_separator="\r\n", prompt="")
        self.host = host
        self.num_atten = num_atten
        self.open(host)


    def __del__(self):
        if self.is_open():
            self.close()

    def open(self, host, port=22):
        """Opens a telnet connection to the attenuator and queries basic
        information.

        @param host: Valid hostname
        @param port: Optional port number, defaults to 22

        """
        self._tnhelper.open(host, port)

        if self.num_atten == 0:
            self.num_atten = 1

        config_str = self._tnhelper.cmd("MN?")

        if config_str.startswith("MN="):
            config_str = config_str[len("MN="):]

        self.properties = dict(zip(['model', 'max_freq', 'max_atten'],
                                   config_str.split("-", 2)))
        self.max_atten = float(self.properties['max_atten'])
        self.min_atten = 0

    def is_open(self):
        """Returns true if telnet connection to attenuator is open."""
        return bool(self._tnhelper.is_open())

    def reopen(self, host, port=22):
        """Close and reopen telnet connection to the attenuator."""
        self._tnhelper.close()
        self._tnhelper.open(host, port)

    def close(self):
        """Closes the telnet connection."""
        self._tnhelper.close()

    def set_atten(self, channel, value):
        """Set attenuation of the attenuator for given channel (0-3).

        @param channel: Zero-based attenuator channel to set attenuation (0-3)
        @param value: Floating point value for attenuation to be set
        """
        if not self.is_open():
            raise error.TestError("Connection not open!")

        if channel >= self.num_atten:
            raise error.TestError("Attenuator channel out of range! Requested "
                                  "%d; max available %d" %
                                  (channel, self.num_atten))

        if not (self.min_atten <= value <= self.max_atten):
            raise error.TestError("Requested attenuator value %d not in range "
                                  "(%d - %d)" %
                                  (value, self.min_atten, self.max_atten))
        # The actual device uses one-based channel for channel numbers.
        if (int(self._tnhelper.cmd("CHAN:%d:SETATT:%d" %
                                  (channel + 1, value))) != 1):
            raise error.TestError("Error while setting attenuation on %d" %
                                  channel)

    def get_atten(self, channel):
        """Returns current attenuation of the attenuator for given channel.

        @param channel: Attenuator channel
        @returns the current attenuation value as a float
        """
        if not self.is_open():
            raise error.TestError("Connection not open!")

        if channel >= self.num_atten or channel < 0:
            raise error.TestError("Attenuator channel out of range! Requested "
                                  "%d; should be between 0 and max available "
                                  "%d" % (channel, self.num_atten))

        return float(self._tnhelper.cmd("CHAN:%d:ATT?" % (channel + 1)))
