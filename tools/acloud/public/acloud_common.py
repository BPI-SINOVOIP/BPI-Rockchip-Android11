#!/usr/bin/env python
#
# Copyright 2016 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Common code used by both acloud and acloud_kernel tools."""

import argparse


def AddCommonArguments(parser):
    """Adds arguments common to parsers.

    Args:
        parser: ArgumentParser object, used to parse flags.
    """
    parser.add_argument("--email",
                        type=str,
                        dest="email",
                        help="Email account to use for authentcation.")
    parser.add_argument("--config-file",
                        type=str,
                        dest="config_file",
                        default=None,
                        help="Path to the config file, default to "
                        "acloud.config in the current working directory.")
    parser.add_argument("--service-account-json-private-key-path",
                        type=str,
                        dest="service_account_json_private_key_path",
                        help="Path to service account's json private key "
                        "file.")
    parser.add_argument("--report-file",
                        type=str,
                        dest="report_file",
                        default=None,
                        help="Dump the report this file in json format. "
                        "If not specified, just log the report.")
    parser.add_argument("--log-file",
                        dest="log_file",
                        type=str,
                        default=None,
                        help="Path to log file.")
    parser.add_argument('--verbose', '-v',
                        action='count',
                        default=0,
                        help="Enable verbose log. Use --verbose or -v for "
                        "logging at INFO level, and -vv for DEBUG level.")
    parser.add_argument("--no-metrics",
                        action="store_true",
                        dest="no_metrics",
                        required=False,
                        default=False,
                        help="Don't log metrics.")

    # Allow for using the underscore args as well to keep it backward
    # compatible with the infra use case. Remove when g3 acloud is
    # deprecated (b/118439885).
    parser.add_argument("--config_file",
                        type=str,
                        dest="config_file",
                        default=None,
                        help=argparse.SUPPRESS)
    parser.add_argument("--service_account_json_private_key_path",
                        type=str,
                        dest="service_account_json_private_key_path",
                        help=argparse.SUPPRESS)
    parser.add_argument("--report_file",
                        type=str,
                        dest="report_file",
                        default=None,
                        help=argparse.SUPPRESS)
    parser.add_argument("--log_file",
                        dest="log_file",
                        type=str,
                        default=None,
                        help=argparse.SUPPRESS)
