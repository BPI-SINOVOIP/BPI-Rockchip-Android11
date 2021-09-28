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
"""Functions to process archive files."""

import os
import tempfile
import tarfile
import urllib.parse
import zipfile


class ZipFileWithPermission(zipfile.ZipFile):
    """Subclassing Zipfile to preserve file permission.

    See https://bugs.python.org/issue15795
    """

    def _extract_member(self, member, targetpath, pwd):
        ret_val = super()._extract_member(member, targetpath, pwd)

        if not isinstance(member, zipfile.ZipInfo):
            member = self.getinfo(member)
        attr = member.external_attr >> 16
        if attr != 0:
            os.chmod(ret_val, attr)
        return ret_val


def unzip(archive_path, target_path):
    """Extracts zip file to a path.

    Args:
        archive_path: Path to the zip file.
        target_path: Path to extract files to.
    """

    with ZipFileWithPermission(archive_path) as zfile:
        zfile.extractall(target_path)


def untar(archive_path, target_path):
    """Extracts tar file to a path.

    Args:
        archive_path: Path to the tar file.
        target_path: Path to extract files to.
    """

    with tarfile.open(archive_path, mode='r') as tfile:
        tfile.extractall(target_path)


ARCHIVE_TYPES = {
    '.zip': unzip,
    '.tar.gz': untar,
    '.tar.bz2': untar,
    '.tar.xz': untar,
}


def is_supported_archive(url):
    """Checks whether the url points to a supported archive."""
    return get_extract_func(url) is not None


def get_extract_func(url):
    """Gets the function to extract an archive.

    Args:
        url: The url to the archive file.

    Returns:
        A function to extract the archive. None if not found.
    """

    parsed_url = urllib.parse.urlparse(url)
    filename = os.path.basename(parsed_url.path)
    for ext, func in ARCHIVE_TYPES.items():
        if filename.endswith(ext):
            return func
    return None


def download_and_extract(url):
    """Downloads and extracts an archive file to a temporary directory.

    Args:
        url: Url to download.

    Returns:
        Path to the temporary directory.
    """

    print('Downloading {}'.format(url))
    archive_file, _headers = urllib.request.urlretrieve(url)

    temporary_dir = tempfile.mkdtemp()
    print('Extracting {} to {}'.format(archive_file, temporary_dir))
    get_extract_func(url)(archive_file, temporary_dir)

    return temporary_dir


def find_archive_root(path):
    """Finds the real root of an extracted archive.

    Sometimes archives has additional layers of directories. This function tries
    to guess the right 'root' path by entering all single sub-directories.

    Args:
        path: Path to the extracted archive.

    Returns:
        The root path we found.
    """
    for root, dirs, files in os.walk(path):
        if files or len(dirs) > 1:
            return root
    return path
