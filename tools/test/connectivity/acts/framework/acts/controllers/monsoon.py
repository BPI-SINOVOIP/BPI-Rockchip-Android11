#!/usr/bin/env python3
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
from acts.controllers.monsoon_lib.api.hvpm.monsoon import Monsoon as HvpmMonsoon
from acts.controllers.monsoon_lib.api.lvpm_stock.monsoon import \
    Monsoon as LvpmStockMonsoon

ACTS_CONTROLLER_CONFIG_NAME = 'Monsoon'
ACTS_CONTROLLER_REFERENCE_NAME = 'monsoons'


def create(configs):
    """Takes a list of Monsoon configs and returns Monsoon Controllers.

    Args:
        configs: A list of serial numbers, or dicts in the form:
            {
                'type': anyof('LvpmStockMonsoon', 'HvpmMonsoon')
                'serial': int
            }

    Returns:
        a list of Monsoon configs

    Raises:
        ValueError if the configuration does not provide the required info.
    """
    objs = []
    for config in configs:
        monsoon_type = None
        if isinstance(config, dict):
            if isinstance(config.get('type', None), str):
                if 'lvpm' in config['type'].lower():
                    monsoon_type = LvpmStockMonsoon
                elif 'hvpm' in config['type'].lower():
                    monsoon_type = HvpmMonsoon
                else:
                    raise ValueError('Unknown monsoon type %s in Monsoon '
                                     'config %s' % (config['type'], config))
            if 'serial' not in config:
                raise ValueError('Monsoon config must specify "serial".')
            serial_number = int(config.get('serial'))
        else:
            serial_number = int(config)
        if monsoon_type is None:
            if serial_number < 20000:
                # This code assumes the LVPM has firmware version 20. If
                # someone has updated the firmware, or somehow found an older
                # version, the power measurement will fail.
                monsoon_type = LvpmStockMonsoon
            else:
                monsoon_type = HvpmMonsoon

        objs.append(monsoon_type(serial=serial_number))
    return objs


def destroy(monsoons):
    for monsoon in monsoons:
        monsoon.release_monsoon_connection()
