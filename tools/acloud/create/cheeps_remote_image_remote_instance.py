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
r"""CheepsRemoteImageRemoteInstance class.

Create class that is responsible for creating a cheeps remote instance AVD with
a remote image.
"""
import logging

from acloud.create import base_avd_create
from acloud.internal import constants
from acloud.internal.lib import auth
from acloud.internal.lib import cheeps_compute_client
from acloud.internal.lib import utils
from acloud.public.actions import base_device_factory
from acloud.public.actions import common_operations


logger = logging.getLogger(__name__)


class CheepsRemoteImageRemoteInstance(base_avd_create.BaseAVDCreate):
    """Create class for a Cheeps remote image remote instance AVD."""

    @utils.TimeExecute(function_description="Total time: ",
                       print_before_call=False, print_status=False)
    def _CreateAVD(self, avd_spec, no_prompts):
        """Create the AVD.

        Args:
            avd_spec: AVDSpec object that tells us what we're going to create.
            no_prompts: Boolean, True to skip all prompts.

        Returns:
            A Report instance.
        """
        logger.info(
            "Creating a cheeps device in project %s, build_id: %s",
            avd_spec.cfg.project, avd_spec.remote_image[constants.BUILD_ID])

        device_factory = CheepsDeviceFactory(avd_spec.cfg, avd_spec)

        report = common_operations.CreateDevices(
            command="create_cheeps",
            cfg=avd_spec.cfg,
            device_factory=device_factory,
            num=avd_spec.num,
            report_internal_ip=avd_spec.report_internal_ip,
            autoconnect=avd_spec.autoconnect,
            avd_type=constants.TYPE_CHEEPS,
            client_adb_port=avd_spec.client_adb_port,
            boot_timeout_secs=avd_spec.boot_timeout_secs)

        # Launch vnc client if we're auto-connecting.
        if avd_spec.connect_vnc:
            utils.LaunchVNCFromReport(report, avd_spec, no_prompts)

        return report


class CheepsDeviceFactory(base_device_factory.BaseDeviceFactory):
    """A class that can produce a cheeps device.

    Attributes:
        _cfg: An AcloudConfig instance.

    """
    LOG_FILES = []

    def __init__(self, cfg, avd_spec=None):
        """Initialize.

        Args:
            cfg: An AcloudConfig instance.
            avd_spec: An AVDSpec instance.
        """
        self.credentials = auth.CreateCredentials(cfg)

        compute_client = cheeps_compute_client.CheepsComputeClient(
            cfg, self.credentials)
        super(CheepsDeviceFactory, self).__init__(compute_client)

        self._cfg = cfg
        self._avd_spec = avd_spec

    def GetBuildInfoDict(self):
        """Get build info dictionary.

        Returns:
          A build info dictionary.
        """
        return {"build_id": self._avd_spec.remote_image[constants.BUILD_ID]}

    def CreateInstance(self):
        """Creates single configured cheeps device.

        Returns:
            String, the name of created instance.
        """
        instance = self._compute_client.GenerateInstanceName(
            build_id=self._avd_spec.remote_image[constants.BUILD_ID],
            build_target=self._avd_spec.remote_image[constants.BUILD_TARGET])

        # Cheeps image specified through args (if any) overrides that in the
        # Acloud config file.
        image_name = (self._avd_spec.stable_cheeps_host_image_name or
                      self._cfg.stable_cheeps_host_image_name)
        image_project = (self._avd_spec.stable_cheeps_host_image_project or
                         self._cfg.stable_cheeps_host_image_project)
        if not (image_name and image_project):
            raise ValueError(
                "Both Cheeps image name and project should be set, either in "
                "Acloud config or via command line args.")

        self._compute_client.CreateInstance(
            instance=instance,
            image_name=image_name,
            image_project=image_project,
            avd_spec=self._avd_spec)
        return instance
