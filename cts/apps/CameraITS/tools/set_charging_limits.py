# Copyright 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import subprocess
import sys

CHARGE_PERCENT_START = 40
CHARGE_PERCENT_STOP = 60


def set_device_charging_limits(device_id):
    """Set the start/stop percentages for charging.

    This can keep battery from overcharging.
    Args:
        device_id:  str; device ID to set limits
    """
    print 'Rooting device %s' % device_id
    cmd = ('adb -s %s root' % device_id)
    process = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    pout, perr = process.communicate()
    if 'cannot' in pout.lower() or perr:  # 'cannot root' returns no error
        print ' Warning: unable to root %s and set charging limits.' % device_id
    else:
        print ' Setting charging limits on %s' % device_id
        cmd = ('adb -s %s shell setprop persist.vendor.charge.start.level %d' % (
                device_id, CHARGE_PERCENT_START))
        process = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        _, perr = process.communicate()
        if not perr:
            print ' Min: %d%%' % CHARGE_PERCENT_START
        else:
            print ' Warning: unable to set charging start limit.'

        cmd = ('adb -s %s shell setprop persist.vendor.charge.stop.level %d' % (
                device_id, CHARGE_PERCENT_STOP))
        process = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        _, perr = process.communicate()
        if not perr:
            print ' Max: %d%%' % CHARGE_PERCENT_STOP
        else:
            print ' Warning: unable to set charging stop limit.'

        print 'Unrooting device %s' % device_id
        cmd = ('adb -s %s unroot' % device_id)
        subprocess.call(cmd.split(), stdout=subprocess.PIPE)


def main():
    """Set charging limits for battery."""

    device_id = None
    for s in sys.argv[1:]:
        if s[:7] == 'device=' and len(s) > 7:
            device_id = s[7:]

    if device_id:
        set_device_charging_limits(device_id)
    else:
        print 'Usage: python %s device=$DEVICE_ID' % os.path.basename(__file__)

if __name__ == '__main__':
    main()
