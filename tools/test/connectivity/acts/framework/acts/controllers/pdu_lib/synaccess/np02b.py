#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

from acts import utils
from acts.controllers import pdu

import re
import telnetlib
import time


class PduDevice(pdu.PduDevice):
    """Implementation of pure abstract PduDevice object for the Synaccess np02b
    Pdu.
    """
    def __init__(self, host, username, password):
        super(PduDevice, self).__init__(host, username, password)
        self.tnhelper = _TNHelperNP02B(host)

    def on_all(self):
        """ Turns on both outlets on the np02b."""
        self.tnhelper.cmd('ps 1')
        self._verify_state({'1': True, '2': True})

    def off_all(self):
        """ Turns off both outlets on the np02b."""
        self.tnhelper.cmd('ps 0')
        self._verify_state({'1': False, '2': False})

    def on(self, outlet):
        """ Turns on specific outlet on the np02b.

        Args:
            outlet: string of the outlet to turn on ('1' or '2')
        """
        self.tnhelper.cmd('pset %s 1' % outlet)
        self._verify_state({outlet: True})

    def off(self, outlet):
        """ Turns off a specifc outlet on the np02b.

        Args:
            outlet: string of the outlet to turn off ('1' or '2')
        """
        self.tnhelper.cmd('pset %s 0' % outlet)
        self._verify_state({outlet: False})

    def reboot(self, outlet):
        """ Toggles a specific outlet on the np02b to off, then to on.

        Args:
            outlet: string of the outlet to reboot ('1' or '2')
        """
        self.off(outlet)
        self._verify_state({outlet: False})
        self.on(outlet)
        self._verify_state({outlet: True})

    def status(self):
        """ Returns the status of the np02b outlets.

        Return:
            a dict mapping outlet strings ('1' and '2') to:
                True if outlet is ON
                False if outlet is OFF
        """
        res = self.tnhelper.cmd('pshow')
        status_list = re.findall('(ON|OFF)', res)
        status_dict = {}
        for i, status in enumerate(status_list):
            status_dict[str(i + 1)] = (status == 'ON')
        return status_dict

    def close(self):
        """Ensure connection to device is closed.

        In this implementation, this shouldn't be necessary, but could be in
        others that open on creation.
        """
        self.tnhelper.close()

    def _verify_state(self, expected_state, timeout=3):
        """Returns when expected_state is reached on device.

        In order to prevent command functions from exiting until the desired
        effect has occurred, this function verifys that the expected_state is a
        subset of the desired state.

        Args:
            expected_state: a dict representing the expected state of one or
                more outlets on the device. Maps outlet strings ('1' and/or '2')
                to:
                    True if outlet is expected to be ON.
                    False if outlet is expected to be OFF.
            timeout (default: 3): time in seconds until raising an exception.

        Return:
            True, if expected_state is reached.

        Raises:
            PduError if expected_state has not been reached by timeout.
        """
        end_time = time.time() + timeout
        while time.time() < end_time:
            actual_state = self.status()
            if expected_state.items() <= actual_state.items():
                return True
            time.sleep(.1)
        raise pdu.PduError('Timeout while verifying state.\n'
                           'Expected State: %s\n'
                           'Actual State: %s' % (expected_state, actual_state))


class _TNHelperNP02B(object):
    """An internal helper class for Telnet with the Synaccess NP02B Pdu. This
    helper is specific to the idiosyncrasies of the NP02B and therefore should
    not be used with other devices.
    """
    def __init__(self, host):
        self._tn = telnetlib.Telnet()
        self.host = host
        self.tx_cmd_separator = '\n\r'
        self.rx_cmd_separator = '\r\n'
        self.prompt = '>'

    """
    Executes a command on the device via telnet.
    Args:
        cmd_str: A string of the command to be run.
    Returns:
        A string of the response from the valid command (often empty).
    """

    def cmd(self, cmd_str):
        # Open session
        try:
            self._tn.open(self.host, timeout=3)
        except:
            raise pdu.PduError("Failed to open telnet session to host (%s)" %
                               self.host)
        time.sleep(.1)

        # Read to end of first prompt
        cmd_str.strip(self.tx_cmd_separator)
        self._tn.read_eager()
        time.sleep(.1)

        # Write command and read all output text
        self._tn.write(utils.ascii_string(cmd_str + self.tx_cmd_separator))
        res = self._tn.read_until(utils.ascii_string(self.prompt), 2)

        # Parses out the commands output
        if res is None:
            raise pdu.PduError("Command failed: %s" % cmd_str)
        res = res.decode()
        if re.search('Invalid', res):
            raise pdu.PduError("Command Invalid: %s" % cmd_str)
        res = res.replace(self.prompt, '')
        res = res.replace(self.tx_cmd_separator, '')
        res = res.replace(self.rx_cmd_separator, '')
        res = res.replace(cmd_str, '')

        # Close session
        self._tn.close()

        time.sleep(0.5)

        return res

    def close(self):
        self._tn.close()