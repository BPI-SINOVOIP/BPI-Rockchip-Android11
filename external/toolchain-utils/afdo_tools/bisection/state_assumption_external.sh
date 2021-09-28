#!/bin/bash -eu
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script returns BAD_STATUS if '2' is in the top line of 'func_a's profile
# and good otherwise

GOOD_STATUS=0
BAD_STATUS=1
SKIP_STATUS=125
PROBLEM_STATUS=127

tmp_dir=$(pwd)/afdo_test_tmp
count_file=${tmp_dir}/.count

# keep count for purpose of filenames
if [ -f "${count_file}" ]; then
  num_call=$(cat "${count_file}")
else
  num_call=0
fi

echo -n $(( ${num_call}+1 )) > "${count_file}"

tmp_file=$(mktemp)
trap "rm -f '${tmp_file}'" EXIT
grep -v '^ ' "$1" > "${tmp_file}"

# copy prof to specific file for later test
if [[ $# -eq 2 ]]; then
  cp "$1" "${tmp_dir}/.second_run_${num_call}"
else
  cp "$1" "${tmp_dir}/.first_run_${num_call}"
fi

if grep -q 'func_a.*2' "${tmp_file}"; then
  exit "${BAD_STATUS}"
fi
exit "${GOOD_STATUS}"
