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
r"""BaseAVDCreate class.

Parent class that will hold common logic for AVD creation use cases.
"""

from acloud.internal import constants
from acloud.internal.lib import utils


class BaseAVDCreate(object):
    """Base class for all AVD intance creation classes."""

    def _CreateAVD(self, avd_spec, no_prompts):
        """Do the actual creation work, should be overridden by child classes.

        Args:
            avd_spec: AVDSpec object that tells us what we're going to create.
            no_prompts: Boolean, True to skip all prompts.
        """
        raise NotImplementedError

    def Create(self, avd_spec, no_prompts):
        """Create the AVD.

        Args:
            avd_spec: AVDSpec object that tells us what we're going to create.
            no_prompts: Boolean, True to skip all prompts.
        """
        self.PrintAvdDetails(avd_spec)
        results = self._CreateAVD(avd_spec, no_prompts)
        utils.PrintDeviceSummary(results)
        return results

    @staticmethod
    def PrintAvdDetails(avd_spec):
        """Display spec information to user.

        Example:
            Creating remote AVD instance with the following details:
            Image:
              aosp/master - aosp_cf_x86_phone-userdebug [1234]
            hw config:
              cpu - 2
              ram - 2GB
              disk - 10GB
              display - 1024x862 (224 DPI)

        Args:
            avd_spec: AVDSpec object that tells us what we're going to create.
        """
        utils.PrintColorString(
            "Creating %s AVD instance with the following details:" %
            avd_spec.instance_type)
        if avd_spec.image_source == constants.IMAGE_SRC_LOCAL:
            utils.PrintColorString("Image (local):")
            utils.PrintColorString("  %s" % (avd_spec.local_image_dir or
                                             avd_spec.local_image_artifact))
        elif avd_spec.image_source == constants.IMAGE_SRC_REMOTE:
            utils.PrintColorString("Image:")
            utils.PrintColorString(
                "  %s - %s [%s]" %
                (avd_spec.remote_image[constants.BUILD_BRANCH],
                 avd_spec.remote_image[constants.BUILD_TARGET],
                 avd_spec.remote_image[constants.BUILD_ID]))
        utils.PrintColorString("hw config:")
        utils.PrintColorString("  cpu - %s" % (avd_spec.hw_property[constants.HW_ALIAS_CPUS]))
        utils.PrintColorString("  ram - %dGB" % (
            int(avd_spec.hw_property[constants.HW_ALIAS_MEMORY]) / 1024))
        if constants.HW_ALIAS_DISK in avd_spec.hw_property:
            utils.PrintColorString("  disk - %dGB" % (
                int(avd_spec.hw_property[constants.HW_ALIAS_DISK]) / 1024))
        utils.PrintColorString(
            "  display - %sx%s (%s DPI)" %
            (avd_spec.hw_property[constants.HW_X_RES],
             avd_spec.hw_property[constants.HW_Y_RES],
             avd_spec.hw_property[constants.HW_ALIAS_DPI]))
        utils.PrintColorString("\n")
