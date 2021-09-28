#!/bin/bash
#
# Copyright (C) 2007 The Android Open Source Project
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

# This script is used by external_updater to replace a package. Don't
# invoke directly.

set -e

tmp_dir=$1
external_dir=$2

echo "Entering $tmp_dir..."
cd $tmp_dir

function CopyIfPresent() {
  if [ -e $external_dir/$1 ]; then
    cp -a -n $external_dir/$1 .
  fi
}

echo "Copying preserved files..."
CopyIfPresent "Android.bp"
CopyIfPresent "Android.mk"
CopyIfPresent "CleanSpec.mk"
CopyIfPresent "LICENSE"
CopyIfPresent "NOTICE"
cp -a -f -n $external_dir/MODULE_LICENSE_* .
CopyIfPresent "METADATA"
CopyIfPresent ".git"
CopyIfPresent ".gitignore"
CopyIfPresent "patches"
CopyIfPresent "post_update.sh"
CopyIfPresent "OWNERS"

echo "Applying patches..."
for p in $tmp_dir/patches/*.diff
do
  [ -e "$p" ] || continue
  echo "Applying $p..."
  patch -p1 -d $tmp_dir < $p;
done

if [ -f $tmp_dir/post_update.sh ]
then
  echo "Running post update script"
  $tmp_dir/post_update.sh $tmp_dir $external_dir
fi

echo "Swapping old and new..."
rm -rf $external_dir
mv $tmp_dir $external_dir

exit 0
