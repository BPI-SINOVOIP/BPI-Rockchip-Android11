#!/bin/bash -eu

# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script extracts command line options to build bad item.
# The generated script will be used by pass level bisection.
#

source android/common.sh

abs_path=$1

# The item will be `-o relative-path-to-object `, which will be used
# for seeking command in populate log.
# We care about the `-o` at the beginning and ` ` at the end are necessary,
# so that we can get build command for exact this object file.
# Example: prebuilt/../clang++ -O3 -MF obj1.o.d -o obj.o obj.cpp
# We should count this command as one to build obj.o, not obj1.o.d.
real_path=$(realpath --relative-to="${BISECT_WORK_BUILD}" "${abs_path}")
item="-o $real_path "

populate_log=${BISECT_BAD_BUILD}/_POPULATE_LOG

output='#!/bin/bash -u\n'
output+='source android/common.sh\n'

result=$(egrep -m 1 -- "${item}" ${populate_log})

# Re-generate bad item to tmp directory location
tmp_ir='/tmp/bisection_bad_item.o'
result=$(sed "s|$item|-o $tmp_ir |g" <<< ${result})

# Remove `:` after cd command
result=$(sed 's|cd:|cd|g' <<< ${result})

# Add environment variable which helps pass level bisection
result=$(sed 's| -o | $LIMIT_FLAGS -o |g' <<< ${result})

output+=${result}

# Symbolic link generated bad item to original object
output+="\nln -f $tmp_ir $abs_path"
output+="\ntouch $abs_path"

echo -e "${output}" > android/cmd_script.sh

chmod u+x android/cmd_script.sh

echo 'Script created as android/cmd_script.sh'

# Check if compiler is LLVM.
if grep -q "clang" android/cmd_script.sh
then
    exit 0
else
    echo 'Pass/transformation level bisection only works for LLVM compiler.'
    exit 1
fi