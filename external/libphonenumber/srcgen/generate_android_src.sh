#!/bin/bash
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [[ -z "${ANDROID_BUILD_TOP}" ]]; then
    echo "Missing environment variables. Did you run build/envsetup.sh and lunch?" >&2
    exit 1
fi

PROJECT_DIR=external/libphonenumber

PACKAGE_TRANSFORMATIONS="\
    com.google:com.android \
"

MODULE_DIRS="\
    libphonenumber \
    geocoder \
    internal/prefixmapper \
"

SOURCE_DIRS="\
    src \
"

TAB_SIZE=2

# Repackage the project's source.
source ${ANDROID_BUILD_TOP}/tools/currysrc/scripts/repackage-common.sh

for i in ${MODULE_DIRS}
do
  for s in ${SOURCE_DIRS}
  do
    IN=${PROJECT_DIR}/$i/$s
    if [ -d $IN ]; then
      OUT=${REPACKAGED_DIR}/$i/$s
      # Copy any resources
      echo Copying resources from ${IN} to ${OUT}
      RESOURCES=$(find ${IN} -type f | egrep -v '(\.java|\/package\.html)' || true)
      for RESOURCE in ${RESOURCES}; do
        SOURCE_DIR=$(dirname ${RESOURCE})
        RELATIVE_SOURCE_DIR=$(echo ${SOURCE_DIR} | sed "s,${IN}/,,")
        RELATIVE_DEST_DIR=$(echo ${RELATIVE_SOURCE_DIR} | sed 's,com/google,com/android,')
        DEST_DIR=${OUT}/${RELATIVE_DEST_DIR}
        mkdir -p ${DEST_DIR}
        cp $RESOURCE ${DEST_DIR}
      done
    fi
  done
done

