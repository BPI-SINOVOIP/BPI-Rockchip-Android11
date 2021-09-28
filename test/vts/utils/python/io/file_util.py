#
# Copyright (C) 2017 The Android Open Source Project
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
#

import logging
import os
import shutil
import tempfile


def FindFile(directory, filename):
    '''Find a file under directory given the filename.

    Args:
        directory: string, directory path
        filename: string, file name to find

    Returns:
        String, path to the file found. None if not found.
    '''
    for (dirpath, dirnames, filenames) in os.walk(
            directory, followlinks=False):
        for fn in filenames:
            if fn == filename:
                return os.path.join(dirpath, filename)

    return None


def Rmdirs(path, ignore_errors=False):
    '''Remove the given directory and its contents recursively.

    Args:
        path: string, directory to delete
        ignore_errors: bool, whether to ignore errors. Defaults to False

    Returns:
        bool, True if directory is deleted.
              False if errors occur or directory does not exist.
    '''
    return_value = False
    if os.path.exists(path):
        try:
            shutil.rmtree(path, ignore_errors=ignore_errors)
            return_value = True
        except OSError as e:
            logging.exception(e)
    return return_value


def Mkdir(path, skip_if_exists=True):
    """Make a leaf directory.

    This method only makes the leaf directory. This means if the parent directory
    doesn't exist, the method will catch an OSError and return False.

    Args:
        path: string, directory to make
        skip_if_exists: bool, True for ignoring exisitng dir. False for throwing
                        error from os.mkdir. Defaults to True

    Returns:
        bool, True if directory is created or directory already exists
              (with skip_if_exists being True).
              False if errors occur or directory already exists (with skip_if_exists being False).
    """
    if skip_if_exists and os.path.exists(path):
        return True

    try:
        os.mkdir(path)
        return True
    except OSError as e:
        logging.exception(e)
        return False


def Makedirs(path, skip_if_exists=True):
    '''Make directories lead to the given path.

    This method makes all parent directories if they don't exist.

    Args:
        path: string, directory to make
        skip_if_exists: bool, True for ignoring exisitng dir. False for throwing
                        error from os.makedirs. Defaults to True

    Returns:
        bool, True if directory is created or directory already exists
              (with skip_if_exists being True).
              False if errors occur or directory already exists (with skip_if_exists being False).
    '''
    if skip_if_exists and os.path.exists(path):
        return True

    try:
        os.makedirs(path)
        return True
    except OSError as e:
        logging.exception(e)
        return False


def MakeTempDir(base_dir):
    """Make a temp directory based on the given path and return its path.

    Args:
        base_dir: string, base directory to make temp directory.

    Returns:
        string, relative path to created temp directory
    """
    Makedirs(base_dir)
    return tempfile.mkdtemp(dir=base_dir)