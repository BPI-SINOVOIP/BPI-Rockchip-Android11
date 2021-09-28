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

"""Unittests for the hooks module."""

import os
import sys
import unittest
from unittest import mock

_path = os.path.realpath(__file__ + '/../..')
if sys.path[0] != _path:
    sys.path.insert(0, _path)
del _path

# We have to import our local modules after the sys.path tweak.  We can't use
# relative imports because this is an executable program, not a module.
# pylint: disable=wrong-import-position
import rh
import rh.config
import rh.hooks


class HooksDocsTests(unittest.TestCase):
    """Make sure all hook features are documented.

    Note: These tests are a bit hokey in that they parse README.md.  But they
    get the job done, so that's all that matters right?
    """

    def setUp(self):
        self.readme = os.path.join(os.path.dirname(os.path.dirname(
            os.path.realpath(__file__))), 'README.md')

    def _grab_section(self, section):
        """Extract the |section| text out of the readme."""
        ret = []
        in_section = False
        with open(self.readme) as fp:
            for line in fp:
                if not in_section:
                    # Look for the section like "## [Tool Paths]".
                    if (line.startswith('#') and
                            line.lstrip('#').strip() == section):
                        in_section = True
                else:
                    # Once we hit the next section (higher or lower), break.
                    if line[0] == '#':
                        break
                    ret.append(line)
        return ''.join(ret)

    def testBuiltinHooks(self):
        """Verify builtin hooks are documented."""
        data = self._grab_section('[Builtin Hooks]')
        for hook in rh.hooks.BUILTIN_HOOKS:
            self.assertIn('* `%s`:' % (hook,), data,
                          msg='README.md missing docs for hook "%s"' % (hook,))

    def testToolPaths(self):
        """Verify tools are documented."""
        data = self._grab_section('[Tool Paths]')
        for tool in rh.hooks.TOOL_PATHS:
            self.assertIn('* `%s`:' % (tool,), data,
                          msg='README.md missing docs for tool "%s"' % (tool,))

    def testPlaceholders(self):
        """Verify placeholder replacement vars are documented."""
        data = self._grab_section('Placeholders')
        for var in rh.hooks.Placeholders.vars():
            self.assertIn('* `${%s}`:' % (var,), data,
                          msg='README.md missing docs for var "%s"' % (var,))


class PlaceholderTests(unittest.TestCase):
    """Verify behavior of replacement variables."""

    def setUp(self):
        self._saved_environ = os.environ.copy()
        os.environ.update({
            'PREUPLOAD_COMMIT_MESSAGE': 'commit message',
            'PREUPLOAD_COMMIT': '5c4c293174bb61f0f39035a71acd9084abfa743d',
        })
        self.replacer = rh.hooks.Placeholders(
            [rh.git.RawDiffEntry(file=x)
             for x in ['path1/file1', 'path2/file2']])

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._saved_environ)

    def testVars(self):
        """Light test for the vars inspection generator."""
        ret = list(self.replacer.vars())
        self.assertGreater(len(ret), 4)
        self.assertIn('PREUPLOAD_COMMIT', ret)

    @mock.patch.object(rh.git, 'find_repo_root', return_value='/ ${BUILD_OS}')
    def testExpandVars(self, _m):
        """Verify the replacement actually works."""
        input_args = [
            # Verify ${REPO_ROOT} is updated, but not REPO_ROOT.
            # We also make sure that things in ${REPO_ROOT} are not double
            # expanded (which is why the return includes ${BUILD_OS}).
            '${REPO_ROOT}/some/prog/REPO_ROOT/ok',
            # Verify lists are merged rather than inserted.
            '${PREUPLOAD_FILES}',
            # Verify each file is preceded with '--file=' prefix.
            '--file=${PREUPLOAD_FILES_PREFIXED}',
            # Verify each file is preceded with '--file' argument.
            '--file',
            '${PREUPLOAD_FILES_PREFIXED}',
            # Verify values with whitespace don't expand into multiple args.
            '${PREUPLOAD_COMMIT_MESSAGE}',
            # Verify multiple values get replaced.
            '${PREUPLOAD_COMMIT}^${PREUPLOAD_COMMIT_MESSAGE}',
            # Unknown vars should be left alone.
            '${THIS_VAR_IS_GOOD}',
        ]
        output_args = self.replacer.expand_vars(input_args)
        exp_args = [
            '/ ${BUILD_OS}/some/prog/REPO_ROOT/ok',
            'path1/file1',
            'path2/file2',
            '--file=path1/file1',
            '--file=path2/file2',
            '--file',
            'path1/file1',
            '--file',
            'path2/file2',
            'commit message',
            '5c4c293174bb61f0f39035a71acd9084abfa743d^commit message',
            '${THIS_VAR_IS_GOOD}',
        ]
        self.assertEqual(output_args, exp_args)

    def testTheTester(self):
        """Make sure we have a test for every variable."""
        for var in self.replacer.vars():
            self.assertIn('test%s' % (var,), dir(self),
                          msg='Missing unittest for variable %s' % (var,))

    def testPREUPLOAD_COMMIT_MESSAGE(self):
        """Verify handling of PREUPLOAD_COMMIT_MESSAGE."""
        self.assertEqual(self.replacer.get('PREUPLOAD_COMMIT_MESSAGE'),
                         'commit message')

    def testPREUPLOAD_COMMIT(self):
        """Verify handling of PREUPLOAD_COMMIT."""
        self.assertEqual(self.replacer.get('PREUPLOAD_COMMIT'),
                         '5c4c293174bb61f0f39035a71acd9084abfa743d')

    def testPREUPLOAD_FILES(self):
        """Verify handling of PREUPLOAD_FILES."""
        self.assertEqual(self.replacer.get('PREUPLOAD_FILES'),
                         ['path1/file1', 'path2/file2'])

    @mock.patch.object(rh.git, 'find_repo_root', return_value='/repo!')
    def testREPO_ROOT(self, m):
        """Verify handling of REPO_ROOT."""
        self.assertEqual(self.replacer.get('REPO_ROOT'), m.return_value)

    @mock.patch.object(rh.hooks, '_get_build_os_name', return_value='vapier os')
    def testBUILD_OS(self, m):
        """Verify handling of BUILD_OS."""
        self.assertEqual(self.replacer.get('BUILD_OS'), m.return_value)


class ExclusionScopeTests(unittest.TestCase):
    """Verify behavior of ExclusionScope class."""

    def testEmpty(self):
        """Verify the in operator for an empty scope."""
        scope = rh.hooks.ExclusionScope([])
        self.assertNotIn('external/*', scope)

    def testGlob(self):
        """Verify the in operator for a scope using wildcards."""
        scope = rh.hooks.ExclusionScope(['vendor/*', 'external/*'])
        self.assertIn('external/tools', scope)

    def testRegex(self):
        """Verify the in operator for a scope using regular expressions."""
        scope = rh.hooks.ExclusionScope(['^vendor/(?!google)',
                                         'external/*'])
        self.assertIn('vendor/', scope)
        self.assertNotIn('vendor/google/', scope)
        self.assertIn('vendor/other/', scope)


class HookOptionsTests(unittest.TestCase):
    """Verify behavior of HookOptions object."""

    @mock.patch.object(rh.hooks, '_get_build_os_name', return_value='vapier os')
    def testExpandVars(self, m):
        """Verify expand_vars behavior."""
        # Simple pass through.
        args = ['who', 'goes', 'there ?']
        self.assertEqual(args, rh.hooks.HookOptions.expand_vars(args))

        # At least one replacement.  Most real testing is in PlaceholderTests.
        args = ['who', 'goes', 'there ?', '${BUILD_OS} is great']
        exp_args = ['who', 'goes', 'there ?', '%s is great' % (m.return_value,)]
        self.assertEqual(exp_args, rh.hooks.HookOptions.expand_vars(args))

    def testArgs(self):
        """Verify args behavior."""
        # Verify initial args to __init__ has higher precedent.
        args = ['start', 'args']
        options = rh.hooks.HookOptions('hook name', args, {})
        self.assertEqual(options.args(), args)
        self.assertEqual(options.args(default_args=['moo']), args)

        # Verify we fall back to default_args.
        args = ['default', 'args']
        options = rh.hooks.HookOptions('hook name', [], {})
        self.assertEqual(options.args(), [])
        self.assertEqual(options.args(default_args=args), args)

    def testToolPath(self):
        """Verify tool_path behavior."""
        options = rh.hooks.HookOptions('hook name', [], {
            'cpplint': 'my cpplint',
        })
        # Check a builtin (and not overridden) tool.
        self.assertEqual(options.tool_path('pylint'), 'pylint')
        # Check an overridden tool.
        self.assertEqual(options.tool_path('cpplint'), 'my cpplint')
        # Check an unknown tool fails.
        self.assertRaises(AssertionError, options.tool_path, 'extra_tool')


class UtilsTests(unittest.TestCase):
    """Verify misc utility functions."""

    def testRunCommand(self):
        """Check _run behavior."""
        # Most testing is done against the utils.RunCommand already.
        # pylint: disable=protected-access
        ret = rh.hooks._run(['true'])
        self.assertEqual(ret.returncode, 0)

    def testBuildOs(self):
        """Check _get_build_os_name behavior."""
        # Just verify it returns something and doesn't crash.
        # pylint: disable=protected-access
        ret = rh.hooks._get_build_os_name()
        self.assertTrue(isinstance(ret, str))
        self.assertNotEqual(ret, '')

    def testGetHelperPath(self):
        """Check get_helper_path behavior."""
        # Just verify it doesn't crash.  It's a dirt simple func.
        ret = rh.hooks.get_helper_path('booga')
        self.assertTrue(isinstance(ret, str))
        self.assertNotEqual(ret, '')



@mock.patch.object(rh.utils, 'run')
@mock.patch.object(rh.hooks, '_check_cmd', return_value=['check_cmd'])
class BuiltinHooksTests(unittest.TestCase):
    """Verify the builtin hooks."""

    def setUp(self):
        self.project = rh.Project(name='project-name', dir='/.../repo/dir',
                                  remote='remote')
        self.options = rh.hooks.HookOptions('hook name', [], {})

    def _test_commit_messages(self, func, accept, msgs, files=None):
        """Helper for testing commit message hooks.

        Args:
          func: The hook function to test.
          accept: Whether all the |msgs| should be accepted.
          msgs: List of messages to test.
          files: List of files to pass to the hook.
        """
        if files:
            diff = [rh.git.RawDiffEntry(file=x) for x in files]
        else:
            diff = []
        for desc in msgs:
            ret = func(self.project, 'commit', desc, diff, options=self.options)
            if accept:
                self.assertFalse(
                    bool(ret), msg='Should have accepted: {{{%s}}}' % (desc,))
            else:
                self.assertTrue(
                    bool(ret), msg='Should have rejected: {{{%s}}}' % (desc,))

    def _test_file_filter(self, mock_check, func, files):
        """Helper for testing hooks that filter by files and run external tools.

        Args:
          mock_check: The mock of _check_cmd.
          func: The hook function to test.
          files: A list of files that we'd check.
        """
        # First call should do nothing as there are no files to check.
        ret = func(self.project, 'commit', 'desc', (), options=self.options)
        self.assertIsNone(ret)
        self.assertFalse(mock_check.called)

        # Second call should include some checks.
        diff = [rh.git.RawDiffEntry(file=x) for x in files]
        ret = func(self.project, 'commit', 'desc', diff, options=self.options)
        self.assertEqual(ret, mock_check.return_value)

    def testTheTester(self, _mock_check, _mock_run):
        """Make sure we have a test for every hook."""
        for hook in rh.hooks.BUILTIN_HOOKS:
            self.assertIn('test_%s' % (hook,), dir(self),
                          msg='Missing unittest for builtin hook %s' % (hook,))

    def test_bpfmt(self, mock_check, _mock_run):
        """Verify the bpfmt builtin hook."""
        # First call should do nothing as there are no files to check.
        ret = rh.hooks.check_bpfmt(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertIsNone(ret)
        self.assertFalse(mock_check.called)

        # Second call will have some results.
        diff = [rh.git.RawDiffEntry(file='Android.bp')]
        ret = rh.hooks.check_bpfmt(
            self.project, 'commit', 'desc', diff, options=self.options)
        self.assertIsNotNone(ret)

    def test_checkpatch(self, mock_check, _mock_run):
        """Verify the checkpatch builtin hook."""
        ret = rh.hooks.check_checkpatch(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertEqual(ret, mock_check.return_value)

    def test_clang_format(self, mock_check, _mock_run):
        """Verify the clang_format builtin hook."""
        ret = rh.hooks.check_clang_format(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertEqual(ret, mock_check.return_value)

    def test_google_java_format(self, mock_check, _mock_run):
        """Verify the google_java_format builtin hook."""
        ret = rh.hooks.check_google_java_format(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertEqual(ret, mock_check.return_value)

    def test_commit_msg_bug_field(self, _mock_check, _mock_run):
        """Verify the commit_msg_bug_field builtin hook."""
        # Check some good messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_bug_field, True, (
                'subj\n\nBug: 1234\n',
                'subj\n\nBug: 1234\nChange-Id: blah\n',
            ))

        # Check some bad messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_bug_field, False, (
                'subj',
                'subj\n\nBUG=1234\n',
                'subj\n\nBUG: 1234\n',
                'subj\n\nBug: N/A\n',
                'subj\n\nBug:\n',
            ))

    def test_commit_msg_changeid_field(self, _mock_check, _mock_run):
        """Verify the commit_msg_changeid_field builtin hook."""
        # Check some good messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_changeid_field, True, (
                'subj\n\nChange-Id: I1234\n',
            ))

        # Check some bad messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_changeid_field, False, (
                'subj',
                'subj\n\nChange-Id: 1234\n',
                'subj\n\nChange-ID: I1234\n',
            ))

    def test_commit_msg_prebuilt_apk_fields(self, _mock_check, _mock_run):
        """Verify the check_commit_msg_prebuilt_apk_fields builtin hook."""
        # Commits without APKs should pass.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_prebuilt_apk_fields,
            True,
            (
                'subj\nTest: test case\nBug: bug id\n',
            ),
            ['foo.cpp', 'bar.py',]
        )

        # Commits with APKs and all the required messages should pass.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_prebuilt_apk_fields,
            True,
            (
                ('Test App\n\nbar.apk\npackage: name=\'com.foo.bar\'\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\nsdkVersion:\'16\'\n'
                 'targetSdkVersion:\'28\'\n\nBuilt here:\n'
                 'http://foo.bar.com/builder\n\n'
                 'This build IS suitable for public release.\n\n'
                 'Bug: 123\nTest: test\nChange-Id: XXXXXXX\n'),
                ('Test App\n\nBuilt here:\nhttp://foo.bar.com/builder\n\n'
                 'This build IS NOT suitable for public release.\n\n'
                 'bar.apk\npackage: name=\'com.foo.bar\'\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\nsdkVersion:\'16\'\n'
                 'targetSdkVersion:\'28\'\n\nBug: 123\nTest: test\n'
                 'Change-Id: XXXXXXX\n'),
                ('Test App\n\nbar.apk\npackage: name=\'com.foo.bar\'\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\nsdkVersion:\'16\'\n'
                 'targetSdkVersion:\'28\'\n\nBuilt here:\n'
                 'http://foo.bar.com/builder\n\n'
                 'This build IS suitable for preview release but IS NOT '
                 'suitable for public release.\n\n'
                 'Bug: 123\nTest: test\nChange-Id: XXXXXXX\n'),
                ('Test App\n\nbar.apk\npackage: name=\'com.foo.bar\'\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\nsdkVersion:\'16\'\n'
                 'targetSdkVersion:\'28\'\n\nBuilt here:\n'
                 'http://foo.bar.com/builder\n\n'
                 'This build IS NOT suitable for preview or public release.\n\n'
                 'Bug: 123\nTest: test\nChange-Id: XXXXXXX\n'),
            ),
            ['foo.apk', 'bar.py',]
        )

        # Commits with APKs and without all the required messages should fail.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_prebuilt_apk_fields,
            False,
            (
                'subj\nTest: test case\nBug: bug id\n',
                # Missing 'package'.
                ('Test App\n\nbar.apk\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\nsdkVersion:\'16\'\n'
                 'targetSdkVersion:\'28\'\n\nBuilt here:\n'
                 'http://foo.bar.com/builder\n\n'
                 'This build IS suitable for public release.\n\n'
                 'Bug: 123\nTest: test\nChange-Id: XXXXXXX\n'),
                # Missing 'sdkVersion'.
                ('Test App\n\nbar.apk\npackage: name=\'com.foo.bar\'\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\n'
                 'targetSdkVersion:\'28\'\n\nBuilt here:\n'
                 'http://foo.bar.com/builder\n\n'
                 'This build IS suitable for public release.\n\n'
                 'Bug: 123\nTest: test\nChange-Id: XXXXXXX\n'),
                # Missing 'targetSdkVersion'.
                ('Test App\n\nbar.apk\npackage: name=\'com.foo.bar\'\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\nsdkVersion:\'16\'\n'
                 'Built here:\nhttp://foo.bar.com/builder\n\n'
                 'This build IS suitable for public release.\n\n'
                 'Bug: 123\nTest: test\nChange-Id: XXXXXXX\n'),
                # Missing build location.
                ('Test App\n\nbar.apk\npackage: name=\'com.foo.bar\'\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\nsdkVersion:\'16\'\n'
                 'targetSdkVersion:\'28\'\n\n'
                 'This build IS suitable for public release.\n\n'
                 'Bug: 123\nTest: test\nChange-Id: XXXXXXX\n'),
                # Missing public release indication.
                ('Test App\n\nbar.apk\npackage: name=\'com.foo.bar\'\n'
                 'versionCode=\'1001\'\nversionName=\'1.0.1001-A\'\n'
                 'platformBuildVersionName=\'\'\ncompileSdkVersion=\'28\'\n'
                 'compileSdkVersionCodename=\'9\'\nsdkVersion:\'16\'\n'
                 'targetSdkVersion:\'28\'\n\nBuilt here:\n'
                 'http://foo.bar.com/builder\n\n'
                 'Bug: 123\nTest: test\nChange-Id: XXXXXXX\n'),
            ),
            ['foo.apk', 'bar.py',]
        )

    def test_commit_msg_test_field(self, _mock_check, _mock_run):
        """Verify the commit_msg_test_field builtin hook."""
        # Check some good messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_test_field, True, (
                'subj\n\nTest: i did done dood it\n',
            ))

        # Check some bad messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_test_field, False, (
                'subj',
                'subj\n\nTEST=1234\n',
                'subj\n\nTEST: I1234\n',
            ))

    def test_commit_msg_relnote_field_format(self, _mock_check, _mock_run):
        """Verify the commit_msg_relnote_field_format builtin hook."""
        # Check some good messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_field_format,
            True,
            (
                'subj',
                'subj\n\nTest: i did done dood it\nBug: 1234',
                'subj\n\nMore content\n\nTest: i did done dood it\nBug: 1234',
                'subj\n\nRelnote: This is a release note\nBug: 1234',
                'subj\n\nRelnote:This is a release note\nBug: 1234',
                'subj\n\nRelnote: This is a release note.\nBug: 1234',
                'subj\n\nRelnote: "This is a release note."\nBug: 1234',
                'subj\n\nRelnote: "This is a \\"release note\\"."\n\nBug: 1234',
                'subj\n\nRelnote: This is a release note.\nChange-Id: 1234',
                'subj\n\nRelnote: This is a release note.\n\nChange-Id: 1234',
                ('subj\n\nRelnote: "This is a release note."\n\n'
                 'Change-Id: 1234'),
                ('subj\n\nRelnote: This is a release note.\n\n'
                 'It has more info, but it is not part of the release note'
                 '\nChange-Id: 1234'),
                ('subj\n\nRelnote: "This is a release note.\n'
                 'It contains a correct second line."'),
                ('subj\n\nRelnote:"This is a release note.\n'
                 'It contains a correct second line."'),
                ('subj\n\nRelnote: "This is a release note.\n'
                 'It contains a correct second line.\n'
                 'And even a third line."\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: "This is a release note.\n'
                 'It contains a correct second line.\n'
                 '\\"Quotes\\" are even used on the third line."\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: This is release note 1.\n'
                 'Relnote: This is release note 2.\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: This is release note 1.\n'
                 'Relnote: "This is release note 2, and it\n'
                 'contains a correctly formatted third line."\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: "This is release note 1 with\n'
                 'a correctly formatted second line."\n\n'
                 'Relnote: "This is release note 2, and it\n'
                 'contains a correctly formatted second line."\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: "This is a release note with\n'
                 'a correctly formatted second line."\n\n'
                 'Bug: 1234'
                 'Here is some extra "quoted" content.'),
                ('subj\n\nRelnote: """This is a release note.\n\n'
                 'This relnote contains an empty line.\n'
                 'Then a non-empty line.\n\n'
                 'And another empty line."""\n\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: """This is a release note.\n\n'
                 'This relnote contains an empty line.\n'
                 'Then an acceptable "quoted" line.\n\n'
                 'And another empty line."""\n\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: """This is a release note."""\n\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: """This is a release note.\n'
                 'It has a second line."""\n\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: """This is a release note.\n'
                 'It has a second line, but does not end here.\n'
                 '"""\n\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: """This is a release note.\n'
                 '"It" has a second line, but does not end here.\n'
                 '"""\n\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: "This is a release note.\n'
                 'It has a second line, but does not end here.\n'
                 '"\n\n'
                 'Bug: 1234'),
            ))

        # Check some bad messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_field_format,
            False,
            (
                'subj\n\nReleaseNote: This is a release note.\n',
                'subj\n\nRelnotes: This is a release note.\n',
                'subj\n\nRel-note: This is a release note.\n',
                'subj\n\nrelnoTes: This is a release note.\n',
                'subj\n\nrel-Note: This is a release note.\n',
                'subj\n\nRelnote: "This is a "release note"."\nBug: 1234',
                'subj\n\nRelnote: This is a "release note".\nBug: 1234',
                ('subj\n\nRelnote: This is a release note.\n'
                 'It contains an incorrect second line.'),
                ('subj\n\nRelnote: "This is a release note.\n'
                 'It contains multiple lines.\n'
                 'But it does not provide an ending quote.\n'),
                ('subj\n\nRelnote: "This is a release note.\n'
                 'It contains multiple lines but no closing quote.\n'
                 'Test: my test "hello world"\n'),
                ('subj\n\nRelnote: This is release note 1.\n'
                 'Relnote: "This is release note 2, and it\n'
                 'contains an incorrectly formatted third line.\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: This is release note 1 with\n'
                 'an incorrectly formatted second line.\n\n'
                 'Relnote: "This is release note 2, and it\n'
                 'contains a correctly formatted second line."\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: "This is release note 1 with\n'
                 'a correctly formatted second line."\n\n'
                 'Relnote: This is release note 2, and it\n'
                 'contains an incorrectly formatted second line.\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: "This is a release note.\n'
                 'It contains a correct second line.\n'
                 'But incorrect "quotes" on the third line."\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: """This is a release note.\n'
                 'It has a second line, but no closing triple quote.\n\n'
                 'Bug: 1234'),
                ('subj\n\nRelnote: "This is a release note.\n'
                 '"It" has a second line, but does not end here.\n'
                 '"\n\n'
                 'Bug: 1234'),
            ))

    def test_commit_msg_relnote_for_current_txt(self, _mock_check, _mock_run):
        """Verify the commit_msg_relnote_for_current_txt builtin hook."""
        diff_without_current_txt = ['bar/foo.txt',
                                    'foo.cpp',
                                    'foo.java',
                                    'foo_current.java',
                                    'foo_current.txt',
                                    'baz/current.java',
                                    'baz/foo_current.txt']
        diff_with_current_txt = diff_without_current_txt + ['current.txt']
        diff_with_subdir_current_txt = \
            diff_without_current_txt + ['foo/current.txt']
        diff_with_experimental_current_txt = \
            diff_without_current_txt + ['public_plus_experimental_current.txt']
        # Check some good messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_for_current_txt,
            True,
            (
                'subj\n\nRelnote: This is a release note\n',
                'subj\n\nRelnote: This is a release note.\n\nChange-Id: 1234',
                ('subj\n\nRelnote: This is release note 1 with\n'
                 'an incorrectly formatted second line.\n\n'
                 'Relnote: "This is release note 2, and it\n'
                 'contains a correctly formatted second line."\n'
                 'Bug: 1234'),
            ),
            files=diff_with_current_txt,
        )
        # Check some good messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_for_current_txt,
            True,
            (
                'subj\n\nRelnote: This is a release note\n',
                'subj\n\nRelnote: This is a release note.\n\nChange-Id: 1234',
                ('subj\n\nRelnote: This is release note 1 with\n'
                 'an incorrectly formatted second line.\n\n'
                 'Relnote: "This is release note 2, and it\n'
                 'contains a correctly formatted second line."\n'
                 'Bug: 1234'),
            ),
            files=diff_with_experimental_current_txt,
        )
        # Check some good messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_for_current_txt,
            True,
            (
                'subj\n\nRelnote: This is a release note\n',
                'subj\n\nRelnote: This is a release note.\n\nChange-Id: 1234',
                ('subj\n\nRelnote: This is release note 1 with\n'
                 'an incorrectly formatted second line.\n\n'
                 'Relnote: "This is release note 2, and it\n'
                 'contains a correctly formatted second line."\n'
                 'Bug: 1234'),
            ),
            files=diff_with_subdir_current_txt,
        )
        # Check some good messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_for_current_txt,
            True,
            (
                'subj',
                'subj\nBug: 12345\nChange-Id: 1234',
                'subj\n\nRelnote: This is a release note\n',
                'subj\n\nRelnote: This is a release note.\n\nChange-Id: 1234',
                ('subj\n\nRelnote: This is release note 1 with\n'
                 'an incorrectly formatted second line.\n\n'
                 'Relnote: "This is release note 2, and it\n'
                 'contains a correctly formatted second line."\n'
                 'Bug: 1234'),
            ),
            files=diff_without_current_txt,
        )
        # Check some bad messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_for_current_txt,
            False,
            (
                'subj'
                'subj\nBug: 12345\nChange-Id: 1234',
            ),
            files=diff_with_current_txt,
        )
        # Check some bad messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_for_current_txt,
            False,
            (
                'subj'
                'subj\nBug: 12345\nChange-Id: 1234',
            ),
            files=diff_with_experimental_current_txt,
        )
        # Check some bad messages.
        self._test_commit_messages(
            rh.hooks.check_commit_msg_relnote_for_current_txt,
            False,
            (
                'subj'
                'subj\nBug: 12345\nChange-Id: 1234',
            ),
            files=diff_with_subdir_current_txt,
        )

    def test_cpplint(self, mock_check, _mock_run):
        """Verify the cpplint builtin hook."""
        self._test_file_filter(mock_check, rh.hooks.check_cpplint,
                               ('foo.cpp', 'foo.cxx'))

    def test_gofmt(self, mock_check, _mock_run):
        """Verify the gofmt builtin hook."""
        # First call should do nothing as there are no files to check.
        ret = rh.hooks.check_gofmt(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertIsNone(ret)
        self.assertFalse(mock_check.called)

        # Second call will have some results.
        diff = [rh.git.RawDiffEntry(file='foo.go')]
        ret = rh.hooks.check_gofmt(
            self.project, 'commit', 'desc', diff, options=self.options)
        self.assertIsNotNone(ret)

    def test_jsonlint(self, mock_check, _mock_run):
        """Verify the jsonlint builtin hook."""
        # First call should do nothing as there are no files to check.
        ret = rh.hooks.check_json(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertIsNone(ret)
        self.assertFalse(mock_check.called)

        # TODO: Actually pass some valid/invalid json data down.

    def test_pylint(self, mock_check, _mock_run):
        """Verify the pylint builtin hook."""
        self._test_file_filter(mock_check, rh.hooks.check_pylint2,
                               ('foo.py',))

    def test_pylint2(self, mock_check, _mock_run):
        """Verify the pylint2 builtin hook."""
        self._test_file_filter(mock_check, rh.hooks.check_pylint2,
                               ('foo.py',))

    def test_pylint3(self, mock_check, _mock_run):
        """Verify the pylint3 builtin hook."""
        self._test_file_filter(mock_check, rh.hooks.check_pylint3,
                               ('foo.py',))

    def test_rustfmt(self, mock_check, _mock_run):
        # First call should do nothing as there are no files to check.
        ret = rh.hooks.check_rustfmt(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertEqual(ret, None)
        self.assertFalse(mock_check.called)

        # Second call will have some results.
        diff = [rh.git.RawDiffEntry(file='lib.rs')]
        ret = rh.hooks.check_rustfmt(
            self.project, 'commit', 'desc', diff, options=self.options)
        self.assertNotEqual(ret, None)

    def test_xmllint(self, mock_check, _mock_run):
        """Verify the xmllint builtin hook."""
        self._test_file_filter(mock_check, rh.hooks.check_xmllint,
                               ('foo.xml',))

    def test_android_test_mapping_format(self, mock_check, _mock_run):
        """Verify the android_test_mapping_format builtin hook."""
        # First call should do nothing as there are no files to check.
        ret = rh.hooks.check_android_test_mapping(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertIsNone(ret)
        self.assertFalse(mock_check.called)

        # Second call will have some results.
        diff = [rh.git.RawDiffEntry(file='TEST_MAPPING')]
        ret = rh.hooks.check_android_test_mapping(
            self.project, 'commit', 'desc', diff, options=self.options)
        self.assertIsNotNone(ret)

    def test_aidl_format(self, mock_check, _mock_run):
        """Verify the aidl_format builtin hook."""
        # First call should do nothing as there are no files to check.
        ret = rh.hooks.check_aidl_format(
            self.project, 'commit', 'desc', (), options=self.options)
        self.assertIsNone(ret)
        self.assertFalse(mock_check.called)

        # Second call will have some results.
        diff = [rh.git.RawDiffEntry(file='IFoo.go')]
        ret = rh.hooks.check_gofmt(
            self.project, 'commit', 'desc', diff, options=self.options)
        self.assertIsNotNone(ret)


if __name__ == '__main__':
    unittest.main()
