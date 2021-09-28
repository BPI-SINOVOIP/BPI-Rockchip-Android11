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
r"""GceLocalImageRemoteInstance class.

Create class that is responsible for creating a gce remote instance AVD with a
local image.
"""

import logging

from acloud.create import base_avd_create
from acloud.internal.lib import utils
from acloud.public import device_driver


logger = logging.getLogger(__name__)


class GceLocalImageRemoteInstance(base_avd_create.BaseAVDCreate):
    """Create class for a gce local image remote instance AVD."""

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
        logger.info("GCE local image: %s", avd_spec.local_image_artifact)

        report = device_driver.CreateGCETypeAVD(
            avd_spec.cfg,
            num=avd_spec.num,
            local_disk_image=avd_spec.local_image_artifact,
            autoconnect=avd_spec.autoconnect,
            report_internal_ip=avd_spec.report_internal_ip,
            avd_spec=avd_spec)

        # Launch vnc client if we're auto-connecting.
        if avd_spec.connect_vnc:
            utils.LaunchVNCFromReport(report, avd_spec, no_prompts)

        return report
