#!/bin/bash -eu
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script returns the result of state_assumption_external.sh on every even
# iteration, and PROBLEM_STATUS on every odd_iteration

PROBLEM_STATUS=127

tmp_dir=$(pwd)/afdo_test_tmp

count_file="${tmp_dir}/.count"
if [[ -f "${count_file}" ]]; then
  num_call=$(cat "${count_file}")
else
  num_call=0
fi

local_count_file=${tmp_dir}/.local_count
if [[ -f "${local_count_file}" ]]; then
  local_count=$(cat "${local_count_file}")
else
  local_count=0
fi

echo -n $(( ${local_count}+1 )) > "${local_count_file}"

# Don't want to fail on performance checks hence local_count >= 2
# but following that, want to fail every other check
if [[ ${local_count} -ge 2 ]] && [[ $(( ${num_call}%2 )) -ne 0 ]]; then
  echo -n $(( ${num_call}+1 )) > "${count_file}"
  exit "${PROBLEM_STATUS}"
fi

# script just needs any second argument to write profs to .second_run_*
$(pwd)/state_assumption_external.sh "$1" 'second_run'
exit $?
