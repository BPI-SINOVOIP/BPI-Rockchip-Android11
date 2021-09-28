#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Running test to verify only valid RW firmware will boot"

readonly ORIG_FW_FILE="${1}"
readonly DEV_FW_FILE="${2}"
readonly CORRUPT_FIRST_BYTE_FW_FILE="${3}"
readonly CORRUPT_LAST_BYTE_FW_FILE="${4}"

check_file_exists "${ORIG_FW_FILE}"
check_file_exists "${DEV_FW_FILE}"
check_file_exists "${CORRUPT_FIRST_BYTE_FW_FILE}"
check_file_exists "${CORRUPT_LAST_BYTE_FW_FILE}"

echo "Making sure all write protect is enabled"
check_hw_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Flashing dev signed version"
flash_rw_firmware "${DEV_FW_FILE}"
check_has_mp_ro_firmware
check_has_dev_rw_firmware
check_running_ro_firmware
check_is_rollback_set_to_initial_val

echo "Flashing corrupt first byte version"
flash_rw_firmware "${CORRUPT_FIRST_BYTE_FW_FILE}"
check_has_mp_ro_firmware
check_has_mp_rw_firmware  # corrupted version has same version string
check_running_ro_firmware
check_is_rollback_set_to_initial_val

echo "Flash corrupt last byte version"
flash_rw_firmware "${CORRUPT_LAST_BYTE_FW_FILE}"
check_has_mp_ro_firmware
check_has_mp_rw_firmware  # corrupted version has same version string
check_running_ro_firmware
check_is_rollback_set_to_initial_val

echo "Flashing valid version"
flash_rw_firmware "${ORIG_FW_FILE}"
check_has_mp_ro_firmware
check_has_mp_rw_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

