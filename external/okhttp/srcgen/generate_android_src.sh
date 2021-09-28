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

PROJECT_DIR=external/okhttp

PACKAGE_TRANSFORMATIONS="\
    com.squareup:com.android \
    okio:com.android.okhttp.okio \
"

MODULE_DIRS="\
    android \
    okhttp \
    okhttp-urlconnection \
    okhttp-android-support \
    okio/okio \
"

SOURCE_DIRS="\
    src/main/java \
"

TAB_SIZE=2

# Repackage the project's source.
source ${ANDROID_BUILD_TOP}/tools/currysrc/scripts/repackage-common.sh

# Remove an unused source file:
rm ${REPACKAGED_DIR}/okhttp/src/main/java/com/android/okhttp/internal/Platform.java
