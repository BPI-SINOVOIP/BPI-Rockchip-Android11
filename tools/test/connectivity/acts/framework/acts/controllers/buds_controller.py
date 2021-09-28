#!/usr/bin/env python3.4
#
#   Copyright 2016 - The Android Open Source Project
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

"""This is the controller module for Pixel Buds devices.

For the device definition, see buds_lib.apollo_lib.
"""

from acts.controllers.buds_lib.apollo_lib import ParentDevice


ACTS_CONTROLLER_CONFIG_NAME = 'BudsDevice'
ACTS_CONTROLLER_REFERENCE_NAME = 'buds_devices'


class ConfigError(Exception):
    """Raised when the configuration is malformatted."""


def create(configs):
    """Creates a Pixel Buds device for each config found within the configs.

    Args:
        configs: The configs can be structured in the following ways:

                    ['serial1', 'serial2', ... ]

                    [
                        {
                            'serial': 'serial1',
                            'label': 'some_info',
                            ...
                        },
                        {
                            'serial': 'serial2',
                            'label': 'other_info',
                            ...
                        }
                    ]
    """
    created_controllers = []

    if not isinstance(configs, list):
        raise ConfigError('Malformatted config %s. Must be a list.' % configs)

    for config in configs:
        if isinstance(config, str):
            created_controllers.append(ParentDevice(config))
        elif isinstance(config, dict):
            serial = config.get('serial', None)
            if not serial:
                raise ConfigError('Buds Device %s is missing entry "serial".' %
                                  config)
            created_controllers.append(ParentDevice(serial))
        else:
            raise ConfigError('Malformatted config: "%s". Must be a string or '
                              'dict' % config)
    return created_controllers


def destroy(buds_device_list):
    pass


def get_info(buds_device_list):
    device_infos = []
    for buds_device in buds_device_list:
        device_infos.append({'serial': buds_device.serial_number,
                             'name': buds_device.device_name})
    return device_infos
