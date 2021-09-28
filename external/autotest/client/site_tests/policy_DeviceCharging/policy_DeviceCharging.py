# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.cros.enterprise import charging_policy_tests


class policy_DeviceCharging(charging_policy_tests.ChargingPolicyTest):
    """
    Client test for device policies that change charging behavior.

    Everything is taken care of in the superclass.
    """
    version = 1
