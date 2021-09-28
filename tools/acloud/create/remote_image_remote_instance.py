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
r"""RemoteImageRemoteInstance class.

Create class that is responsible for creating a remote instance AVD with a
remote image.
"""
from acloud.create import base_avd_create
from acloud.internal.lib import utils
from acloud.public.actions import common_operations
from acloud.public.actions import remote_instance_cf_device_factory
from acloud.internal import constants


class RemoteImageRemoteInstance(base_avd_create.BaseAVDCreate):
    """Create class for a remote image remote instance AVD."""

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
        device_factory = remote_instance_cf_device_factory.RemoteInstanceDeviceFactory(
            avd_spec)
        report = common_operations.CreateDevices(
            "create_cf", avd_spec.cfg, device_factory, avd_spec.num,
            report_internal_ip=avd_spec.report_internal_ip,
            autoconnect=avd_spec.autoconnect,
            avd_type=constants.TYPE_CF,
            boot_timeout_secs=avd_spec.boot_timeout_secs,
            unlock_screen=avd_spec.unlock_screen,
            wait_for_boot=False)
        # Launch vnc client if we're auto-connecting.
        if avd_spec.connect_vnc:
            utils.LaunchVNCFromReport(report, avd_spec, no_prompts)
        if avd_spec.connect_webrtc:
            utils.LaunchBrowserFromReport(report)

        return report
