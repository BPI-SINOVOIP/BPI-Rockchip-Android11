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

from zeep import client
from acts.libs.proc import job
from xml.etree import ElementTree
import requests
import asyncio
import time
import threading
import re
import os
import logging


class Contest(object):
    """ Controller interface for Rohde Schwarz CONTEST sequencer software. """

    # Remote Server parameter / operation names
    TESTPLAN_PARAM = 'Testplan'
    TESTPLAN_VERSION_PARAM = 'TestplanVersion'
    KEEP_ALIVE_PARAM = 'KeepContestAlive'
    START_TESTPLAN_OPERATION = 'StartTestplan'

    # Results dictionary keys
    POS_ERROR_KEY = 'pos_error'
    TTFF_KEY = 'ttff'
    SENSITIVITY_KEY = 'sensitivity'

    # Waiting times
    OUTPUT_WAITING_INTERVAL = 5

    # Maximum number of times to retry if the Contest system is not responding
    MAXIMUM_OUTPUT_READ_RETRIES = 25

    # Root directory for the FTP server in the remote computer
    FTP_ROOT = 'D:\\Logs\\'

    def __init__(self, logger, remote_ip, remote_port, automation_listen_ip,
                 automation_port, dut_on_func, dut_off_func, ftp_usr, ftp_pwd):
        """
        Initializes the Contest software controller.

        Args:
            logger: a logger handle.
            remote_ip: the Remote Server's IP address.
            remote_port: port number used by the Remote Server.
            automation_listen_ip: local IP address in which to listen for
                Automation Server connections.
            automation_port: port used for Contest's DUT automation requests.
            dut_on_func: function to turn the DUT on.
            dut_off_func: function to turn the DUT off.
            ftp_usr: username to login to the FTP server on the remote host
            ftp_pwd: password to authenticate ftp_user in the ftp server
        """
        self.log = logger
        self.ftp_user = ftp_usr
        self.ftp_pass = ftp_pwd

        self.remote_server_ip = remote_ip

        server_url = 'http://{}:{}/RemoteServer'.format(remote_ip, remote_port)

        # Initialize the SOAP client to interact with Contest's Remote Server
        try:
            self.soap_client = client.Client(server_url + '/RemoteServer?wsdl')
        except requests.exceptions.ConnectionError:
            self.log.error('Could not connect to the remote endpoint. Is '
                           'Remote Server running on the Windows computer?')
            raise

        # Assign a value to asyncio_loop in case the automation server is not
        # started
        self.asyncio_loop = None

        # Start the automation server if an IP and port number were passed
        if automation_listen_ip and automation_port:
            self.start_automation_server(automation_port, automation_listen_ip,
                                         dut_on_func, dut_off_func)

    def start_automation_server(self, automation_port, automation_listen_ip,
                                dut_on_func, dut_off_func):
        """ Starts the Automation server in a separate process.

        Args:
            automation_listen_ip: local IP address in which to listen for
                Automation Server connections.
            automation_port: port used for Contest's DUT automation requests.
            dut_on_func: function to turn the DUT on.
            dut_off_func: function to turn the DUT off.
        """

        # Start an asyncio event loop to run the automation server
        self.asyncio_loop = asyncio.new_event_loop()

        # Start listening for automation requests on a separate thread. This
        # will start a new thread in which a socket will listen for incoming
        # connections and react to Contest's automation commands

        def start_automation_server(asyncio_loop):
            AutomationServer(self.log, automation_port, automation_listen_ip,
                             dut_on_func, dut_off_func, asyncio_loop)

        automation_daemon = threading.Thread(
            target=start_automation_server, args=[self.asyncio_loop])
        automation_daemon.start()

    def execute_testplan(self, testplan):
        """ Executes a test plan with Contest's Remote Server sequencer.

        Waits until and exit code is provided in the output. Logs the ouput with
        the class logger and pulls the json report from the server if the test
        succeeds.

        Arg:
            testplan: the test plan's name in the Contest system

        Returns:
            a dictionary with test results if the test finished successfully,
            and None if it finished with an error exit code.
        """

        self.soap_client.service.DoSetParameterValue(self.TESTPLAN_PARAM,
                                                     testplan)
        self.soap_client.service.DoSetParameterValue(
            self.TESTPLAN_VERSION_PARAM, 16)
        self.soap_client.service.DoSetParameterValue(self.KEEP_ALIVE_PARAM,
                                                     'true')

        # Remote Server sometimes doesn't respond to the request immediately and
        # frequently times out producing an exception. A shorter timeout will
        # throw the exception earlier and allow the script to continue.
        with self.soap_client.options(timeout=5):
            try:
                self.soap_client.service.DoStartOperation(
                    self.START_TESTPLAN_OPERATION)
            except requests.exceptions.ReadTimeout:
                pass

        self.log.info('Started testplan {} in Remote Server.'.format(testplan))

        testplan_directory = None
        read_retries = 0

        while True:

            time.sleep(self.OUTPUT_WAITING_INTERVAL)
            output = self.soap_client.service.DoGetOutput()

            # Output might be None while the instrument is busy.
            if output:
                self.log.debug(output)

                # Obtain the path to the folder where reports generated by the
                # test equipment will be stored in the remote computer
                if not testplan_directory:
                    prefix = re.escape('Testplan Directory: ' + self.FTP_ROOT)
                    match = re.search('(?<={}).+(?=\\\\)'.format(prefix),
                                      output)
                    if match:
                        testplan_directory = match.group(0)

                # An exit code in the output indicates that the measurement is
                # completed.
                match = re.search('(?<=Exit code: )-?\d+', output)
                if match:
                    exit_code = int(match.group(0))
                    break

                # Reset the not-responding counter
                read_retries = 0

            else:
                # If the output has been None for too many retries in a row,
                # the testing instrument is assumed to be unresponsive.
                read_retries += 1
                if read_retries == self.MAXIMUM_OUTPUT_READ_RETRIES:
                    raise RuntimeError('The Contest test sequencer is not '
                                       'responding.')

        self.log.info(
            'Contest testplan finished with exit code {}.'.format(exit_code))

        if exit_code in [0, 1]:
            self.log.info('Testplan reports are stored in {}.'.format(
                testplan_directory))

            return self.pull_test_results(testplan_directory)

    def pull_test_results(self, testplan_directory):
        """ Downloads the test reports from the remote host and parses the test
        summary to obtain the results.

        Args:
            testplan_directory: directory where to look for reports generated
                by the test equipment in the remote computer

        Returns:
             a JSON object containing the test results
        """

        if not testplan_directory:
            raise ValueError('Invalid testplan directory.')

        # Download test reports from the remote host
        job.run('wget -r --user={} --password={} -P {} ftp://{}/{}'.format(
            self.ftp_user, self.ftp_pass, logging.log_path,
            self.remote_server_ip, testplan_directory))

        # Open the testplan directory
        testplan_path = os.path.join(logging.log_path, self.remote_server_ip,
                                     testplan_directory)

        # Find the report.json file in the testcase folder
        dir_list = os.listdir(testplan_path)
        xml_path = None

        for dir in dir_list:
            if 'TestCaseName' in dir:
                xml_path = os.path.join(testplan_path, dir,
                                        'SummaryReport.xml')
                break

        if not xml_path:
            raise RuntimeError('Could not find testcase directory.')

        # Return the obtained report as a dictionary
        xml_tree = ElementTree.ElementTree()
        xml_tree.parse(source=xml_path)

        results_dictionary = {}

        col_iterator = xml_tree.iter('column')
        for col in col_iterator:
            # Look in the text of the first child for the required metrics
            if col.text == '2D position error [m]':
                results_dictionary[self.POS_ERROR_KEY] = {
                    'min': float(next(col_iterator).text),
                    'med': float(next(col_iterator).text),
                    'avg': float(next(col_iterator).text),
                    'max': float(next(col_iterator).text)
                }
            elif col.text == 'Time to first fix [s]':
                results_dictionary[self.TTFF_KEY] = {
                    'min': float(next(col_iterator).text),
                    'med': float(next(col_iterator).text),
                    'avg': float(next(col_iterator).text),
                    'max': float(next(col_iterator).text)
                }

        message_iterator = xml_tree.iter('message')
        for message in message_iterator:
            # Look for the line showing sensitivity
            if message.text:
                # The typo in 'successfull' is intended as it is present in the
                # test logs generated by the Contest system.
                match = re.search('(?<=Margin search completed, the lowest '
                                  'successfull output power is )-?\d+.?\d+'
                                  '(?= dBm)', message.text)
                if match:
                    results_dictionary[self.SENSITIVITY_KEY] = float(
                        match.group(0))
                    break

        return results_dictionary

    def destroy(self):
        """ Closes all open connections and kills running threads. """
        if self.asyncio_loop:
            # Stopping the asyncio loop will let the Automation Server exit
            self.asyncio_loop.call_soon_threadsafe(self.asyncio_loop.stop)


class AutomationServer:
    """ Server object that handles DUT automation requests from Contest's Remote
    Server.
    """

    def __init__(self, logger, port, listen_ip, dut_on_func, dut_off_func,
                 asyncio_loop):
        """ Initializes the Automation Server.

        Opens a listening socket using a asyncio and waits for incoming
        connections.

        Args:
            logger: a logger handle
            port: port used for Contest's DUT automation requests
            listen_ip: local IP in which to listen for connections
            dut_on_func: function to turn the DUT on
            dut_off_func: function to turn the DUT off
            asyncio_loop: asyncio event loop to listen and process incoming
                data asynchronously
        """

        self.log = logger

        # Define a protocol factory that will provide new Protocol
        # objects to the server created by asyncio. This Protocol
        # objects will handle incoming commands
        def aut_protocol_factory():
            return self.AutomationProtocol(logger, dut_on_func, dut_off_func)

        # Each client connection will create a new protocol instance
        coro = asyncio_loop.create_server(aut_protocol_factory, listen_ip,
                                          port)

        self.server = asyncio_loop.run_until_complete(coro)

        # Serve requests until Ctrl+C is pressed
        self.log.info('Automation Server listening on {}'.format(
            self.server.sockets[0].getsockname()))
        asyncio_loop.run_forever()

    class AutomationProtocol(asyncio.Protocol):
        """ Defines the protocol for communication with Contest's Automation
        client. """

        AUTOMATION_DUT_ON = 'DUT_SWITCH_ON'
        AUTOMATION_DUT_OFF = 'DUT_SWITCH_OFF'
        AUTOMATION_OK = 'OK'

        NOTIFICATION_TESTPLAN_START = 'AtTestplanStart'
        NOTIFICATION_TESTCASE_START = 'AtTestcaseStart'
        NOTIFICATION_TESCASE_END = 'AfterTestcase'
        NOTIFICATION_TESTPLAN_END = 'AfterTestplan'

        def __init__(self, logger, dut_on_func, dut_off_func):
            """ Keeps the function handles to be used upon incoming requests.

            Args:
                logger: a logger handle
                dut_on_func: function to turn the DUT on
                dut_off_func: function to turn the DUT off
            """

            self.log = logger
            self.dut_on_func = dut_on_func
            self.dut_off_func = dut_off_func

        def connection_made(self, transport):
            """ Called when a connection has been established.

            Args:
                transport: represents the socket connection.
            """

            # Keep a reference to the transport as it will allow to write
            # data to the socket later.
            self.transport = transport

            peername = transport.get_extra_info('peername')
            self.log.info('Connection from {}'.format(peername))

        def data_received(self, data):
            """ Called when some data is received.

            Args:
                 data: non-empty bytes object containing the incoming data
             """
            command = data.decode()

            # Remove the line break and newline characters at the end
            command = re.sub('\r?\n$', '', command)

            self.log.info("Command received from Contest's Automation "
                          "client: {}".format(command))

            if command == self.AUTOMATION_DUT_ON:
                self.log.info("Contest's Automation client requested to set "
                              "DUT to on state.")
                self.send_ok()
                self.dut_on_func()
                return
            elif command == self.AUTOMATION_DUT_OFF:
                self.log.info("Contest's Automation client requested to set "
                              "DUT to off state.")
                self.dut_off_func()
                self.send_ok()
            elif command.startswith(self.NOTIFICATION_TESTPLAN_START):
                self.log.info('Test plan is starting.')
                self.send_ok()
            elif command.startswith(self.NOTIFICATION_TESTCASE_START):
                self.log.info('Test case is starting.')
                self.send_ok()
            elif command.startswith(self.NOTIFICATION_TESCASE_END):
                self.log.info('Test case finished.')
                self.send_ok()
            elif command.startswith(self.NOTIFICATION_TESTPLAN_END):
                self.log.info('Test plan finished.')
                self.send_ok()
            else:
                self.log.error('Unhandled automation command: ' + command)
                raise ValueError()

        def send_ok(self):
            """ Sends an OK message to the Automation server. """
            self.log.info("Sending OK response to Contest's Automation client")
            self.transport.write(
                bytearray(
                    self.AUTOMATION_OK + '\n',
                    encoding='utf-8',
                    ))

        def eof_received(self):
            """ Called when the other end signals it won’t send any more
            data.
            """
            self.log.info('Received EOF from Contest Automation client.')
