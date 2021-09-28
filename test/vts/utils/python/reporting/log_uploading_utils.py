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

import logging
import os

from vts.proto import VtsReportMessage_pb2 as ReportMsg
from vts.runners.host import keys
from vts.utils.python.reporting import report_file_utils
from vts.utils.python.systrace import systrace_controller
from vts.utils.python.web import feature_utils


class LogUploadingFeature(feature_utils.Feature):
    """Feature object for log uploading functionality.

    Attributes:
        enabled: boolean, True if log uploading is enabled, False otherwise.
        web: (optional) WebFeature, object storing web feature util for test run.
        _report_file_util: report file util object for uploading logs to distant.
        _report_file_util_gcs: report file util object for uploading logs to gcs.
    """

    _TOGGLE_PARAM = keys.ConfigKeys.IKEY_ENABLE_LOG_UPLOADING
    _REQUIRED_PARAMS = [
        keys.ConfigKeys.IKEY_ANDROID_DEVICE,
    ]
    _OPTIONAL_PARAMS = [
        keys.ConfigKeys.KEY_TESTBED_NAME,
        keys.ConfigKeys.IKEY_LOG_UPLOADING_USE_DATE_DIRECTORY,
        keys.ConfigKeys.IKEY_LOG_UPLOADING_PATH,
        keys.ConfigKeys.IKEY_LOG_UPLOADING_URL_PREFIX,
        keys.ConfigKeys.IKEY_LOG_UPLOADING_GCS_BUCKET_NAME,
        keys.ConfigKeys.IKEY_SERVICE_JSON_PATH
    ]

    def __init__(self, user_params, web=None):
        """Initializes the log uploading feature.

        Args:
            user_params: A dictionary from parameter name (String) to parameter value.
            web: (optional) WebFeature, object storing web feature util for test run
        """
        self.ParseParameters(
            toggle_param_name=self._TOGGLE_PARAM,
            required_param_names=self._REQUIRED_PARAMS,
            optional_param_names=self._OPTIONAL_PARAMS,
            user_params=user_params)
        self.web = web

        if self.enabled:
            logging.info("Log uploading is enabled")
        else:
            logging.debug("Log uploading is disabled")
            return

        if all(hasattr(self, attr) for attr in [
                    keys.ConfigKeys.IKEY_LOG_UPLOADING_GCS_BUCKET_NAME,
                    keys.ConfigKeys.IKEY_SERVICE_JSON_PATH]):
            gcs_bucket_name = getattr(
                self, keys.ConfigKeys.IKEY_LOG_UPLOADING_GCS_BUCKET_NAME)
            gcs_url_prefix = "https://storage.cloud.google.com/" + gcs_bucket_name + "/"
            self._report_file_util_gcs = report_file_utils.ReportFileUtil(
                flatten_source_dir=True,
                use_destination_date_dir=getattr(
                    self,
                    keys.ConfigKeys.IKEY_LOG_UPLOADING_USE_DATE_DIRECTORY,
                    True),
                destination_dir=gcs_bucket_name,
                url_prefix=gcs_url_prefix,
                gcs_key_path=getattr(self,
                                     keys.ConfigKeys.IKEY_SERVICE_JSON_PATH))

        if hasattr(self, keys.ConfigKeys.IKEY_LOG_UPLOADING_PATH):
            self._report_file_util = report_file_utils.ReportFileUtil(
                flatten_source_dir=True,
                use_destination_date_dir=getattr(
                    self,
                    keys.ConfigKeys.IKEY_LOG_UPLOADING_USE_DATE_DIRECTORY,
                    True),
                destination_dir=getattr(
                    self, keys.ConfigKeys.IKEY_LOG_UPLOADING_PATH),
                url_prefix=getattr(
                    self, keys.ConfigKeys.IKEY_LOG_UPLOADING_URL_PREFIX, None))

    def UploadLogs(self, prefix=None, dryrun=False):
        """Save test logs and add log urls to report message.

        Args:
            prefix: string, file name prefix. Will be auto-generated if not provided.
            dryrun: bool, whether to perform a dry run that attaches
                    destination URLs to result message only. if True,
                    log files will not be actually uploaded.
        """
        if not self.enabled:
            return

        if not prefix:
            prefix = '%s_%s_' % (
                getattr(self, keys.ConfigKeys.KEY_TESTBED_NAME, ''),
                self.web.report_msg.start_timestamp)

        def path_filter(path):
            '''filter to exclude proto files in log uploading'''
            return not path.endswith('_proto.msg')

        for object_name in ['_report_file_util_gcs', '_report_file_util']:
            if hasattr(self, object_name):
                report_file_util = getattr(self, object_name)
                urls = report_file_util.SaveReportsFromDirectory(
                    source_dir=logging.log_path,
                    file_name_prefix=prefix,
                    file_path_filters=path_filter,
                    dryrun=dryrun)
                if urls is None:
                    logging.error('Error happened when saving logs.')
                elif self.web and self.web.enabled:
                    self.web.AddLogUrls(urls)