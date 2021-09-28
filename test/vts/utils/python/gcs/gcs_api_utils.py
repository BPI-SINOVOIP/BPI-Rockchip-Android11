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

import google.auth
import logging
import os

from google.cloud import exceptions
from google.cloud import storage

# OS environment variable name for google application credentials.
_GOOGLE_CRED_ENV_VAR = 'GOOGLE_APPLICATION_CREDENTIALS'
# URL to the Google Cloud storage authentication.
_READ_WRITE_SCOPE_URL = 'https://www.googleapis.com/auth/devstorage.read_write'


class GcsApiUtils(object):
    """GCS (Google Cloud Storage) API utility provider.

    Attributes:
        _key_path: string, path to the JSON key file of the service account.
        _bucket_name: string, Google Cloud Storage bucket name.
        _credentials: credentials object for the service account.
        _project: string, Google Cloud project name of the service account.
        _enabled: boolean, whether this GcsApiUtils object is enabled.
    """

    def __init__(self, key_path, bucket_name):
        self._key_path = key_path
        self._bucket_name = bucket_name
        os.environ[_GOOGLE_CRED_ENV_VAR] = key_path
        self._enabled = True
        try:
            self._credentials, self._project = google.auth.default()
            if self._credentials.requires_scopes:
                self._credentials = self._credentials.with_scopes(
                    [_READ_WRITE_SCOPE_URL])
        except google.auth.exceptions.DefaultCredentialsError as e:
            logging.exception(e)
            self._enabled = False

    @property
    def Enabled(self):
        """Gets private variable _enabled.

        Returns:
            self._enabled: boolean, whether this GcsApiUtils object is enabled.
        """
        return self._enabled

    @Enabled.setter
    def Enabled(self, enabled):
        """Sets private variable _enabled."""
        self._enabled = enabled

    def ListFilesWithPrefix(self, dir_path, strict=True):
        """Returns a list of files under a given GCS prefix.

        GCS uses prefixes to resemble the concept of directories.

        For instance, if we have a directory called 'corpus,'
        then we have a file named corpus.

        Then we can have files like 'corpus/ILight/ILight_corpus_seed/132,'
        which may appear that the file named '132' is inside the directory
        ILight_corpus_seed, whose parent directory is ILight, whose parent
        directory is corpus.

        However, we only have 1 explicit file that resembles a directory
        role here: 'corpus.' We do not have directories 'corpus/ILight' or
        'corpus/ILight/ILight_corpus.'

        Here, we have only 2 files:
        'corpus/'
        'corpus/ILight/ILight_corpus_seed/132'

        Given the two prefixes (directories),
            corpus/ILight/ILight_corpus_seed
            corpus/ILight/ILight_corpus_seed_01

        ListFilesWithPrefix(corpus/ILight/ILight_corpus_seed, strict=True)
        will only list files in corpus/ILight/ILight_corpus_seed,
        not in corpus/ILight/ILight_corpus_seed_01.

        ListFilesWithPrefix(corpus/ILight/ILight_corpus_seed, strict=False)
        will list files in both corpus/ILight/ILight_corpus_seed,
        and corpus/ILight/ILight_corpus_seed_01.

        Args:
            dir_path: path to the GCS directory of interest.

        Returns:
            a list of absolute path filenames of the content of the given GCS directory.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return []

        if strict and not dir_path.endswith('/'):
            dir_path += '/'
        client = storage.Client(credentials=self._credentials)
        bucket = client.get_bucket(self._bucket_name)
        dir_list = list(bucket.list_blobs(prefix=dir_path))
        return [file.name for file in dir_list]

    def CountFiles(self, dir_path):
        """Counts the number of files under a given GCS prefix.

        Args:
            dir_path: path to the GCS prefix of interest.

        Returns:
            number of files, if files exist under the prefix.
            0, if prefix doesnt exist.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return 0

        return len(self.ListFilesWithPrefix(dir_path))

    def PrefixExists(self, dir_path):
        """Checks if a file containing the prefix exists in the GCS bucket.

        This is effectively "counting" the number of files
        inside the directory. Depending on whether the prefix/directory
        file exist or not, this function may return the number of files
        in the diretory or the number + 1 (the prefix/directory file).

        Returns:
            True, if such prefix exists in the GCS bucket.
            False, otherwise.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return False

        return self.CountFiles(dir_path) is not 0

    def FileExists(self, file_path):
        """Checks if a file exists in the GCS bucket.

        Returns:
            True, if the specific file exists in the GCS bucket.
            False, otherwise.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return False

        client = storage.Client(credentials=self._credentials)
        bucket = client.get_bucket(self._bucket_name)
        blob = bucket.blob(file_path)
        return blob.exists()

    def DownloadFile(self, src_file_path, dest_file_path):
        """Downloads a file to a local destination directory.

        Args:
            src_file_path: source file path, directory/filename in GCS.
            dest_file_path: destination file path, directory/filename in local.

        Raises:
            exception when the source file does not exist in GCS.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return

        client = storage.Client(credentials=self._credentials)
        bucket = client.get_bucket(self._bucket_name)
        blob = bucket.blob(src_file_path)
        blob.download_to_filename(dest_file_path)
        logging.info('File %s downloaded to %s.', src_file_path,
                     dest_file_path)

    def PrepareDownloadDestination(self, src_dir, dest_dir):
        """Makes prerequisite directories in the local destination.

        Args:
            src_dir: source directory, in GCS.
            dest_dir: destination directory, in local.

        Returns:
            local_dest_folder, path to the local folder created (or had already existed).
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return

        local_dest_folder = os.path.join(dest_dir, os.path.basename(src_dir))
        if not os.path.exists(local_dest_folder):
            os.makedirs(local_dest_folder)
        return local_dest_folder

    def DownloadDir(self, src_dir, dest_dir):
        """Downloads a GCS src directory to a local dest dir.

        Args:
            src_dir: source directory, directory in GCS.
            dest_dir: destination directory, directory in local.

        Raises:
            exception when a source file does not exist in GCS.

        Returns:
            True, if the source directory exists and files successfully downloaded.
            False, if the source directory does not exist.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return False

        if self.PrefixExists(src_dir):
            logging.info('successfully found the GCS directory.')
            self.PrepareDownloadDestination(src_dir, dest_dir)
            filelist = self.ListFilesWithPrefix(src_dir)
            for src_file_path in filelist:
                dest_file_path = os.path.join(
                    dest_dir,
                    os.path.join(
                        os.path.basename(src_dir),
                        os.path.basename(src_file_path)))
                try:
                    self.DownloadFile(src_file_path, dest_file_path)
                except exceptions.NotFound as e:
                    logging.error('download failed for file: %s',
                                  src_file_path)
            return True
        else:
            logging.error('requested GCS directory does not exist.')
            return False

    def UploadFile(self, src_file_path, dest_file_path):
        """Uploads a file to a GCS bucket.

        Args:
            src_file_path: source file path, directory/filename in local.
            dest_file_path: destination file path, directory/filename in GCS.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return

        client = storage.Client(credentials=self._credentials)
        bucket = client.get_bucket(self._bucket_name)
        blob = bucket.blob(dest_file_path)
        blob.upload_from_filename(src_file_path)
        logging.info('File %s uploaded to %s.', src_file_path, dest_file_path)

    def UploadDir(self, src_dir, dest_dir):
        """Uploads a local src dir to a GCS dest dir.

        Args:
           src_dir: source directory, directory in local.
           dest_dir: destination directory, directory in GCS.

        Returns:
            True, if the source directory exists and files successfully uploaded.
            False, if the source directory does not exist.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return False

        if os.path.exists(src_dir):
            logging.info('successfully found the local directory.')
            src_basedir = os.path.basename(src_dir)
            for dirpath, _, filenames in os.walk(src_dir):
                for filename in filenames:
                    src_file_path = os.path.join(dirpath, filename)
                    dest_file_path = os.path.join(
                        dest_dir, src_file_path.replace(src_dir, src_basedir))
                    self.UploadFile(src_file_path, dest_file_path)
            return True
        else:
            logging.error('requested local directory does not exist.')
            return False

    def MoveFile(self, src_file_path, dest_file_path, log_error=True):
        """Renames a blob, which effectively changes its path.

        Args:
            src_file_path: source file path in GCS.
            dest_dest_path: destination file path in GCS.

        Returns:
            True if susccessful, False otherwise.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return False

        client = storage.Client(credentials=self._credentials)
        bucket = client.get_bucket(self._bucket_name)
        blob = bucket.blob(src_file_path)
        try:
            new_blob = bucket.rename_blob(blob, dest_file_path)
        except exceptions.NotFound as e:
            if log_error:
                logging.exception('file move was unsuccessful with error %s.',
                                  e)
            return False
        return True

    def DeleteFile(self, file_path):
        """Deletes a blob, which effectively deletes its corresponding file.

        Args:
            file_path: string, path to the file to remove.

        Returns:
            True if successful, False otherwise.
        """
        if not self._enabled:
            logging.error('This GcsApiUtils object is not enabled.')
            return False

        client = storage.Client(credentials=self._credentials)
        bucket = client.get_bucket(self._bucket_name)
        blob = bucket.blob(file_path)
        try:
            blob.delete()
        except exceptions.NotFound as e:
            logging.exception('file delete was unsuccessful with error %s.', e)
            return False
        return True
