#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

import logging
import os
import pandas as pd
import re
import time
import acts.test_utils.bt.bt_test_utils as bt_utils

from acts.libs.proc import job
from acts.base_test import BaseTestClass
from acts.test_utils.bt.bt_power_test_utils import MediaControl
from acts.test_utils.abstract_devices.bluetooth_handsfree_abstract_device import BluetoothHandsfreeAbstractDeviceFactory as bt_factory

PHONE_MUSIC_FILE_DIRECTORY = '/sdcard/Music'
FORCE_SAR_ADB_COMMAND = ('am broadcast -n'
                         'com.google.android.apps.scone/.coex.TestReceiver -a '
                         'com.google.android.apps.scone.coex.SIMULATE_STATE ')

DEFAULT_DURATION = 5
DEFAULT_MAX_ERROR_THRESHOLD = 2
DEFAULT_AGG_MAX_ERROR_THRESHOLD = 2


class BtSarBaseTest(BaseTestClass):
    """ Base class for all BT SAR Test classes.

        This class implements functions common to BT SAR test Classes.
    """
    BACKUP_BT_SAR_TABLE_NAME = 'backup_bt_sar_table.csv'

    def __init__(self, controllers):
        BaseTestClass.__init__(self, controllers)
        self.power_file_paths = [
            '/vendor/etc/bluetooth_power_limits.csv',
            '/data/vendor/radio/bluetooth_power_limits.csv'
        ]
        self.sar_file_name = os.path.basename(self.power_file_paths[0])

    def setup_class(self):
        """Initializes common test hardware and parameters.

        This function initializes hardware and compiles parameters that are
        common to all tests in this class and derived classes.
        """
        super().setup_class()

        self.test_params = self.user_params.get('bt_sar_test_params', {})
        if not self.test_params:
            self.log.warning(
                'bt_sar_test_params was not found in the config file.')

        self.user_params.update(self.test_params)
        req_params = ['bt_devices', 'calibration_params']

        self.unpack_userparams(
            req_params,
            duration=DEFAULT_DURATION,
            custom_sar_path=None,
            music_files=None,
            sort_order=None,
            max_error_threshold=DEFAULT_MAX_ERROR_THRESHOLD,
            agg_error_threshold=DEFAULT_AGG_MAX_ERROR_THRESHOLD,
            tpc_threshold=[2, 8],
        )

        self.attenuator = self.attenuators[0]
        self.dut = self.android_devices[0]

        # To prevent default file from being overwritten
        self.dut.adb.shell('cp {} {}'.format(self.power_file_paths[0],
                                             self.power_file_paths[1]))

        self.sar_file_path = self.power_file_paths[1]
        self.atten_min = 0
        self.atten_max = int(self.attenuator.get_max_atten())

        # Initializing media controller
        if self.music_files:
            music_src = self.music_files[0]
            music_dest = PHONE_MUSIC_FILE_DIRECTORY
            success = self.dut.push_system_file(music_src, music_dest)
            if success:
                self.music_file = os.path.join(PHONE_MUSIC_FILE_DIRECTORY,
                                               os.path.basename(music_src))
            # Initialize media_control class
            self.media = MediaControl(self.dut, self.music_file)

        #Initializing BT device controller
        if self.bt_devices:
            attr, idx = self.bt_devices.split(':')
            self.bt_device_controller = getattr(self, attr)[int(idx)]
            self.bt_device = bt_factory().generate(self.bt_device_controller)
        else:
            self.log.error('No BT devices config is provided!')

        bt_utils.enable_bqr(self.android_devices)

        self.log_path = os.path.join(logging.log_path, 'results')
        os.makedirs(self.log_path, exist_ok=True)

        # Reading BT SAR table from the phone
        self.bt_sar_df = self.read_sar_table(self.dut)

    def setup_test(self):
        super().setup_test()

        # Starting BT on the master
        self.dut.droid.bluetoothFactoryReset()
        bt_utils.enable_bluetooth(self.dut.droid, self.dut.ed)

        # Starting BT on the slave
        self.bt_device.reset()
        self.bt_device.power_on()

        # Connect master and slave
        bt_utils.connect_phone_to_headset(self.dut, self.bt_device, 60)

        # Playing music
        self.media.play()

        # Find and set PL10 level for the DUT
        self.pl10_atten = self.set_PL10_atten_level(self.dut)

    def teardown_test(self):

        #Stopping Music
        if hasattr(self, 'media'):
            self.media.stop()

        # Stopping BT on slave
        self.bt_device.reset()
        self.bt_device.power_off()

        #Stopping BT on master
        bt_utils.disable_bluetooth(self.dut.droid)

        #Resetting the atten to initial levels
        self.attenuator.set_atten(self.atten_min)
        self.log.info('Attenuation set to {} dB'.format(self.atten_min))

    def teardown_class(self):

        super().teardown_class()
        self.dut.droid.bluetoothFactoryReset()

        # Stopping BT on slave
        self.bt_device.reset()
        self.bt_device.power_off()

        #Stopping BT on master
        bt_utils.disable_bluetooth(self.dut.droid)

    def set_sar_state(self, ad, signal_dict):
        """Sets the SAR state corresponding to the BT SAR signal.

        The SAR state is forced using an adb command that takes
        device signals as input.

        Args:
            ad: android_device object.
            signal_dict: dict of BT SAR signals read from the SAR file.
        Returns:
            enforced_state: dict of device signals.
        """

        signal_dict = {k: max(int(v), 0) for (k, v) in signal_dict.items()}

        #Reading signal_dict
        head = signal_dict['Head']
        wifi_5g = signal_dict['WIFI5Ghz']
        hotspot_voice = signal_dict['HotspotVoice']
        btmedia = signal_dict['BTMedia']
        cell = signal_dict['Cell']
        imu = signal_dict['IMU']

        wifi = wifi_5g
        wifi_24g = 0 if wifi_5g else 1

        enforced_state = {
            'Wifi': wifi,
            'Wifi AP': hotspot_voice,
            'Earpiece': head,
            'Bluetooth': 1,
            'Motion': imu,
            'Voice': 0,
            'Wifi 2.4G': wifi_24g,
            'Radio': cell,
            'Bluetooth connected': 1,
            'Bluetooth media': btmedia
        }

        #Forcing the SAR state
        adb_output = ad.adb.shell('{} '
                                  '--ei earpiece {} '
                                  '--ei wifi {} '
                                  '--ei wifi_24g {} '
                                  '--ei voice 0 '
                                  '--ei wifi_ap {} '
                                  '--ei bluetooth 1 '
                                  '--ei bt_media {} '
                                  '--ei radio_power {} '
                                  '--ei motion {} '
                                  '--ei bt_connected 1'.format(
                                      FORCE_SAR_ADB_COMMAND, head, wifi,
                                      wifi_24g, hotspot_voice, btmedia, cell,
                                      imu))

        # Checking if command was successfully enforced
        if 'result=0' in adb_output:
            self.log.info('Requested BT SAR state successfully enforced.')
            return enforced_state
        else:
            self.log.error("Couldn't force BT SAR state.")

    def parse_bt_logs(self, ad, begin_time, regex=''):
        """Returns bt software stats by parsing logcat since begin_time.

        The quantity to be fetched is dictated by the regex provided.

        Args:
             ad: android_device object.
             begin_time: time stamp to start the logcat parsing.
             regex: regex for fetching the required BT software stats.

        Returns:
             stat: the desired BT stat.
        """
        # Waiting for logcat to update
        time.sleep(1)
        bt_adb_log = ad.adb.logcat('-b all -t %s' % begin_time)
        for line in bt_adb_log.splitlines():
            if re.findall(regex, line):
                stat = re.findall(regex, line)[0]
                return stat

        raise ValueError('Failed to parse BT logs.')

    def get_current_power_cap(self, ad, begin_time):
        """ Returns the enforced software power cap since begin_time.

        Returns the enforced power cap since begin_time by parsing logcat.
        Function should follow a function call that forces a SAR state

        Args:
            ad: android_device obj.
            begin_time: time stamp to start.

        Returns:
            read enforced power cap
        """
        power_cap_regex = 'Bluetooth Tx Power Cap\s+(\d+)'
        power_cap = self.parse_bt_logs(ad, begin_time, power_cap_regex)
        return int(power_cap)

    def get_current_device_state(self, ad, begin_time):
        """ Returns the device state of the android dut since begin_time.

        Returns the device state of the android dut by parsing logcat since
        begin_time. Function should follow a function call that forces
        a SAR state.

        Args:
            ad: android_device obj.
            begin_time: time stamp to start.

        Returns:
            device_state: device state of the android device.
        """

        device_state_regex = 'updateDeviceState: DeviceState: ([\s*\S+\s]+)'
        device_state = self.parse_bt_logs(ad, begin_time, device_state_regex)
        return device_state

    def read_sar_table(self, ad):
        """Extracts the BT SAR table from the phone.

        Extracts the BT SAR table from the phone into the android device
        log path directory.

        Args:
            ad: android_device object.

        Returns:
            df : BT SAR table (as pandas DataFrame).
        """
        output_path = os.path.join(ad.device_log_path, self.sar_file_name)
        ad.adb.pull('{} {}'.format(self.sar_file_path, output_path))
        df = pd.read_csv(os.path.join(ad.device_log_path, self.sar_file_name))
        self.log.info('BT SAR table read from the phone')
        return df

    def push_table(self, ad, src_path):
        """Pushes a BT SAR table to the phone.

        Pushes a BT SAR table to the android device and reboots the device.
        Also creates a backup file if backup flag is True.

        Args:
            ad: android_device object.
            src_path: path to the  BT SAR table.
        """
        #Copying the to-be-pushed file for logging
        if os.path.dirname(src_path) != ad.device_log_path:
            job.run('cp {} {}'.format(src_path, ad.device_log_path))

        #Pushing the file provided in the config
        ad.push_system_file(src_path, self.sar_file_path)
        self.log.info('BT SAR table pushed')
        ad.reboot()
        self.bt_sar_df = self.read_sar_table(self.dut)

    def set_PL10_atten_level(self, ad):
        """Finds the attenuation level at which the phone is at PL10

        Finds PL10 attenuation level by sweeping the attenuation range.
        If the power level is not achieved during sweep,
        returns the max atten level

        Args:
            ad: android object class
        Returns:
            atten : attenuation level when the phone is at PL10
        """
        BT_SAR_ATTEN_STEP = 3

        for atten in range(self.atten_min, self.atten_max, BT_SAR_ATTEN_STEP):
            self.attenuator.set_atten(atten)
            # Sleep required for BQR to reflect the change in parameters
            time.sleep(2)
            metrics = bt_utils.get_bt_metric(ad)
            if metrics['pwlv'][ad.serial] == 10:
                self.log.info('PL10 located at {}'.format(atten +
                                                          BT_SAR_ATTEN_STEP))
                return atten + BT_SAR_ATTEN_STEP

        self.log.warn(
            "PL10 couldn't be located in the given attenuation range")
        return atten
