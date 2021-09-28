#!/usr/bin/env python
#
# Copyright 2018 - The Android Open Source Project
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
"""Common code used by acloud create methods/classes."""

import logging
import os

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import android_build_client
from acloud.internal.lib import auth
from acloud.internal.lib import utils


logger = logging.getLogger(__name__)


def ParseHWPropertyArgs(dict_str, item_separator=",", key_value_separator=":"):
    """Helper function to initialize a dict object from string.

    e.g.
    cpu:2,dpi:240,resolution:1280x800
    -> {"cpu":"2", "dpi":"240", "resolution":"1280x800"}

    Args:
        dict_str: A String to be converted to dict object.
        item_separator: String character to separate items.
        key_value_separator: String character to separate key and value.

    Returns:
        Dict created from key:val pairs in dict_str.

    Raises:
        error.MalformedDictStringError: If dict_str is malformed.
    """
    hw_dict = {}
    if not dict_str:
        return hw_dict

    for item in dict_str.split(item_separator):
        if key_value_separator not in item:
            raise errors.MalformedDictStringError(
                "Expecting ':' in '%s' to make a key-val pair" % item)
        key, value = item.split(key_value_separator)
        if not value or not key:
            raise errors.MalformedDictStringError(
                "Missing key or value in %s, expecting form of 'a:b'" % item)
        hw_dict[key.strip()] = value.strip()

    return hw_dict


def GetCvdHostPackage():
    """Get cvd host package path.

    Look for the host package in $ANDROID_HOST_OUT and dist dir then verify
    existence and get cvd host package path.

    Return:
        A string, the path to the host package.

    Raises:
        errors.GetCvdLocalHostPackageError: Can't find cvd host package.
    """
    dirs_to_check = list(filter(None, [os.environ.get(constants.ENV_ANDROID_HOST_OUT)]))
    dist_dir = utils.GetDistDir()
    if dist_dir:
        dirs_to_check.append(dist_dir)

    for path in dirs_to_check:
        cvd_host_package = os.path.join(path, constants.CVD_HOST_PACKAGE)
        if os.path.exists(cvd_host_package):
            logger.debug("cvd host package: %s", cvd_host_package)
            return cvd_host_package
    raise errors.GetCvdLocalHostPackageError(
        "Can't find the cvd host package (Try lunching a cuttlefish target"
        " like aosp_cf_x86_phone-userdebug and running 'm'): \n%s" %
        '\n'.join(dirs_to_check))


def DownloadRemoteArtifact(cfg, build_target, build_id, artifact, extract_path,
                           decompress=False):
    """Download remote artifact.

    Args:
        cfg: An AcloudConfig instance.
        build_target: String, the build target, e.g. cf_x86_phone-userdebug.
        build_id: String, Build id, e.g. "2263051", "P2804227"
        artifact: String, zip image or cvd host package artifact.
        extract_path: String, a path include extracted files.
        decompress: Boolean, if true decompress the artifact.
    """
    build_client = android_build_client.AndroidBuildClient(
        auth.CreateCredentials(cfg))
    temp_file = os.path.join(extract_path, artifact)
    build_client.DownloadArtifact(
        build_target,
        build_id,
        artifact,
        temp_file)
    if decompress:
        utils.Decompress(temp_file, extract_path)
        try:
            os.remove(temp_file)
            logger.debug("Deleted temporary file %s", temp_file)
        except OSError as e:
            logger.error("Failed to delete temporary file: %s", str(e))
