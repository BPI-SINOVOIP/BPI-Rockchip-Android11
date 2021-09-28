#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import os
import unittest

import mock
from acts.test_utils.instrumentation.device.apps.hotword_model_extractor import \
    HotwordModelExtractor
from acts.test_utils.instrumentation.device.apps.hotword_model_extractor import \
    MODEL_DIR

GOOD_PACKAGE = 'good_package'
GOOD_MODEL = 'good_model'
BAD_PACKAGE = 'bad_package'
BAD_MODEL = 'bad_model'


def mock_pull_from_device(_, hotword_pkg, __):
    """Mocks the AppInstaller.pull_from_device method."""
    return mock.MagicMock() if hotword_pkg == GOOD_PACKAGE else None


class MockZipFile(object):
    """Class for mocking zipfile.ZipFile"""
    def extract(self, path, _):
        if path == os.path.join(MODEL_DIR, GOOD_MODEL):
            return path
        raise KeyError

    def __enter__(self):
        return self

    def __exit__(self, *_):
        pass


@mock.patch('acts.test_utils.instrumentation.device.apps.app_installer.'
            'AppInstaller.pull_from_device', side_effect=mock_pull_from_device)
@mock.patch('zipfile.ZipFile', return_value=MockZipFile())
class HotwordModelExtractorTest(unittest.TestCase):
    """Unit tests for HotwordModelExtractor."""
    def setUp(self):
        self.extractor = HotwordModelExtractor(mock.MagicMock())

    def test_package_not_installed(self, *_):
        result = self.extractor._extract(BAD_PACKAGE, GOOD_MODEL, '')
        self.assertIsNone(result)

    def test_voice_model_not_found(self, *_):
        result = self.extractor._extract(GOOD_PACKAGE, BAD_MODEL, '')
        self.assertIsNone(result)

    def test_extract_model(self, *_):
        result = self.extractor._extract(GOOD_PACKAGE, GOOD_MODEL, '')
        self.assertEqual(result, 'res/raw/good_model')


if __name__ == '__main__':
    unittest.main()
