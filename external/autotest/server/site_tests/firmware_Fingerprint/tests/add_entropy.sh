#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Running test to validate adding entropy only succeeds when running RO"

echo "Making sure all write protect is enabled"
check_hw_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val


echo "Adding entropy should fail when running RW"
if (add_entropy); then
  echo "Should not be able to add entropy when running RW FW"
  exit 1
fi

echo "Validating rollback didn't change"
check_is_rollback_set_to_initial_val

echo "Adding entropy from RO should succeed"
reboot_ec_to_ro
add_entropy

echo "Validating Block ID changes, but nothing else"
check_rollback_block_id_matches "2"
check_rollback_min_version_matches "0"
check_rollback_rw_version_matches "0"

echo "Adding entropy with reset (double write) from RO should succeed"
reboot_ec_to_ro
add_entropy reset

echo "Validating Block ID increases by 2, but nothing else"
check_rollback_block_id_matches "4"
check_rollback_min_version_matches "0"
check_rollback_rw_version_matches "0"

echo "Switching back to RW"
reboot_ec

echo "Validating nothing changed"
check_rollback_block_id_matches "4"
check_rollback_min_version_matches "0"
check_rollback_rw_version_matches "0"
