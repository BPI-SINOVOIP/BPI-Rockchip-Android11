#!/usr/bin/env python3
#
#   Copyright 2018 Google, Inc.
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
#

from acts.controllers.attenuator_lib._tnhelper import _ascii_string

import logging
import telnetlib

ID = '.A'
LOGIN_PWD = 'admn'
ON = 'On'
OFF = 'Off'
PASSWORD = 'Password: '
PORT = 23
RPM_PROMPT = 'Switched CDU: '
SEPARATOR = '\n'
TIMEOUT = 3
USERNAME = 'Username: '


class RpmControllerError(Exception):
    """Error related to RPM switch."""

class RpmController(object):
    """Class representing telnet to RPM switch.

    Each object represents a telnet connection to the RPM switch's IP.

    Attributes:
        tn: represents a connection to RPM switch.
        host: IP address of the RPM controller.
    """
    def __init__(self, host):
        """Initializes the RPM controller object.

        Establishes a telnet connection and login to the switch.
        """
        self.host = host
        logging.info('RPM IP: %s' % self.host)

        self.tn = telnetlib.Telnet(self.host)
        self.tn.open(self.host, PORT, TIMEOUT)
        self.run(USERNAME, LOGIN_PWD)
        result = self.run(PASSWORD, LOGIN_PWD)
        if RPM_PROMPT not in result:
            raise RpmControllerError('Failed to login to rpm controller %s'
                                     % self.host)

    def run(self, prompt, cmd_str):
        """Method to run commands on the RPM.

        This method simply runs a command and returns output in decoded format.
        The calling methods should take care of parsing the expected result
        from this output.

        Args:
            prompt: Expected prompt before running a command.
            cmd_str: Command to run on RPM.

        Returns:
            Decoded text returned by the command.
        """
        cmd_str = '%s%s' % (cmd_str, SEPARATOR)
        res = self.tn.read_until(_ascii_string(prompt), TIMEOUT)

        self.tn.write(_ascii_string(cmd_str))
        idx, val, txt = self.tn.expect(
            [_ascii_string('\S+%s' % SEPARATOR)], TIMEOUT)

        return txt.decode()

    def set_rpm_port_state(self, rpm_port, state):
        """Method to turn on/off rpm port.

        Args:
            rpm_port: port number of the switch to turn on.
            state: 'on' or 'off'

        Returns:
            True: if the state is set to the expected value
        """
        port = '%s%s' % (ID, rpm_port)
        logging.info('Turning %s port: %s' % (state, port))
        self.run(RPM_PROMPT, '%s %s' % (state.lower(), port))
        result = self.run(RPM_PROMPT, 'status %s' % port)
        if port not in result:
            raise RpmControllerError('Port %s doesn\'t exist' % port)
        return state in result

    def turn_on(self, rpm_port):
        """Method to turn on a port on the RPM switch.

        Args:
            rpm_port: port number of the switch to turn on.

        Returns:
            True if the port is turned on.
            False if not turned on.
        """
        return self.set_rpm_port_state(rpm_port, ON)

    def turn_off(self, rpm_port):
        """Method to turn off a port on the RPM switch.

        Args:
            rpm_port: port number of the switch to turn off.

        Returns:
            True if the port is turned off.
            False if not turned off.
        """
        return self.set_rpm_port_state(rpm_port, OFF)

    def __del__(self):
        """Close the telnet connection. """
        self.tn.close()


def create_telnet_session(ip):
    """Returns telnet connection object to RPM's IP."""
    return RpmController(ip)

def turn_on_ap(pcap, ssid, rpm_port, rpm_ip=None, rpm=None):
    """Turn on the AP.

    This method turns on the RPM port the AP is connected to,
    verify the SSID of the AP is found in the scan result through the
    packet capturer.

    Either IP addr of the RPM switch or the existing telnet connection
    to the RPM is required. Multiple APs might be connected to the same RPM
    switch. Instead of connecting/terminating telnet for each AP, the test
    can maintain a single telnet connection for all the APs.

    Args:
        pcap: packet capture object.
        ssid: SSID of the wifi network.
        rpm_port: Port number on the RPM switch the AP is connected to.
        rpm_ip: IP address of the RPM switch.
        rpm: telnet connection object to the RPM switch.
    """
    if not rpm and not rpm_ip:
        logging.error("Failed to turn on AP. Need telnet object or RPM IP")
        return False
    elif not rpm:
        rpm = create_telnet_session(rpm_ip)

    return rpm.turn_on(rpm_port) and pcap.start_scan_and_find_network(ssid)

def turn_off_ap(rpm_port, rpm_ip=None, rpm=None):
    """ Turn off AP.

    This method turns off the RPM port the AP is connected to.

    Either IP addr of the RPM switch or the existing telnet connection
    to the RPM is required.

    Args:
        rpm_port: Port number on the RPM switch the AP is connected to.
        rpm_ip: IP address of the RPM switch.
        rpm: telnet connection object to the RPM switch.
    """
    if not rpm and not rpm_ip:
        logging.error("Failed to turn off AP. Need telnet object or RPM IP")
        return False
    elif not rpm:
        rpm = create_telnet_session(rpm_ip)

    return rpm.turn_off(rpm_port)
