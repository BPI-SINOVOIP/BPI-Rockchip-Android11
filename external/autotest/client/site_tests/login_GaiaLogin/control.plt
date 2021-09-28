# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib.cros import power_load_util

AUTHOR = "Chrome OS Team"
NAME = "login_GaiaLogin.plt"
ATTRIBUTES = "suite:power_daily"
TIME = "SHORT"
TEST_CATEGORY = "General"
TEST_CLASS = "login"
TEST_TYPE = "client"

DOC = """
This test verifies that logging into production Gaia works correctly.

It logs in using the telemetry gaia_login flag, and navigates to
accounts.google.com to verify that we're logged in to gaia, as opposed
to fake telemetry login.
"""

username = power_load_util.get_username()
password = power_load_util.get_password()
job.run_test('login_GaiaLogin', username=username, password=password,
             tag=NAME.split('.')[1])

