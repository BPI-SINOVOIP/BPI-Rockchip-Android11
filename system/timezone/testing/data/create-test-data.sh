#!/usr/bin/env bash
#
# Copyright (C) 2017 The Android Open Source Project
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
#
# Updates the test data files.
TIMEZONE_DIR=${ANDROID_BUILD_TOP}/system/timezone
DISTRO_TOOLS_DIR=${TIMEZONE_DIR}/distro/tools
REFERENCE_DISTRO_FILES=${TIMEZONE_DIR}/output_data

# Fail on error
set -e

# Test 1: A set of data newer than the system-image data from ${TIMEZONE_DIR}
IANA_VERSION=2030a
TEST_DIR=test1

# Create fake distro input files.
./transform-distro-files.sh ${REFERENCE_DISTRO_FILES} ${IANA_VERSION} ./${TEST_DIR}/output_data

# Create the distro .zip
mkdir -p ${TEST_DIR}/output_data/distro
mkdir -p ${TEST_DIR}/output_data/version
${DISTRO_TOOLS_DIR}/create-distro.py \
    -iana_version ${IANA_VERSION} \
    -revision 1 \
    -tzdata ${TEST_DIR}/output_data/iana/tzdata \
    -icu ${TEST_DIR}/output_data/icu_overlay/icu_tzdata.dat \
    -tzlookup ${TEST_DIR}/output_data/android/tzlookup.xml \
    -telephonylookup ${TEST_DIR}/output_data/android/telephonylookup.xml \
    -output_distro_dir ${TEST_DIR}/output_data/distro \
    -output_version_file ${TEST_DIR}/output_data/version/tz_version

# Test 2: A set of data older than the system-image data from ${TIMEZONE_DIR}
IANA_VERSION=2016a
TEST_DIR=test2

# Create fake distro input files.
./transform-distro-files.sh ${REFERENCE_DISTRO_FILES} ${IANA_VERSION} ./${TEST_DIR}/output_data

# Create the distro .zip
mkdir -p ${TEST_DIR}/output_data/distro
mkdir -p ${TEST_DIR}/output_data/version
${DISTRO_TOOLS_DIR}/create-distro.py \
    -iana_version ${IANA_VERSION} \
    -revision 1 \
    -tzdata ${TEST_DIR}/output_data/iana/tzdata \
    -icu ${TEST_DIR}/output_data/icu_overlay/icu_tzdata.dat \
    -tzlookup ${TEST_DIR}/output_data/android/tzlookup.xml \
    -telephonylookup ${TEST_DIR}/output_data/android/telephonylookup.xml \
    -output_distro_dir ${TEST_DIR}/output_data/distro \
    -output_version_file ${TEST_DIR}/output_data/version/tz_version

# Test 3: A corrupted set of data like test 1, but with a truncated ICU
# overlay file. This test data set exists because it is (currently) a good way
# to trigger a boot loop which enables easy watchdog and recovery testing.
IANA_VERSION=2030a
TEST_DIR=test3

# Create fake distro input files.
./transform-distro-files.sh ${REFERENCE_DISTRO_FILES} ${IANA_VERSION} ./${TEST_DIR}/output_data

# Corrupt icu_tzdata.dat by truncating it
truncate --size 27766 ${TEST_DIR}/output_data/icu_overlay/icu_tzdata.dat

# Create the distro .zip
mkdir -p ${TEST_DIR}/output_data/distro
mkdir -p ${TEST_DIR}/output_data/version
${DISTRO_TOOLS_DIR}/create-distro.py \
    -iana_version ${IANA_VERSION} \
    -revision 1 \
    -tzdata ${TEST_DIR}/output_data/iana/tzdata \
    -icu ${TEST_DIR}/output_data/icu_overlay/icu_tzdata.dat \
    -tzlookup ${TEST_DIR}/output_data/android/tzlookup.xml \
    -telephonylookup ${TEST_DIR}/output_data/android/telephonylookup.xml \
    -output_distro_dir ${TEST_DIR}/output_data/distro \
    -output_version_file ${TEST_DIR}/output_data/version/tz_version
