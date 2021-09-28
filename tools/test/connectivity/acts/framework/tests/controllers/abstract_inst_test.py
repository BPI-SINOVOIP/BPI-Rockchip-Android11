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
"""Python unittest module for GNSS Abstract Instrument Library."""

import socket
import unittest
from unittest.mock import Mock
from unittest.mock import patch
import acts.controllers.abstract_inst as pyinst


class SocketInstrumentTest(unittest.TestCase):
    """A class for unit-testing acts.controllers.gnssinst_lib.abstract_inst"""

    @patch('socket.create_connection')
    def test__connect_socket(self, mock_connect):
        """test socket connection normal completion."""
        mock_connect.return_value.recv.return_value = b'Dummy Instrument\n'

        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')
        test_inst._connect_socket()

        mock_connect.assert_called_with(('192.168.1.11', '5050'), timeout=120)

    @patch('socket.create_connection')
    def test__connect_socket_timeout(self, mock_connect):
        """test socket connection with timeout."""
        mock_connect.side_effect = socket.timeout

        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        with self.assertRaises(pyinst.SocketInstrumentError):
            test_inst._connect_socket()

    @patch('socket.create_connection')
    def test__connect_socket_error(self, mock_connect):
        """test socket connection with socket error."""
        mock_connect.side_effect = socket.error

        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        with self.assertRaises(pyinst.SocketInstrumentError):
            test_inst._connect_socket()

    def test__send(self):
        """test send function with normal completion."""
        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        test_inst._socket = Mock()

        test_inst._send('TestCommand')

        test_inst._socket.sendall.assert_called_with(b'TestCommand\n')

    def test__send_timeout(self):
        """test send function with timeout."""
        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        test_inst._socket = Mock()
        test_inst._socket.sendall.side_effect = socket.timeout

        with self.assertRaises(pyinst.SocketInstrumentError):
            test_inst._send('TestCommand')

    def test__send_error(self):
        """test send function with error."""
        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        test_inst._socket = Mock()
        test_inst._socket.sendall.side_effect = socket.error

        with self.assertRaises(pyinst.SocketInstrumentError):
            test_inst._send('TestCommand')

    def test__recv(self):
        """test recv function with normal completion."""
        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        test_inst._socket = Mock()
        test_inst._socket.recv.return_value = b'TestResponse\n'

        mock_resp = test_inst._recv()

        self.assertEqual(mock_resp, 'TestResponse')

    def test__recv_timeout(self):
        """test recv function with timeout."""
        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        test_inst._socket = Mock()
        test_inst._socket.recv.side_effect = socket.timeout

        with self.assertRaises(pyinst.SocketInstrumentError):
            test_inst._recv()

    def test__recv_error(self):
        """test recv function with error."""
        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        test_inst._socket = Mock()
        test_inst._socket.recv.side_effect = socket.error

        with self.assertRaises(pyinst.SocketInstrumentError):
            test_inst._recv()

    @patch('socket.create_connection')
    def test__close_socket(self, mock_connect):
        """test socket close normal completion."""
        mock_connect.return_value.recv.return_value = b'Dummy Instrument\n'

        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')
        test_inst._connect_socket()
        test_inst._close_socket()

        mock_connect.return_value.shutdown.assert_called_with(socket.SHUT_RDWR)
        mock_connect.return_value.close.assert_called_with()

    def test__query(self):
        """test query function with normal completion."""
        test_inst = pyinst.SocketInstrument('192.168.1.11', '5050')

        test_inst._socket = Mock()
        test_inst._socket.recv.return_value = b'TestResponse\n'

        mock_resp = test_inst._query('TestCommand')

        test_inst._socket.sendall.assert_called_with(b'TestCommand;*OPC?\n')
        self.assertEqual(mock_resp, 'TestResponse')


if __name__ == '__main__':
    unittest.main()
