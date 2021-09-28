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
"""google SDK tools installer.

This class will return the path to the google sdk tools and download them into
a temporary dir if they don't exist. The caller is expected to call cleanup
when they're done.

Example usage:
google_sdk = GoogleSDK()
google_sdk_bin_path = google_sdk.GetSDKBinPath()

# Caller is done.
google_sdk.CleanUp()
"""

import logging
import os
import platform
import shutil
import sys
import tempfile
from six.moves import urllib

from acloud import errors
from acloud.internal.lib import utils


logger = logging.getLogger(__name__)

SDK_BIN_PATH = os.path.join("google-cloud-sdk", "bin")
GCLOUD_BIN = "gcloud"
GCLOUD_COMPONENT_NOT_INSTALLED = "Not Installed"
GCP_SDK_VERSION = "209.0.0"
GCP_SDK_TOOLS_URL = "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads"
LINUX_GCP_SDK_64_URL = "%s/google-cloud-sdk-%s-linux-x86_64.tar.gz" % (
    GCP_SDK_TOOLS_URL, GCP_SDK_VERSION)
LINUX_GCP_SDK_32_URL = "%s/google-cloud-sdk-%s-linux-x86.tar.gz" % (
    GCP_SDK_TOOLS_URL, GCP_SDK_VERSION)
WIN_GCP_SDK_64_URL = "%s/google-cloud-sdk-%s-windows-x86_64.zip" % (
    GCP_SDK_TOOLS_URL, GCP_SDK_VERSION)
WIN_GCP_SDK_32_URL = "%s/google-cloud-sdk-%s-windows-x86.zip" % (
    GCP_SDK_TOOLS_URL, GCP_SDK_VERSION)
MAC_GCP_SDK_64_URL = "%s/google-cloud-sdk-%s-darwin-x86_64.tar.gz" % (
    GCP_SDK_TOOLS_URL, GCP_SDK_VERSION)
MAC_GCP_SDK_32_URL = "%s/google-cloud-sdk-%s-darwin-x86.tar.gz" % (
    GCP_SDK_TOOLS_URL, GCP_SDK_VERSION)
LINUX = "linux"
WIN = "windows"
MAC = "darwin"


def GetSdkUrl():
    """Get google SDK tool url.

    Return:
        String, a URL of google SDK tools.

    Raises:
        OSTypeError when OS type is neither linux, mac, or windows.
    """
    is_64bits = sys.maxsize > 2**32
    os_type = platform.system().lower()
    if is_64bits:
        if os_type == LINUX:
            return LINUX_GCP_SDK_64_URL
        elif os_type == MAC:
            return MAC_GCP_SDK_64_URL
        elif os_type == WIN:
            return WIN_GCP_SDK_64_URL
    else:
        if os_type == LINUX:
            return LINUX_GCP_SDK_32_URL
        elif os_type == MAC:
            return MAC_GCP_SDK_32_URL
        elif os_type == WIN:
            return WIN_GCP_SDK_32_URL
    raise errors.OSTypeError("no gcloud for os type: %s" % (os_type))


def SDKInstalled():
    """Check google SDK tools installed.

    We'll try to find where gcloud is and assume the other google sdk tools
    live in the same location.

    Return:
        Boolean, return True if gcloud is installed, False otherwise.
    """
    if utils.FindExecutable(GCLOUD_BIN):
        return True
    return False


class GoogleSDK(object):
    """Google SDK tools installer."""

    def __init__(self):
        """GoogleSDKInstaller initialize.

        Make sure the GCloud SDK is installed. If not, this function will assist
        users to install.
        """
        self._tmp_path = None
        self._tmp_sdk_path = None
        if not SDKInstalled():
            self.DownloadGcloudSDKAndExtract()

    @staticmethod
    def InstallGcloudComponent(gcloud_runner, component):
        """Install gcloud component.

        Args:
            gcloud_runner: A GcloudRunner class to run "gcloud" command.
            component: String, name of gcloud component.
        """
        result = gcloud_runner.RunGcloud([
            "components", "list", "--format", "get(state.name)", "--filter",
            "ID=%s" % component
        ])
        if result.strip() == GCLOUD_COMPONENT_NOT_INSTALLED:
            gcloud_runner.RunGcloud(
                ["components", "install", "--quiet", component])

    def GetSDKBinPath(self):
        """Get google SDK tools bin path.

        We assumed there are google sdk tools somewhere and will raise if we
        can't find it. The order we're looking for is:
        1. Builtin gcloud (usually /usr/bin), we'll return /usr/bin with the
           assumption other sdk tools live there.
        2. Downloaded google sdk (self._tmp_dir), we assumed the caller already
           downloaded/extracted and set the self._tmp_sdk_path for us to return.
           We'll make sure it exists prior to returning it.

        Return:
            String, return google SDK tools bin path.

        Raise:
            NoGoogleSDKDetected if we can't find the sdk path.
        """
        builtin_gcloud = utils.FindExecutable(GCLOUD_BIN)
        if builtin_gcloud:
            return os.path.dirname(builtin_gcloud)
        elif os.path.exists(self._tmp_sdk_path):
            return self._tmp_sdk_path
        raise errors.NoGoogleSDKDetected("no sdk path.")

    def DownloadGcloudSDKAndExtract(self):
        """Download the google SDK tools and decompress it.

        Download the google SDK from the GCP web.
        Reference https://cloud.google.com/sdk/docs/downloads-versioned-archives.
        """
        self._tmp_path = tempfile.mkdtemp(prefix="gcloud")
        url = GetSdkUrl()
        filename = url[url.rfind("/") + 1:]
        file_path = os.path.join(self._tmp_path, filename)
        logger.info("Download file from: %s", url)
        logger.info("Save the file to: %s", file_path)
        url_stream = urllib.request.urlopen(url)
        metadata = url_stream.info()
        file_size = int(metadata.get("Content-Length"))
        logger.info("Downloading google SDK: %s bytes.", file_size)
        with open(file_path, 'wb') as output:
            output.write(url_stream.read())
        utils.Decompress(file_path, self._tmp_path)
        self._tmp_sdk_path = os.path.join(self._tmp_path, SDK_BIN_PATH)

    def CleanUp(self):
        """Clean google sdk tools install folder."""
        if self._tmp_path and os.path.exists(self._tmp_path):
            shutil.rmtree(self._tmp_path)
