# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import socket
import telnetlib

from autotest_lib.client.common_lib import error

SHORT_TIMEOUT = 2
LONG_TIMEOUT = 30

class TelnetHelper(object):
    """Helper class to run basic string commands on a telnet host."""

    def __init__(self, tx_cmd_separator="\n", rx_cmd_separator="\n", prompt=""):
        self._tn = None

        self._tx_cmd_separator = tx_cmd_separator
        self._rx_cmd_separator = rx_cmd_separator
        self._prompt = prompt

    def open(self, hostname, port=22):
        """Opens telnet connection to attenuator host.

        @param hostname: Valid hostname
        @param port: Optional port number, defaults to 22

        """
        if self._tn:
            self._tn.close()

        self._tn = telnetlib.Telnet()

        try:
            self._tn.open(hostname, port, LONG_TIMEOUT)
        except socket.timeout as e:
            raise error.TestError("Timed out while opening telnet connection")

    def is_open(self):
        """Returns true if telnet connection is open."""
        return bool(self._tn)

    def close(self):
        """Closes telnet connection."""
        if self._tn:
            self._tn.close()
            self._tn = None

    def cmd(self, cmd_str, wait_ret=True):
        """Run command on attenuator.

        @param cmd_str: Command to run
        @param wait_ret: Wait for command output or not
        @returns command output
        """
        if not isinstance(cmd_str, str):
            raise error.TestError("Invalid command string %s" % cmd_str)

        if not self.is_open():
            raise error.TestError("Telnet connection not open for commands")

        cmd_str.strip(self._tx_cmd_separator)
        try:
            self._tn.read_until(self._prompt, SHORT_TIMEOUT)
        except EOFError as e:
            raise error.TestError("Connection closed. EOFError (%s)" % e)

        try:
            self._tn.write(cmd_str + self._tx_cmd_separator)
        except socket.error as e:
            raise error.TestError("Connection closed. Socket error (%s)." % e)

        if wait_ret is False:
            return None

        try:
            match_channel_idx, _, ret_text = \
                    self._tn.expect(["\S+" + self._rx_cmd_separator],
                                    SHORT_TIMEOUT)
        except EOFError as e:
            raise error.TestError("Connection closed. EOFError (%s)" % e)

        if match_channel_idx == -1:
            raise error.TestError("Telnet command failed to return valid data. "
                                  "Data returned: %s" % ret_text)

        ret_text = ret_text.strip()

        return ret_text
