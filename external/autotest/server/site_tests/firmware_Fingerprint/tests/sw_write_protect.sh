#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Running test to validate software write protect cannot be disabled when \
hardware write protect is enabled"

echo "Making sure all write protect is enabled"
check_hw_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Reboot to RO"
reboot_ec_to_ro
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_ro_firmware
check_hw_and_sw_write_protect_enabled

echo "Disabling software write protect when hardware write protect is enabled \
when running RO"
if (disable_sw_write_protect); then
  echo "Disabling software write protect should fail"
  exit 1
fi

echo "Validating write protection did not change"
check_hw_and_sw_write_protect_enabled

echo "Reboot to RW"
reboot_ec
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_hw_and_sw_write_protect_enabled

echo "Disabling software write protect when hardware write protect is enabled \
when running RW"
if (disable_sw_write_protect); then
  echo "Disabling software write protect should fail"
  exit 1
fi

echo "Validating write protection did not change"
check_hw_and_sw_write_protect_enabled
