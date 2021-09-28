#!/usr/bin/env python3
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

"""A stub implementation of a DUT interface.

This a stub interface which allows automated test to run
without automating the hardware. This here for two reasons, first
as an example of how to write a dut implementation, and second as
an implementation that can be used to test case without writing
out the full implementation.
"""

import logging

class BluethoothDevice:
    """The api interface used in the test for the stub.

    This is interface which defines all the functions that can be
    called by the bt test suite.
    """

    def __init__(self, config):
        print('Init Stub with ', config)
        logging.info('Init Stub with '+str(config))

    def answer_phone(self):
        input('Answer the phone and then press enter\n')

    def hang_up(self):
        input('Hang up the phone and then press enter\n')

    def toggle_pause(self):
        input('Press pause on device then press enter\n')

    def volume(self, direction):
        """Adjust the volume specified by the value of direction.

        Args:
            direction: A string that is either UP or DOWN
            that indicates which way to adjust the volume.
        """

        return input('move volume '+direction+' and then press enter\n')

    def connect(self, android):
        input('Connect device and press enter\n')

    def is_bt_connected(self):
        con = input('Is device connected? y/n').lower()
        while con not in ['y', 'n']:
            con = input('Is device connected? y/n').lower()
        return con == 'y'

    def close(self):
        """This where the hardware is released.
        """
        print('Close Stub')
        logging.info('Close Stub')

