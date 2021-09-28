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


def le_scan_for_device_by_name(fd,
                               log,
                               search_name,
                               timeout,
                               partial_match=False):
    """Scan for and returns the first BLE advertisement with the device name.

    Args:
        fd: The Fuchsia device to start LE scanning on.
        name: The name to find.
        timeout: How long to scan for.
        partial_match: Only do a partial match for the LE advertising name.
          This will return the first result that had a partial match.

    Returns:
        The dictionary of device information.
    """
    scan_filter = {"name_substring": search_name}
    fd.gattc_lib.bleStartBleScan(scan_filter)
    end_time = time.time() + timeout
    found_device = None
    while time.time() < end_time and not found_device:
        time.sleep(1)
        scan_res = fd.gattc_lib.bleGetDiscoveredDevices()['result']
        for device in scan_res:
            name, did, connectable = device["name"], device["id"], device[
                "connectable"]
            if name == search_name or (partial_match and search_name in name):
                log.info("Successfully found advertisement! name, id: {}, {}".
                         format(name, did))
                found_device = device
    fd.gattc_lib.bleStopBleScan()
    if not found_device:
        log.error("Failed to find device with name {}.".format(search_name))
        return found_device
    return found_device
