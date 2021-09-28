#!/usr/bin/env python3
#
#   Copyright 2016 - Google
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
#
#
# Defines utilities that can be used to create android user account

import re
import time
import logging as log



def get_all_users(android_device):
    all_users = {}
    out = android_device.adb.shell("pm list users")

    for user in re.findall("UserInfo{(.*\d*\w):", out):
        all = user.split(":")
        all_users[all[1]] = all_users.get(all[1], all[0])
    return all_users


def create_new_user(android_device, user_name):
    out = android_device.adb.shell("pm create-user {}".format(user_name))
    return re.search("Success(.* (.*\d))", out).group(2)


def switch_user(android_device, user_id):
    prev_user = get_current_user(android_device)
    android_device.adb.shell("am switch-user {}".format(user_id))
    if not _wait_for_user_to_take_place(android_device, prev_user):
        log.error("Failed to successfully switch user {}".format(user_id))
        return False
    return True


def remove_user(android_device, user_id):
    return "Success" in android_device.adb.shell("pm remove-user {}".format(user_id))


def get_current_user(android_device):
    out = android_device.adb.shell("dumpsys activity")
    result = re.search("mCurrentUserId:(\d+)", out)
    return result.group(1)


def _wait_for_user_to_take_place(android_device, user_id, timeout=10):
    start_time = time.time()
    while (start_time + timeout) > time.time():
        time.sleep(1)
        if user_id != get_current_user(android_device):
            return True
    return False
