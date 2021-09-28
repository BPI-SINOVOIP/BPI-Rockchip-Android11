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
r"""RemoteImageLocalInstance class.

Create class that is responsible for creating a local instance AVD with a
remote image.
"""
import logging
import os
import sys

from acloud import errors
from acloud.create import create_common
from acloud.create import local_image_local_instance
from acloud.internal import constants
from acloud.internal.lib import utils
from acloud.setup import setup_common


logger = logging.getLogger(__name__)

# Download remote image variables.
_CUTTLEFISH_COMMON_BIN_PATH = "/usr/lib/cuttlefish-common/bin/"
_CONFIRM_DOWNLOAD_DIR = ("Download dir %(download_dir)s does not have enough "
                         "space (available space %(available_space)sGB, "
                         "require %(required_space)sGB).\nPlease enter "
                         "alternate path or 'q' to exit: ")
# The downloaded image artifacts will take up ~8G:
#   $du -lh --time $ANDROID_PRODUCT_OUT/aosp_cf_x86_phone-img-eng.XXX.zip
#   422M
# And decompressed becomes 7.2G (as of 11/2018).
# Let's add an extra buffer (~2G) to make sure user has enough disk space
# for the downloaded image artifacts.
_REQUIRED_SPACE = 10


@utils.TimeExecute(function_description="Downloading Android Build image")
def DownloadAndProcessImageFiles(avd_spec):
    """Download the CF image artifacts and process them.

    It will download two artifacts and process them in this function. One is
    cvd_host_package.tar.gz, the other is rom image zip. If the build_id is
    "1234" and build_target is "aosp_cf_x86_phone-userdebug",
    the image zip name is "aosp_cf_x86_phone-img-1234.zip".

    Args:
        avd_spec: AVDSpec object that tells us what we're going to create.

    Returns:
        extract_path: String, path to image folder.
    """
    cfg = avd_spec.cfg
    build_id = avd_spec.remote_image[constants.BUILD_ID]
    build_target = avd_spec.remote_image[constants.BUILD_TARGET]

    extract_path = os.path.join(
        avd_spec.image_download_dir,
        constants.TEMP_ARTIFACTS_FOLDER,
        build_id)

    logger.debug("Extract path: %s", extract_path)
    # TODO(b/117189191): If extract folder exists, check if the files are
    # already downloaded and skip this step if they are.
    if not os.path.exists(extract_path):
        os.makedirs(extract_path)
        remote_image = "%s-img-%s.zip" % (build_target.split('-')[0],
                                          build_id)
        artifacts = [constants.CVD_HOST_PACKAGE, remote_image]
        for artifact in artifacts:
            create_common.DownloadRemoteArtifact(
                cfg, build_target, build_id, artifact, extract_path, decompress=True)
    return extract_path


def ConfirmDownloadRemoteImageDir(download_dir):
    """Confirm download remote image directory.

    If available space of download_dir is less than _REQUIRED_SPACE, ask
    the user to choose a different download dir or to exit out since acloud will
    fail to download the artifacts due to insufficient disk space.

    Args:
        download_dir: String, a directory for download and decompress.

    Returns:
        String, Specific download directory when user confirm to change.
    """
    while True:
        download_dir = os.path.expanduser(download_dir)
        if not os.path.exists(download_dir):
            answer = utils.InteractWithQuestion(
                "No such directory %s.\nEnter 'y' to create it, enter "
                "anything else to exit out[y/N]: " % download_dir)
            if answer.lower() == "y":
                os.makedirs(download_dir)
            else:
                sys.exit(constants.EXIT_BY_USER)

        stat = os.statvfs(download_dir)
        available_space = stat.f_bavail*stat.f_bsize/(1024)**3
        if available_space < _REQUIRED_SPACE:
            download_dir = utils.InteractWithQuestion(
                _CONFIRM_DOWNLOAD_DIR % {"download_dir":download_dir,
                                         "available_space":available_space,
                                         "required_space":_REQUIRED_SPACE})
            if download_dir.lower() == "q":
                sys.exit(constants.EXIT_BY_USER)
        else:
            return download_dir


class RemoteImageLocalInstance(local_image_local_instance.LocalImageLocalInstance):
    """Create class for a remote image local instance AVD.

    RemoteImageLocalInstance just defines logic in downloading the remote image
    artifacts and leverages the existing logic to launch a local instance in
    LocalImageLocalInstance.
    """

    def GetImageArtifactsPath(self, avd_spec):
        """Download the image artifacts and return the paths to them.

        Args:
            avd_spec: AVDSpec object that tells us what we're going to create.

        Raises:
            errors.NoCuttlefishCommonInstalled: cuttlefish-common doesn't install.

        Returns:
            Tuple of (local image file, host bins package) paths.
        """
        if not setup_common.PackageInstalled("cuttlefish-common"):
            raise errors.NoCuttlefishCommonInstalled(
                "Package [cuttlefish-common] is not installed!\n"
                "Please run 'acloud setup --host' to install.")

        avd_spec.image_download_dir = ConfirmDownloadRemoteImageDir(
            avd_spec.image_download_dir)

        image_dir = DownloadAndProcessImageFiles(avd_spec)
        launch_cvd_path = os.path.join(image_dir, "bin",
                                       constants.CMD_LAUNCH_CVD)
        if not os.path.exists(launch_cvd_path):
            raise errors.GetCvdLocalHostPackageError(
                "No launch_cvd found. Please check downloaded artifacts dir: %s"
                % image_dir)
        return image_dir, image_dir
