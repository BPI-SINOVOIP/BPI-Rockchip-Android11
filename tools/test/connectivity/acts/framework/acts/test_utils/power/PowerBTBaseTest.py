#!/usr/bin/env python3.4
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import os
import time
import acts.test_utils.bt.bt_power_test_utils as btputils
import acts.test_utils.bt.bt_test_utils as btutils
import acts.test_utils.power.PowerBaseTest as PBT
from acts.test_utils.abstract_devices.bluetooth_handsfree_abstract_device import BluetoothHandsfreeAbstractDeviceFactory as bt_factory
from math import copysign

BLE_LOCATION_SCAN_DISABLE = 'settings put secure location_mode 0'
PHONE_MUSIC_FILE_DIRECTORY = '/sdcard/Music'
INIT_ATTEN = [0]


def ramp_attenuation(obj_atten, attenuation_target):
    """Ramp the attenuation up or down for BT tests.

    Ramp the attenuation slowly so it won't have dramatic signal drop to affect
    Link.

    Args:
        obj_atten: attenuator object, a single port attenuator
        attenuation_target: target attenuation level to reach to.
    """
    attenuation_step_max = 20
    sign = lambda x: copysign(1, x)
    attenuation_delta = obj_atten.get_atten() - attenuation_target
    while abs(attenuation_delta) > attenuation_step_max:
        attenuation_intermediate = obj_atten.get_atten(
        ) - sign(attenuation_delta) * attenuation_step_max
        obj_atten.set_atten(attenuation_intermediate)
        time.sleep(5)
        attenuation_delta = obj_atten.get_atten() - attenuation_target
    obj_atten.set_atten(attenuation_target)


class PowerBTBaseTest(PBT.PowerBaseTest):
    """Base class for BT power related tests.

    Inherited from the PowerBaseTest class
    """
    def setup_class(self):

        super().setup_class()
        # Get music file and push it to the phone
        music_files = self.user_params.get('music_files', [])
        if music_files:
            music_src = music_files[0]
            music_dest = PHONE_MUSIC_FILE_DIRECTORY
            success = self.dut.push_system_file(music_src, music_dest)
            if success:
                self.music_file = os.path.join(PHONE_MUSIC_FILE_DIRECTORY,
                                               os.path.basename(music_src))
            # Initialize media_control class
            self.media = btputils.MediaControl(self.dut, self.music_file)
        # Set Attenuator to the initial attenuation
        if hasattr(self, 'attenuators'):
            self.set_attenuation(INIT_ATTEN)
            self.attenuator = self.attenuators[0]
        # Create the BTOE(Bluetooth-Other-End) device object
        bt_devices = self.user_params.get('bt_devices', [])
        if bt_devices:
            attr, idx = bt_devices.split(':')
            self.bt_device_controller = getattr(self, attr)[int(idx)]
            self.bt_device = bt_factory().generate(self.bt_device_controller)
        else:
            self.log.error('No BT devices config is provided!')
        # Turn off screen as all tests will be screen off
        self.dut.droid.goToSleepNow()

    def setup_test(self):

        super().setup_test()
        self.unpack_userparams(volume=0.9)
        # Reset BT to factory defaults
        self.dut.droid.bluetoothFactoryReset()
        self.bt_device.reset()
        self.bt_device.power_on()
        btutils.enable_bluetooth(self.dut.droid, self.dut.ed)

    def teardown_test(self):
        """Tear down necessary objects after test case is finished.

        Bring down the AP interface, delete the bridge interface, stop the
        packet sender, and reset the ethernet interface for the packet sender
        """
        super().teardown_test()
        self.dut.droid.bluetoothFactoryReset()
        self.dut.adb.shell(BLE_LOCATION_SCAN_DISABLE)
        if hasattr(self, 'media'):
            self.media.stop()
        # Set Attenuator to the initial attenuation
        if hasattr(self, 'attenuators'):
            self.set_attenuation(INIT_ATTEN)
        self.bt_device.reset()
        self.bt_device.power_off()
        btutils.disable_bluetooth(self.dut.droid)

    def teardown_class(self):
        """Clean up the test class after tests finish running

        """
        super().teardown_class()
        self.dut.droid.bluetoothFactoryReset()
        self.dut.adb.shell(BLE_LOCATION_SCAN_DISABLE)
        self.bt_device.reset()
        self.bt_device.power_off()
        btutils.disable_bluetooth(self.dut.droid)
