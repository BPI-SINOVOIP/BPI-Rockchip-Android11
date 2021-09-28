# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib
import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class servo_ConsoleStress(FirmwareTest):
    """Verify the given console by running the same command many times.

    Use the given command to verify the specified console. This command should
    have output that doesn't change. This test will fail if the command output
    changes at all or the control fails to run.
    """
    version = 1

    # Give the EC some time to enter/resume from hibernate
    EC_SETTLE_TIME = 10

    def _get_servo_cmd_output(self, cmd):
        """Get the output from the specified servo control"""
        return self.servo.get(cmd).strip()


    def _get_console_cmd_output(self, cmd):
        """Run the command on the console specified by the test args."""
        return self._test_console_obj.send_command_get_output(cmd,
                ['%s.*>' % cmd])[0].strip()

    def cleanup(self):
        """Restore the chan settings"""
        if hasattr(self, '_test_console_obj'):
            self._test_console_obj.send_command('chan restore')
        super(servo_ConsoleStress, self).cleanup()


    def run_once(self, attempts, cmd_type, cmd):
        """Make sure cmd output doesn't change during any of the runs."""
        if cmd_type == 'servo':
            self._get_test_cmd_output = self._get_servo_cmd_output
        elif cmd_type in ['ec', 'cr50']:
            self._test_console_obj = getattr(self, cmd_type)
            # Set chan to 0, so only console task output will print. This will
            # prevent other task output from corrupting the command output.
            self._test_console_obj.send_command('chan save')
            self._test_console_obj.send_command('chan 0')
            self._get_test_cmd_output = self._get_console_cmd_output
        else:
            raise error.TestError('Invalid cmd type %r' % cmd_type)

        start = self._get_test_cmd_output(cmd)
        if not start:
            raise error.TestError('Could not get %s %s output' % (cmd_type,
                    cmd))
        logging.info('start output: %r', start)

        # Run the command for the given number of tries. If the output changes
        # or the command fails to run raise an error.
        for i in range(attempts):
            try:
                output = self._get_test_cmd_output(cmd)
            except Exception, e:
                raise error.TestFail('failed to get %s %r during run %d' %
                        (cmd_type, cmd, i))

            # The command will be run hundreds of times. Print the run number
            # command output once every hundred runs, so you can tell the test
            # is progressing but don't spam too much output.
            if (i % 100) == 0:
                logging.info('run %d %r', i, start)

            if start != output:
                logging.info('MISMATCH:\n %s', '\n'.join(difflib.unified_diff(
                        start.splitlines(), output.splitlines())))
                raise error.TestFail('%s %r output changed after %d runs' %
                                     (cmd_type, cmd, i))
