#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Make sure all write protect is enabled"
check_hw_and_sw_write_protect_enabled

echo "Validate initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Reading from flash while running RW firmware should fail"
if (read_from_flash "test.bin"); then
  echo "Should not be able to read from flash"
  exit 1
fi

echo "Reboot to RO"
reboot_ec_to_ro
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_ro_firmware

echo "Reading from flash while running RO firmware should fail"
if (read_from_flash "test.bin"); then
  echo "Should not be able to read from flash while running RO"
  exit 1
fi

echo "Reboot to RW"
reboot_ec
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
