#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Running test to validate that RO can update the RW firmware"

readonly DEV_FW_FILE="${1}"
readonly RB0_FW_FILE="${2}"

check_file_exists "${DEV_FW_FILE}"
check_file_exists "${RB0_FW_FILE}"

echo "Making sure all write protect is enabled"
check_hw_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_dev_rw_firmware
check_has_dev_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Flashing RB0 firmware"
flash_rw_firmware "${RB0_FW_FILE}"

echo "Validating we're running RB0 as RW"
check_has_dev_ro_firmware
check_has_rb0_rw_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Flashing dev firmware"
flash_rw_firmware "${DEV_FW_FILE}"
check_has_dev_rw_firmware
check_has_dev_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val
