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
"""A helper module to communicate over telnet with AttenuatorInstruments.

User code shouldn't need to directly access this class.
"""

import logging
import telnetlib
import re
from acts.controllers import attenuator
from acts.libs.proc import job


def _ascii_string(uc_string):
    return str(uc_string).encode('ASCII')


class _TNHelper(object):
    """An internal helper class for Telnet+SCPI command-based instruments.

    It should only be used by those implementation control libraries and not by
    any user code directly.
    """

    def __init__(self, tx_cmd_separator='\n', rx_cmd_separator='\n',
                 prompt=''):
        self._tn = None
        self._ip_address = None
        self._port = None

        self.tx_cmd_separator = tx_cmd_separator
        self.rx_cmd_separator = rx_cmd_separator
        self.prompt = prompt

    def open(self, host, port=23):
        self._ip_address = host
        self._port = port
        if self._tn:
            self._tn.close()
        logging.debug("Telnet Server IP = %s" % host)
        self._tn = telnetlib.Telnet()
        self._tn.open(host, port, 10)

    def is_open(self):
        return bool(self._tn)

    def close(self):
        if self._tn:
            self._tn.close()
            self._tn = None

    def diagnose_telnet(self):
        """Function that diagnoses telnet connections.

        This function diagnoses telnet connections and can be used in case of
        command failures. The function checks if the devices is still reachable
        via ping, and whether or not it can close and reopen the telnet
        connection.

        Returns:
            False when telnet server is unreachable or unresponsive
            True when telnet server is reachable and telnet connection has been
            successfully reopened
        """
        logging.debug('Diagnosing telnet connection')
        try:
            job_result = job.run('ping {} -c 5'.format(self._ip_address))
        except:
            logging.error("Unable to ping telnet server.")
            return False
        ping_output = job_result.stdout
        if not re.search(r' 0% packet loss', ping_output):
            logging.error('Ping Packets Lost. Result: {}'.format(ping_output))
            return False
        try:
            self.close()
        except:
            logging.error('Cannot close telnet connection.')
            return False
        try:
            self.open(self._ip_address, self._port)
        except:
            logging.error('Cannot reopen telnet connection.')
            return False
        logging.debug('Telnet connection likely recovered')
        return True

    def cmd(self, cmd_str, wait_ret=True):
        if not isinstance(cmd_str, str):
            raise TypeError('Invalid command string', cmd_str)

        if not self.is_open():
            raise attenuator.InvalidOperationError(
                'Telnet connection not open for commands')

        cmd_str.strip(self.tx_cmd_separator)
        self._tn.read_until(_ascii_string(self.prompt), 2)
        self._tn.write(_ascii_string(cmd_str + self.tx_cmd_separator))

        if wait_ret is False:
            return None

        match_idx, match_val, ret_text = self._tn.expect(
            [_ascii_string('\S+' + self.rx_cmd_separator)], 1)

        if match_idx == -1:
            logging.debug('Telnet Command: {}'.format(cmd_str))
            logging.debug('Telnet Reply: ({},{},{})'.format(
                match_idx, match_val, ret_text))
            self.diagnose_telnet()
            raise attenuator.InvalidDataError(
                'Telnet command failed to return valid data')

        ret_text = ret_text.decode()
        ret_text = ret_text.strip(
            self.tx_cmd_separator + self.rx_cmd_separator + self.prompt)

        return ret_text