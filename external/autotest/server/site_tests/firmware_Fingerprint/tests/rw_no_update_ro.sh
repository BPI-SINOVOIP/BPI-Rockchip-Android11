#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Running test to verify that RW cannot update RO"

readonly fw_file="${1}"

check_file_exists "${fw_file}"

echo "Making sure all write protect is enabled"
check_hw_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Flashing RO firmware (expected to fail)"
flash_ro_cmd="flashrom --fast-verify -V -p ec:type=fp -i EC_RO -w ${fw_file}"
if ${flash_ro_cmd}; then
  echo "Expected flashing of read-only firmware to fail"
  exit 1
fi
