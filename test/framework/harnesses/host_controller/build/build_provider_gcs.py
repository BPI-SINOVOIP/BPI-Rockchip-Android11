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
import re
import zipfile

from host_controller.build import build_provider
from host_controller.utils.gcp import gcs_utils
from vts.utils.python.common import cmd_utils

_GCLOUD_AUTH_ENV_KEY = "run_gcs_key"


class BuildProviderGCS(build_provider.BuildProvider):
    """A build provider for GCS (Google Cloud Storage)."""

    def __init__(self):
        super(BuildProviderGCS, self).__init__()
        if _GCLOUD_AUTH_ENV_KEY in os.environ:
            gcloud_path = BuildProviderGCS.GetGcloudPath()
            if gcloud_path is not None:
                auth_cmd = "%s auth activate-service-account --key-file=%s" % (
                    gcloud_path, os.environ[_GCLOUD_AUTH_ENV_KEY])
                _, stderr, ret_code = cmd_utils.ExecuteOneShellCommand(
                    auth_cmd)
                if ret_code == 0:
                    logging.info(stderr)
                else:
                    logging.error(stderr)

    @staticmethod
    def GetGcloudPath():
        """Returns the gcloud file path if found; None otherwise."""
        sh_stdout, _, ret_code = cmd_utils.ExecuteOneShellCommand(
            "which gcloud")
        if ret_code == 0:
            return sh_stdout.strip()
        else:
            logging.error("`gcloud` doesn't exist on the host; "
                          "please install Google Cloud SDK before retrying.")
            return None

    def Fetch(self, path, full_device_images=False, set_suite_as=None):
        """Fetches Android device artifact file(s) from GCS.

        Args:
            path: string, the path of a directory which keeps artifacts.
            set_suite_as: string, the test suite name to use for the given
                          artifact. Used when the file name does not follow
                          the standard "android-*ts.zip" file name pattern.

        Returns:
            a dict containing the device image info.
            a dict containing the test suite package info.
            a dict containing the info about custom tool files.
        """
        if not path.startswith("gs://"):
            path = "gs://" + re.sub("^/*", "", path)
        path = re.sub("/*$", "", path)
        gsutil_path = gcs_utils.GetGsutilPath()
        if gsutil_path:
            temp_dir_path = self.CreateNewTmpDir()
            # IsGcsFile returns False if path is directory or doesn't exist.
            # cp command returns non-zero if path doesn't exist.
            if not gcs_utils.IsGcsFile(gsutil_path, path):
                dest_path = temp_dir_path
                if "latest.zip" in path:
                    gsutil_ls_path = re.sub("latest.zip", "*.zip", path)
                    lines_gsutil_ls = gcs_utils.List(gsutil_path, gsutil_ls_path)
                    if lines_gsutil_ls:
                        lines_gsutil_ls.sort()
                        path = lines_gsutil_ls[-1]
                        copy_command = "%s cp %s %s" % (gsutil_path, path,
                                                        temp_dir_path)
                    else:
                        logging.error(
                            "There is no file(s) that matches the URL %s.",
                            path)
                        return (self.GetDeviceImage(),
                                self.GetTestSuitePackage(),
                                self.GetAdditionalFile())
                else:
                    copy_command = "%s cp -r %s/* %s" % (gsutil_path, path,
                                                         temp_dir_path)
            else:
                dest_path = os.path.join(temp_dir_path, os.path.basename(path))
                copy_command = "%s cp %s %s" % (gsutil_path, path,
                                                temp_dir_path)

            _, _, ret_code = cmd_utils.ExecuteOneShellCommand(copy_command)
            if ret_code == 0:
                self.SetFetchedFile(dest_path, temp_dir_path,
                                    full_device_images, set_suite_as)
            else:
                logging.error("Error in copy file from GCS (code %s).",
                              ret_code)
        return (self.GetDeviceImage(), self.GetTestSuitePackage(),
                self.GetAdditionalFile())
