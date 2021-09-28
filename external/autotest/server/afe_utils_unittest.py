#!/usr/bin/python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import urlparse
import unittest

import logging
import common
from autotest_lib.server import afe_utils


from autotest_lib.site_utils import stable_version_classify as sv

class FakeConfigFromHost(object):
    def get_config_value(self, _namespace, item, **kargs):
        return {
            "stable_version_config_repo_enable": True,
            "stable_version_config_repo_opt_in_boards": ":all",
        }[item]

class FakeHostInfo(object):
    def __init__(self, board, cros_stable_version, servo_cros_stable_version):
        self._board = board
        self._cros_stable_version = cros_stable_version
        self._servo_cros_stable_version = servo_cros_stable_version

    @property
    def board(self):
        return self._board

    @property
    def cros_stable_version(self):
        return self._cros_stable_version

    @property
    def servo_cros_stable_version(self):
        return self._servo_cros_stable_version


class AfeUtilsTestCase(unittest.TestCase):
    def test_get_stable_cros_image_name_v2(self):
        board = "xxx-board"
        host_info = FakeHostInfo(
            board=board,
            servo_cros_stable_version="some garbage",
            cros_stable_version="R1-2.3.4"
        )
        expected = "xxx-board-release/R1-2.3.4"
        config = FakeConfigFromHost()
        out = afe_utils.get_stable_cros_image_name_v2(info=host_info, _config_override=config)
        self.assertEqual(out, expected)

    def test_get_stable_servo_cros_image_name_v2(self):
        board = "xxx-board"
        servo_cros_stable_version="R7-8.9.10"
        expected = "xxx-board-release/R7-8.9.10"
        config = FakeConfigFromHost()
        out = afe_utils.get_stable_servo_cros_image_name_v2(board="xxx-board", servo_version_from_hi=servo_cros_stable_version, _config_override=config)
        self.assertEqual(out, expected)


if __name__ == '__main__':
    unittest.main()
