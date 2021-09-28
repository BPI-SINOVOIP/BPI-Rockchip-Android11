#!/usr/bin/env python3
#
#   Copyright 2017 - The Android Open Source Project
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

import enum
"""This module has the global key values that are used across framework
modules.
"""


class Config(enum.Enum):
    """Enum values for test config related lookups.
    """
    # Keys used to look up values from test config files.
    # These keys define the wording of test configs and their internal
    # references.
    key_log_path = 'logpath'
    key_testbeds_under_test = 'testbeds_under_test'
    key_testbed = 'testbed'
    key_testbed_name = 'name'
    # configpath is the directory. key_config_full_path is the file path.
    key_config_path = 'configpath'
    key_config_full_path = 'config_full_path'
    key_test_paths = 'testpaths'
    key_port = 'Port'
    key_address = 'Address'
    key_test_case_iterations = 'test_case_iterations'
    key_test_failure_tracebacks = 'test_failure_tracebacks'
    # Config names for controllers packaged in ACTS.
    key_android_device = 'AndroidDevice'
    key_bluetooth_pts_device = 'BluetoothPtsDevice'
    key_fuchsia_device = 'FuchsiaDevice'
    key_buds_device = 'BudsDevice'
    key_chameleon_device = 'ChameleonDevice'
    key_native_android_device = 'NativeAndroidDevice'
    key_relay_device = 'RelayDevice'
    key_access_point = 'AccessPoint'
    key_attenuator = 'Attenuator'
    key_iperf_server = 'IPerfServer'
    key_iperf_client = 'IPerfClient'
    key_packet_sender = 'PacketSender'
    key_monsoon = 'Monsoon'
    key_sniffer = 'Sniffer'
    key_arduino_wifi_dongle = 'ArduinoWifiDongle'
    key_packet_capture = 'PacketCapture'
    key_openwrt_ap = 'OpenWrtAP'
    # Internal keys, used internally, not exposed to user's config files.
    ikey_user_param = 'user_params'
    ikey_testbed_name = 'testbed_name'
    ikey_logger = 'log'
    ikey_logpath = 'log_path'
    ikey_summary_writer = 'summary_writer'
    # module name of controllers packaged in ACTS.
    m_key_monsoon = 'monsoon'
    m_key_android_device = 'android_device'
    m_key_fuchsia_device = 'fuchsia_device'
    m_key_bluetooth_pts_device = 'bluetooth_pts_device'
    m_key_buds_device = 'buds_controller'
    m_key_chameleon_device = 'chameleon_controller'
    m_key_native_android_device = 'native_android_device'
    m_key_relay_device = 'relay_device_controller'
    m_key_access_point = 'access_point'
    m_key_attenuator = 'attenuator'
    m_key_iperf_server = 'iperf_server'
    m_key_iperf_client = 'iperf_client'
    m_key_packet_sender = 'packet_sender'
    m_key_sniffer = 'sniffer'
    m_key_arduino_wifi_dongle = 'arduino_wifi_dongle'
    m_key_packet_capture = 'packet_capture'
    m_key_openwrt_ap = 'openwrt_ap'

    # A list of keys whose values in configs should not be passed to test
    # classes without unpacking first.
    reserved_keys = (key_testbed, key_log_path, key_test_paths)

    # Controller names packaged with ACTS.
    builtin_controller_names = [
        key_android_device,
        key_bluetooth_pts_device,
        key_fuchsia_device,
        key_buds_device,
        key_native_android_device,
        key_relay_device,
        key_access_point,
        key_attenuator,
        key_iperf_server,
        key_iperf_client,
        key_packet_sender,
        key_monsoon,
        key_sniffer,
        key_chameleon_device,
        key_arduino_wifi_dongle,
        key_packet_capture,
        key_openwrt_ap,
    ]

    # Keys that are file or folder paths.
    file_path_keys = [key_relay_device]


def get_name_by_value(value):
    for name, member in Config.__members__.items():
        if member.value == value:
            return name
    return None


def get_module_name(name_in_config):
    """Translates the name of a controller in config file to its module name.
    """
    return value_to_value(name_in_config, 'm_%s')


def value_to_value(ref_value, pattern):
    """Translates the value of a key to the value of its corresponding key. The
    corresponding key is chosen based on the variable name pattern.
    """
    ref_key_name = get_name_by_value(ref_value)
    if not ref_key_name:
        return None
    target_key_name = pattern % ref_key_name
    try:
        return getattr(Config, target_key_name).value
    except AttributeError:
        return None
