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
#

import logging

from vts.utils.python.common import cmd_utils


def GetGsutilPath():
    """Finds gsutil in PATH.

    Instead of a Python library, gsutil binary is used to avoid packaging GCS
    PIP package as part of VTS HC (Host Controller).

    Returns:
        The gsutil file path if found; None otherwise.
    """
    sh_stdout, sh_stderr, ret_code = cmd_utils.ExecuteOneShellCommand(
        "which gsutil")
    if ret_code == 0:
        return sh_stdout.strip()
    else:
        logging.fatal("`gsutil` doesn't exist on the host; "
                      "please install Google Cloud SDK before retrying.")
        return None


def IsGcsFile(gsutil_path, url):
    """Checks whether a given path is for a GCS file.

    Args:
        gsutil_path: string, the path of a gsutil binary.
        url: string, the GCS URL. e.g., gs://<bucket>/<file>.

    Returns:
        True if url is a file, False otherwise.
    """
    check_command = "%s stat %s" % (gsutil_path, url)
    _, _, ret_code = cmd_utils.ExecuteOneShellCommand(check_command)
    return ret_code == 0


def Copy(gsutil_path, src_urls, dst_url):
    """Copies files between local file system and GCS.

    Args:
        gsutil_path: string, the path of a gsutil binary.
        src_urls: string, the source paths or GCS URLs separated by spaces.
        dst_url: string, the destination path or GCS URL.

    Returns:
        True if the command succeeded, False otherwise.
    """
    copy_command = "%s cp %s %s" % (gsutil_path, src_urls, dst_url)
    _, _, ret_code = cmd_utils.ExecuteOneShellCommand(copy_command)
    return ret_code == 0


def List(gsutil_path, url):
    """Lists a directory or file on GCS.

    Args:
        gsutil_path: string, the path of a gsutil binary.
        url: string, the GCS URL of the directory or file.

    Returns:
        list of strings, the GCS URLs of the listed files.
    """
    ls_command = "%s ls %s" % (gsutil_path, url)
    stdout, _, ret_code = cmd_utils.ExecuteOneShellCommand(ls_command)
    return stdout.strip("\n").split("\n") if ret_code == 0 else []


def Remove(gsutil_path, url, recursive=False):
    """Removes a directory or file on GCS.

    Args:
        gsutil_path: string, the path of a gsutil binary.
        url: string, the GCS URL of the directory or file.
        recursive: boolean, whether to remove the directory recursively.

    Returns:
        True if the command succeeded, False otherwise.
    """
    if "/" not in url.lstrip("gs://").rstrip("/"):
        logging.error("Cannot remove bucket %s", url)
        return False
    rm_command = "%s rm -f%s %s" % (
        gsutil_path, ("r" if recursive else ""), url)
    ret_code = cmd_utils.ExecuteOneShellCommand(rm_command)
    return ret_code == 0
