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
"""A client that manages Cheeps Virtual Device on compute engine.

** CheepsComputeClient **

CheepsComputeClient derives from AndroidComputeClient. It manges a google
compute engine project that is setup for running Cheeps Virtual Devices.
It knows how to create a host instance from a Cheeps Stable Host Image, fetch
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
       cheeps_compute_client.CheepsComputeClient
"""

import logging

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import gcompute_client


logger = logging.getLogger(__name__)


class CheepsComputeClient(android_compute_client.AndroidComputeClient):
    """Client that manages Cheeps based Android Virtual Device.

    Cheeps is a VM that run Chrome OS which runs on GCE.
    """
    # This is the timeout for betty to start.
    BOOT_TIMEOUT_SECS = 15*60
    # This is printed by betty.sh.
    BOOT_COMPLETED_MSG = "VM successfully started"
    # systemd prints this if betty.sh returns nonzero status code.
    BOOT_FAILED_MSG = "betty.service: Failed with result 'exit-code'"

    def CheckBootFailure(self, serial_out, instance):
        """Overrides superclass. Determines if there's a boot failure."""
        if self.BOOT_FAILED_MSG in serial_out:
            raise errors.DeviceBootError("Betty failed to start")

    # pylint: disable=too-many-locals,arguments-differ
    def CreateInstance(self, instance, image_name, image_project, avd_spec):
        """ Creates a cheeps instance in GCE.

        Args:
            instance: name of the VM
            image_name: the GCE image to use
            image_project: project the GCE image is in
            avd_spec: An AVDSpec instance.
        """
        metadata = self._metadata.copy()
        metadata[constants.INS_KEY_AVD_TYPE] = constants.TYPE_CHEEPS
        # Update metadata by avd_spec
        if avd_spec:
            metadata["cvd_01_x_res"] = avd_spec.hw_property[constants.HW_X_RES]
            metadata["cvd_01_y_res"] = avd_spec.hw_property[constants.HW_Y_RES]
            metadata["cvd_01_dpi"] = avd_spec.hw_property[constants.HW_ALIAS_DPI]
            metadata[constants.INS_KEY_DISPLAY] = ("%sx%s (%s)" % (
                avd_spec.hw_property[constants.HW_X_RES],
                avd_spec.hw_property[constants.HW_Y_RES],
                avd_spec.hw_property[constants.HW_ALIAS_DPI]))

            if avd_spec.username:
                metadata["user"] = avd_spec.username
                metadata["password"] = avd_spec.password

            if avd_spec.remote_image[constants.BUILD_ID]:
                metadata['android_build_id'] = avd_spec.remote_image[constants.BUILD_ID]

            if avd_spec.remote_image[constants.BUILD_TARGET]:
                metadata['android_build_target'] = avd_spec.remote_image[constants.BUILD_TARGET]

        gcompute_client.ComputeClient.CreateInstance(
            self,
            instance=instance,
            image_name=image_name,
            image_project=image_project,
            disk_args=None,
            metadata=metadata,
            machine_type=self._machine_type,
            network=self._network,
            zone=self._zone)
