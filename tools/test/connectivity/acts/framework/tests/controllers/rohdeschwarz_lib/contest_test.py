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

from acts import base_test
from acts import asserts
from unittest import mock
import socket
import time

# TODO(markdr): Remove this hack after adding zeep to setup.py.
import sys
sys.modules['zeep'] = mock.Mock()

from acts.controllers.rohdeschwarz_lib import contest


class ContestTest(base_test.BaseTestClass):
    """ Unit tests for the contest controller."""

    LOCAL_HOST_IP = '127.0.0.1'

    def test_automation_server_end_to_end(self):
        """ End to end test for the Contest object's ability to start an
        Automation Server and respond to the commands sent through the
        socket interface. """

        automation_port = 5555

        # Instantiate the mock Contest object. This will start a thread in the
        # background running the Automation server.
        with mock.patch('zeep.client.Client') as zeep_client:

            # Create a MagicMock instance
            zeep_client.return_value = mock.MagicMock()

            controller = contest.Contest(
                logger=self.log,
                remote_ip=None,
                remote_port=None,
                automation_listen_ip=self.LOCAL_HOST_IP,
                automation_port=automation_port,
                dut_on_func=None,
                dut_off_func=None,
                ftp_pwd=None,
                ftp_usr=None)

            # Give some time for the server to initialize as it's running on
            # a different thread.
            time.sleep(0.01)

            # Start a socket connection and send a command
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((self.LOCAL_HOST_IP, automation_port))
                s.sendall(b'AtTestcaseStart')
                data = s.recv(1024)
                asserts.assert_true(data == b'OK\n', "Received OK response.")

        controller.destroy()

    def test_automation_protocol_calls_dut_off_func_for_on_command(self):
        """ Tests the AutomationProtocol's ability to turn the DUT off
        upon receiving the requests."""

        dut_on_func = mock.Mock()
        protocol = contest.AutomationServer.AutomationProtocol(
            mock.Mock(), dut_on_func, mock.Mock())
        protocol.send_ok = mock.Mock()
        protocol.data_received(b'DUT_SWITCH_ON')
        asserts.assert_true(dut_on_func.called, 'Function was not called.')
        asserts.assert_true(protocol.send_ok.called, 'OK response not sent.')

    def test_automation_protocol_calls_dut_on_func_for_off_command(self):
        """ Tests the Automation server's ability to turn the DUT on
        upon receiving the requests."""

        dut_off_func = mock.Mock()
        protocol = contest.AutomationServer.AutomationProtocol(
            mock.Mock(), mock.Mock(), dut_off_func)
        protocol.send_ok = mock.Mock()
        protocol.data_received(b'DUT_SWITCH_OFF')
        asserts.assert_true(dut_off_func.called, 'Function was not called.')
        asserts.assert_true(protocol.send_ok.called, 'OK response not sent.')

    def test_automation_protocol_handles_testcase_start_command(self):
        """ Tests the Automation server's ability to handle a testcase start
        command."""

        protocol = contest.AutomationServer.AutomationProtocol(
            mock.Mock(), mock.Mock(), None)
        protocol.send_ok = mock.Mock()
        protocol.data_received(b'AtTestcaseStart name_of_the_testcase')
        asserts.assert_true(protocol.send_ok.called, 'OK response not sent.')

    def test_automation_protocol_handles_testplan_start_command(self):
        """ Tests the Automation server's ability to handle a testplan start
        command."""

        protocol = contest.AutomationServer.AutomationProtocol(
            mock.Mock(), mock.Mock(), None)
        protocol.send_ok = mock.Mock()
        protocol.data_received(b'AtTestplanStart')
        asserts.assert_true(protocol.send_ok.called, 'OK response not sent.')

    def test_automation_protocol_handles_testcase_end_command(self):
        """ Tests the Automation server's ability to handle a testcase end
        command."""

        protocol = contest.AutomationServer.AutomationProtocol(
            mock.Mock(), mock.Mock(), None)
        protocol.send_ok = mock.Mock()
        protocol.data_received(b'AfterTestcase')
        asserts.assert_true(protocol.send_ok.called, 'OK response not sent.')

    def test_automation_protocol_handles_testplan_end_command(self):
        """ Tests the Automation server's ability to handle a testplan start
        command."""

        protocol = contest.AutomationServer.AutomationProtocol(
            mock.Mock(), mock.Mock(), None)
        protocol.send_ok = mock.Mock()
        protocol.data_received(b'AfterTestplan')
        asserts.assert_true(protocol.send_ok.called, 'OK response not sent.')

    # Makes all time.sleep commands call a mock function that returns
    # immediately, rather than sleeping.
    @mock.patch('time.sleep')
    # Prevents the controller to try to download the results from the FTP server
    @mock.patch('acts.controllers.gnssinst_lib.rohdeschwarz.contest'
                '.Contest.pull_test_results')
    def test_execute_testplan_stops_reading_output_on_exit_line(
            self, time_mock, results_func_mock):
        """ Makes sure that execute_test plan returns after receiving an
        exit code.

        Args:
            time_mock: time.sleep mock object.
            results_func_mock: Contest.pull_test_results mock object.
        """

        service_output = mock.Mock()
        # An array of what return values. If a value is an Exception, the
        # Exception is raised instead.
        service_output.side_effect = [
            'Output line 1\n', 'Output line 2\n',
            'Testplan Directory: \\\\a\\b\\c\n'
            'Exit code: 0\n',
            AssertionError('Tried to read output after exit code was sent.')
        ]

        with mock.patch('zeep.client.Client') as zeep_client:
            zeep_client.return_value.service.DoGetOutput = service_output
            controller = contest.Contest(
                logger=self.log,
                remote_ip=None,
                remote_port=None,
                automation_listen_ip=None,
                automation_port=None,
                dut_on_func=None,
                dut_off_func=None,
                ftp_usr=None,
                ftp_pwd=None)

        controller.execute_testplan('TestPlan')
        controller.destroy()

    # Makes all time.sleep commands call a mock function that returns
    # immediately, rather than sleeping.
    @mock.patch('time.sleep')
    # Prevents the controller to try to download the results from the FTP server
    @mock.patch.object(contest.Contest, 'pull_test_results')
    def test_execute_testplan_detects_results_directory(
            self, time_mock, results_func_mock):
        """ Makes sure that execute_test is able to detect the testplan
        directory from the test output.

        Args:
            time_mock: time.sleep mock object.
            results_func_mock: Contest.pull_test_results mock object.
        """

        results_directory = 'results\directory\\name'

        service_output = mock.Mock()
        # An array of what return values. If a value is an Exception, the
        # Exception is raised instead.
        service_output.side_effect = [
            'Testplan Directory: {}{}\\ \n'.format(
                contest.Contest.FTP_ROOT, results_directory), 'Exit code: 0\n'
        ]

        with mock.patch('zeep.client.Client') as zeep_client:
            zeep_client.return_value.service.DoGetOutput = service_output
            controller = contest.Contest(
                logger=self.log,
                remote_ip=None,
                remote_port=None,
                automation_listen_ip=None,
                automation_port=None,
                dut_on_func=None,
                dut_off_func=None,
                ftp_usr=None,
                ftp_pwd=None)

        controller.execute_testplan('TestPlan')

        controller.pull_test_results.assert_called_with(results_directory)
        controller.destroy()

    # Makes all time.sleep commands call a mock function that returns
    # immediately, rather than sleeping.
    @mock.patch('time.sleep')
    # Prevents the controller to try to download the results from the FTP server
    @mock.patch.object(contest.Contest, 'pull_test_results')
    def test_execute_testplan_fails_when_contest_is_unresponsive(
            self, time_mock, results_func_mock):
        """ Makes sure that execute_test plan returns after receiving an
        exit code.

        Args:
            time_mock: time.sleep mock object.
            results_func_mock: Contest.pull_test_results mock object.
        """

        service_output = mock.Mock()
        # An array of what return values. If a value is an Exception, the
        # Exception is raised instead.
        mock_output = [None] * contest.Contest.MAXIMUM_OUTPUT_READ_RETRIES
        mock_output.append(
            AssertionError('Test did not failed after too many '
                           'unsuccessful retries.'))
        service_output.side_effect = mock_output

        with mock.patch('zeep.client.Client') as zeep_client:
            zeep_client.return_value.service.DoGetOutput = service_output
            controller = contest.Contest(
                logger=self.log,
                remote_ip=None,
                remote_port=None,
                automation_listen_ip=None,
                automation_port=None,
                dut_on_func=None,
                dut_off_func=None,
                ftp_usr=None,
                ftp_pwd=None)

        try:
            controller.execute_testplan('TestPlan')
        except RuntimeError:
            pass

        controller.destroy()
