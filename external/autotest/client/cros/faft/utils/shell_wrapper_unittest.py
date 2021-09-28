"""Unit tests for shell_wrapper.py."""

import mock
import StringIO
import unittest

from autotest_lib.client.cros.faft.utils import shell_wrapper


class TestLocalShell(unittest.TestCase):
    """Tests for shell_wrapper.LocalShell()."""

    @mock.patch('subprocess.Popen')
    def testSuccessTokenAbsent(self, mock_subproc_popen):
        cmd = 'foo'
        success_token = 'unexpected'
        mock_process = mock.Mock()
        mock_process.stdout = StringIO.StringIO('sucessfully executed foo')
        mock_subproc_popen.return_value = mock_process
        os_if = mock.Mock()
        local_shell = shell_wrapper.LocalShell(os_if)
        self.assertFalse(
                local_shell.run_command_check_output(cmd, success_token))

    @mock.patch('subprocess.Popen')
    def testSuccessTokenPresent(self, mock_subproc_popen):
        cmd = 'bar'
        success_token = 'expected'
        mock_process = mock.Mock()
        mock_process.stdout = StringIO.StringIO(
                'successfully executed bar. expected is expected.')
        mock_subproc_popen.return_value = mock_process
        os_if = mock.Mock()
        local_shell = shell_wrapper.LocalShell(os_if)
        self.assertTrue(
                local_shell.run_command_check_output(cmd, success_token))

    @mock.patch('subprocess.Popen')
    def testSuccessTokenMalformed(self, mock_subproc_popen):
        cmd = 'baz'
        success_token = 'malformed token \n'
        mock_process = mock.Mock()
        mock_process.stdout = StringIO.StringIO('successfully executed baz')
        mock_subproc_popen.return_value = mock_process
        os_if = mock.Mock()
        local_shell = shell_wrapper.LocalShell(os_if)
        self.assertRaises(shell_wrapper.UnsupportedSuccessToken,
                          local_shell.run_command_check_output, cmd,
                          success_token)


if __name__ == '__main__':
    unittest.main()
