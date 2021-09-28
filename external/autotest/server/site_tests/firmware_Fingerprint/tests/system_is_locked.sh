#!/bin/bash

# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. $(dirname "$(readlink -f "${0}")")/common.sh

echo "Running test to validate system_is_locked()"

readonly ORIGINAL_FW_FILE="$1"

check_file_exists "${ORIGINAL_FW_FILE}"

echo "Making sure hardware write protect is ENABLED and software write \
protect is ENABLED"
check_hw_and_sw_write_protect_enabled

echo "Validating initial state"
check_has_mp_rw_firmware
check_has_mp_ro_firmware
check_running_rw_firmware
check_is_rollback_set_to_initial_val

echo "Checking that firmware is functional"
check_firmware_is_functional

echo "Checking that we cannot access raw frame"
check_raw_fpframe_fails

