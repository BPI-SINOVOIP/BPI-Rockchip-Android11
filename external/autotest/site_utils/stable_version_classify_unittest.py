#!/usr/bin/python2
# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import unicode_literals
import unittest
import common
from autotest_lib.site_utils import stable_version_classify as sv


class StableVersionClassifyModelBoard(unittest.TestCase):
    """test that classify board and classify model report the
    correct information source based on the config values"""
    def test_empty_config(self):
        fc = _FakeConfig(enable=False, boards=[], models=[])
        self.assertEqual(
            sv.classify_board('xxx-board', _config_override=fc),
            sv.FROM_AFE,
        )
        self.assertEqual(
            sv.classify_model('xxx-model', _config_override=fc),
            sv.FROM_AFE
        )

    def test_empty_config_but_enabled(self):
        fc = _FakeConfig(enable=True, boards=[], models=[])
        self.assertEqual(
            sv.classify_board('xxx-board', _config_override=fc),
            sv.FROM_AFE,
        )
        self.assertEqual(
            sv.classify_model('xxx-model', _config_override=fc),
            sv.FROM_AFE
        )

    def test_just_nocturne_config(self):
        fc = _FakeConfig(enable=True, boards=[u'nocturne'], models=[u'nocturne'])
        self.assertEqual(
            sv.classify_board('xxx-board', _config_override=fc),
            sv.FROM_AFE,
        )
        self.assertEqual(
            sv.classify_model('xxx-model', _config_override=fc),
            sv.FROM_AFE,
        )
        self.assertEqual(
            sv.classify_board('nocturne', _config_override=fc),
            sv.FROM_HOST_CONFIG,
        )
        self.assertEqual(
            sv.classify_model('nocturne', _config_override=fc),
            sv.FROM_HOST_CONFIG,
        )


    def test_enable_all(self):
        fc = _FakeConfig(enable=True, boards=[u':all'], models=[u':all'])
        self.assertEqual(
            sv.classify_board('xxx-board', _config_override=fc),
            sv.FROM_HOST_CONFIG,
        )
        self.assertEqual(
            sv.classify_model('xxx-model', _config_override=fc),
            sv.FROM_HOST_CONFIG,
        )
        self.assertEqual(
            sv.classify_board('nocturne', _config_override=fc),
            sv.FROM_HOST_CONFIG,
        )
        self.assertEqual(
            sv.classify_model('nocturne', _config_override=fc),
            sv.FROM_HOST_CONFIG,
        )


_TEXT = (type(u''), type(b''))


class _FakeConfig(object):
    def __init__(self, boards=None, models=None, enable=None):
        assert isinstance(boards, list)
        assert isinstance(models, list)
        assert isinstance(enable, bool)
        self.boards = boards
        self.models = models
        self.enable = enable

    def get_config_value(self, namespace, key, type=None, default=None):
        assert isinstance(namespace, _TEXT)
        assert isinstance(key, _TEXT)
        assert namespace == 'CROS'
        if key == 'stable_version_config_repo_enable':
            return self.enable
        if key == 'stable_version_config_repo_opt_in_boards':
            return self.boards
        if key == 'stable_version_config_repo_opt_in_models':
            return self.models
        assert False, "unrecognized key %s" % key



if __name__ == '__main__':
    unittest.main()
