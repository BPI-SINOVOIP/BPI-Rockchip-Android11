#!/bin/bash
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Execute cfassembler for the given programs.
prog="$0"
cfassembler="$1"
shift
soong_zip="$1"
shift
gen_dir="$1"
shift
out="$1"
shift

for IN in $@ ; do
  # Strip cts/tools/vm-tests-tf/src
  TESTDIR_DFH=${IN#cts/tools/vm-tests-tf/src}
  TESTDIR=${TESTDIR_DFH%.dfh}
  TESTNAME=`basename $TESTDIR`
  TESTDIRPARENT=`dirname $TESTDIR`

  GENDIR="$gen_dir/tmp/tests/$TESTDIR"
  GENDIRP="$gen_dir/tmp/tests/$TESTDIRPARENT"

  mkdir -p $GENDIR || exit 1
  # Assemble.
  $cfassembler $IN $GENDIR || exit 1
  # Wrap inside jar.
  $soong_zip -o $GENDIRP/$TESTNAME.jar -C $GENDIR -D $GENDIR || exit 1
  rm $GENDIR/classes.dex || exit 1
  rmdir $GENDIR || exit 1
done

# Create the final jar.
$soong_zip -o $out -C $gen_dir/tmp -D $gen_dir/tmp || exit 1
rm -rf $gen_dir/tmp || exit 1
