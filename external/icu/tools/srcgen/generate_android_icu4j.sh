#!/bin/bash

# Copyright (C) 2015 The Android Open Source Project
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

# A script for generating the source code of the subset of ICU used by Android in libcore.

# Flag for applying java doc patches or not
APPLY_DOC_PATCH=1

# Build Options used by Android.bp
while true; do
  case "$1" in
    --no-doc-patch ) APPLY_DOC_PATCH=0; shift ;;
    --srcgen-tool ) SRCGEN_TOOL="$2"; shift 2;;
    --gen ) GEN_DIR="$2"; shift 2 ;;
    -- ) shift; break ;;
    * ) break ;;
  esac
done

if [ -n "$SRCGEN_TOOL" ]; then
    source $(dirname $BASH_SOURCE)/common.sh --do-not-make
    SRCGEN_TOOL_BINARY=${SRCGEN_TOOL}
else
    source $(dirname $BASH_SOURCE)/common.sh
    SRCGEN_TOOL_BINARY=${ANDROID_HOST_OUT}/bin/android_icu4j_srcgen_binary
fi

if [ -n "$GEN_DIR" ]; then
    ANDROID_ICU4J_DIR=${GEN_DIR}/android_icu4j
    mkdir -p ${ANDROID_ICU4J_DIR}
fi

WHITELIST_API_FILE=${ICU_SRCGEN_DIR}/whitelisted-public-api.txt
CORE_PLATFORM_API_FILE=${ICU_SRCGEN_DIR}/core-platform-api.txt
INTRA_CORE_API_FILE=${ICU_SRCGEN_DIR}/intra-core-api.txt
UNSUPPORTED_APP_USAGE_FILE=${ICU_SRCGEN_DIR}/unsupported-app-usage.json

# Clean out previous generated code / resources.
DEST_SRC_DIR=${ANDROID_ICU4J_DIR}/src/main/java
rm -rf ${DEST_SRC_DIR}
mkdir -p ${DEST_SRC_DIR}

DEST_RESOURCE_DIR=${ANDROID_ICU4J_DIR}/resources
rm -rf ${DEST_RESOURCE_DIR}
mkdir -p ${DEST_RESOURCE_DIR}

# Generate the source code needed by Android.
# Branches used for testing new versions of ICU will have have the ${WHITELIST_API_FILE} file
# that prevents new (stable) APIs being added to the Android public SDK API. The file should
# not exist on "normal" release branches and master.
ICU4J_BASE_COMMAND="${SRCGEN_TOOL_BINARY} Icu4jTransform"
if [ -e "${WHITELIST_API_FILE}" ]; then
  ICU4J_BASE_COMMAND+=" --hide-non-whitelisted-api ${WHITELIST_API_FILE}"
fi
${ICU4J_BASE_COMMAND} ${INPUT_DIRS} ${DEST_SRC_DIR} ${CORE_PLATFORM_API_FILE} ${INTRA_CORE_API_FILE} ${UNSUPPORTED_APP_USAGE_FILE}

# Copy / transform the resources needed by the android_icu4j code.
for INPUT_DIR in ${INPUT_DIRS}; do
  RESOURCES=$(find ${INPUT_DIR} -type f | egrep -v '(\.java|\/package\.html|\/ICUConfig\.properties)' || true )
  for RESOURCE in ${RESOURCES}; do
    SOURCE_DIR=$(dirname ${RESOURCE})
    RELATIVE_SOURCE_DIR=$(echo ${SOURCE_DIR} | sed "s,${INPUT_DIR}/,,")
    RELATIVE_DEST_DIR=$(echo ${RELATIVE_SOURCE_DIR} | sed 's,com/ibm/icu,android/icu,')
    DEST_DIR=${DEST_RESOURCE_DIR}/${RELATIVE_DEST_DIR}
    mkdir -p ${DEST_DIR}
    cp $RESOURCE ${DEST_DIR}
  done
done

# Create the ICUConfig.properties for Android.
mkdir -p ${ANDROID_ICU4J_DIR}/resources/android/icu
sed 's,com.ibm.icu,android.icu,' ${ANDROID_BUILD_TOP}/external/icu/icu4j/main/classes/core/src/com/ibm/icu/ICUConfig.properties > ${ANDROID_ICU4J_DIR}/resources/android/icu/ICUConfig.properties

# Clean out previous generated sample code.
SAMPLE_DEST_DIR=${ANDROID_ICU4J_DIR}/src/samples/java
rm -rf ${SAMPLE_DEST_DIR}
mkdir -p ${SAMPLE_DEST_DIR}

echo Processing sample code
# Create the android_icu4j sample code
${SRCGEN_TOOL_BINARY} Icu4jBasicTransform ${SAMPLE_INPUT_FILES} ${SAMPLE_DEST_DIR}

# Clean out previous generated test code.
TEST_DEST_DIR=${ANDROID_ICU4J_DIR}/src/main/tests
rm -rf ${TEST_DEST_DIR}
mkdir -p ${TEST_DEST_DIR}

# Create a temporary directory into which the testdata.jar can be unzipped. It must be called src
# as that is what is used to determine the root of the directory containing all the files to
# copy and that is used to calculate the relative path to the file that is used for its output path.
echo Unpacking testdata.jar
TESTDATA_DIR=$(mktemp -d)/src
mkdir -p ${TESTDATA_DIR}
unzip ${ICU4J_DIR}/main/shared/data/testdata.jar com/ibm/icu/* -d ${TESTDATA_DIR}

echo Processing test code
# Create the android_icu4j test code
ALL_TEST_INPUT_DIRS="${TEST_INPUT_DIRS} ${TESTDATA_DIR}"
${SRCGEN_TOOL_BINARY} Icu4jTestsTransform ${ALL_TEST_INPUT_DIRS} ${TEST_DEST_DIR}
# Apply line-based javadoc patches
if [ "$APPLY_DOC_PATCH" -eq "1" ]; then
    ${ANDROID_BUILD_TOP}/external/icu/tools/srcgen/javadoc_patches/apply_patches.sh
fi

# Copy the data files.
echo Copying test data
for INPUT_DIR in ${ALL_TEST_INPUT_DIRS}; do
  RESOURCES=$(find ${INPUT_DIR} -type f | egrep -v '(\.java|com\.ibm\.icu.*\.dat|/package\.html)' || true )
  for RESOURCE in ${RESOURCES}; do
    SOURCE_DIR=$(dirname ${RESOURCE})
    RELATIVE_SOURCE_DIR=$(echo ${SOURCE_DIR} | sed "s,${INPUT_DIR}/,,")
    RELATIVE_DEST_DIR=$(echo ${RELATIVE_SOURCE_DIR} | sed 's,com/ibm/icu,android/icu,')
    DEST_DIR=${TEST_DEST_DIR}/${RELATIVE_DEST_DIR}
    mkdir -p ${DEST_DIR}
    cp $RESOURCE ${DEST_DIR}
  done
done

echo Repackaging serialized test data
# Excludes JavaTimeZone.dat files as they depend on sun.util.calendar.ZoneInfo
for INPUT_DIR in ${ALL_TEST_INPUT_DIRS}; do
  RESOURCES=$(find ${INPUT_DIR} -type f | egrep '(/com\.ibm\.icu.*\.dat)' | egrep -v "JavaTimeZone.dat" || true )
  for RESOURCE in ${RESOURCES}; do
    SOURCE_DIR=$(dirname ${RESOURCE})
    RELATIVE_SOURCE_DIR=$(echo ${SOURCE_DIR} | sed "s,${INPUT_DIR}/,,")
    RELATIVE_DEST_DIR=$(echo ${RELATIVE_SOURCE_DIR} | sed 's,com/ibm/icu,android/icu,')
    SOURCE_NAME=$(basename ${RESOURCE})
    DEST_NAME=${SOURCE_NAME/com.ibm/android}
    DEST_DIR=${TEST_DEST_DIR}/${RELATIVE_DEST_DIR}
    mkdir -p ${DEST_DIR}
    # A simple textual substitution works even though the file is binary as 'com.ibm' and 'android'
    # are the same length.
    sed 's|com[./]ibm|android|g' $RESOURCE > ${DEST_DIR}/${DEST_NAME} 
  done
done
