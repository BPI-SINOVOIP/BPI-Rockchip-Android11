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
"""Base setup subtask runner.

BaseTaskRunner defines basic methods which will be called in the setup process.

the flow in each child task runner will be in below manner:
Check ShouldRun() -> PrintWelcomMessage() -> _Run()
"""

from __future__ import print_function
import logging
import os
import textwrap


logger = logging.getLogger(__name__)
_PARAGRAPH_BREAK = "="


class BaseTaskRunner(object):
    """A basic task runner class for setup cmd."""

    # WELCOME_MESSAGE and WELCOME_MESSAGE_TITLE should both be defined as
    # strings.
    WELCOME_MESSAGE = None
    WELCOME_MESSAGE_TITLE = None

    def PrintWelcomeMessage(self):
        """Print out the welcome message in a fancy format.

        This method will print out the welcome message in the following manner
        given the following example:
        e.g.
        WELCOME_MESSAGE_TITLE = "title"
        WELCOME_MESSAGE = (
            "this is one long str "
            "broken into multiple lines "
            "based on the screen width"
        )

        actual output will be:
        ===========================
         [title]
         this is one long str
         broken into multiple lines
         based on the screen width
        ===========================
        """
        if not self.WELCOME_MESSAGE and not self.WELCOME_MESSAGE_TITLE:
            logger.debug("No welcome message for %s", self.__class__.__name__)
            return

        # define the layout of message.
        console_width = int(os.popen('stty size', 'r').read().split()[1])
        break_width = int(console_width / 2)

        # start to print welcome message.
        print("\n" +_PARAGRAPH_BREAK * break_width)
        print(" [%s] " % self.WELCOME_MESSAGE_TITLE)
        print(textwrap.fill(
            self.WELCOME_MESSAGE,
            break_width - 2,
            initial_indent=" ",
            subsequent_indent=" "))
        print(_PARAGRAPH_BREAK * break_width + "\n")

    # pylint: disable=no-self-use
    def ShouldRun(self):
        """Check if setup should run.

        Returns:
            Boolean, True if setup should run False otherwise.
        """
        return True

    def Run(self, force_setup=False):
        """Main entry point to the task runner.

        Args:
            force_setup: Boolean, True to force execute Run method no matter
                         the result of ShoudRun.
        """
        if self.ShouldRun() or force_setup:
            self.PrintWelcomeMessage()
            self._Run()
        else:
            logger.info("Skipping setup step: %s", self.__class__.__name__)

    def _Run(self):
        """run the setup procedure."""
        raise NotImplementedError()
