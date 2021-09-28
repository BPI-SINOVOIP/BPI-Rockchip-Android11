#!/usr/bin/env python3
#
#   Copyright 2019 - Google, Inc.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import logging
from acts.libs.proc import job


class ApIwconfigError(Exception):
    """Error related to configuring the wireless interface via iwconfig."""


class ApIwconfig(object):
    """Class to configure wireless interface via iwconfig

    """

    PROGRAM_FILE = '/usr/local/sbin/iwconfig'

    def __init__(self, ap):
        """Initialize the ApIwconfig class.

        Args:
            ap: the ap object within ACTS
        """
        self.ssh = ap.ssh

    def ap_iwconfig(self, interface, arguments=None):
        """Configure the wireless interface using iwconfig.

        Returns:
            output: the output of the command, if any
        """
        iwconfig_command = '%s %s %s' % (self.PROGRAM_FILE, interface,
                                         arguments)
        output = self.ssh.run(iwconfig_command)

        return output
