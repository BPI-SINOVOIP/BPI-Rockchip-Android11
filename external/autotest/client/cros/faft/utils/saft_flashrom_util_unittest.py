"""Unit tests for saft_flashrom_util.py."""

import mock
import StringIO
import unittest

from autotest_lib.client.common_lib import autotemp
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.faft.utils import (os_interface,
                                                 saft_flashrom_util)


class TestFlashromUtil(unittest.TestCase):
    """Tests for saft_flashrom_util.flashrom_util()."""

    def setUp(self):
        self._tempdir = autotemp.tempdir(unique_id='saft_flashrom_util_test')
        self.addCleanup(self._tempdir.clean)

        self.os_if = os_interface.OSInterface(
                self._tempdir.name, test_mode=True)
        self.flashrom_util = saft_flashrom_util.flashrom_util(self.os_if)

    @mock.patch('subprocess.Popen')
    def testTargetIsBroken(self, mock_subproc_popen):
        """check_target should raise error.CmdError if flashrom is broken"""

        bad_flashrom = mock.Mock()
        bad_flashrom.stdout = StringIO.StringIO('broken flashrom stdout')
        bad_flashrom.stderr = StringIO.StringIO('broken flashrom stderr')
        bad_flashrom.returncode = 1

        mock_subproc_popen.return_value = bad_flashrom
        with self.assertRaises(error.CmdError):
            self.flashrom_util.check_target()

    @mock.patch('subprocess.Popen')
    def testTargetIsNotBroken(self, mock_subproc_popen):
        """check_target should return True if flashrom is working"""

        good_flashrom = mock.Mock()
        good_flashrom.stdout = StringIO.StringIO('working flashrom stdout')
        good_flashrom.stderr = StringIO.StringIO('working flashrom stderr')
        good_flashrom.returncode = 0

        mock_subproc_popen.return_value = good_flashrom
        self.assertTrue(self.flashrom_util.check_target())


if __name__ == '__main__':
    unittest.main()
