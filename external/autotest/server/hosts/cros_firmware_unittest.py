# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock
import unittest

import common
from autotest_lib.server import utils
from server.hosts import cros_firmware


VERSION_OUTPUT = """
{
  "any-model": {
    "host": { "versions": { "ro": "Google_Kukui.12573.13.0", "rw": "Google_Kukui.12573.13.0" },
      "keys": { "root": "b11d74edd286c144e1135b49e7f0bc20cf041f10", "recovery": "c14bd720b70d97394257e3e826bd8f43de48d4ed" },
      "image": "images/bios-kukui.ro-12573-13-0.rw-12573-13-0.bin" },
    "ec": { "versions": { "ro": "kukui_v2.0.2352-5c2c3c7a0", "rw": "kukui_v2.0.2352-5c2c3c7a0" },
      "image": "images/ec-kukui.ro-2-0-2352.rw-2-0-2352.bin" },
    "signature_id": "kukui"
  }
}
"""

NO_VERSION_OUTPUT = """
{
}
"""

UNIBUILD_VERSION_OUTPUT = """
{
  "kukui": {
    "host": { "versions": { "ro": "Google_Kukui.12573.13.0", "rw": "Google_Kukui.12573.13.0" },
      "keys": { "root": "b11d74edd286c144e1135b49e7f0bc20cf041f10", "recovery": "c14bd720b70d97394257e3e826bd8f43de48d4ed" },
      "image": "images/bios-kukui.ro-12573-13-0.rw-12573-13-0.bin" },
    "ec": { "versions": { "ro": "kukui_v2.0.2352-5c2c3c7a0", "rw": "kukui_v2.0.2352-5c2c3c7a0" },
      "image": "images/ec-kukui.ro-2-0-2352.rw-2-0-2352.bin" },
    "signature_id": "kukui"
  },
  "kodama": {
    "host": { "versions": { "ro": "Google_Kodama.12573.14.0", "rw": "Google_Kodama.12573.15.0" },
      "keys": { "root": "b11d74edd286c144e1135b49e7f0bc20cf041f10", "recovery": "c14bd720b70d97394257e3e826bd8f43de48d4ed" },
      "image": "images/bios-kodama.ro-12573-14-0.rw-12573-15-0.bin" },
    "ec": { "versions": { "ro": "kodama_v2.0.2354-8c3c92f29", "rw": "kodama_v2.0.2354-8c3c92f29" },
      "image": "images/ec-kodama.ro-2-0-2354.rw-2-0-2354.bin" },
    "signature_id": "kodama"
  },
  "krane": {
    "host": { "versions": { "ro": "Google_Krane.12573.13.0", "rw": "Google_Krane.12573.13.0" },
      "keys": { "root": "b11d74edd286c144e1135b49e7f0bc20cf041f10", "recovery": "c14bd720b70d97394257e3e826bd8f43de48d4ed" },
      "image": "images/bios-krane.ro-12573-13-0.rw-12573-13-0.bin" },
    "ec": { "versions": { "ro": "krane_v2.0.2352-5c2c3c7a0", "rw": "krane_v2.0.2352-5c2c3c7a0" },
      "image": "images/ec-krane.ro-2-0-2352.rw-2-0-2352.bin" },
    "signature_id": "krane"
  }
}
"""


class FirmwareVersionVerifierTest(unittest.TestCase):
    """Tests for FirmwareVersionVerifier."""

    def test_get_available_firmware_on_update_with_failure(self):
        """Test _get_available_firmware when update script exit_status=1."""
        result = utils.CmdResult(exit_status=1)
        host = mock.Mock()
        host.run.return_value = result

        fw = cros_firmware._get_available_firmware(host, 'lumpy')
        self.assertIsNone(fw)

    def test_get_available_firmware_returns_version(self):
        """_get_available_firmware returns BIOS version."""
        result = utils.CmdResult(stdout=VERSION_OUTPUT, exit_status=0)
        host = mock.Mock()
        host.run.return_value = result

        fw = cros_firmware._get_available_firmware(host, 'kukui')
        self.assertEqual(fw, 'Google_Kukui.12573.13.0')

    def test_get_available_firmware_returns_none(self):
        """_get_available_firmware returns None."""
        result = utils.CmdResult(stdout=NO_VERSION_OUTPUT, exit_status=0)
        host = mock.Mock()
        host.run.return_value = result

        fw = cros_firmware._get_available_firmware(host, 'kukui')
        self.assertIsNone(fw)

    def test_get_available_firmware_unibuild(self):
        """_get_available_firmware on unibuild board with multiple models."""
        result = utils.CmdResult(stdout=UNIBUILD_VERSION_OUTPUT,
                                 exit_status=0)
        host = mock.Mock()
        host.run.return_value = result

        fw = cros_firmware._get_available_firmware(host, 'kukui')
        self.assertEqual(fw, 'Google_Kukui.12573.13.0')

        fw = cros_firmware._get_available_firmware(host, 'kodama')
        self.assertEqual(fw, 'Google_Kodama.12573.15.0')

        fw = cros_firmware._get_available_firmware(host, 'flapjack')
        self.assertIsNone(fw)


if __name__ == '__main__':
    unittest.main()
