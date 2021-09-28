#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
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
"""
A comprehensive interface for performing test actions on an Apollo device.
"""

import time

from acts.controllers.buds_lib.apollo_lib import DeviceError
from acts.controllers.buds_lib.test_actions.agsa_acts import AgsaOTAError
from acts.controllers.buds_lib.test_actions.base_test_actions import BaseTestAction
from acts.controllers.buds_lib.test_actions.base_test_actions import timed_action
from acts.controllers.buds_lib.test_actions.bt_utils import BTUtils
from acts.libs.utils.timer import TimeRecorder
from acts.test_utils.tel.tel_test_utils import initiate_call
from acts.test_utils.tel.tel_test_utils import wait_for_droid_in_call
from acts.utils import wait_until

PACKAGE_NAME_AGSA = 'com.google.android.googlequicksearchbox'
PACKAGE_NAME_GMS = 'com.google.android.gms'
PACKAGE_NAME_NEARBY = 'com.google.android.gms.policy_nearby'
PACKAGE_NAME_SETTINGS = 'com.android.settings'
BISTO_MP_DETECT_HEADER = 'Pixel Buds'
BISTO_MP_DEVICE_TEXT = 'Pixel Buds'
BISTO_MP_DETECT_TEXT = BISTO_MP_DETECT_HEADER + BISTO_MP_DEVICE_TEXT
BISTO_MP_CANCEL_TEXT = 'CANCEL'
BISTO_MP_CONNECT_TEXT = 'TAP TO CONNECT'
BISTO_MP_CONNECT_FAIL_TEXT = 'Can\'t connect to'
BISTO_MP_CONNECT_RETRY_TEXT = 'TRY AGAIN'
BISTO_MP_CONNECTED_TEXT = 'Now set up your Google Assistant'
BISTO_MP_CONNECTED_EXIT_TEXT = 'NO THANKS'
BISTO_MP_EXIT_PROMPT_TEXT = 'Exit setup?'
BISTO_MP_EXIT_CONFIRM_TEXT = 'EXIT'
PROFILES_CONNECTED = {
    'HFP(pri.)': 'TRUE',
    'A2DP(pri)': 'TRUE',
}
PROFILES_DISCONNECTED = {
    'HFP(pri.)': 'FALSE',
    'A2DP(pri)': 'FALSE',
}
COMP_PROFILE_CONNECTED = {'Comp': 'TRUE'}
COMP_PROFILE_DISCONNECTED = {'Comp': 'FALSE'}
AVRCPSTATUS = 'AvrcpPlayPause'
DEFAULT_TIMEOUT = 60  # wait 60 seconds max for bond/connect.
DEFAULT_CMD_INTERVAL = 0.5  # default interval between serial commands
DEFAULT_CMD_RETRY = 5  # default retry times when a command failed.
DEFAULT_BT_PROFILES = [
    'HFP Pri', 'HFP Sec', 'A2DP Pri', 'A2DP Sec', 'CTRL', 'AUDIO', 'DEBUG',
    'TRANS'
]
DEFAULT_BT_STATUS = ['A2DP(pri)', 'HFP(pri.)', 'Comp']


class TestActsError(Exception):
    """Exception from Apollo Acts Error."""


class ApolloTestActions(BaseTestAction):
    """Test action class for all Apollo test actions."""

    def __init__(self, apollo_dev, logger=None):
        """
        Args:
             apollo_dev: apollo.lib.apollo_lib.Device the Apollo device
        """
        super(ApolloTestActions, self).__init__(logger)
        self.dut = apollo_dev
        # need a embedded timer for connection time measurements.
        self.measurement_timer = TimeRecorder()

    def bluetooth_get_status(self):
        status = self.dut.get_bt_status()
        self.logger.info(status)

    def wait_for_bluetooth_disconnection(self, timeout=60):
        """ Set pairing mode and disconnect.

        This action will wait until the apollo profiles are false.

        Args:
             timeout: integer, timeout value in seconds.
        """
        result = True
        apollo_status = self.dut.get_bt_status()
        self.logger.info('Waiting for the disconnection.')
        time.sleep(1)
        ini_time = time.time()
        while len(apollo_status) != len(
            [s for s in apollo_status.values() if s == 'FALSE']):
            apollo_status = self.dut.get_bt_status()
            if (time.time() - ini_time) > timeout:
                self.logger.warning('Timeout waiting for the disconnection.')
                return False
            time.sleep(1)
        return result

    def pair(self, phone, companion_app=True):
        """Pairs phone with apollo and validates bluetooth profiles.

        Args:
            phone: android phone
            apollo: apollo device
            companion_app (optional): True if the phone has a companion app
                                      installed. False otherwise.

        Raises:
            TestActsError: Bluetooth pairing failed/ Dut BT status check failed.
        """
        bt_util = BTUtils()
        target_addr = self.dut.bluetooth_address
        if bt_util.android_device_in_connected_state(phone, target_addr):
            self.logger.info('Already paired and connected, skipping pairing.')
        else:
            if bt_util.android_device_in_paired_state(phone, target_addr):
                self.logger.info(
                    'Device is paired but not connected, unpair first.')
                if not bt_util.bt_unpair(phone, self.dut):
                    raise TestActsError('Unable to unpair the device')
            bt_util.bt_pair_and_connect(phone, self.dut)
            self.logger.info('DEVICE PAIRED')
            if companion_app:
                profiles = PROFILES_CONNECTED.copy()
                profiles.update(COMP_PROFILE_CONNECTED)
            else:
                profiles = PROFILES_CONNECTED
            self.logger.info(profiles)
            if not bt_util.check_device_bt(device=self.dut, profiles=profiles):
                raise TestActsError('Dut BT status check failed.')
            else:
                return True

    def unpair(self, phone, companion_app=True, factory_reset_dut=True):
        """Unpairs phone from apollo and validates bluetooth profiles.

        Args:
            phone: android phone
            apollo: apollo device
            companion_app (optional): True if the phone has a companion app
                                      installed. False otherwise.

        Raises:
            TestActsError: Bluetooth unpairing/Dut BT status check failed.
        """
        bt_util = BTUtils()
        target_addr = self.dut.bluetooth_address
        if not bt_util.android_device_in_paired_state(phone, target_addr):
            self.logger.info('Device is already unpaired, skipping unpairing.')
        else:
            result = bt_util.bt_unpair(
                phone, self.dut, factory_reset_dut=factory_reset_dut)
            if not result:
                raise TestActsError('Bluetooth unpairing failed.')
            if companion_app:
                profiles = PROFILES_DISCONNECTED.copy()
                profiles.update(COMP_PROFILE_DISCONNECTED)
            else:
                profiles = PROFILES_DISCONNECTED
            if not bt_util.check_device_bt(device=self.dut, profiles=profiles):
                raise TestActsError('Dut BT status check failed.')
            else:
                return True

    def is_paired(self, phone):
        """Check if the given apollo is paired with the android device.

        Args:
            phone: android phone
            apollo: apollo device

        Returns:
            Bool: True if apollo is paired with the phone.
        """
        bt_util = BTUtils()
        target_addr = self.dut.bluetooth_address
        return bt_util.android_device_in_paired_state(phone, target_addr)

    def send_music_play_event_and_validate(self):
        """Send the play event on Apollo and validate the response and DSP
        Status.

        Raises:
            TestActsError: Error while playing the music.
        """
        play_detection_timeout = 1
        if self.dut.is_streaming():
            self.logger.info('Music already streaming. Skipping play event..')
            return
        self.logger.info('Playing video...')
        is_played = self.dut.music_control_events(
            AVRCPSTATUS, self.dut.apollo_log_regex.AVRCP_PLAY_REGEX)
        if not is_played:
            self.logger.error('AVRCP Played status not found')
            raise TestActsError('AVRCP Played status not found.')
        wait_until(
            lambda: self.dut.is_streaming(),
            play_detection_timeout,
            sleep_s=0.25)
        if not self.dut.is_streaming():
            self.logger.error('Device is NOT in a deviceA2DPStreaming state')
            raise TestActsError(
                'Device is NOT in a deviceA2DPStreaming state.')

    def send_music_pause_event_and_validate(self):
        """Send the pause event on Apollo and validate the responses and DSP
        Status.

        Raises:
            TestActsError: Error while pausing the music.
        """
        paused_detection_timeout = 10
        if not self.dut.is_streaming():
            self.logger.info('Music not streaming. Skipping pause event..')
            return
        self.logger.info("Pausing video...")
        is_paused = self.dut.music_control_events(
            AVRCPSTATUS, self.dut.apollo_log_regex.AVRCP_PAUSE_REGEX)
        if not is_paused:
            self.logger.error('AVRCP Paused statue not found')
            raise TestActsError('AVRCP Paused status not found.')
        wait_until(
            lambda: not self.dut.is_streaming(),
            paused_detection_timeout,
            sleep_s=0.25)
        if self.dut.is_streaming():
            self.logger.error('Device is still in deviceA2DPStreaming state')
            raise TestActsError(
                'Device is still in deviceA2DPStreaming state.')

    def vol_down_and_validate(self):
        """Send volume down twice and validate by comparing two levels

        Raises:
            TestActsError: Error
        """
        self.logger.info('Decreasing volume')
        before_vol = self.dut.volume('Down', 1)
        time.sleep(2)
        after_vol = self.dut.volume('Down', 1)
        if not after_vol or not before_vol or after_vol >= before_vol:
            self.logger.error(
                'Unable to decrease the volume. Before: %s. After: %s' %
                (before_vol, after_vol))
            raise TestActsError('error decreasing volume')

    def vol_up_and_validate(self):
        """Send volume up twice and validate by comparing two levels

        Raises:
            TestActsError: Error
        """
        self.logger.info('Increasing volume')
        before_vol = self.dut.volume('Up', 1)
        time.sleep(2)
        after_vol = self.dut.volume('Up', 1)
        if not after_vol or not before_vol or after_vol <= before_vol:
            self.logger.error(
                'Unable to increase the volume. Before: %s. After: %s' %
                (before_vol, after_vol))
            raise TestActsError('error increasing volume')

    def call_and_validate_ringing(self,
                                  calling_phone,
                                  number_to_call,
                                  call_retries=10):
        for i in range(call_retries):
            initiate_call(self.logger, calling_phone, number_to_call)
            is_calling = wait_for_droid_in_call(
                self.logger, calling_phone, max_time=10)
            if is_calling:
                self.logger.info('Call initiated!')
                break
            else:
                self.logger.warning('Call is not initiating.')
                if i == call_retries:
                    self.logger.error('Call initiation retries exhausted')
                    raise TestActsError(
                        '%s retries failed to initiate the call' %
                        (call_retries))
            self.logger.warning('Retrying call...')
        # wait for offhook state and return
        wait_until(
            (lambda: calling_phone.droid.telecomGetCallState() == 'OFFHOOK'),
            timeout_s=40,
            condition=True,
            sleep_s=.5)
        self.logger.info('Phone call initiated on %s' % calling_phone.serial)

    def answer_phone_and_validate_call_received(self, receiving_phone):
        # wait until the phone rings (assumes that a call is initiated prior to
        # running the command)
        wait_until(
            lambda: receiving_phone.droid.telecomGetCallState() == 'RINGING',
            timeout_s=40,
            condition=True,
            sleep_s=.5)
        self.logger.info('Ring detected on %s - now answering the call...' %
                         (receiving_phone.serial))
        # answer the phone call
        self.dut.tap()
        # wait until OFFHOOK state
        wait_until(
            lambda: receiving_phone.droid.telecomGetCallState() == 'OFFHOOK',
            timeout_s=40,
            condition=True,
            sleep_s=.5)

    def hangup_phone_and_validate_call_hung(self, receiving_phone):
        # wait for phone to be in OFFHOOK state (assumed that a call is answered
        # and engaged)
        wait_until(
            lambda: receiving_phone.droid.telecomGetCallState() == 'OFFHOOK',
            timeout_s=40,
            condition=True,
            sleep_s=.5)
        # end the call (post and pre 1663 have different way of ending call)
        self.logger.info(
            'Hanging up the call on %s...' % receiving_phone.serial)
        if self.dut.version < 1663:
            self.dut.tap()
        else:
            self.dut.hold(duration=100)
        # wait for idle state
        wait_until(
            lambda: receiving_phone.droid.telecomGetCallState() == 'IDLE',
            timeout_s=40,
            condition=True,
            sleep_s=.5)

    @timed_action
    def factory_reset(self):
        ret = False
        try:
            self.dut.factory_reset()
            ret = True
        except DeviceError as ex:
            self.logger.warning('Failed to reset Apollo: %s' % ex)
        return ret

    @timed_action
    def wait_for_magic_pairing_notification(self, android_act, timeout=60):
        dut_detected = False
        start_time = time.time()
        self.logger.info('Waiting for MP prompt: %s' % BISTO_MP_DEVICE_TEXT)
        while not dut_detected:
            android_act.dut.ui_util.uia.wait.update()
            self.sleep(1)
            if android_act.dut.ui_util.uia(
                    textContains=BISTO_MP_DETECT_HEADER, enabled=True).exists:
                if android_act.dut.ui_util.uia(
                        textContains=BISTO_MP_DEVICE_TEXT,
                        enabled=True).exists:
                    self.logger.info('DUT Apollo MP prompt detected!')
                    dut_detected = True
                else:
                    self.logger.info(
                        'NONE DUT Apollo MP prompt detected! Cancel and RETRY!'
                    )
                    android_act.dut.ui_util.click_by_text(BISTO_MP_CANCEL_TEXT)
            if time.time() - start_time > timeout:
                break
        if not dut_detected:
            self.logger.info(
                'Failed to get %s MP prompt' % BISTO_MP_DEVICE_TEXT)
        return dut_detected

    @timed_action
    def start_magic_pairing(self, android_act, timeout=30, retries=3):
        paired = False
        android_act.dut.ui_util.click_by_text(
            BISTO_MP_CONNECT_TEXT, timeout=timeout)
        connect_start_time = time.time()
        count = 0
        timeout = 30

        while not paired and count < retries:
            android_act.dut.ui_util.uia.wait.update()
            self.sleep(1)
            if time.time() - connect_start_time > timeout:
                self.logger.info('Time out! %s seconds' % time)
                android_act.app_force_close_agsa()
                self.logger.info('Timeout(s): %s' % timeout)
                break
            if android_act.dut.ui_util.uia(
                    textContains=BISTO_MP_CONNECT_FAIL_TEXT,
                    enabled=True).exists:
                count += 1
                self.logger.info('MP FAILED! Retry %s.' % count)
                android_act.dut.ui_util.click_by_text(
                    BISTO_MP_CONNECT_RETRY_TEXT)
                connect_start_time = time.time()
            elif android_act.dut.ui_util.uia(
                    textContains=BISTO_MP_CONNECTED_TEXT, enabled=True).exists:
                self.logger.info('MP SUCCESSFUL! Exiting AGSA...')
                paired = True
                android_act.dut.ui_util.click_by_text(
                    BISTO_MP_CONNECTED_EXIT_TEXT)
                android_act.dut.ui_util.wait_for_text(
                    BISTO_MP_EXIT_PROMPT_TEXT)
                android_act.dut.ui_util.click_by_text(
                    BISTO_MP_EXIT_CONFIRM_TEXT)
        return paired

    @timed_action
    def turn_bluetooth_on(self):
        self.dut.cmd('pow 1')
        return True

    @timed_action
    def turn_bluetooth_off(self):
        self.dut.cmd('pow 0')
        return True

    @timed_action
    def wait_for_bluetooth_a2dp_hfp(self,
                                    timeout=DEFAULT_TIMEOUT,
                                    interval=DEFAULT_CMD_INTERVAL):
        """Wait for BT connection by checking if A2DP and HFP connected.

        This is used for BT pair+connect test.

        Args:
            timeout: float, timeout value in second.
            interval: float, float, interval between polling BT profiles.
            timer: TimeRecorder, time recorder to save the connection time.
        """
        # Need to check these two profiles
        pass_profiles = ['A2DP Pri', 'HFP Pri']
        # TODO(b/122730302): Change to just raise an error
        ret = False
        try:
            ret = self._wait_for_bluetooth_profile_connection(
                pass_profiles, timeout, interval, self.measurement_timer)
        except DeviceError as ex:
            self.logger.warning('Failed to wait for BT connection: %s' % ex)
        return ret

    def _wait_for_bluetooth_profile_connection(self, profiles_to_check,
                                               timeout, interval, timer):
        """A generic method to wait for specified BT profile connection.

        Args:
            profiles_to_check: list, profile names (A2DP, HFP, etc.) to be
                               checked.
            timeout: float, timeout value in second.
            interval: float, interval between polling BT profiles.
            timer: TimeRecorder, time recorder to save the connection time.

        Returns:
            bool, True if checked profiles are connected, False otherwise.
        """
        timer.start_timer(profiles_to_check, force=True)
        start_time = time.time()
        while time.time() - start_time < timeout:
            profiles = self._bluetooth_check_profile_connection()
            for profile in profiles:
                if profiles[profile]:
                    timer.stop_timer(profile)
            # now check if the specified profile connected.
            all_connected = True
            for profile in profiles_to_check:
                if not profiles[profile]:
                    all_connected = False
                    break
            if all_connected:
                return True
            time.sleep(interval)
        # make sure the profile timer are stopped.
        timer.stop_timer(profiles_to_check)
        return False

    def _bluetooth_check_profile_connection(self):
        """Return profile connection in a boolean dict.

        key=<profile name>, val = T/F
        """
        profiles = dict()
        output = self.dut.get_conn_devices()
        # need to strip all whitespaces.
        conn_devs = {}

        for key in output:
            conn_devs[key.strip()] = output[key].strip()
        for key in conn_devs:
            self.logger.info('%s:%s' % (key, conn_devs[key]))
            if 'XXXXXXXX' in conn_devs[key]:
                profiles[key] = conn_devs[key]
            else:
                profiles[key] = False
        return profiles

    @timed_action
    def wait_for_bluetooth_status_connection_all(
            self, timeout=DEFAULT_TIMEOUT, interval=DEFAULT_CMD_INTERVAL):
        """Wait for BT connection by checking if A2DP, HFP and COMP connected.

        This is used for BT reconnect test.

        Args:
            timeout: float, timeout value in second.
            interval: float, float, interval between polling BT profiles.
        """
        ret = False
        self.measurement_timer.start_timer(DEFAULT_BT_STATUS, force=True)
        # All profile not connected by default.
        connected_status = {key: False for key in DEFAULT_BT_STATUS}
        start_time = time.time()
        while time.time() < start_time + timeout:
            try:
                time.sleep(interval)
                status = self.dut.get_bt_status()
                for key in DEFAULT_BT_STATUS:
                    if (not connected_status[key] and key in status
                            and 'TRUE' == status[key]):
                        self.measurement_timer.stop_timer(key)
                        connected_status[key] = True
                        self.logger.info(
                            'BT status %s connected at %fs.' %
                            (key, self.measurement_timer.elapsed(key)))
                if False not in connected_status.values():
                    ret = True
                    break
            except DeviceError as ex:
                self.logger.warning(
                    'Device exception when waiting for reconnection: %s' % ex)
        self.measurement_timer.stop_timer(DEFAULT_BT_STATUS)
        return ret

    def initiate_ota_via_agsa_verify_transfer_completion_in_logcat(
            self,
            agsa_action,
            dfu_path,
            destination=None,
            force=True,
            apply_image=True,
            reconnect=True):
        """
        Starts an OTA by issuing an intent to AGSA after copying the dfu file to
        the appropriate location on the phone

        Args:
            agsa_action: projects.agsa.lib.test_actions.agsa_acts
                         .AgsaTestActions
            dfu_path: string - absolute path of dfu file
            destination: string - absolute path of file on phone if not
                         specified will use
                         /storage/emulated/0/Android/data/com.google.android
                         .googlequicksearchbox/files/download_cache/apollo.dfu
            force: value set in the intent sent to AGSA
            True if success False otherwise
        """
        try:
            agsa_action.initiate_agsa_and_wait_until_transfer(
                dfu_path, destination=destination, force=force)
            if apply_image:
                # set in case
                self.dut.set_in_case(reconnect=reconnect)
        except AgsaOTAError as ex:
            self.logger.error('Failed to OTA via AGSA %s' % ex)
            return False
        except DeviceError as ex:
            self.logger.error('Failed to bring up device %s' % ex)
            return False
        return True

    @timed_action
    def wait_for_bluetooth_a2dp_hfp_rfcomm_connect(
            self, address, timeout=DEFAULT_TIMEOUT,
            interval=DEFAULT_CMD_INTERVAL):
        """Wait for BT reconnection by checking if A2DP, HFP and COMP connected
        to the specified address.

        This is used for BT connection switch test.

        Args:
            address: str, MAC of the address to connect.
            timeout: float, timeout value in second.
            interval: float, float, interval between polling BT profiles.

        Returns:
            True if the specified address is connected. False otherwise.
        """
        last_4_hex = address.replace(':', '')[-4:].lower()
        profiles_to_check = ['HFP Pri', 'A2DP Pri', 'CTRL', 'AUDIO']
        self.measurement_timer.start_timer(profiles_to_check, force=True)
        end_time = time.time() + timeout
        all_connected = True
        while time.time() < end_time:
            all_connected = True
            profiles = self._bluetooth_check_profile_connection()
            for profile in profiles_to_check:
                if (profile in profiles and profiles[profile]
                        and last_4_hex in profiles[profile].lower()):
                    self.measurement_timer.stop_timer(profile)
                else:
                    all_connected = False
            if all_connected:
                break
            time.sleep(interval)
        # make sure the profile timer are stopped.
        self.measurement_timer.stop_timer(profiles_to_check)

        return all_connected
