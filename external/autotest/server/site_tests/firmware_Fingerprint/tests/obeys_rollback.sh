#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Running test to validate rollback version is obeyed"

readonly RB0_FW_FILE="$1"
readonly RB1_FW_FILE="$2"
readonly RB9_FW_FILE="$3"

check_file_exists "${RB0_FW_FILE}"
check_file_exists "${RB1_FW_FILE}"
check_file_exists "${RB9_FW_FILE}"

echo "Making sure all write protect is enabled"
check_hw_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_dev_rw_firmware
check_has_dev_ro_firmware
check_running_rw_firmware
check_fingerprint_task_is_running
check_is_rollback_set_to_initial_val

echo "Flashing RB1 version"
flash_rw_firmware "${RB1_FW_FILE}"
check_has_dev_ro_firmware
check_has_rb1_rw_firmware
check_running_rw_firmware
check_fingerprint_task_is_running
check_rollback_block_id_matches "2"
check_rollback_min_version_matches "1"
check_rollback_rw_version_matches "1"

echo "Flashing RB0 version"
flash_rw_firmware "${RB0_FW_FILE}"
check_has_dev_ro_firmware
check_has_rb0_rw_firmware
check_running_ro_firmware
check_fingerprint_task_is_not_running
check_rollback_block_id_matches "2"
check_rollback_min_version_matches "1"
check_rollback_rw_version_matches "0"

echo "Flashing RB9 version"
flash_rw_firmware "${RB9_FW_FILE}"
check_has_dev_ro_firmware
check_has_rb9_rw_firmware
check_running_rw_firmware
check_fingerprint_task_is_running
check_rollback_block_id_matches "3"
check_rollback_min_version_matches "9"
check_rollback_rw_version_matches "9"
