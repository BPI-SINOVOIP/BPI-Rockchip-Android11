#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Running test to verify booting into RO works"

echo "Making sure all write protect is enabled"
check_hw_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Rebooting into RO version"
reboot_ec_to_ro

echo "Validating that we're now running the RO version"
check_running_ro_firmware

echo "Validating flash protection hasn't changed"
check_hw_and_sw_write_protect_enabled

echo "Rebooting back into RW"
reboot_ec

echo "Validating we're now running RW version"
check_running_rw_firmware

echo "Validating flash protection hasn't changed"
check_hw_and_sw_write_protect_enabled
