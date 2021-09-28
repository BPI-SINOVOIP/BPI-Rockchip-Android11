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

"""Base device factory.

BaseDeviceFactory provides basic interface to create a device factory.

"""


class BaseDeviceFactory(object):
    """A class that provides basic interface to create a device factory."""

    LATEST = "latest"

    def __init__(self, compute_client):

        self._compute_client = compute_client

    def GetComputeClient(self):
        """To get client object.

        Returns:
          Returns an instance of gcompute_client.ComputeClient or its subclass.
        """
        return self._compute_client

    # pylint: disable=no-self-use
    def CreateInstance(self):
        """Creates single configured device.

        Subclasses has to define this function

        Returns:
          The name of the created instance.
        """
        return

    # pylint: disable=no-self-use
    def GetBuildInfoDict(self):
        """Get build info dictionary.

        Returns:
          A build info dictionary.
        """
        return None
