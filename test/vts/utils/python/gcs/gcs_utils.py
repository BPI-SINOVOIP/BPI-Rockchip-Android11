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

import logging
import os
import re
import zipfile

from vts.proto import VtsReportMessage_pb2 as ReportMsg
from vts.runners.host import keys
from vts.utils.python.common import cmd_utils
from vts.utils.python.web import feature_utils


class GcsUtils(feature_utils.Feature):
    """GCS (Google Cloud Storage) utility provider.

    Attributes:
        _TOGGLE_PARAM: String, the name of the parameter used to toggle the feature
        _REQUIRED_PARAMS: list, the list of parameter names that are required
        _OPTIONAL_PARAMS: list, the list of parameter names that are optional
    """

    _TOGGLE_PARAM = None
    _REQUIRED_PARAMS = [keys.ConfigKeys.IKEY_SERVICE_JSON_PATH]
    _OPTIONAL_PARAMS = []

    def __init__(self, user_params):
        """Initializes the gcs util provider.

        Args:
            user_params: A dictionary from parameter name (String) to parameter value.
        """
        self.ParseParameters(
            toggle_param_name=self._TOGGLE_PARAM,
            required_param_names=self._REQUIRED_PARAMS,
            optional_param_names=self._OPTIONAL_PARAMS,
            user_params=user_params)

    def GetGcloudAuth(self):
        """Connects to a service account with access to the gcloud bucket."""
        gcloud_path = GcsUtils.GetGcloudPath()
        gcloud_key = getattr(self, keys.ConfigKeys.IKEY_SERVICE_JSON_PATH)
        if gcloud_path is not None:
            auth_cmd = "%s auth activate-service-account --key-file %s" % (
                gcloud_path, gcloud_key)
            _, stderr, ret_code = cmd_utils.ExecuteOneShellCommand(auth_cmd)
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

    @staticmethod
    def GetGsutilPath():
        """Returns the gsutil file path if found; None otherwise."""
        sh_stdout, sh_stderr, ret_code = cmd_utils.ExecuteOneShellCommand(
            "which gsutil")
        if ret_code == 0:
            return sh_stdout.strip()
        else:
            logging.error("`gsutil` doesn't exist on the host; "
                          "please install Google Cloud SDK before retrying.")
            return None
