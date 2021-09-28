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
"""A client that manages Cuttlefish Virtual Device on compute engine.

** CvdComputeClient **

CvdComputeClient derives from AndroidComputeClient. It manges a google
compute engine project that is setup for running Cuttlefish Virtual Devices.
It knows how to create a host instance from Cuttlefish Stable Host Image, fetch
Android build, and start Android within the host instance.

** Class hierarchy **

  base_cloud_client.BaseCloudApiClient
                ^
                |
       gcompute_client.ComputeClient
                ^
                |
       android_compute_client.AndroidComputeClient
                ^
                |
       cvd_compute_client_multi_stage.CvdComputeClient

"""

import logging
import os
import stat
import subprocess
import tempfile
import time

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import android_build_client
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import gcompute_client
from acloud.internal.lib import utils
from acloud.internal.lib.ssh import Ssh
from acloud.pull import pull


logger = logging.getLogger(__name__)

_DECOMPRESS_KERNEL_ARG = "-decompress_kernel=true"
_GPU_ARG = "-gpu_mode=drm_virgl"
_DEFAULT_BRANCH = "aosp-master"
_FETCHER_BUILD_TARGET = "aosp_cf_x86_phone-userdebug"
_FETCHER_NAME = "fetch_cvd"
# Time info to write in report.
_FETCH_ARTIFACT = "fetch_artifact_time"
_GCE_CREATE = "gce_create_time"
_LAUNCH_CVD = "launch_cvd_time"
# WebRTC args for launching AVD
_GUEST_ENFORCE_SECURITY_FALSE = "--guest_enforce_security=false"
_START_WEBRTC = "--start_webrtc"
_VM_MANAGER = "--vm_manager=crosvm"
_WEBRTC_ARGS = [_GUEST_ENFORCE_SECURITY_FALSE, _START_WEBRTC, _VM_MANAGER]
_WEBRTC_PUBLIC_IP = "--webrtc_public_ip=%s"


def _ProcessBuild(build_id=None, branch=None, build_target=None):
    """Create a Cuttlefish fetch_cvd build string.

    Args:
        build_id: A specific build number to load from. Takes precedence over `branch`.
        branch: A manifest-branch at which to get the latest build.
        build_target: A particular device to load at the desired build.

    Returns:
        A string, used in the fetch_cvd cmd or None if all args are None.
    """
    if not build_target:
        return build_id or branch
    elif build_target and not branch:
        branch = _DEFAULT_BRANCH
    return (build_id or branch) + "/" + build_target


class CvdComputeClient(android_compute_client.AndroidComputeClient):
    """Client that manages Android Virtual Device."""

    DATA_POLICY_CREATE_IF_MISSING = "create_if_missing"

    def __init__(self,
                 acloud_config,
                 oauth2_credentials,
                 boot_timeout_secs=None,
                 ins_timeout_secs=None,
                 report_internal_ip=None,
                 gpu=None):
        """Initialize.

        Args:
            acloud_config: An AcloudConfig object.
            oauth2_credentials: An oauth2client.OAuth2Credentials instance.
            boot_timeout_secs: Integer, the maximum time to wait for the AVD
                               to boot up.
            ins_timeout_secs: Integer, the maximum time to wait for the
                              instance ready.
            report_internal_ip: Boolean to report the internal ip instead of
                                external ip.
            gpu: String, GPU to attach to the device.
        """
        super(CvdComputeClient, self).__init__(acloud_config, oauth2_credentials)

        self._fetch_cvd_version = acloud_config.fetch_cvd_version
        self._build_api = (
            android_build_client.AndroidBuildClient(oauth2_credentials))
        self._ssh_private_key_path = acloud_config.ssh_private_key_path
        self._boot_timeout_secs = boot_timeout_secs
        self._ins_timeout_secs = ins_timeout_secs
        self._report_internal_ip = report_internal_ip
        self._gpu = gpu
        # Store all failures result when creating one or multiple instances.
        self._all_failures = dict()
        self._extra_args_ssh_tunnel = acloud_config.extra_args_ssh_tunnel
        self._ssh = None
        self._ip = None
        self._user = constants.GCE_USER
        self._execution_time = {_FETCH_ARTIFACT: 0, _GCE_CREATE: 0, _LAUNCH_CVD: 0}

    def InitRemoteHost(self, ssh, ip, user):
        """Init remote host.

        Check if we can ssh to the remote host, stop any cf instances running
        on it, and remove existing files.

        Args:
            ssh: Ssh object.
            ip: namedtuple (internal, external) that holds IP address of the
                remote host, e.g. "external:140.110.20.1, internal:10.0.0.1"
            user: String of user log in to the instance.
        """
        self._ssh = ssh
        self._ip = ip
        self._user = user
        self._ssh.WaitForSsh(timeout=self._ins_timeout_secs)
        self.StopCvd()
        self.CleanUp()

    # pylint: disable=arguments-differ,too-many-locals,broad-except
    def CreateInstance(self, instance, image_name, image_project,
                       build_target=None, branch=None, build_id=None,
                       kernel_branch=None, kernel_build_id=None,
                       kernel_build_target=None, blank_data_disk_size_gb=None,
                       avd_spec=None, extra_scopes=None,
                       system_build_target=None, system_branch=None,
                       system_build_id=None):

        """Create/Reuse a single configured cuttlefish device.
        1. Prepare GCE instance.
           Create a new instnace or get IP address for reusing the specific instance.
        2. Put fetch_cvd on the instance.
        3. Invoke fetch_cvd to fetch and run the instance.

        Args:
            instance: instance name.
            image_name: A string, the name of the GCE image.
            image_project: A string, name of the project where the image lives.
                           Assume the default project if None.
            build_target: Target name, e.g. "aosp_cf_x86_phone-userdebug"
            branch: Branch name, e.g. "aosp-master"
            build_id: Build id, a string, e.g. "2263051", "P2804227"
            kernel_branch: Kernel branch name, e.g. "kernel-common-android-4.14"
            kernel_build_id: Kernel build id, a string, e.g. "223051", "P280427"
            kernel_build_target: String, Kernel build target name.
            blank_data_disk_size_gb: Size of the blank data disk in GB.
            avd_spec: An AVDSpec instance.
            extra_scopes: A list of extra scopes to be passed to the instance.
            system_build_target: Target name for the system image,
                                e.g. "cf_x86_phone-userdebug"
            system_branch: A String, branch name for the system image.
            system_build_id: A string, build id for the system image.

        Returns:
            A string, representing instance name.
        """

        # A blank data disk would be created on the host. Make sure the size of
        # the boot disk is large enough to hold it.
        boot_disk_size_gb = (
            int(self.GetImage(image_name, image_project)["diskSizeGb"]) +
            blank_data_disk_size_gb)

        if avd_spec and avd_spec.instance_name_to_reuse:
            self._ip = self._ReusingGceInstance(avd_spec)
        else:
            self._ip = self._CreateGceInstance(instance, image_name, image_project,
                                               extra_scopes, boot_disk_size_gb,
                                               avd_spec)
        self._ssh = Ssh(ip=self._ip,
                        user=constants.GCE_USER,
                        ssh_private_key_path=self._ssh_private_key_path,
                        extra_args_ssh_tunnel=self._extra_args_ssh_tunnel,
                        report_internal_ip=self._report_internal_ip)
        try:
            self._ssh.WaitForSsh(timeout=self._ins_timeout_secs)
            if avd_spec:
                if avd_spec.instance_name_to_reuse:
                    self.StopCvd()
                    self.CleanUp()
                return instance

            # TODO: Remove following code after create_cf deprecated.
            self.UpdateFetchCvd()

            self.FetchBuild(build_id, branch, build_target, system_build_id,
                            system_branch, system_build_target, kernel_build_id,
                            kernel_branch, kernel_build_target)
            kernel_build = self.GetKernelBuild(kernel_build_id,
                                               kernel_branch,
                                               kernel_build_target)
            self.LaunchCvd(instance,
                           blank_data_disk_size_gb=blank_data_disk_size_gb,
                           kernel_build=kernel_build,
                           boot_timeout_secs=self._boot_timeout_secs)

            return instance
        except Exception as e:
            self._all_failures[instance] = e
            return instance

    def _GetLaunchCvdArgs(self, avd_spec=None, blank_data_disk_size_gb=None,
                          kernel_build=None, decompress_kernel=None):
        """Get launch_cvd args.

        Args:
            avd_spec: An AVDSpec instance.
            blank_data_disk_size_gb: Size of the blank data disk in GB.
            kernel_build: String, kernel build info.
            decompress_kernel: Boolean, if true decompress the kernel.

        Returns:
            String, args of launch_cvd.
        """
        launch_cvd_args = []
        if blank_data_disk_size_gb > 0:
            # Policy 'create_if_missing' would create a blank userdata disk if
            # missing. If already exist, reuse the disk.
            launch_cvd_args.append(
                "-data_policy=" + self.DATA_POLICY_CREATE_IF_MISSING)
            launch_cvd_args.append(
                "-blank_data_image_mb=%d" % (blank_data_disk_size_gb * 1024))
        if avd_spec:
            launch_cvd_args.append(
                "-x_res=" + avd_spec.hw_property[constants.HW_X_RES])
            launch_cvd_args.append(
                "-y_res=" + avd_spec.hw_property[constants.HW_Y_RES])
            launch_cvd_args.append(
                "-dpi=" + avd_spec.hw_property[constants.HW_ALIAS_DPI])
            if constants.HW_ALIAS_DISK in avd_spec.hw_property:
                launch_cvd_args.append(
                    "-data_policy=" + self.DATA_POLICY_CREATE_IF_MISSING)
                launch_cvd_args.append(
                    "-blank_data_image_mb="
                    + avd_spec.hw_property[constants.HW_ALIAS_DISK])
            if constants.HW_ALIAS_CPUS in avd_spec.hw_property:
                launch_cvd_args.append(
                    "-cpus=%s" % avd_spec.hw_property[constants.HW_ALIAS_CPUS])
            if constants.HW_ALIAS_MEMORY in avd_spec.hw_property:
                launch_cvd_args.append(
                    "-memory_mb=%s" % avd_spec.hw_property[constants.HW_ALIAS_MEMORY])
            if avd_spec.connect_webrtc:
                launch_cvd_args.append(_WEBRTC_PUBLIC_IP % self._ip.external)
                launch_cvd_args.extend(_WEBRTC_ARGS)
        else:
            resolution = self._resolution.split("x")
            launch_cvd_args.append("-x_res=" + resolution[0])
            launch_cvd_args.append("-y_res=" + resolution[1])
            launch_cvd_args.append("-dpi=" + resolution[3])

        if kernel_build:
            launch_cvd_args.append("-kernel_path=kernel")

        if self._launch_args:
            launch_cvd_args.append(self._launch_args)

        if decompress_kernel:
            launch_cvd_args.append(_DECOMPRESS_KERNEL_ARG)

        if self._gpu:
            launch_cvd_args.append(_GPU_ARG)

        return launch_cvd_args

    @staticmethod
    def GetKernelBuild(kernel_build_id, kernel_branch, kernel_build_target):
        """Get kernel build args for fetch_cvd.

        Args:
            kernel_branch: Kernel branch name, e.g. "kernel-common-android-4.14"
            kernel_build_id: Kernel build id, a string, e.g. "223051", "P280427"
            kernel_build_target: String, Kernel build target name.

        Returns:
            String of kernel build args for fetch_cvd.
            If no kernel build then return None.
        """
        # kernel_target have default value "kernel". If user provide kernel_build_id
        # or kernel_branch, then start to process kernel image.
        if kernel_build_id or kernel_branch:
            return _ProcessBuild(kernel_build_id, kernel_branch, kernel_build_target)
        return None

    def StopCvd(self):
        """Stop CVD.

        If stop_cvd fails, assume that it's because there was no previously
        running device.
        """
        ssh_command = "./bin/stop_cvd"
        try:
            self._ssh.Run(ssh_command)
        except subprocess.CalledProcessError as e:
            logger.debug("Failed to stop_cvd (possibly no running device): %s", e)

    def CleanUp(self):
        """Clean up the files/folders on the existing instance.

        If previous AVD have these files/folders, reusing the instance may have
        side effects if not cleaned. The path in the instance is /home/vsoc-01/*
        if the GCE user is vsoc-01.
        """

        ssh_command = "'/bin/rm -rf /home/%s/*'" % self._user
        try:
            self._ssh.Run(ssh_command)
        except subprocess.CalledProcessError as e:
            logger.debug("Failed to clean up the files/folders: %s", e)

    @utils.TimeExecute(function_description="Launching AVD(s) and waiting for boot up",
                       result_evaluator=utils.BootEvaluator)
    def LaunchCvd(self, instance, avd_spec=None,
                  blank_data_disk_size_gb=None, kernel_build=None,
                  decompress_kernel=None,
                  boot_timeout_secs=None):
        """Launch CVD.

        Launch AVD with launch_cvd. If the process is failed, acloud would show
        error messages and auto download log files from remote instance.

        Args:
            instance: String, instance name.
            avd_spec: An AVDSpec instance.
            blank_data_disk_size_gb: Size of the blank data disk in GB.
            kernel_build: String, kernel build info.
            decompress_kernel: Boolean, if true decompress the kernel.
            boot_timeout_secs: Integer, the maximum time to wait for the
                               command to respond.

        Returns:
           dict of faliures, return this dict for BootEvaluator to handle
           LaunchCvd success or fail messages.
        """
        timestart = time.time()
        error_msg = ""
        launch_cvd_args = self._GetLaunchCvdArgs(avd_spec,
                                                 blank_data_disk_size_gb,
                                                 kernel_build,
                                                 decompress_kernel)
        boot_timeout_secs = boot_timeout_secs or self.BOOT_TIMEOUT_SECS
        ssh_command = "./bin/launch_cvd -daemon " + " ".join(launch_cvd_args)
        try:
            self._ssh.Run(ssh_command, boot_timeout_secs)
        except (subprocess.CalledProcessError, errors.DeviceConnectionError) as e:
            # TODO(b/140475060): Distinguish the error is command return error
            # or timeout error.
            error_msg = ("Device %s did not finish on boot within timeout (%s secs)"
                         % (instance, boot_timeout_secs))
            self._all_failures[instance] = error_msg
            utils.PrintColorString(str(e), utils.TextColors.FAIL)
            if avd_spec and not avd_spec.no_pull_log:
                self._PullAllLogFiles(instance)

        self._execution_time[_LAUNCH_CVD] = round(time.time() - timestart, 2)
        return {instance: error_msg} if error_msg else {}

    def _PullAllLogFiles(self, instance):
        """Pull all log files from instance.

        1. Download log files to temp folder.
        2. Show messages about the download folder for users.

        Args:
            instance: String, instance name.
        """
        log_files = pull.GetAllLogFilePaths(self._ssh)
        download_folder = pull.GetDownloadLogFolder(instance)
        pull.PullLogs(self._ssh, log_files, download_folder)

    @utils.TimeExecute(function_description="Reusing GCE instance")
    def _ReusingGceInstance(self, avd_spec):
        """Reusing a cuttlefish existing instance.

        Args:
            avd_spec: An AVDSpec instance.

        Returns:
            ssh.IP object, that stores internal and external ip of the instance.
        """
        gcompute_client.ComputeClient.AddSshRsaInstanceMetadata(
            self, constants.GCE_USER, avd_spec.cfg.ssh_public_key_path,
            avd_spec.instance_name_to_reuse)
        ip = gcompute_client.ComputeClient.GetInstanceIP(
            self, instance=avd_spec.instance_name_to_reuse, zone=self._zone)

        return ip

    @utils.TimeExecute(function_description="Creating GCE instance")
    def _CreateGceInstance(self, instance, image_name, image_project,
                           extra_scopes, boot_disk_size_gb, avd_spec):
        """Create a single configured cuttlefish device.

        Override method from parent class.
        Args:
            instance: String, instance name.
            image_name: String, the name of the GCE image.
            image_project: String, the name of the project where the image.
            extra_scopes: A list of extra scopes to be passed to the instance.
            boot_disk_size_gb: Integer, size of the boot disk in GB.
            avd_spec: An AVDSpec instance.

        Returns:
            ssh.IP object, that stores internal and external ip of the instance.
        """
        timestart = time.time()
        metadata = self._metadata.copy()

        if avd_spec:
            metadata[constants.INS_KEY_AVD_TYPE] = avd_spec.avd_type
            metadata[constants.INS_KEY_AVD_FLAVOR] = avd_spec.flavor
            metadata[constants.INS_KEY_DISPLAY] = ("%sx%s (%s)" % (
                avd_spec.hw_property[constants.HW_X_RES],
                avd_spec.hw_property[constants.HW_Y_RES],
                avd_spec.hw_property[constants.HW_ALIAS_DPI]))

        disk_args = self._GetDiskArgs(
            instance, image_name, image_project, boot_disk_size_gb)
        gcompute_client.ComputeClient.CreateInstance(
            self,
            instance=instance,
            image_name=image_name,
            image_project=image_project,
            disk_args=disk_args,
            metadata=metadata,
            machine_type=self._machine_type,
            network=self._network,
            zone=self._zone,
            gpu=self._gpu,
            extra_scopes=extra_scopes,
            tags=["appstreaming"] if (
                avd_spec and avd_spec.connect_webrtc) else None)
        ip = gcompute_client.ComputeClient.GetInstanceIP(
            self, instance=instance, zone=self._zone)
        logger.debug("'instance_ip': %s", ip.internal
                     if self._report_internal_ip else ip.external)

        self._execution_time[_GCE_CREATE] = round(time.time() - timestart, 2)
        return ip

    @utils.TimeExecute(function_description="Uploading build fetcher to instance")
    def UpdateFetchCvd(self):
        """Download fetch_cvd from the Build API, and upload it to a remote instance.

        The version of fetch_cvd to use is retrieved from the configuration file. Once fetch_cvd
        is on the instance, future commands can use it to download relevant Cuttlefish files from
        the Build API on the instance itself.
        """
        # TODO(schuffelen): Support fetch_cvd_version="latest" when there is
        # stronger automated testing on it.
        download_dir = tempfile.mkdtemp()
        download_target = os.path.join(download_dir, _FETCHER_NAME)
        self._build_api.DownloadArtifact(
            build_target=_FETCHER_BUILD_TARGET,
            build_id=self._fetch_cvd_version,
            resource_id=_FETCHER_NAME,
            local_dest=download_target,
            attempt_id="latest")
        fetch_cvd_stat = os.stat(download_target)
        os.chmod(download_target, fetch_cvd_stat.st_mode | stat.S_IEXEC)
        self._ssh.ScpPushFile(src_file=download_target, dst_file=_FETCHER_NAME)
        os.remove(download_target)
        os.rmdir(download_dir)

    @utils.TimeExecute(function_description="Downloading build on instance")
    def FetchBuild(self, build_id, branch, build_target, system_build_id,
                   system_branch, system_build_target, kernel_build_id,
                   kernel_branch, kernel_build_target):
        """Execute fetch_cvd on the remote instance to get Cuttlefish runtime files.

        Args:
            fetch_args: String of arguments to pass to fetch_cvd.
        """
        timestart = time.time()
        fetch_cvd_args = ["-credential_source=gce"]

        default_build = _ProcessBuild(build_id, branch, build_target)
        if default_build:
            fetch_cvd_args.append("-default_build=" + default_build)
        system_build = _ProcessBuild(system_build_id, system_branch, system_build_target)
        if system_build:
            fetch_cvd_args.append("-system_build=" + system_build)
        kernel_build = self.GetKernelBuild(kernel_build_id,
                                           kernel_branch,
                                           kernel_build_target)
        if kernel_build:
            fetch_cvd_args.append("-kernel_build=" + kernel_build)

        self._ssh.Run("./fetch_cvd " + " ".join(fetch_cvd_args))
        self._execution_time[_FETCH_ARTIFACT] = round(time.time() - timestart, 2)

    def GetInstanceIP(self, instance=None):
        """Override method from parent class.

        It need to get the IP address in the common_operation. If the class
        already defind the ip address, return the ip address.

        Args:
            instance: String, representing instance name.

        Returns:
            ssh.IP object, that stores internal and external ip of the instance.
        """
        if self._ip:
            return self._ip
        return gcompute_client.ComputeClient.GetInstanceIP(
            self, instance=instance, zone=self._zone)

    @property
    def all_failures(self):
        """Return all_failures"""
        return self._all_failures

    @property
    def execution_time(self):
        """Return execution_time"""
        return self._execution_time
