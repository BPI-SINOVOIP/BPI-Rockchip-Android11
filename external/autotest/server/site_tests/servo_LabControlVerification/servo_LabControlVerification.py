# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test to validate basic, critial servo controls for the lab work."""

import logging
import os
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server import test
from autotest_lib.server.hosts import servo_host

class servo_LabControlVerification(test.test):
    """Ensure control list works, ensure all consoles are talkable."""
    version = 1

    CTRLS_FILE = 'control_sequence.txt'

    def load_ctrl_sequence(self):
        """Look for |CTRLS_FILE| file in same directory to load sequence."""
        ctrls = []
        current_dir = os.path.dirname(os.path.realpath(__file__))
        ctrl_path = os.path.join(current_dir, self.CTRLS_FILE)
        if not os.path.exists(ctrl_path):
            raise error.TestError('File %s needs to exist.' % self.CTRLS_FILE)
        with open(ctrl_path, 'r') as f:
            raw_ctrl_sequence = f.read().strip().split('\n')
        # In addition to the common file, there might be controls that can
        # only be tested for specific versions of servo. Load the additional
        # controls for that servo.
        self.servo_version = self.servo_proxy.get_servo_version()
        for servo_type in self.servo_version.split('_with_'):
            # The split with 'with' is the current servo convention for multi
            # systems such as servo_v4_with_servo_micro.
            servo_type_sequence = '%s_%s' % (servo_type, self.CTRLS_FILE)
            servo_type_sequence_path = os.path.join(current_dir,
                                                    servo_type_sequence)
            if os.path.exists(servo_type_sequence_path):
                with open(servo_type_sequence_path, 'r') as f:
                    raw_ctrl_sequence.extend(f.read().strip().split('\n'))
            else:
                logging.debug('No control sequence file found under %s',
                              servo_type_sequence)
        control_sequence = [raw_ctrl.split(':') for raw_ctrl in
                            raw_ctrl_sequence]
        for ctrl_elems in control_sequence:
            # Note: the attributes are named after the arguments expected in
            # servo.py to be able to use the dictionary as a kwargs. Be mindful
            # of changing them &| keep them in sync.
            ctrl = {'ctrl_name': ctrl_elems[0]}
            if len(ctrl_elems) == 2:
                # This a set servod control.
                ctrl['ctrl_value'] = ctrl_elems[1]
            elif len(ctrl_elems) > 2:
                logging.warn('The line containing %r in the control sequence '
                             'file has an unkown format. Ignoring for now.',
                             ctrl)
            ctrls.append(ctrl)
        return ctrls

    def assert_servod_running(self, host):
        """Assert that servod is running on the host.

        Args:
          host: host running servod

        Raises:
          AutoservRunError: if servod is not running on the host.
        """
        host.run_grep('servodutil show -p %d' % self.servo_port,
                      stdout_err_regexp='No servod scratch entry found')

    def initialize(self, host, port=9999, board='nami'):
        """Check whether servo is already running on |port| and start if not.

        Args:
          port: port on which servod should run
          board: board to start servod with, if servod not already running

        raises:
          error.AutoservRunError if trying to start servod and it fails
        """
        # TODO(coconutruben): board is set to nami for now as that will allow
        # servod to come up and the nami overlay does not have any crazy changes
        # from normal boards. When the new servod is rolled out and it can infer
        # board names itself, remove the board attribute here.
        self.servo_port = port
        self.test_managed_servod = False
        try:
            self.assert_servod_running(host=host)
            logging.debug('Servod already running.')
        except error.AutoservRunError:
            self.test_managed_servod = True
            logging.debug('Servod not running. Test will manage it.')
        if self.test_managed_servod:
            # This indicates the test should start (and manage) servod itself.
            host.run_background('sudo servod -p %d -b %s' % (self.servo_port,
                                                             board))
            # Give servod plenty of time to come up.
            time.sleep(20)
            self.assert_servod_running(host=host)
        # Servod came up successfully - build a servo host and use it to verify
        # basic functionality.
        servo_args = {servo_host.SERVO_HOST_ATTR: host.hostname,
                      servo_host.SERVO_PORT_ATTR: 9999}
        self.servo_host_proxy = servo_host.ServoHost(is_in_lab=False,
                                                     **servo_args)
        self.servo_host_proxy.connect_servo()
        self.servo_proxy = self.servo_host_proxy.get_servo()
        self.ctrls = self.load_ctrl_sequence()

    def run_once(self):
        """Go through control sequence, and verify each of Cr50, EC, AP UART."""
        failed = False
        for ctrl in self.ctrls:
            # Depending on the number of elements in the control determine
            # whether it's 'get' or 'set'
            if len(ctrl) == 1:
                ctrl_type = 'get'
                ctrl_func = self.servo_proxy.get
            if len(ctrl) == 2:
                ctrl_type = 'set'
                ctrl_func = self.servo_proxy.set_nocheck
            logstr = 'About to %s control %r' % (ctrl_type, ctrl['ctrl_name'])
            if ctrl_type == 'set':
                logstr = '%s to %s' % (logstr, ctrl['ctrl_value'])
            logging.info(logstr)
            try:
                ctrl_func(**ctrl)
                logging.info('Success running %r', ctrl['ctrl_name'])
            except error.TestFail as e:
                failed = True
                logging.error('Error running %r. %s', ctrl['ctrl_name'], str(e))
        if self.servo_version != 'servo_v3':
            # Servo V3 does not support Cr50 console. Skip this verification.
            try:
                self.servo_proxy.get('cr50_version')
                logging.info('Success talking on the Cr50 UART.')
            except error.TestFail as e:
                failed = True
                logging.error('Failed to get output on the Cr50 UART.')
        try:
            self.servo_proxy.get('ec_board')
            logging.info('Success talking on the EC UART.')
        except error.TestFail as e:
            failed = True
            logging.error('Failed to get output on the EC UART.')
        try:
            # Note: Since the AP UART has a login challange, the way we verify
            # whether there is output is by rebooting the device, and catching
            # any coreboot or kernel output.
            self.servo_proxy.set_nocheck('cpu_uart_capture', 'on')
            self.servo_proxy.set_nocheck('cold_reset', 'on')
            self.servo_proxy.set_nocheck('cold_reset', 'off')
            # Give the system 7s to reboot and fill some data in the AP UART
            # buffer.
            time.sleep(7)
            content = self.servo_proxy.get('cpu_uart_stream')
            if content:
                logging.debug('Content from the AP UART: %s', str(content))
                logging.info('Success talking on the AP UART.')
            else:
                raise error.TestFail('No output found on AP UART after reboot.')
        except error.TestFail as e:
            failed = True
            logging.error('Failed to get output on the AP UART.')
        if failed:
            raise error.TestFail('At least one control failed. Check logs.')

    def cleanup(self, host):
        """Stop servod if the test started it."""
        self.servo_host_proxy.close()
        if self.test_managed_servod:
            # If test started servod, then turn it off.
            # Note: this is still valid even if there was a failure on init
            # to start servod as it's a noop if no servod is on that port.
            host.run_background('sudo servodutil stop -p %d' % self.servo_port)
