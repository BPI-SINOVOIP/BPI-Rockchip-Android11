# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import unittest
import tempfile

import common
from autotest_lib.utils.side_effects import config_loader
from autotest_lib.utils.side_effects.proto import config_pb2


class LoadTestCase(unittest.TestCase):
    """Test loading side_effects_config.json."""

    def setUp(self):
        self._results_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self._results_dir)

    def test_missing_config_file(self):
        parsed_config = config_loader.load(self._results_dir)
        self.assertIsNone(parsed_config)

    def test_full_config(self):
        config = {
            'tko': {
                'proxy_socket': '/file-system/foo-socket',
                'mysql_user': 'foo-user',
                'mysql_password_file': '/file-system/foo-password-file'
            },
            'google_storage': {
                'bucket': 'foo-bucket',
                'credentials_file': '/file-system/foo-creds'
            },
            'this_field_is_ignored': True
        }
        path = os.path.join(self._results_dir,
                            'side_effects_config.json')
        with open(path, 'w') as f:
            f.write(json.dumps(config))
        parsed_config = config_loader.load(self._results_dir)

        self.assertEqual(parsed_config.tko.proxy_socket,
                         '/file-system/foo-socket')
        self.assertEqual(parsed_config.tko.mysql_user, 'foo-user')
        self.assertEqual(parsed_config.tko.mysql_password_file,
                         '/file-system/foo-password-file')
        self.assertEqual(parsed_config.google_storage.bucket, 'foo-bucket')
        self.assertEqual(parsed_config.google_storage.credentials_file,
                         '/file-system/foo-creds')
        self.assertEqual(parsed_config.chrome_perf.enabled, False)
        self.assertEqual(parsed_config.cts.enabled, False)

    def test_bad_config(self):
        path = os.path.join(self._results_dir,
                            'side_effects_config.json')
        with open(path, 'w') as f:
            f.write('Not a JSON, hence unparseable')

        with self.assertRaisesRegexp(Exception, 'JSON'):
            parsed_config = config_loader.load(self._results_dir)


class ValidateTKOTestCase(unittest.TestCase):
    """Test validating the TKO-related fields of a side_effects.Config proto."""

    def setUp(self):
        self._tempdir = tempfile.mkdtemp()
        self._config = config_pb2.Config(
            tko=config_pb2.TKOConfig(
                proxy_socket=_tempfile(self._tempdir),
                mysql_user='foo-user',
                mysql_password_file=_tempfile(self._tempdir)
            ))

    def tearDown(self):
        shutil.rmtree(self._tempdir)

    def test_complete_config(self):
        config_loader.validate_tko(self._config)

    def test_missing_proxy_socket_path(self):
        self._config.tko.proxy_socket = ''
        with self.assertRaisesRegexp(ValueError, 'proxy socket'):
            config_loader.validate_tko(self._config)

    def test_missing_mysql_user(self):
        self._config.tko.mysql_user = ''
        with self.assertRaisesRegexp(ValueError, 'MySQL user'):
            config_loader.validate_tko(self._config)

    def test_missing_mysql_password_file_path(self):
        self._config.tko.mysql_password_file = ''
        with self.assertRaisesRegexp(ValueError, 'MySQL password'):
            config_loader.validate_tko(self._config)

    def test_missing_proxy_socket_file(self):
        os.remove(self._config.tko.proxy_socket)
        with self.assertRaisesRegexp(OSError, 'proxy socket'):
            config_loader.validate_tko(self._config)

    def test_missing_mysql_password_file(self):
        os.remove(self._config.tko.mysql_password_file)
        with self.assertRaisesRegexp(OSError, 'MySQL password'):
            config_loader.validate_tko(self._config)


class ValidateGoogleStorageTestCase(unittest.TestCase):
    """Test validating the TKO-related fields of a side_effects.Config proto."""

    def setUp(self):
        self._tempdir = tempfile.mkdtemp()
        self._config = config_pb2.Config(
            google_storage=config_pb2.GoogleStorageConfig(
                bucket='foo-bucket',
                credentials_file=_tempfile(self._tempdir)
            ))

    def tearDown(self):
        shutil.rmtree(self._tempdir)

    def test_complete_config(self):
        config_loader.validate_google_storage(self._config)

    def test_missing_bucket(self):
        self._config.google_storage.bucket = ''
        with self.assertRaisesRegexp(ValueError, 'bucket'):
            config_loader.validate_google_storage(self._config)

    def test_missing_credentials_file_path(self):
        self._config.google_storage.credentials_file = ''
        with self.assertRaisesRegexp(ValueError, 'credentials'):
            config_loader.validate_google_storage(self._config)

    def test_missing_credentials_file(self):
        os.remove(self._config.google_storage.credentials_file)
        with self.assertRaisesRegexp(OSError, 'credentials'):
            config_loader.validate_google_storage(self._config)


def _tempfile(tempdir):
    _, name = tempfile.mkstemp(dir=tempdir)
    return name


if __name__ == '__main__':
    unittest.main()
