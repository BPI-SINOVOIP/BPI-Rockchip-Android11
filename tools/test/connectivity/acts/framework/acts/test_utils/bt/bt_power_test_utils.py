#!/usr/bin/env python3
#
#   Copyright 2017 Google, Inc.
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
import time
import acts.test_utils.bt.BleEnum as bleenum
import acts.test_utils.instrumentation.device.command.instrumentation_command_builder as icb

BLE_LOCATION_SCAN_ENABLE = 'settings put global ble_scan_always_enabled 1'
BLE_LOCATION_SCAN_DISABLE = 'settings put global ble_scan_always_enabled 0'
SCREEN_WAIT_TIME = 1


class MediaControl(object):
    """Media control using adb shell for power testing.

    Object to control media play status using adb.
    """

    def __init__(self, android_device, music_file):
        """Initialize the media_control class.

        Args:
            android_dut: android_device object
            music_file: location of the music file
        """
        self.android_device = android_device
        self.music_file = music_file

    def player_on_foreground(self):
        """Turn on screen and make sure media play is on foreground

        All media control keycode only works when screen is on and media player
        is on the foreground. Turn off screen first and turn it on to make sure
        all operation is based on the same screen status. Otherwise, 'MENU' key
        would block command to be sent.
        """
        self.android_device.droid.goToSleepNow()
        time.sleep(SCREEN_WAIT_TIME)
        self.android_device.droid.wakeUpNow()
        time.sleep(SCREEN_WAIT_TIME)
        self.android_device.send_keycode('MENU')
        time.sleep(SCREEN_WAIT_TIME)

    def play(self):
        """Start playing music.

        """
        self.player_on_foreground()
        PLAY = 'am start -a android.intent.action.VIEW -d file://{} -t audio/wav'.format(
            self.music_file)
        self.android_device.adb.shell(PLAY)

    def pause(self):
        """Pause music.

        """
        self.player_on_foreground()
        self.android_device.send_keycode('MEDIA_PAUSE')

    def resume(self):
        """Pause music.

        """
        self.player_on_foreground()
        self.android_device.send_keycode('MEDIA_PLAY')

    def stop(self):
        """Stop music and close media play.

        """
        self.player_on_foreground()
        self.android_device.send_keycode('MEDIA_STOP')


def start_apk_ble_adv(dut, adv_mode, adv_power_level, adv_duration):
    """Trigger BLE advertisement from power-test.apk.

    Args:
        dut: Android device under test, type AndroidDevice obj
        adv_mode: The BLE advertisement mode.
            {0: 'LowPower', 1: 'Balanced', 2: 'LowLatency'}
        adv_power_leve: The BLE advertisement TX power level.
            {0: 'UltraLowTXPower', 1: 'LowTXPower', 2: 'MediumTXPower,
            3: HighTXPower}
        adv_duration: duration of advertisement in seconds, type int
    """

    adv_duration = str(adv_duration) + 's'
    builder = icb.InstrumentationTestCommandBuilder.default()
    builder.add_test_class(
        "com.google.android.device.power.tests.ble.BleAdvertise")
    builder.set_manifest_package("com.google.android.device.power")
    builder.set_runner("androidx.test.runner.AndroidJUnitRunner")
    builder.add_key_value_param("cool-off-duration", "0s")
    builder.add_key_value_param("idle-duration", "0s")
    builder.add_key_value_param(
        "com.android.test.power.receiver.ADVERTISE_MODE", adv_mode)
    builder.add_key_value_param("com.android.test.power.receiver.POWER_LEVEL",
                                adv_power_level)
    builder.add_key_value_param(
        "com.android.test.power.receiver.ADVERTISING_DURATION", adv_duration)

    adv_command = builder.build() + ' &'
    logging.info('Start BLE {} at {} for {} seconds'.format(
        bleenum.AdvertiseSettingsAdvertiseMode(adv_mode).name,
        bleenum.AdvertiseSettingsAdvertiseTxPower(adv_power_level).name,
        adv_duration))
    dut.adb.shell_nb(adv_command)


def start_apk_ble_scan(dut, scan_mode, scan_duration):
    """Build the command to trigger BLE scan from power-test.apk.

    Args:
        dut: Android device under test, type AndroidDevice obj
        scan_mode: The BLE scan mode.
            {0: 'LowPower', 1: 'Balanced', 2: 'LowLatency', -1: 'Opportunistic'}
        scan_duration: duration of scan in seconds, type int
    Returns:
        adv_command: the command for BLE scan
    """
    scan_duration = str(scan_duration) + 's'
    builder = icb.InstrumentationTestCommandBuilder.default()
    builder.add_test_class("com.google.android.device.power.tests.ble.BleScan")
    builder.set_manifest_package("com.google.android.device.power")
    builder.set_runner("androidx.test.runner.AndroidJUnitRunner")
    builder.add_key_value_param("cool-off-duration", "0s")
    builder.add_key_value_param("idle-duration", "0s")
    builder.add_key_value_param("com.android.test.power.receiver.SCAN_MODE",
                                scan_mode)
    builder.add_key_value_param("com.android.test.power.receiver.MATCH_MODE",
                                2)
    builder.add_key_value_param(
        "com.android.test.power.receiver.SCAN_DURATION", scan_duration)
    builder.add_key_value_param(
        "com.android.test.power.receiver.CALLBACK_TYPE", 1)
    builder.add_key_value_param("com.android.test.power.receiver.FILTER",
                                'true')

    scan_command = builder.build() + ' &'
    logging.info('Start BLE {} scans for {} seconds'.format(
        bleenum.ScanSettingsScanMode(scan_mode).name, scan_duration))
    dut.adb.shell_nb(scan_command)
