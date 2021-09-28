#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
import mock
import unittest

from acts.libs.uicd.uicd_cli import UicdCli
from acts.libs.uicd.uicd_cli import UicdError

_MOCK_WALK = {'/dir1': [('/dir1', (), ('file1', 'file2'))],
              '/dir2': [('/dir2', ('dir3',), ('file3',)),
                        ('/dir2/dir3', (), ())],
              '/dir3': [('/dir3', (), ('file1',))]}


def _mock_walk(path, **_):
    return _MOCK_WALK.get(path, [])


class UicdCliTest(unittest.TestCase):
    """Tests the acts.libs.uicd.uicd_cli.UicdCli class."""

    # _set_workflows

    @mock.patch('os.walk', _mock_walk)
    @mock.patch('os.makedirs')
    @mock.patch('tempfile.mkdtemp')
    @mock.patch('shutil.rmtree')
    @mock.patch.object(UicdCli, '_setup_cli')
    def test_set_workflows_sets_correct_file_path(self, *_):
        """Tests that the workflow name is mapped correctly to its path."""
        nc = UicdCli('', '/dir1')
        self.assertIn('file1', nc._workflows,
                      'Workflow file not added to dictionary.')
        self.assertEqual(nc._workflows['file1'], '/dir1/file1',
                         'Workflow name does not point to the correct path.')

    @mock.patch('os.walk', _mock_walk)
    @mock.patch('os.makedirs')
    @mock.patch('tempfile.mkdtemp')
    @mock.patch('shutil.rmtree')
    @mock.patch.object(UicdCli, '_setup_cli')
    def test_set_workflows_adds_workflows_from_directories(self, *_):
        """Tests that providing a directory name adds all files from that
        directory. Also tests that no directories are added to the dictionary.
        """
        nc = UicdCli('', ['/dir1', '/dir2'])
        for file_name in ['file1', 'file2', 'file3']:
            self.assertIn(file_name, nc._workflows,
                          'Workflow file not added to dictionary.')
        for dir_name in ['dir1', 'dir2', 'dir3']:
            self.assertNotIn(dir_name, nc._workflows,
                             'Directories should not be added to dictionary.')

    @mock.patch('os.walk', _mock_walk)
    @mock.patch('os.makedirs')
    @mock.patch('tempfile.mkdtemp')
    @mock.patch('shutil.rmtree')
    @mock.patch.object(UicdCli, '_setup_cli')
    def test_set_workflows_rejects_duplicate_workflow_names(self, *_):
        """Tests that _set_workflows raises an exception if two or more
        workflows of the same name are provided.
        """
        expected_msg = 'Uicd workflows may not share the same name.'
        with self.assertRaisesRegex(UicdError, expected_msg):
            nc = UicdCli('', ['/dir1', '/dir3'])

    # run

    @mock.patch('os.makedirs')
    @mock.patch('tempfile.mkdtemp', return_value='/base')
    @mock.patch('shutil.rmtree')
    @mock.patch.object(UicdCli, '_setup_cli')
    @mock.patch.object(UicdCli, '_set_workflows')
    def test_run_generates_correct_uicd_cmds(self, *_):
        """Tests that the correct cmds are generated upon calling run()."""
        nc = UicdCli('', [])
        nc._workflows = {'test': '/workflows/test'}
        # No log path set
        with mock.patch('acts.libs.proc.job.run') as job_run:
            nc.run('SERIAL', 'test')
            expected_cmd = 'java -jar /base/uicd-commandline.jar ' \
                           '-d SERIAL -i /workflows/test'
            job_run.assert_called_with(expected_cmd.split(), timeout=120)
        # Log path set
        nc._log_path = '/logs'
        with mock.patch('acts.libs.proc.job.run') as job_run:
            nc.run('SERIAL', 'test')
            expected_cmd = 'java -jar /base/uicd-commandline.jar ' \
                           '-d SERIAL -i /workflows/test -o /logs'
            job_run.assert_called_with(expected_cmd.split(), timeout=120)


if __name__ == '__main__':
    unittest.main()
