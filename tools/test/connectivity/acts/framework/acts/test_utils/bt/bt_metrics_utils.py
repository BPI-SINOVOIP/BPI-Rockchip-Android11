# Copyright (C) 2016 The Android Open Source Project
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
import base64


def get_bluetooth_metrics(ad, bluetooth_proto_module):
    """
    Get metric proto from the Bluetooth stack

    Parameter:
      ad - Android device
      bluetooth_proto_module - the Bluetooth protomodule returned by
                                compile_import_proto()

    Return:
      a protobuf object representing Bluetooth metric

    """
    bluetooth_log = bluetooth_proto_module.BluetoothLog()
    proto_native_str_64 = \
        ad.adb.shell("/system/bin/dumpsys bluetooth_manager --proto-bin")
    proto_native_str = base64.b64decode(proto_native_str_64)
    bluetooth_log.MergeFromString(proto_native_str)
    return bluetooth_log

def get_bluetooth_profile_connection_stats_map(bluetooth_log):
    return project_pairs_list_to_map(bluetooth_log.profile_connection_stats,
                                     lambda stats : stats.profile_id,
                                     lambda stats : stats.num_times_connected,
                                     lambda a, b : a + b)

def get_bluetooth_headset_profile_connection_stats_map(bluetooth_log):
    return project_pairs_list_to_map(bluetooth_log.headset_profile_connection_stats,
                                     lambda stats : stats.profile_id,
                                     lambda stats : stats.num_times_connected,
                                     lambda a, b : a + b)

def project_pairs_list_to_map(pairs_list, get_key, get_value, merge_value):
    """
    Project a list of pairs (A, B) into a map of [A] --> B
    :param pairs_list:  list of pairs (A, B)
    :param get_key: function used to get key from pair (A, B)
    :param get_value: function used to get value from pair (A, B)
    :param merge_value: function used to merge values of B
    :return: a map of [A] --> B
    """
    result = {}
    for item in pairs_list:
        my_key = get_key(item)
        if my_key in result:
            result[my_key] = merge_value(result[my_key], get_value(item))
        else:
            result[my_key] = get_value(item)
    return result
