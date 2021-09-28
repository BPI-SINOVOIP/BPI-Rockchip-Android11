#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
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

import mock
import os
import unittest

from vts.utils.python.fuzzer import corpus_manager

dut = mock.Mock()
dut.build_alias = 'Pie.118.wj12e'
dut.product_type = 'Pixel3_XL'
dut.serial = 'HT1178BBZWQ'


class CorpusManagerTest(unittest.TestCase):
    """Unit tests for corpus_manager module."""

    def SetUp(self):
        """Setup tasks."""
        self.category = "category_default"
        self.name = "name_default"

    def testInitializationDisabled(self):
        """Tests the disabled initilization of a CorpusManager object."""
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        self.assertEqual(_corpus_manager.enabled, False)

    def testInitializationEnabled(self):
        """Tests the enabled initilization of a CorpusManager object.

        If we initially begin with enabled=True, it will automatically
        attempt to connect GCS API.
        """
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        self.assertEqual(_corpus_manager.enabled, True)
        self.assertEqual(_corpus_manager._gcs_path, 'corpus/Pie/Pixel3_XL')
        self.assertEqual(_corpus_manager._device_serial, 'HT1178BBZWQ')

    def testFetchCorpusSeedEmpty_STAGE_1(self):
        """Tests the FetchCorpusSeed function of a CorpusManager object.

        This is tested with an empty seed directory for stage 1 algorithm.
        """
        corpus_manager.SCHEDULING_ALGORITHM = 1
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._FetchCorpusSeedFromPriority = mock.MagicMock()
        _corpus_manager.FetchCorpusSeed('ILight', '/tmp/tmpDir1')
        _corpus_manager._FetchCorpusSeedFromPriority.assert_called_with(
            'ILight', '/tmp/tmpDir1', 'corpus_seed')

    def testFetchCorpusSeedEmpty_STAGE_2(self):
        """Tests the FetchCorpusSeed function of a CorpusManager object.

        This is tested with an empty seed directory for stage 2 algorithm.
        """
        corpus_manager.SCHEDULING_ALGORITHM = 2
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = []
        _corpus_manager.FetchCorpusSeed('ILight', '/tmp/tmpDir1')
        self.assertEquals(
            _corpus_manager._gcs_api_utils.ListFilesWithPrefix.call_count, 3)

    def testFetchCorpusSeedEmpty_STAGE_3_LOCKED(self):
        """Tests the FetchCorpusSeed function of a CorpusManager object.

        This is tested with an empty seed directory for stage 3 algorithm.
        """
        corpus_manager.SCHEDULING_ALGORITHM = 3
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.PrefixExists_return_value = True
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = []
        res = _corpus_manager.FetchCorpusSeed('ILight', '/tmp/tmpDir1')
        self.assertEquals(res, 'locked')

    def testFetchCorpusSeedEmpty_STAGE_3_NOT_LOCKED(self):
        """Tests the FetchCorpusSeed function of a CorpusManager object.

        This is tested with an empty seed directory for stage 3 algorithm.
        """
        corpus_manager.SCHEDULING_ALGORITHM = 3
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.PrefixExists.return_value = False
        _corpus_manager._gcs_api_utils.ContFiles.return_value = 0
        _corpus_manager.add_lock = mock.MagicMock()
        _corpus_manager._FetchCorpusSeedDirectory = mock.MagicMock()
        res = _corpus_manager.FetchCorpusSeed('ILight', '/tmp/tmpDir1')
        self.assertEquals(res, 'directory')

    def testFetchCorpusSeedValid_STAGE_1(self):
        """Tests the FetchCorpusSeed function of a CorpusManager object.

        This is tested with a valid seed directory for stage 1 algorithm.
        """
        corpus_manager.SCHEDULING_ALGORITHM = 1
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = [
            'dir/file1', 'dir/file2', 'dir/file3', 'dir/file4', 'dir/file5'
        ]
        _corpus_manager._gcs_api_utils.MoveFile.return_value = True
        _corpus_manager.FetchCorpusSeed('ILight', '/tmp/tmpDir1')
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_seed')
        _corpus_manager._gcs_api_utils.MoveFile.assert_called()
        _corpus_manager._gcs_api_utils.PrepareDownloadDestination.assert_called(
        )
        _corpus_manager._gcs_api_utils.DownloadFile.assert_called()

    def testFetchCorpusSeedValid_STAGE_2(self):
        """Tests the FetchCorpusSeed function of a CorpusManager object.

        This is tested with a valid seed directory for stage 2 algorithm.
        """
        corpus_manager.SCHEDULING_ALGORITHM = 2
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = [
            'dir/file1', 'dir/file2', 'dir/file3', 'dir/file4', 'dir/file5'
        ]
        _corpus_manager._gcs_api_utils.MoveFile.return_value = True
        _corpus_manager.FetchCorpusSeed('ILight', '/tmp/tmpDir1')
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_seed_high')
        _corpus_manager._gcs_api_utils.MoveFile.assert_called()
        _corpus_manager._gcs_api_utils.PrepareDownloadDestination.assert_called(
        )
        _corpus_manager._gcs_api_utils.DownloadFile.assert_called()

    def testFetchCorpusSeedValid_STAGE_3(self):
        """Tests the FetchCorpusSeed function of a CorpusManager object.

        This is tested with a valid seed directory for stage 3 algorithm.
        """
        corpus_manager.SCHEDULING_ALGORITHM = 3
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = [
            'dir/file1', 'dir/file2', 'dir/file3', 'dir/file4', 'dir/file5'
        ]
        _corpus_manager._gcs_api_utils.PrefixExists.return_value = False
        _corpus_manager.add_lock = mock.MagicMock()
        res = _corpus_manager.FetchCorpusSeed('ILight', '/tmp/tmpDir1')
        self.assertEquals(res, 'directory')
        _corpus_manager._gcs_api_utils.PrepareDownloadDestination.assert_called(
        )
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_seed')
        _corpus_manager._gcs_api_utils.DownloadFile.assert_called()

    def testUploadCorpusOutDir(self):
        """Tests the UploadCorpusOutDir function of a CorpusManager object."""
        corpus_manager.MEASURE_CORPUS = False
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.UploadDir.return_value = True
        _corpus_manager._ClassifyPriority = mock.MagicMock()
        _corpus_manager.UploadCorpusOutDir('ILight', '/tmp/tmpDir1')
        _corpus_manager._gcs_api_utils.UploadDir.assert_called_with(
            '/tmp/tmpDir1/ILight_corpus_out',
            'corpus/Pie/Pixel3_XL/ILight/incoming/tmpDir1')
        _corpus_manager._ClassifyPriority.assert_called_with(
            'ILight', '/tmp/tmpDir1')

    def testInuseToDestSeed(self):
        """Tests the InuseToDest function of a CorpusManager object."""
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()

        _corpus_manager.InuseToDest(
            'ILight',
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_inuse/corpus_number_1',
            'corpus_seed')
        _corpus_manager._gcs_api_utils.MoveFile.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_inuse/corpus_number_1',
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_seed/corpus_number_1',
            True)

        _corpus_manager.InuseToDest(
            'ILight',
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_inuse/corpus_number_1',
            'corpus_complete')
        _corpus_manager._gcs_api_utils.MoveFile.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_inuse/corpus_number_1',
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_complete/corpus_number_1',
            True)

        _corpus_manager.InuseToDest(
            'ILight',
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_inuse/corpus_number_1',
            'corpus_crash')
        _corpus_manager._gcs_api_utils.MoveFile.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_inuse/corpus_number_1',
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_crash/corpus_number_1',
            True)

    def test_ClassifyPriority1(self):
        """Tests the _ClassifyPriority1 function of a CorpusManager object."""
        corpus_manager.SCHEDULING_ALGORITHM = 1
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = [
            'dir/file1', 'dir/file2', 'dir/file3', 'dir/file4', 'dir/file5'
        ]
        _corpus_manager._CorpusIsDuplicate = mock.MagicMock()
        _corpus_manager._CorpusIsDuplicate.return_value = False
        _corpus_manager._ClassifyPriority1('ILight', '/tmp/tmpDir1')
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/incoming/tmpDir1/ILight_corpus_out')
        self.assertEquals(_corpus_manager._gcs_api_utils.MoveFile.call_count,
                          5)

    def test_ClassifyPriority2(self):
        """Tests the _ClassifyPriority2 function of a CorpusManager object."""
        corpus_manager.SCHEDULING_ALGORITHM = 2
        corpus_manager.os.path.exists = mock.MagicMock()
        corpus_manager.os.path.exists.return_value = True
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = [
            'dir/file1', 'dir/file2', 'dir/file3', 'dir/file4', 'dir/file5'
        ]
        _corpus_manager._CorpusIsDuplicate = mock.MagicMock()
        _corpus_manager._CorpusIsDuplicate.return_value = False
        _corpus_manager._ClassifyPriority2('ILight', '/tmp/tmpDir1')
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/incoming/tmpDir1/ILight_corpus_out')
        self.assertEquals(_corpus_manager._gcs_api_utils.MoveFile.call_count,
                          5)

    def test_ClassifyPriority3(self):
        """Tests the _ClassifyPriority3 function of a CorpusManager object."""
        corpus_manager.SCHEDULING_ALGORITHM = 3
        corpus_manager.REPEAT_TIMES = 5
        corpus_manager.os.path.exists = mock.MagicMock()
        corpus_manager.os.path.exists.return_value = True
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = [
            'dir/file1', 'dir/file2', 'dir/file3', 'dir/file4', 'dir/file5'
        ]
        _corpus_manager._gcs_api_utils.PrefixExists.return_value = True
        _corpus_manager._MoveCorpusDirectory = mock.MagicMock()
        _corpus_manager._ClassifyPriority3('ILight', '/tmp/tmpDir1')
        self.assertEquals(_corpus_manager._MoveCorpusDirectory.call_count, 2)
        _corpus_manager._MoveCorpusDirectory.assert_called_with(
            'ILight', '/tmp/tmpDir1', 'incoming_child', 'corpus_complete')

    def test_MoveCorpusDirectory(self):
        """Tests the _MoveCorpusDirectory function of a CorpusManager object."""
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        _corpus_manager.enabled = True
        _corpus_manager._gcs_api_utils = mock.MagicMock()
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.return_value = [
            'dir/file1', 'dir/file2', 'dir/file3', 'dir/file4', 'dir/file5'
        ]
        _corpus_manager._MoveCorpusDirectory('ILight', '/tmp/tmpDir1',
                                             'corpus_seed', 'corpus_complete')
        _corpus_manager._gcs_api_utils.ListFilesWithPrefix.assert_called_with(
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_seed', strict=True)
        self.assertEquals(_corpus_manager._gcs_api_utils.MoveFile.call_count,
                          5)

    def test_GetDirPaths(self):
        """Tests the _GetDirPaths function of a CorpusManager object."""
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        self.assertEqual(
            _corpus_manager._GetDirPaths('corpus_seed', 'ILight'),
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_seed')
        self.assertEqual(
            _corpus_manager._GetDirPaths('incoming_parent', 'ILight',
                                         '/tmp/tmpDir1'),
            'corpus/Pie/Pixel3_XL/ILight/incoming/tmpDir1')
        self.assertEqual(
            _corpus_manager._GetDirPaths('incoming_child', 'ILight',
                                         '/tmp/tmpDir1'),
            'corpus/Pie/Pixel3_XL/ILight/incoming/tmpDir1/ILight_corpus_out')
        self.assertEqual(
            _corpus_manager._GetDirPaths('corpus_seed', 'ILight'),
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_seed')

    def test_GetFilePaths(self):
        """Tests the _GetFilePaths function of a CorpusManager object."""
        _corpus_manager = corpus_manager.CorpusManager({}, dut)
        self.assertEqual(
            _corpus_manager._GetFilePaths('corpus_seed', 'ILight',
                                          'some_dir/corpus_number_1'),
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_seed/corpus_number_1')
        self.assertEqual(
            _corpus_manager._GetFilePaths('corpus_inuse', 'ILight',
                                          'some_dir/corpus_number_1'),
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_inuse/corpus_number_1')
        self.assertEqual(
            _corpus_manager._GetFilePaths('corpus_complete', 'ILight',
                                          'somedir/corpus_number_1'),
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_complete/corpus_number_1'
        )
        self.assertEqual(
            _corpus_manager._GetFilePaths('corpus_crash', 'ILight',
                                          'somedir/corpus_number_1'),
            'corpus/Pie/Pixel3_XL/ILight/ILight_corpus_crash/corpus_number_1')


if __name__ == "__main__":
    unittest.main()
