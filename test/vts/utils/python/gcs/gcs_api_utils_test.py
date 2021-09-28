#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import unittest

try:
    from unittest import mock
except ImportError:
    # TODO: Remove when we stop supporting Python 2
    import mock

from google.auth import exceptions as auth_exceptions

from vts.utils.python.gcs import gcs_api_utils


def simple_ListFilesWithPrefix(dir_path):
    return [
        '%s/file1' % dir_path,
        '%s/file2' % dir_path,
        '%s/file3' % dir_path,
        '%s/file4' % dir_path
    ]


def simple_DownloadFile(src_file_path, dest_file_path):
    return None


def simple_UploadFile(src_file_path, dest_file_path):
    return None


def simple_PrefixExists(dir_path):
    if dir_path is 'valid_source_dir':
        return True
    else:
        return False


def simple_os_path_exists(path):
    return True


def simple__init__(key_path, bucket_name):
    return None


def simple_PrepareDownloadDestination(src_dir, dest_dir):
    return os.path.join(dest_dir, os.path.basename(src_dir))


class GcsApiUtilsTest(unittest.TestCase):
    """Unit tests for gcs_utils module."""

    def SetUp(self):
        """Setup tasks."""
        self.category = "category_default"
        self.name = "name_default"

    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.ListFilesWithPrefix',
        side_effect=simple_ListFilesWithPrefix)
    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.__init__',
        side_effect=simple__init__)
    def testCountFiles(self, simple__init__, simple_ListFilesWithPrefix):
        """Tests the CountFiles function."""
        _gcs_api_utils = gcs_api_utils.GcsApiUtils('key/path', 'vts-fuzz')
        length = _gcs_api_utils.CountFiles('corpus/ILight/ILight_corpus_seed')
        simple_ListFilesWithPrefix.assert_called()
        self.assertEqual(length, 4)

    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.ListFilesWithPrefix',
        side_effect=simple_ListFilesWithPrefix)
    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.__init__',
        side_effect=simple__init__)
    def testPrefixExists(self, simple__init__, simple_ListFilesWithPrefix):
        """Tests the PrefixExists function."""
        _gcs_api_utils = gcs_api_utils.GcsApiUtils('key/path', 'vts-fuzz')
        dir_exist = _gcs_api_utils.PrefixExists(
            'corpus/ILight/ILight_corpus_seed')
        simple_ListFilesWithPrefix.assert_called()
        self.assertEqual(dir_exist, True)

    @mock.patch('os.path.exists', side_effect=simple_os_path_exists)
    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.__init__',
        side_effect=simple__init__)
    def testPrepareDownloadDestination(self, simple__init__,
                                       simple_os_path_exists):
        """Tests the PrepareDownloadDestination function."""
        _gcs_api_utils = gcs_api_utils.GcsApiUtils('key/path', 'vts-fuzz')
        local_dest_folder = _gcs_api_utils.PrepareDownloadDestination(
            'corpus/ILight/ILight_corpus_seed', 'tmp/tmp4772')
        self.assertEqual(local_dest_folder, 'tmp/tmp4772/ILight_corpus_seed')

    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.DownloadFile',
        side_effect=simple_DownloadFile)
    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.PrefixExists',
        side_effect=simple_PrefixExists)
    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.ListFilesWithPrefix',
        side_effect=simple_ListFilesWithPrefix)
    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.PrepareDownloadDestination',
        side_effect=simple_PrepareDownloadDestination)
    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.__init__',
        side_effect=simple__init__)
    def testDownloadDir(self, simple__init__,
                        simple_PrepareDownloadDestination,
                        simple_ListFilesWithPrefix, simple_PrefixExists,
                        simple_DownloadFile):
        """Tests the DownloadDir function"""
        _gcs_api_utils = gcs_api_utils.GcsApiUtils('key/path', 'vts-fuzz')
        _gcs_api_utils.DownloadDir('valid_source_dir',
                                   'local_destination/dest')
        num_DownloadFile_called = simple_DownloadFile.call_count
        self.assertEqual(num_DownloadFile_called, 4)
        local_dest_folder = simple_PrepareDownloadDestination.return_value

    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.UploadFile',
        side_effect=simple_UploadFile)
    @mock.patch('os.path.exists', side_effect=simple_os_path_exists)
    @mock.patch('os.listdir', side_effect=simple_ListFilesWithPrefix)
    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.GcsApiUtils.__init__',
        side_effect=simple__init__)
    def testUploadDir(self, simple__init__, simple_ListFilesWithPrefix,
                      simple_os_path_exists, simple_UploadFile):
        _gcs_api_utils = gcs_api_utils.GcsApiUtils('key/path', 'vts-fuzz')
        _gcs_api_utils.UploadDir('valid_source_dir', 'GCS_destination/dest')
        num_UploadFile_called = simple_UploadFile.call_count
        self.assertEqual(num_UploadFile_called, 4)

    @mock.patch(
        'vts.utils.python.gcs.gcs_api_utils.google.auth.default',
        side_effect=auth_exceptions.DefaultCredentialsError('unit test'))
    def testCredentialsError(self, mock_default):
        """Tests authentication failure in __init__."""
        _gcs_api_utils = gcs_api_utils.GcsApiUtils('key/path', 'vts-fuzz')
        self.assertFalse(_gcs_api_utils.Enabled)
        mock_default.assert_called()


if __name__ == "__main__":
    unittest.main()
