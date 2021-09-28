#!/usr/bin/env python3
# Copyright 2019 The Android Open Source Project
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

"""Unittests for the utils module."""

import datetime
import os
import sys
import unittest

_path = os.path.realpath(__file__ + '/../..')
if sys.path[0] != _path:
    sys.path.insert(0, _path)
del _path

# We have to import our local modules after the sys.path tweak.  We can't use
# relative imports because this is an executable program, not a module.
# pylint: disable=wrong-import-position
import rh
import rh.utils


class TimeDeltaStrTests(unittest.TestCase):
    """Verify behavior of timedelta_str object."""

    def test_same(self):
        """Check timedelta of 0 seconds."""
        delta = datetime.timedelta(0)
        self.assertEqual('0.000s', rh.utils.timedelta_str(delta))

    def test_millisecondss(self):
        """Check timedelta of milliseconds."""
        delta = datetime.timedelta(seconds=0.123456)
        self.assertEqual('0.123s', rh.utils.timedelta_str(delta))

    def test_seconds(self):
        """Check timedelta of seconds."""
        delta = datetime.timedelta(seconds=12.3)
        self.assertEqual('12.300s', rh.utils.timedelta_str(delta))

    def test_minutes(self):
        """Check timedelta of minutes."""
        delta = datetime.timedelta(seconds=72.3)
        self.assertEqual('1m12.300s', rh.utils.timedelta_str(delta))

    def test_hours(self):
        """Check timedelta of hours."""
        delta = datetime.timedelta(seconds=4000.3)
        self.assertEqual('1h6m40.300s', rh.utils.timedelta_str(delta))


class CompletedProcessTests(unittest.TestCase):
    """Verify behavior of CompletedProcess object."""

    def test_empty_cmdstr(self):
        """Check cmdstr with an empty command."""
        result = rh.utils.CompletedProcess(args=[])
        self.assertEqual('', result.cmdstr)

    def test_basic_cmdstr(self):
        """Check cmdstr with a basic command command."""
        result = rh.utils.CompletedProcess(args=['ls', 'a b'])
        self.assertEqual("ls 'a b'", result.cmdstr)

    def test_str(self):
        """Check str() handling."""
        # We don't enforce much, just that it doesn't crash.
        result = rh.utils.CompletedProcess()
        self.assertNotEqual('', str(result))
        result = rh.utils.CompletedProcess(args=[])
        self.assertNotEqual('', str(result))

    def test_repr(self):
        """Check repr() handling."""
        # We don't enforce much, just that it doesn't crash.
        result = rh.utils.CompletedProcess()
        self.assertNotEqual('', repr(result))
        result = rh.utils.CompletedProcess(args=[])
        self.assertNotEqual('', repr(result))


class CalledProcessErrorTests(unittest.TestCase):
    """Verify behavior of CalledProcessError object."""

    def test_basic(self):
        """Basic test we can create a normal instance."""
        rh.utils.CalledProcessError(0, ['mycmd'])
        rh.utils.CalledProcessError(1, ['mycmd'], exception=Exception('bad'))

    def test_stringify(self):
        """Check stringify() handling."""
        # We don't assert much so we leave flexibility in changing format.
        err = rh.utils.CalledProcessError(0, ['mycmd'])
        self.assertIn('mycmd', err.stringify())
        err = rh.utils.CalledProcessError(
            0, ['mycmd'], exception=Exception('bad'))
        self.assertIn('mycmd', err.stringify())

    def test_str(self):
        """Check str() handling."""
        # We don't assert much so we leave flexibility in changing format.
        err = rh.utils.CalledProcessError(0, ['mycmd'])
        self.assertIn('mycmd', str(err))
        err = rh.utils.CalledProcessError(
            0, ['mycmd'], exception=Exception('bad'))
        self.assertIn('mycmd', str(err))

    def test_repr(self):
        """Check repr() handling."""
        # We don't assert much so we leave flexibility in changing format.
        err = rh.utils.CalledProcessError(0, ['mycmd'])
        self.assertNotEqual('', repr(err))
        err = rh.utils.CalledProcessError(
            0, ['mycmd'], exception=Exception('bad'))
        self.assertNotEqual('', repr(err))


class RunCommandTests(unittest.TestCase):
    """Verify behavior of run helper."""

    def test_basic(self):
        """Simple basic test."""
        ret = rh.utils.run(['true'])
        self.assertEqual('true', ret.cmdstr)
        self.assertIsNone(ret.stdout)
        self.assertIsNone(ret.stderr)

    def test_stdout_capture(self):
        """Verify output capturing works."""
        ret = rh.utils.run(['echo', 'hi'], redirect_stdout=True)
        self.assertEqual('hi\n', ret.stdout)
        self.assertIsNone(ret.stderr)

    def test_stderr_capture(self):
        """Verify stderr capturing works."""
        ret = rh.utils.run(['sh', '-c', 'echo hi >&2'], redirect_stderr=True)
        self.assertIsNone(ret.stdout)
        self.assertEqual('hi\n', ret.stderr)

    def test_stdout_utf8(self):
        """Verify reading UTF-8 data works."""
        ret = rh.utils.run(['printf', r'\xc3\x9f'], redirect_stdout=True)
        self.assertEqual(u'ß', ret.stdout)
        self.assertIsNone(ret.stderr)

    def test_stdin_utf8(self):
        """Verify writing UTF-8 data works."""
        ret = rh.utils.run(['cat'], redirect_stdout=True, input=u'ß')
        self.assertEqual(u'ß', ret.stdout)
        self.assertIsNone(ret.stderr)


if __name__ == '__main__':
    unittest.main()
