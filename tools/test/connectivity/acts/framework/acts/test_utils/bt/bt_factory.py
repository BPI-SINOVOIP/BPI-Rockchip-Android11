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

import importlib


def create(configs):
    """Used to create instance of bt implementation.

    A list of of configuration is extracted from configs.
    The modules names are extracted and passed to import_module
    to get the specific implementation, which gets appended to a
    device list.
    Args:
        configs: A configurations dictionary that contains
        a list of configs for each device in configs['user_params']['BtDevice'].

    Returns:
        A list of bt implementations.
    """
    bt_devices = []
    for config in configs:
        bt_name = config['bt_module']
        bt = importlib.import_module('acts.test_utils.bt.bt_implementations.%s'
                                      % bt_name)
        bt_devices.append(bt.BluethoothDevice(config))
    return bt_devices


def destroy(bt_device_list):
    for bt in bt_device_list:
        bt.close()
