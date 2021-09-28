#!/usr/bin python3
#
#       Copyright 2019 - The Android Open Source Project
#
#       Licensed under the Apache License, Version 2.0 (the "License");
#       you may not use this file except in compliance with the License.
#       You may obtain a copy of the License at
#
#               http://www.apache.org/licenses/LICENSE-2.0
#
#       Unless required by applicable law or agreed to in writing, software
#       distributed under the License is distributed on an "AS IS" BASIS,
#       WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#       See the License for the specific language governing permissions and
#       limitations under the License.
"""Python unittest module for pdu_lib.synaccess.np02b"""

import unittest
from unittest.mock import Mock
from unittest.mock import patch

from acts.controllers.pdu import PduError
from acts.controllers.pdu_lib.synaccess.np02b import _TNHelperNP02B, PduDevice

# Test Constants
HOST = '192.168.1.2'
VALID_COMMAND_STR = 'cmd'
VALID_COMMAND_BYTE_STR = b'cmd\n\r'
VALID_RESPONSE_STR = ''
VALID_RESPONSE_BYTE_STR = b'\n\r\r\n\r\n'
STATUS_COMMAND_STR = 'pshow'
STATUS_COMMAND_BYTE_STR = b'pshow\n\r'
STATUS_RESPONSE_STR = (
    'Port | Name    |Status   1 |    Outlet1 |   OFF|   2 |    Outlet2 |   ON |'
)
STATUS_RESPONSE_BYTE_STR = (
    b'Port | Name    |Status   1 |    Outlet1 |   OFF|   2 |    Outlet2 |   '
    b'ON |\n\r\r\n\r\n')
INVALID_COMMAND_OUTPUT_BYTE_STR = b'Invalid Command\n\r\r\n\r\n>'
VALID_STATUS_DICT = {'1': False, '2': True}
INVALID_STATUS_DICT = {'1': False, '2': False}


class _TNHelperNP02BTest(unittest.TestCase):
    """Unit tests for _TNHelperNP02B."""
    @patch('acts.controllers.pdu_lib.synaccess.np02b.time.sleep')
    @patch('acts.controllers.pdu_lib.synaccess.np02b.telnetlib')
    def test_cmd_is_properly_written(self, telnetlib_mock, sleep_mock):
        """cmd should strip whitespace and encode in ASCII."""
        tnhelper = _TNHelperNP02B(HOST)
        telnetlib_mock.Telnet().read_until.return_value = (
            VALID_RESPONSE_BYTE_STR)
        res = tnhelper.cmd(VALID_COMMAND_STR)
        telnetlib_mock.Telnet().write.assert_called_with(
            VALID_COMMAND_BYTE_STR)

    @patch('acts.controllers.pdu_lib.synaccess.np02b.time.sleep')
    @patch('acts.controllers.pdu_lib.synaccess.np02b.telnetlib')
    def test_cmd_valid_command_output_is_properly_parsed(
            self, telnetlib_mock, sleep_mock):
        """cmd should strip the prompt, separators and command from the
        output."""
        tnhelper = _TNHelperNP02B(HOST)
        telnetlib_mock.Telnet().read_until.return_value = (
            VALID_RESPONSE_BYTE_STR)
        res = tnhelper.cmd(VALID_COMMAND_STR)
        self.assertEqual(res, VALID_RESPONSE_STR)

    @patch('acts.controllers.pdu_lib.synaccess.np02b.time.sleep')
    @patch('acts.controllers.pdu_lib.synaccess.np02b.telnetlib')
    def test_cmd_status_output_is_properly_parsed(self, telnetlib_mock,
                                                  sleep_mock):
        """cmd should strip the prompt, separators and command from the output,
        returning just the status information."""
        tnhelper = _TNHelperNP02B(HOST)
        telnetlib_mock.Telnet().read_until.return_value = (
            STATUS_RESPONSE_BYTE_STR)
        res = tnhelper.cmd(STATUS_COMMAND_STR)
        self.assertEqual(res, STATUS_RESPONSE_STR)

    @patch('acts.controllers.pdu_lib.synaccess.np02b.time.sleep')
    @patch('acts.controllers.pdu_lib.synaccess.np02b.telnetlib')
    def test_cmd_invalid_command_raises_error(self, telnetlib_mock,
                                              sleep_mock):
        """cmd should raise PduError when an invalid command is given."""
        tnhelper = _TNHelperNP02B(HOST)
        telnetlib_mock.Telnet().read_until.return_value = (
            INVALID_COMMAND_OUTPUT_BYTE_STR)
        with self.assertRaises(PduError):
            res = tnhelper.cmd('Some invalid command.')


class NP02BPduDeviceTest(unittest.TestCase):
    """Unit tests for NP02B PduDevice implementation."""
    @patch('acts.controllers.pdu_lib.synaccess.np02b._TNHelperNP02B.cmd')
    def test_status_parses_output_to_valid_dictionary(self, tnhelper_cmd_mock):
        """status should parse helper response correctly into dict."""
        np02b = PduDevice(HOST, None, None)
        tnhelper_cmd_mock.return_value = STATUS_RESPONSE_STR
        self.assertEqual(np02b.status(), VALID_STATUS_DICT)

    @patch('acts.controllers.pdu_lib.synaccess.np02b._TNHelperNP02B.cmd')
    def test_verify_state_matches_state(self, tnhelper_cmd_mock):
        """verify_state should return true when expected state is a subset of
        actual state"""
        np02b = PduDevice(HOST, None, None)
        tnhelper_cmd_mock.return_value = STATUS_RESPONSE_STR
        self.assertTrue(np02b._verify_state(VALID_STATUS_DICT))

    @patch('acts.controllers.pdu_lib.synaccess.np02b.time')
    @patch('acts.controllers.pdu_lib.synaccess.np02b._TNHelperNP02B.cmd')
    def test_verify_state_throws_error(self, tnhelper_cmd_mock, time_mock):
        """verify_state should throw error after timeout when actual state never
        reaches expected state"""
        time_mock.time.side_effect = [1, 2, 10]
        np02b = PduDevice(HOST, None, None)
        tnhelper_cmd_mock.return_value = STATUS_RESPONSE_STR
        with self.assertRaises(PduError):
            self.assertTrue(np02b._verify_state(INVALID_STATUS_DICT))