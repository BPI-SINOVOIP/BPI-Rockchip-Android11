#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

test_read_from_flash_in_bootloader_mode_without_modifying_RDP_level() {
  local file_read_from_flash="test.bin"

  # Given:
  #   * Hardware write protect is disabled
  #       (so we can use bootloader to read and change RDP level)
  #   * Software write protect is enabled
  #   * RDP is at level 1
  #
  # Then:
  #   * Reading from flash without changing the RDP level should fail
  #     (and we should not have read any bytes from flash).
  #   * The firmware should still be functional because mass erase is NOT
  #     triggered since we are NOT changing the RDP level.
  echo "Reading firmware without modifying RDP level"
  # This should fail and the file should be empty
  if read_from_flash_in_bootloader_mode_without_modifying_RDP_level \
    "${file_read_from_flash}"; then
    echo "Should not be able to read from flash"
    exit 1
  fi

  check_file_size_equals_zero "${file_read_from_flash}"

  echo "Checking that firmware is still functional"
  check_firmware_is_functional

  rm -rf "${file_read_from_flash}"
}

test_read_from_flash_in_bootloader_mode_while_setting_RDP_to_level_0() {
  local file_read_from_flash="test.bin"
  local original_fw_file="$1"
  local file_expected_byte_size="$(get_file_size ${original_fw_file})"

  # Given:
  #   * Hardware write protect is disabled
  #       (so we can use bootloader to read and change RDP level)
  #   * Software write protect is enabled
  #   * RDP is at level 1
  #
  # Then:
  #   * Setting the RDP level to 0 (after being at level 1) should trigger
  #     a mass erase.
  #   * A mass erase sets all flash bytes to 0xFF, so all bytes read from flash
  #     should have that value.
  #   * Since the flash was mass erased, the firmware should no longer function.
  echo "Reading firmware after setting RDP to level 0"
  # This command partially fails (and returns an error) because it causes the
  # flash to be mass erased, but we should still have a file with the contents
  # that we can compare against.
  read_from_flash_in_bootloader_mode_while_setting_RDP_to_level_0 \
    "${file_read_from_flash}" || true

  echo "Checking that value read is made up entirely of OxFF bytes"
  check_file_contains_all_0xFF_bytes \
    "${file_read_from_flash}" "${file_expected_byte_size}"

  # Make sure the flash was really erased
  echo "Checking that firmware is non-functional"
  check_firmware_is_not_functional

  rm -rf "${file_read_from_flash}"
}

echo "Running test to validate RDP level 1"

readonly ORIGINAL_FW_FILE="$1"

check_file_exists "${ORIGINAL_FW_FILE}"

echo "Making sure hardware write protect is DISABLED and software write \
protect is ENABLED"
check_hw_write_protect_disabled_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Checking that firmware is functional"
check_firmware_is_functional

test_read_from_flash_in_bootloader_mode_without_modifying_RDP_level

test_read_from_flash_in_bootloader_mode_while_setting_RDP_to_level_0 \
  "${ORIGINAL_FW_FILE}"
