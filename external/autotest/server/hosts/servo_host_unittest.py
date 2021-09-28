import mock
import unittest

import common

from autotest_lib.server.hosts import servo_host


class MockCmd(object):
    """Simple mock command with base command and results"""

    def __init__(self, cmd, exit_status, stdout):
        self.cmd = cmd
        self.stdout = stdout
        self.exit_status = exit_status


class MockHost(servo_host.ServoHost):
    """Simple host for running mock'd host commands"""

    def __init__(self, *args):
        self._mock_cmds = {c.cmd: c for c in args}
        self._init_attributes()
        self.hostname = "some_hostname"

    def run(self, command, **kwargs):
        """Finds the matching result by command value"""
        mock_cmd = self._mock_cmds[command]
        file_out = kwargs.get('stdout_tee', None)
        if file_out:
            file_out.write(mock_cmd.stdout)
        return mock_cmd


class ServoHostServoStateTestCase(unittest.TestCase):
    """Tests to verify changing the servo_state"""
    def test_return_broken_if_state_not_defined(self):
        host = MockHost()
        self.assertIsNotNone(host)
        self.assertIsNone(host._servo_state)
        self.assertIsNotNone(host.get_servo_state())
        self.assertEqual(host.get_servo_state(), servo_host.SERVO_STATE_BROKEN)

    def test_verify_set_state_broken_if_raised_error(self):
        host = MockHost()
        host._is_localhost = True
        host._repair_strategy = mock.Mock()
        host._repair_strategy.verify.side_effect = Exception('something_ex')
        try:
            host.verify(silent=True)
            self.assertEqual("Should not be reached", 'expecting error')
        except:
            pass
        self.assertEqual(host.get_servo_state(), servo_host.SERVO_STATE_BROKEN)

    def test_verify_set_state_working_if_no_raised_error(self):
        host = MockHost()
        host._repair_strategy = mock.Mock()
        host.verify(silent=True)
        self.assertEqual(host.get_servo_state(), servo_host.SERVO_STATE_WORKING)

    def test_repair_set_state_broken_if_raised_error(self):
        host = MockHost()
        host._is_localhost = True
        host._repair_strategy = mock.Mock()
        host._repair_strategy.repair.side_effect = Exception('something_ex')
        try:
            host.repair(silent=True)
            self.assertEqual("Should not be reached", 'expecting error')
        except:
            pass
        self.assertEqual(host.get_servo_state(), servo_host.SERVO_STATE_BROKEN)

    def test_repair_set_state_working_if_no_raised_error(self):
        host = MockHost()
        host._is_labstation = False
        host._repair_strategy = mock.Mock()
        host.repair(silent=True)
        self.assertEqual(host.get_servo_state(), servo_host.SERVO_STATE_WORKING)


if __name__ == '__main__':
    unittest.main()
