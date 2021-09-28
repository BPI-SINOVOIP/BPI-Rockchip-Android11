# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error

class platform_CryptohomeGetEnrollmentId(test.test):
    version = 1

    def has_device_secret(self):
        # Using the same way of getting the device secret as we use in
        # cryptohomed.conf.
        exit_code = utils.system(
            'vpd_get_value stable_device_secret_DO_NOT_SHARE',
            ignore_status=True)
        return exit_code == 0

    def check_enrollment_id(self):
        eid = utils.system_output('cryptohome --action=get_enrollment_id')
        return len(eid) == 64

    def run_once(self):
        if (self.has_device_secret()):
            if (not self.check_enrollment_id()):
                raise error.TestFail('Invalid enrollment ID value.')
