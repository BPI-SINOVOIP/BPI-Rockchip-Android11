#!/usr/bin/env python3
# Copyright 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unittests for the config module."""

import os
import shutil
import sys
import tempfile
import unittest

_path = os.path.realpath(__file__ + '/../..')
if sys.path[0] != _path:
    sys.path.insert(0, _path)
del _path

# We have to import our local modules after the sys.path tweak.  We can't use
# relative imports because this is an executable program, not a module.
# pylint: disable=wrong-import-position
import rh.hooks
import rh.config


class PreUploadConfigTests(unittest.TestCase):
    """Tests for the PreUploadConfig class."""

    def testMissing(self):
        """Instantiating a non-existent config file should be fine."""
        rh.config.PreUploadConfig()


class FileTestCase(unittest.TestCase):
    """Helper class for tests cases to setup configuration files."""

    def setUp(self):
        self.tempdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tempdir)

    def _write_config(self, data, filename='temp.cfg'):
        """Helper to write out a config file for testing.

        Returns:
          Path to the file where the configuration was written.
        """
        path = os.path.join(self.tempdir, filename)
        with open(path, 'w') as fp:
            fp.write(data)
        return path

    def _write_local_config(self, data):
        """Helper to write out a local config file for testing."""
        return self._write_config(
            data, filename=rh.config.LocalPreUploadFile.FILENAME)

    def _write_global_config(self, data):
        """Helper to write out a global config file for testing."""
        return self._write_config(
            data, filename=rh.config.GlobalPreUploadFile.FILENAME)


class PreUploadFileTests(FileTestCase):
    """Tests for the PreUploadFile class."""

    def testEmpty(self):
        """Instantiating an empty config file should be fine."""
        path = self._write_config('')
        rh.config.PreUploadFile(path)

    def testValid(self):
        """Verify a fully valid file works."""
        path = self._write_config("""# This be a comment me matey.
[Hook Scripts]
name = script --with "some args"

[Builtin Hooks]
cpplint = true

[Builtin Hooks Options]
cpplint = --some 'more args'

[Options]
ignore_merged_commits = true
""")
        rh.config.PreUploadFile(path)

    def testUnknownSection(self):
        """Reject unknown sections."""
        path = self._write_config('[BOOGA]')
        self.assertRaises(rh.config.ValidationError, rh.config.PreUploadFile,
                          path)

    def testUnknownBuiltin(self):
        """Reject unknown builtin hooks."""
        path = self._write_config('[Builtin Hooks]\nbooga = borg!')
        self.assertRaises(rh.config.ValidationError, rh.config.PreUploadFile,
                          path)

    def testEmptyCustomHook(self):
        """Reject empty custom hooks."""
        path = self._write_config('[Hook Scripts]\nbooga = \t \n')
        self.assertRaises(rh.config.ValidationError, rh.config.PreUploadFile,
                          path)

    def testInvalidIni(self):
        """Reject invalid ini files."""
        path = self._write_config('[Hook Scripts]\n =')
        self.assertRaises(rh.config.ValidationError, rh.config.PreUploadFile,
                          path)

    def testInvalidString(self):
        """Catch invalid string quoting."""
        path = self._write_config("""[Hook Scripts]
name = script --'bad-quotes
""")
        self.assertRaises(rh.config.ValidationError, rh.config.PreUploadFile,
                          path)


class LocalPreUploadFileTests(FileTestCase):
    """Test for the LocalPreUploadFile class."""

    def testInvalidSectionConfig(self):
        """Reject local config that uses invalid sections."""
        path = self._write_config("""[Builtin Hooks Exclude Paths]
cpplint = external/ 'test directory' ^vendor/(?!google/)
""")
        self.assertRaises(rh.config.ValidationError,
                          rh.config.LocalPreUploadFile,
                          path)


class PreUploadSettingsTests(FileTestCase):
    """Tests for the PreUploadSettings class."""

    def testGlobalConfigs(self):
        """Verify global configs stack properly."""
        self._write_global_config("""[Builtin Hooks]
commit_msg_bug_field = true
commit_msg_changeid_field = true
commit_msg_test_field = false""")
        self._write_local_config("""[Builtin Hooks]
commit_msg_bug_field = false
commit_msg_test_field = true""")
        config = rh.config.PreUploadSettings(paths=(self.tempdir,),
                                             global_paths=(self.tempdir,))
        self.assertEqual(config.builtin_hooks,
                         ['commit_msg_changeid_field', 'commit_msg_test_field'])

    def testGlobalExcludeScope(self):
        """Verify exclude scope is valid for global config."""
        self._write_global_config("""[Builtin Hooks Exclude Paths]
cpplint = external/ 'test directory' ^vendor/(?!google/)
""")
        rh.config.PreUploadSettings(global_paths=(self.tempdir,))


if __name__ == '__main__':
    unittest.main()
