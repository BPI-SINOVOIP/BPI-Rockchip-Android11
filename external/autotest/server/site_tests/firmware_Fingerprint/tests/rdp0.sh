#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

test_read_from_flash_in_bootloader_mode_without_modifying_RDP_level() {
  local file_read_from_flash="test.bin"
  local original_fw_file="$1"

  # Given:
  #   * Hardware write protect is disabled
  #   * Software write protect is disabled
  #   * RDP is at level 0
  #
  # Then:
  #   * Reading from flash without changing the RDP level should succeed
  #     (we're already at level 0). Thus we should be able to read the entire
  #     firmware out of flash and it should exactly match the firmware that we
  #     flashed for testing.
  echo "Reading firmware without modifying RDP level"
  read_from_flash_in_bootloader_mode_without_modifying_RDP_level \
    "${file_read_from_flash}"
  if [[ $? -ne 0 ]]; then
    echo "Failed to read firmware"
    exit 1
  fi

  echo "Checking that value read matches the flashed version"
  check_files_match "${file_read_from_flash}" "${original_fw_file}"

  echo "Checking that firmware is still functional"
  check_firmware_is_functional

  rm -rf "${file_read_from_flash}"
}

test_read_from_flash_in_bootloader_mode_while_setting_RDP_to_level_0() {
  local file_read_from_flash="test.bin"
  local original_fw_file="$1"

  # Given:
  #   * Hardware write protect is disabled
  #   * Software write protect is disabled
  #   * RDP is at level 0
  #
  # Then:
  #   * Changing the RDP level to 0 should have no effect (we're already at
  #     level 0). Thus we should be able to read the entire firmware out of
  #     flash and it should exactly match the firmware that we flashed for
  #     testing.
  echo "Reading firmware while setting RDP to level 0"
  read_from_flash_in_bootloader_mode_while_setting_RDP_to_level_0 \
    "${file_read_from_flash}"
  if [[ $? -ne 0 ]]; then
    echo "Failed to read firmware"
    exit 1
  fi

  echo "Checking that value read matches the flashed version"
  check_files_match "${file_read_from_flash}" "${original_fw_file}"

  echo "Checking that firmware is still functional"
  check_firmware_is_functional

  rm -rf "${file_read_from_flash}"
}

echo "Running test to validate that we can read when RDP is set to level 0"

readonly ORIGINAL_FW_FILE="$1"

check_file_exists "${ORIGINAL_FW_FILE}"

echo "Making sure all write protect is disabled"
check_hw_and_sw_write_protect_disabled

echo "Validating initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_rollback_is_unset

echo "Checking that firmware is functional"
check_firmware_is_functional

test_read_from_flash_in_bootloader_mode_without_modifying_RDP_level \
  "${ORIGINAL_FW_FILE}"


test_read_from_flash_in_bootloader_mode_while_setting_RDP_to_level_0 \
  "${ORIGINAL_FW_FILE}"
