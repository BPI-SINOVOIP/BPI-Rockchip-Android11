#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
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

import time
from acts import signals
from acts.controllers.buds_lib import apollo_lib

AVRCP_WAIT_TIME = 3


def get_serial_object(pri_ad, serial_device):
    """This function will creates object for serial device connected.

    Args:
        pri_ad: Android device.
        serial_device: serial device connected.

    Returns:
        object of serial device, otherwise Abort the class.
    """
    devices = apollo_lib.get_devices()
    for device in devices:
        if device['serial_number'] in serial_device:
            return apollo_lib.BudsDevice(device['serial_number'])
    pri_ad.log.error('Apollo device not found')
    raise signals.TestAbortAll('Apollo device not found')


def avrcp_actions(pri_ad, buds_device):
    """Performs avrcp controls like volume up, volume down

    Args:
        pri_ad: Android device.
        buds_device: serial device object to perform avrcp actions.

    Returns:
        True if successful, otherwise otherwise raises Exception. 
    """
    pri_ad.log.debug("Setting voume to 0")
    pri_ad.droid.setMediaVolume(0)
    current_volume = pri_ad.droid.getMediaVolume()
    pri_ad.log.info('Current volume to {}'.format(current_volume))
    for _ in range(5):
        buds_device.volume('Up')
        time.sleep(AVRCP_WAIT_TIME)
    pri_ad.log.info('Volume increased to {}'.format(
        pri_ad.droid.getMediaVolume()))
    if current_volume == pri_ad.droid.getMediaVolume():
        pri_ad.log.error('Increase volume failed')
        raise signals.TestFailure("Increase volume failed")
    current_volume = pri_ad.droid.getMediaVolume()
    for _ in range(5):
        buds_device.volume('Down')
        time.sleep(AVRCP_WAIT_TIME)
    pri_ad.log.info('Volume decreased to {}'.format(
        pri_ad.droid.getMediaVolume()))
    if current_volume == pri_ad.droid.getMediaVolume():
        pri_ad.log.error('Decrease volume failed')
        raise signals.TestFailure("Decrease volume failed")
    return True
