# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Test overlay."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import shutil
import subprocess
import tempfile
import unittest
from . import overlay
import re


class BindOverlayTest(unittest.TestCase):

  def setUp(self):
    self.source_dir = tempfile.mkdtemp()
    self.destination_dir = tempfile.mkdtemp()
    os.mkdir(os.path.join(self.source_dir, 'base_dir'))
    os.mkdir(os.path.join(self.source_dir, 'base_dir', 'base_project'))
    os.mkdir(os.path.join(self.source_dir, 'base_dir', 'base_project', '.git'))
    os.mkdir(os.path.join(self.source_dir, 'overlays'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest1'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest1', 'from_dir'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest1', 'from_dir', '.git'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest1', 'upper_subdir'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest1', 'upper_subdir',
                          'lower_subdir'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest1', 'upper_subdir',
                          'lower_subdir', 'from_unittest1'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest1', 'upper_subdir',
                          'lower_subdir', 'from_unittest1', '.git'))
    os.symlink(
            os.path.join(self.source_dir, 'overlays', 'unittest1',
                'upper_subdir', 'lower_subdir'),
            os.path.join(self.source_dir, 'overlays', 'unittest1',
                'upper_subdir', 'subdir_symlink')
            )
    open(os.path.join(self.source_dir,
                      'overlays', 'unittest1', 'from_file'), 'a').close()
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest2'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest2', 'upper_subdir'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest2', 'upper_subdir',
                          'lower_subdir'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest2', 'upper_subdir',
                          'lower_subdir', 'from_unittest2'))
    os.mkdir(os.path.join(self.source_dir,
                          'overlays', 'unittest2', 'upper_subdir',
                          'lower_subdir', 'from_unittest2', '.git'))

  def tearDown(self):
    shutil.rmtree(self.source_dir)

  def testValidTargetOverlayBinds(self):
    with tempfile.NamedTemporaryFile('w+t') as test_config:
      test_config.write(
        '<?xml version="1.0" encoding="UTF-8" ?>'
        '<config>'
        '  <target name="unittest">'
        '    <overlay name="unittest1"/>'
        '  </target>'
        '</config>'
        )
      test_config.flush()
      o = overlay.BindOverlay(
          config_file=test_config.name,
          target='unittest',
          source_dir=self.source_dir)
    self.assertIsNotNone(o)
    bind_mounts = o.GetBindMounts()
    bind_source = os.path.join(self.source_dir, 'overlays/unittest1/from_dir')
    bind_destination = os.path.join(self.source_dir, 'from_dir')
    self.assertEqual(bind_mounts[bind_destination], overlay.BindMount(bind_source, False))

  def testMultipleOverlays(self):
    with tempfile.NamedTemporaryFile('w+t') as test_config:
      test_config.write(
        '<?xml version="1.0" encoding="UTF-8" ?>'
        '<config>'
        '  <target name="unittest">'
        '    <overlay name="unittest1"/>'
        '    <overlay name="unittest2"/>'
        '  </target>'
        '</config>'
        )
      test_config.flush()
      o = overlay.BindOverlay(
          config_file=test_config.name,
          target='unittest',
          source_dir=self.source_dir)
    self.assertIsNotNone(o)
    bind_mounts = o.GetBindMounts()
    bind_source = os.path.join(self.source_dir,
      'overlays/unittest1/upper_subdir/lower_subdir/from_unittest1')
    bind_destination = os.path.join(self.source_dir, 'upper_subdir/lower_subdir/from_unittest1')
    self.assertEqual(bind_mounts[bind_destination], overlay.BindMount(bind_source, False))
    bind_source = os.path.join(self.source_dir,
      'overlays/unittest2/upper_subdir/lower_subdir/from_unittest2')
    bind_destination = os.path.join(self.source_dir,
      'upper_subdir/lower_subdir/from_unittest2')
    self.assertEqual(bind_mounts[bind_destination], overlay.BindMount(bind_source, False))

  def testMultipleOverlaysWithWhitelist(self):
    with tempfile.NamedTemporaryFile('w+t') as test_config:
      test_config.write(
        '<?xml version="1.0" encoding="UTF-8" ?>'
        '<config>'
        '  <target name="unittest">'
        '    <overlay name="unittest1"/>'
        '    <overlay name="unittest2"/>'
        '  </target>'
        '</config>'
        )
      test_config.flush()
      rw_whitelist = set('overlays/unittest1/uppser_subdir/lower_subdir/from_unittest1')
      o = overlay.BindOverlay(
          config_file=test_config.name,
          target='unittest',
          source_dir=self.source_dir)
    self.assertIsNotNone(o)
    bind_mounts = o.GetBindMounts()
    bind_source = os.path.join(self.source_dir,
      'overlays/unittest1/upper_subdir/lower_subdir/from_unittest1')
    bind_destination = os.path.join(self.source_dir, 'upper_subdir/lower_subdir/from_unittest1')
    self.assertEqual(
        bind_mounts[bind_destination],
        overlay.BindMount(source_dir=bind_source, readonly=False))
    bind_source = os.path.join(self.source_dir,
      'overlays/unittest2/upper_subdir/lower_subdir/from_unittest2')
    bind_destination = os.path.join(self.source_dir,
      'upper_subdir/lower_subdir/from_unittest2')
    self.assertEqual(bind_mounts[bind_destination], overlay.BindMount(bind_source, False))

  def testValidOverlaidDir(self):
    with tempfile.NamedTemporaryFile('w+t') as test_config:
      test_config.write(
        '<?xml version="1.0" encoding="UTF-8" ?>'
        '<config>'
        '  <target name="unittest">'
        '    <overlay name="unittest1"/>'
        '  </target>'
        '</config>'
        )
      test_config.flush()
      o = overlay.BindOverlay(
          config_file=test_config.name,
          target='unittest',
          source_dir=self.source_dir,
          destination_dir=self.destination_dir)
    self.assertIsNotNone(o)
    bind_mounts = o.GetBindMounts()
    bind_source = os.path.join(self.source_dir, 'overlays/unittest1/from_dir')
    bind_destination = os.path.join(self.destination_dir, 'from_dir')
    self.assertEqual(bind_mounts[bind_destination], overlay.BindMount(bind_source, False))

  def testValidFilesystemViewDirectoryBind(self):
    with tempfile.NamedTemporaryFile('w+t') as test_config:
      test_config.write(
        '<?xml version="1.0" encoding="UTF-8" ?>'
        '<config>'
        '  <target name="unittest">'
        '    <view name="unittestview"/>'
        '  </target>'
        '  <view name="unittestview">'
        '    <path source="overlays/unittest1/from_dir" '
        '    destination="to_dir"/>'
        '  </view>'
        '</config>'
        )
      test_config.flush()
      o = overlay.BindOverlay(
          config_file=test_config.name,
          target='unittest',
          source_dir=self.source_dir)
    self.assertIsNotNone(o)
    bind_mounts = o.GetBindMounts()
    bind_source = os.path.join(self.source_dir, 'overlays/unittest1/from_dir')
    bind_destination = os.path.join(self.source_dir, 'to_dir')
    self.assertEqual(bind_mounts[bind_destination], overlay.BindMount(bind_source, False))

  def testValidFilesystemViewFileBind(self):
    with tempfile.NamedTemporaryFile('w+t') as test_config:
      test_config.write(
          '<?xml version="1.0" encoding="UTF-8" ?>'
          '<config>'
          '  <target name="unittest">'
          '    <view name="unittestview"/>'
          '  </target>'
          '  <view name="unittestview">'
          '    <path source="overlays/unittest1/from_file" '
          '      destination="to_file"/>'
          '  </view>'
          '</config>'
          )
      test_config.flush()
      o = overlay.BindOverlay(
          config_file=test_config.name,
          target='unittest',
          source_dir=self.source_dir)
    self.assertIsNotNone(o)
    bind_mounts = o.GetBindMounts()
    bind_source = os.path.join(self.source_dir, 'overlays/unittest1/from_file')
    bind_destination = os.path.join(self.source_dir, 'to_file')
    self.assertEqual(bind_mounts[bind_destination], overlay.BindMount(bind_source, False))

  def testInvalidTarget(self):
    with tempfile.NamedTemporaryFile('w+t') as test_config:
      test_config.write(
        '<?xml version="1.0" encoding="UTF-8" ?>'
        '<config>'
        '  <target name="unittest">'
        '    <overlay name="unittest1"/>'
        '  </target>'
        '</config>'
        )
      test_config.flush()
      with self.assertRaises(KeyError):
        overlay.BindOverlay(
            config_file=test_config.name,
            target='unknown',
            source_dir=self.source_dir)


if __name__ == '__main__':
  unittest.main()
