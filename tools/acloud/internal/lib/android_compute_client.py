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
"""A client that manages Android compute engine instances.

** AndroidComputeClient **

AndroidComputeClient derives from ComputeClient. It manges a google
compute engine project that is setup for running Android instances.
It knows how to create android GCE images and instances.

** Class hierarchy **

  base_cloud_client.BaseCloudApiClient
                ^
                |
       gcompute_client.ComputeClient
                ^
                |
    gcompute_client.AndroidComputeClient
"""

import getpass
import logging
import os
import uuid

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import gcompute_client
from acloud.internal.lib import utils


logger = logging.getLogger(__name__)


class AndroidComputeClient(gcompute_client.ComputeClient):
    """Client that manages Anadroid Virtual Device."""
    IMAGE_NAME_FMT = "img-{uuid}-{build_id}-{build_target}"
    DATA_DISK_NAME_FMT = "data-{instance}"
    BOOT_COMPLETED_MSG = "VIRTUAL_DEVICE_BOOT_COMPLETED"
    BOOT_STARTED_MSG = "VIRTUAL_DEVICE_BOOT_STARTED"
    BOOT_TIMEOUT_SECS = 5 * 60  # 5 mins, usually it should take ~2 mins
    BOOT_CHECK_INTERVAL_SECS = 10

    OPERATION_TIMEOUT_SECS = 20 * 60  # Override parent value, 20 mins

    NAME_LENGTH_LIMIT = 63
    # If the generated name ends with '-', replace it with REPLACER.
    REPLACER = "e"

    def __init__(self, acloud_config, oauth2_credentials):
        """Initialize.

        Args:
            acloud_config: An AcloudConfig object.
            oauth2_credentials: An oauth2client.OAuth2Credentials instance.
        """
        super(AndroidComputeClient, self).__init__(acloud_config,
                                                   oauth2_credentials)
        self._zone = acloud_config.zone
        self._machine_type = acloud_config.machine_type
        self._min_machine_size = acloud_config.min_machine_size
        self._network = acloud_config.network
        self._orientation = acloud_config.orientation
        self._resolution = acloud_config.resolution
        self._metadata = acloud_config.metadata_variable.copy()
        self._ssh_public_key_path = acloud_config.ssh_public_key_path
        self._launch_args = acloud_config.launch_args
        self._instance_name_pattern = acloud_config.instance_name_pattern
        self._AddPerInstanceSshkey()

    def _AddPerInstanceSshkey(self):
        """Add per-instance ssh key.

        Assign the ssh publick key to instacne then use ssh command to
        control remote instance via the ssh publick key. Added sshkey for two
        users. One is vsoc01, another is current user.

        """
        if self._ssh_public_key_path:
            rsa = self._LoadSshPublicKey(self._ssh_public_key_path)
            logger.info("ssh_public_key_path is specified in config: %s, "
                        "will add the key to the instance.",
                        self._ssh_public_key_path)
            self._metadata["sshKeys"] = "{0}:{2}\n{1}:{2}".format(getpass.getuser(),
                                                                  constants.GCE_USER,
                                                                  rsa)
        else:
            logger.warning(
                "ssh_public_key_path is not specified in config, "
                "only project-wide key will be effective.")

    @classmethod
    def _FormalizeName(cls, name):
        """Formalize the name to comply with RFC1035.

        The name must be 1-63 characters long and match the regular expression
        [a-z]([-a-z0-9]*[a-z0-9])? which means the first character must be a
        lowercase letter, and all following characters must be a dash,
        lowercase letter, or digit, except the last character, which cannot be
        a dash.

        Args:
          name: A string.

        Returns:
          name: A string that complies with RFC1035.
        """
        name = name.replace("_", "-").lower()
        name = name[:cls.NAME_LENGTH_LIMIT]
        if name[-1] == "-":
            name = name[:-1] + cls.REPLACER
        return name

    def _CheckMachineSize(self):
        """Check machine size.

        Check if the desired machine type |self._machine_type| meets
        the requirement of minimum machine size specified as
        |self._min_machine_size|.

        Raises:
            errors.DriverError: if check fails.
        """
        if self.CompareMachineSize(self._machine_type, self._min_machine_size,
                                   self._zone) < 0:
            raise errors.DriverError(
                "%s does not meet the minimum required machine size %s" %
                (self._machine_type, self._min_machine_size))

    @classmethod
    def GenerateImageName(cls, build_target=None, build_id=None):
        """Generate an image name given build_target, build_id.

        Args:
            build_target: Target name, e.g. "aosp_cf_x86_phone-userdebug"
            build_id: Build id, a string, e.g. "2263051", "P2804227"

        Returns:
            A string, representing image name.
        """
        if not build_target and not build_id:
            return "image-" + uuid.uuid4().hex
        name = cls.IMAGE_NAME_FMT.format(
            build_target=build_target,
            build_id=build_id,
            uuid=uuid.uuid4().hex[:8])
        return cls._FormalizeName(name)

    @classmethod
    def GetDataDiskName(cls, instance):
        """Get data disk name for an instance.

        Args:
            instance: An instance_name.

        Returns:
            The corresponding data disk name.
        """
        name = cls.DATA_DISK_NAME_FMT.format(instance=instance)
        return cls._FormalizeName(name)

    def GenerateInstanceName(self, build_target=None, build_id=None):
        """Generate an instance name given build_target, build_id.

        Target is not used as instance name has a length limit.

        Args:
            build_target: Target name, e.g. "aosp_cf_x86_phone-userdebug"
            build_id: Build id, a string, e.g. "2263051", "P2804227"

        Returns:
            A string, representing instance name.
        """
        name = self._instance_name_pattern.format(build_target=build_target,
                                                  build_id=build_id,
                                                  uuid=uuid.uuid4().hex[:8])
        return self._FormalizeName(name)

    def CreateDisk(self,
                   disk_name,
                   source_image,
                   size_gb,
                   zone=None,
                   source_project=None,
                   disk_type=gcompute_client.PersistentDiskType.STANDARD):
        """Create a gce disk.

        Args:
            disk_name: String, name of disk.
            source_image: String, name to the image name.
            size_gb: Integer, size in gigabytes.
            zone: String, name of the zone, e.g. us-central1-b.
            source_project: String, required if the image is located in a different
                            project.
            disk_type: String, a value from PersistentDiskType, STANDARD
                       for regular hard disk or SSD for solid state disk.
        """
        if self.CheckDiskExists(disk_name, self._zone):
            raise errors.DriverError(
                "Failed to create disk %s, already exists." % disk_name)
        if source_image and not self.CheckImageExists(source_image):
            raise errors.DriverError(
                "Failed to create disk %s, source image %s does not exist." %
                (disk_name, source_image))
        super(AndroidComputeClient, self).CreateDisk(
            disk_name,
            source_image=source_image,
            size_gb=size_gb,
            zone=zone or self._zone)

    @staticmethod
    def _LoadSshPublicKey(ssh_public_key_path):
        """Load the content of ssh public key from a file.

        Args:
            ssh_public_key_path: String, path to the public key file.
                               E.g. ~/.ssh/acloud_rsa.pub
        Returns:
            String, content of the file.

        Raises:
            errors.DriverError if the public key file does not exist
            or the content is not valid.
        """
        key_path = os.path.expanduser(ssh_public_key_path)
        if not os.path.exists(key_path):
            raise errors.DriverError(
                "SSH public key file %s does not exist." % key_path)

        with open(key_path) as f:
            rsa = f.read()
            rsa = rsa.strip() if rsa else rsa
            utils.VerifyRsaPubKey(rsa)
        return rsa

    # pylint: disable=too-many-locals, arguments-differ
    @utils.TimeExecute("Creating GCE Instance")
    def CreateInstance(self,
                       instance,
                       image_name,
                       machine_type=None,
                       metadata=None,
                       network=None,
                       zone=None,
                       disk_args=None,
                       image_project=None,
                       gpu=None,
                       extra_disk_name=None,
                       avd_spec=None,
                       extra_scopes=None,
                       tags=None):
        """Create a gce instance with a gce image.

        Args:
            instance: String, instance name.
            image_name: String, source image used to create this disk.
            machine_type: String, representing machine_type,
                          e.g. "n1-standard-1"
            metadata: Dict, maps a metadata name to its value.
            network: String, representing network name, e.g. "default"
            zone: String, representing zone name, e.g. "us-central1-f"
            disk_args: A list of extra disk args (strings), see _GetDiskArgs
                       for example, if None, will create a disk using the given
                       image.
            image_project: String, name of the project where the image
                           belongs. Assume the default project if None.
            gpu: String, type of gpu to attach. e.g. "nvidia-tesla-k80", if
                 None no gpus will be attached. For more details see:
                 https://cloud.google.com/compute/docs/gpus/add-gpus
            extra_disk_name: String,the name of the extra disk to attach.
            avd_spec: AVDSpec object that tells us what we're going to create.
            extra_scopes: List, extra scopes (strings) to be passed to the
                          instance.
            tags: A list of tags to associate with the instance. e.g.
                 ["http-server", "https-server"]
        """
        self._CheckMachineSize()
        disk_args = self._GetDiskArgs(instance, image_name)
        metadata = self._metadata.copy()
        metadata["cfg_sta_display_resolution"] = self._resolution
        metadata["t_force_orientation"] = self._orientation
        metadata[constants.INS_KEY_AVD_TYPE] = avd_spec.avd_type

        # Use another METADATA_DISPLAY to record resolution which will be
        # retrieved in acloud list cmd. We try not to use cvd_01_x_res
        # since cvd_01_xxx metadata is going to deprecated by cuttlefish.
        metadata[constants.INS_KEY_DISPLAY] = ("%sx%s (%s)" % (
            avd_spec.hw_property[constants.HW_X_RES],
            avd_spec.hw_property[constants.HW_Y_RES],
            avd_spec.hw_property[constants.HW_ALIAS_DPI]))

        super(AndroidComputeClient, self).CreateInstance(
            instance, image_name, self._machine_type, metadata, self._network,
            self._zone, disk_args, image_project, gpu, extra_disk_name,
            extra_scopes=extra_scopes, tags=tags)

    def CheckBootFailure(self, serial_out, instance):
        """Determine if serial output has indicated any boot failure.

        Subclass has to define this function to detect failures
        in the boot process

        Args:
            serial_out: string
            instance: string, instance name.

        Raises:
            Raises errors.DeviceBootError exception if a failure is detected.
        """
        pass

    def CheckBoot(self, instance):
        """Check once to see if boot completes.

        Args:
            instance: string, instance name.

        Returns:
            True if the BOOT_COMPLETED_MSG or BOOT_STARTED_MSG appears in serial
            port output, otherwise False.
        """
        try:
            serial_out = self.GetSerialPortOutput(instance=instance, port=1)
            self.CheckBootFailure(serial_out, instance)
            return ((self.BOOT_COMPLETED_MSG in serial_out)
                    or (self.BOOT_STARTED_MSG in serial_out))
        except errors.HttpError as e:
            if e.code == 400:
                logger.debug("CheckBoot: Instance is not ready yet %s", str(e))
                return False
            raise

    def WaitForBoot(self, instance, boot_timeout_secs=None):
        """Wait for boot to completes or hit timeout.

        Args:
            instance: string, instance name.
            boot_timeout_secs: Integer, the maximum time in seconds used to
                               wait for the AVD to boot.
        """
        boot_timeout_secs = boot_timeout_secs or self.BOOT_TIMEOUT_SECS
        logger.info("Waiting for instance to boot up %s for %s secs",
                    instance, boot_timeout_secs)
        timeout_exception = errors.DeviceBootTimeoutError(
            "Device %s did not finish on boot within timeout (%s secs)" %
            (instance, boot_timeout_secs)),
        utils.PollAndWait(
            func=self.CheckBoot,
            expected_return=True,
            timeout_exception=timeout_exception,
            timeout_secs=boot_timeout_secs,
            sleep_interval_secs=self.BOOT_CHECK_INTERVAL_SECS,
            instance=instance)
        logger.info("Instance boot completed: %s", instance)

    def GetInstanceIP(self, instance, zone=None):
        """Get Instance IP given instance name.

        Args:
            instance: String, representing instance name.
            zone: String, representing zone name, e.g. "us-central1-f"

        Returns:
            ssh.IP object, that stores internal and external ip of the instance.
        """
        return super(AndroidComputeClient, self).GetInstanceIP(
            instance, zone or self._zone)

    def GetSerialPortOutput(self, instance, zone=None, port=1):
        """Get serial port output.

        Args:
            instance: string, instance name.
            zone: String, representing zone name, e.g. "us-central1-f"
            port: int, which COM port to read from, 1-4, default to 1.

        Returns:
            String, contents of the output.

        Raises:
            errors.DriverError: For malformed response.
        """
        return super(AndroidComputeClient, self).GetSerialPortOutput(
            instance, zone or self._zone, port)

    def GetInstanceNamesByIPs(self, ips, zone=None):
        """Get Instance names by IPs.

        This function will go through all instances, which
        could be slow if there are too many instances.  However, currently
        GCE doesn't support search for instance by IP.

        Args:
            ips: A set of IPs.
            zone: String, representing zone name, e.g. "us-central1-f"

        Returns:
            A dictionary where key is ip and value is instance name or None
            if instance is not found for the given IP.
        """
        return super(AndroidComputeClient, self).GetInstanceNamesByIPs(
            ips, zone or self._zone)
