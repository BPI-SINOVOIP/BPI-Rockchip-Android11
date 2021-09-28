# Copyright 2019 - The Android Open Source Project
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

"""RemoteInstanceDeviceFactory provides basic interface to create a cuttlefish
device factory."""

import glob
import logging
import os
import shutil
import tempfile

from acloud import errors
from acloud.create import create_common
from acloud.internal import constants
from acloud.internal.lib import auth
from acloud.internal.lib import cvd_compute_client_multi_stage
from acloud.internal.lib import utils
from acloud.internal.lib import ssh
from acloud.public.actions import base_device_factory


logger = logging.getLogger(__name__)

_USER_BUILD = "userbuild"


class RemoteInstanceDeviceFactory(base_device_factory.BaseDeviceFactory):
    """A class that can produce a cuttlefish device.

    Attributes:
        avd_spec: AVDSpec object that tells us what we're going to create.
        cfg: An AcloudConfig instance.
        local_image_artifact: A string, path to local image.
        cvd_host_package_artifact: A string, path to cvd host package.
        report_internal_ip: Boolean, True for the internal ip is used when
                            connecting from another GCE instance.
        credentials: An oauth2client.OAuth2Credentials instance.
        compute_client: An object of cvd_compute_client.CvdComputeClient.
        ssh: An Ssh object.
    """
    def __init__(self, avd_spec, local_image_artifact=None,
                 cvd_host_package_artifact=None):
        """Constructs a new remote instance device factory."""
        self._avd_spec = avd_spec
        self._cfg = avd_spec.cfg
        self._local_image_artifact = local_image_artifact
        self._cvd_host_package_artifact = cvd_host_package_artifact
        self._report_internal_ip = avd_spec.report_internal_ip
        self.credentials = auth.CreateCredentials(avd_spec.cfg)
        # Control compute_client with enable_multi_stage
        compute_client = cvd_compute_client_multi_stage.CvdComputeClient(
            acloud_config=avd_spec.cfg,
            oauth2_credentials=self.credentials,
            ins_timeout_secs=avd_spec.ins_timeout_secs,
            report_internal_ip=avd_spec.report_internal_ip,
            gpu=avd_spec.gpu)
        super(RemoteInstanceDeviceFactory, self).__init__(compute_client)
        self._ssh = None

    def CreateInstance(self):
        """Create a single configured cuttlefish device.

        GCE:
        1. Create gcp instance.
        2. Upload local built artifacts to remote instance or fetch build on
           remote instance.
        3. Launch CVD.

        Remote host:
        1. Init remote host.
        2. Download the artifacts to local and upload the artifacts to host
        3. Launch CVD.

        Returns:
            A string, representing instance name.
        """
        if self._avd_spec.instance_type == constants.INSTANCE_TYPE_HOST:
            instance = self._InitRemotehost()
            self._ProcessRemoteHostArtifacts()
            self._LaunchCvd(instance=instance,
                            decompress_kernel=True,
                            boot_timeout_secs=self._avd_spec.boot_timeout_secs)
        else:
            instance = self._CreateGceInstance()
            # If instance is failed, no need to go next step.
            if instance in self.GetFailures():
                return instance
            try:
                self._ProcessArtifacts(self._avd_spec.image_source)
                self._LaunchCvd(instance=instance,
                                boot_timeout_secs=self._avd_spec.boot_timeout_secs)
            except errors.DeviceConnectionError as e:
                self._SetFailures(instance, e)

        return instance

    def _InitRemotehost(self):
        """Initialize remote host.

        Determine the remote host instance name, and activate ssh. It need to
        get the IP address in the common_operation. So need to pass the IP and
        ssh to compute_client.

        build_target: The format is like "aosp_cf_x86_phone". We only get info
                      from the user build image file name. If the file name is
                      not custom format (no "-"), we will use $TARGET_PRODUCT
                      from environment variable as build_target.

        Returns:
            A string, representing instance name.
        """
        image_name = os.path.basename(
            self._local_image_artifact) if self._local_image_artifact else ""
        build_target = (os.environ.get(constants.ENV_BUILD_TARGET) if "-" not
                        in image_name else image_name.split("-")[0])
        build_id = _USER_BUILD
        if self._avd_spec.image_source == constants.IMAGE_SRC_REMOTE:
            build_id = self._avd_spec.remote_image[constants.BUILD_ID]

        instance = "%s-%s-%s-%s" % (constants.INSTANCE_TYPE_HOST,
                                    self._avd_spec.remote_host,
                                    build_id, build_target)
        ip = ssh.IP(ip=self._avd_spec.remote_host)
        self._ssh = ssh.Ssh(
            ip=ip,
            user=self._avd_spec.host_user,
            ssh_private_key_path=(self._avd_spec.host_ssh_private_key_path or
                                  self._cfg.ssh_private_key_path),
            extra_args_ssh_tunnel=self._cfg.extra_args_ssh_tunnel,
            report_internal_ip=self._report_internal_ip)
        self._compute_client.InitRemoteHost(
            self._ssh, ip, self._avd_spec.host_user)
        return instance

    @utils.TimeExecute(function_description="Downloading Android Build artifact")
    def _DownloadArtifacts(self, extract_path):
        """Download the CF image artifacts and process them.

        - Download image from the Android Build system, then decompress it.
        - Download cvd host package from the Android Build system.

        Args:
            extract_path: String, a path include extracted files.
        """
        cfg = self._avd_spec.cfg
        build_id = self._avd_spec.remote_image[constants.BUILD_ID]
        build_target = self._avd_spec.remote_image[constants.BUILD_TARGET]

        # Image zip
        remote_image = "%s-img-%s.zip" % (build_target.split('-')[0], build_id)
        create_common.DownloadRemoteArtifact(
            cfg, build_target, build_id, remote_image, extract_path, decompress=True)

        # Cvd host package
        create_common.DownloadRemoteArtifact(
            cfg, build_target, build_id, constants.CVD_HOST_PACKAGE,
            extract_path)

    def _ProcessRemoteHostArtifacts(self):
        """Process remote host artifacts.

        - If images source is local, tool will upload images from local site to
          remote host.
        - If images source is remote, tool will download images from android
          build to local and unzip it then upload to remote host, because there
          is no permission to fetch build rom on the remote host.
        """
        if self._avd_spec.image_source == constants.IMAGE_SRC_LOCAL:
            self._UploadArtifacts(
                self._local_image_artifact, self._cvd_host_package_artifact,
                self._avd_spec.local_image_dir)
        else:
            try:
                artifacts_path = tempfile.mkdtemp()
                logger.debug("Extracted path of artifacts: %s", artifacts_path)
                self._DownloadArtifacts(artifacts_path)
                self._UploadArtifacts(
                    None,
                    os.path.join(artifacts_path, constants.CVD_HOST_PACKAGE),
                    artifacts_path)
            finally:
                shutil.rmtree(artifacts_path)

    def _ProcessArtifacts(self, image_source):
        """Process artifacts.

        - If images source is local, tool will upload images from local site to
          remote instance.
        - If images source is remote, tool will download images from android
          build to remote instance. Before download images, we have to update
          fetch_cvd to remote instance.

        Args:
            image_source: String, the type of image source is remote or local.
        """
        if image_source == constants.IMAGE_SRC_LOCAL:
            self._UploadArtifacts(self._local_image_artifact,
                                  self._cvd_host_package_artifact,
                                  self._avd_spec.local_image_dir)
        elif image_source == constants.IMAGE_SRC_REMOTE:
            self._compute_client.UpdateFetchCvd()
            self._FetchBuild(
                self._avd_spec.remote_image[constants.BUILD_ID],
                self._avd_spec.remote_image[constants.BUILD_BRANCH],
                self._avd_spec.remote_image[constants.BUILD_TARGET],
                self._avd_spec.system_build_info[constants.BUILD_ID],
                self._avd_spec.system_build_info[constants.BUILD_BRANCH],
                self._avd_spec.system_build_info[constants.BUILD_TARGET],
                self._avd_spec.kernel_build_info[constants.BUILD_ID],
                self._avd_spec.kernel_build_info[constants.BUILD_BRANCH],
                self._avd_spec.kernel_build_info[constants.BUILD_TARGET])

    def _FetchBuild(self, build_id, branch, build_target, system_build_id,
                    system_branch, system_build_target, kernel_build_id,
                    kernel_branch, kernel_build_target):
        """Download CF artifacts from android build.

        Args:
            build_branch: String, git branch name. e.g. "aosp-master"
            build_target: String, the build target, e.g. cf_x86_phone-userdebug
            build_id: String, build id, e.g. "2263051", "P2804227"
            kernel_branch: Kernel branch name, e.g. "kernel-common-android-4.14"
            kernel_build_id: Kernel build id, a string, e.g. "223051", "P280427"
            kernel_build_target: String, Kernel build target name.
            system_build_target: Target name for the system image,
                                 e.g. "cf_x86_phone-userdebug"
            system_branch: A String, branch name for the system image.
            system_build_id: A string, build id for the system image.

        """
        self._compute_client.FetchBuild(
            build_id, branch, build_target, system_build_id,
            system_branch, system_build_target, kernel_build_id,
            kernel_branch, kernel_build_target)

    def _CreateGceInstance(self):
        """Create a single configured cuttlefish device.

        Override method from parent class.
        build_target: The format is like "aosp_cf_x86_phone". We only get info
                      from the user build image file name. If the file name is
                      not custom format (no "-"), we will use $TARGET_PRODUCT
                      from environment variable as build_target.

        Returns:
            A string, representing instance name.
        """
        image_name = os.path.basename(
            self._local_image_artifact) if self._local_image_artifact else ""
        build_target = (os.environ.get(constants.ENV_BUILD_TARGET) if "-" not
                        in image_name else image_name.split("-")[0])
        build_id = _USER_BUILD
        if self._avd_spec.image_source == constants.IMAGE_SRC_REMOTE:
            build_id = self._avd_spec.remote_image[constants.BUILD_ID]
            build_target = self._avd_spec.remote_image[constants.BUILD_TARGET]

        if self._avd_spec.instance_name_to_reuse:
            instance = self._avd_spec.instance_name_to_reuse
        else:
            instance = self._compute_client.GenerateInstanceName(
                build_target=build_target, build_id=build_id)

        # Create an instance from Stable Host Image
        self._compute_client.CreateInstance(
            instance=instance,
            image_name=self._cfg.stable_host_image_name,
            image_project=self._cfg.stable_host_image_project,
            blank_data_disk_size_gb=self._cfg.extra_data_disk_size_gb,
            avd_spec=self._avd_spec)
        ip = self._compute_client.GetInstanceIP(instance)
        self._ssh = ssh.Ssh(ip=ip,
                            user=constants.GCE_USER,
                            ssh_private_key_path=self._cfg.ssh_private_key_path,
                            extra_args_ssh_tunnel=self._cfg.extra_args_ssh_tunnel,
                            report_internal_ip=self._report_internal_ip)
        return instance

    @utils.TimeExecute(function_description="Processing and uploading local images")
    def _UploadArtifacts(self,
                         local_image_zip,
                         cvd_host_package_artifact,
                         images_dir):
        """Upload local images and avd local host package to instance.

        There are two ways to upload local images.
        1. Using local image zip, it would be decompressed by install_zip.sh.
        2. Using local image directory, this directory contains all images.
           Images are compressed/decompressed by lzop during upload process.

        Args:
            local_image_zip: String, path to zip of local images which
                             build from 'm dist'.
            cvd_host_package_artifact: String, path to cvd host package.
            images_dir: String, directory of local images which build
                        from 'm'.
        """
        if local_image_zip:
            remote_cmd = ("/usr/bin/install_zip.sh . < %s" % local_image_zip)
            logger.debug("remote_cmd:\n %s", remote_cmd)
            self._ssh.Run(remote_cmd)
        else:
            # Compress image files for faster upload.
            artifact_files = [os.path.basename(image) for image in glob.glob(
                os.path.join(images_dir, "*.img"))]
            cmd = ("tar -cf - --lzop -S -C {images_dir} {artifact_files} | "
                   "{ssh_cmd} -- tar -xf - --lzop -S".format(
                       images_dir=images_dir,
                       artifact_files=" ".join(artifact_files),
                       ssh_cmd=self._ssh.GetBaseCmd(constants.SSH_BIN)))
            logger.debug("cmd:\n %s", cmd)
            ssh.ShellCmdWithRetry(cmd)

        # host_package
        remote_cmd = ("tar -x -z -f - < %s" % cvd_host_package_artifact)
        logger.debug("remote_cmd:\n %s", remote_cmd)
        self._ssh.Run(remote_cmd)

    def _LaunchCvd(self, instance, decompress_kernel=None,
                   boot_timeout_secs=None):
        """Launch CVD.

        Args:
            instance: String, instance name.
            boot_timeout_secs: Integer, the maximum time to wait for the
                               command to respond.
        """
        kernel_build = None
        # TODO(b/140076771) Support kernel image for local image mode.
        if self._avd_spec.image_source == constants.IMAGE_SRC_REMOTE:
            kernel_build = self._compute_client.GetKernelBuild(
                self._avd_spec.kernel_build_info[constants.BUILD_ID],
                self._avd_spec.kernel_build_info[constants.BUILD_BRANCH],
                self._avd_spec.kernel_build_info[constants.BUILD_TARGET])
        self._compute_client.LaunchCvd(
            instance,
            self._avd_spec,
            self._cfg.extra_data_disk_size_gb,
            kernel_build,
            decompress_kernel,
            boot_timeout_secs)

    def GetFailures(self):
        """Get failures from all devices.

        Returns:
            A dictionary that contains all the failures.
            The key is the name of the instance that fails to boot,
            and the value is an errors.DeviceBootError object.
        """
        return self._compute_client.all_failures

    def _SetFailures(self, instance, error_msg):
        """Set failures from this device.

        Record the failures for any steps in AVD creation.

        Args:
            instance: String of instance name.
            error_msg: String of error message.
        """
        self._compute_client.all_failures[instance] = error_msg

    def GetBuildInfoDict(self):
        """Get build info dictionary.

        Returns:
            A build info dictionary. None for local image case.
        """
        if self._avd_spec.image_source == constants.IMAGE_SRC_LOCAL:
            return None
        build_info_dict = {
            key: val for key, val in self._avd_spec.remote_image.items() if val}

        # kernel_target have default value "kernel". If user provide kernel_build_id
        # or kernel_branch, then start to process kernel image.
        if (self._avd_spec.kernel_build_info[constants.BUILD_ID]
                or self._avd_spec.kernel_build_info[constants.BUILD_BRANCH]):
            build_info_dict.update(
                {"kernel_%s" % key: val
                 for key, val in self._avd_spec.kernel_build_info.items() if val}
            )
        build_info_dict.update(
            {"system_%s" % key: val
             for key, val in self._avd_spec.system_build_info.items() if val}
        )
        return build_info_dict
