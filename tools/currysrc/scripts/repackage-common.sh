#!/bin/bash
# Copyright (C) 2019 The Android Open Source Project
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

# Common logic for use by repackaging scripts.
# The following environment variables must be set before including this script:
#
#   PROJECT_DIR
#        the root directory (relative to ${ANDROID_BUILD_TOP}) of the project within which the
#        repackaging is to be done. e.g. external/conscrypt
#
#   MODULE_DIRS
#        a space separated list of the module directories (relative to the PROJECT_DIR) whose
#        sources need repackaging. e.g. core common android
#
#   SOURCE_DIRS
#        a space separated list of the source directories (relative to the MODULE_DIRS) that are to
#        be repackaged. If the ${PROJECT_DIR}/${MODULE_DIR}/${SOURCE_DIR} does not exist then it is
#        ignored. e.g. src/main/java src/main/test
#
#   PACKAGE_TRANSFORMATIONS
#        a space separated list of the package transformations to apply. Must be in the form
#        <old package prefix>:<new package prefix>.
#
#   UNSUPPORTED_APP_USAGE_CLASS
#        the fully qualified path to the UnsupportedAppUsage annotation to insert.
#
# The following environment variables are optional.
#
#   TAB_SIZE
#        the tab size for formatting any inserted code, e.g. annotations. Defaults to 4.
#
# The following environment variables can be used after including this file:
#   REPACKAGED_DIR
#        the absolute path to the directory into which the repackaged source has been written.
#
# This should be used as follows:
#
#if [[ -z "${ANDROID_BUILD_TOP}" ]]; then
#    echo "Missing environment variables. Did you run build/envsetup.sh and lunch?" >&2
#    exit 1
#fi
#
# PROJECT_DIR=...
# MODULE_DIRS=...
# SOURCE_DIRS=...
# PACKAGE_TRANSFORMATIONS=...
# source ${ANDROID_BUILD_TOP}/tools/currysrc/scripts/repackage-common.sh
# ...any post transformation changes, e.g. to remove unnecessary files.

if [[ -z "${ANDROID_BUILD_TOP}" ]]; then
    echo "Missing environment variables. Did you run build/envsetup.sh and lunch?" >&2
    exit 1
fi

if [[ -z "${PROJECT_DIR}" ]]; then
  echo "PROJECT_DIR is not set" >&2
  exit 1
fi

PROJECT_DIR=${ANDROID_BUILD_TOP}/${PROJECT_DIR}

if [[ ! -d "${PROJECT_DIR}" ]]; then
  echo "${PROJECT_DIR} does not exist" >&2
  exit 1
fi

if [[ -z "${MODULE_DIRS}" ]]; then
  echo "MODULE_DIRS is not set" >&2
  exit 1
fi

if [[ -z "${SOURCE_DIRS}" ]]; then
  echo "SOURCE_DIRS is not set" >&2
  exit 1
fi

if [[ -z "${PACKAGE_TRANSFORMATIONS}" ]]; then
  echo "PACKAGE_TRANSFORMATIONS is not set" >&2
  exit 1
fi

set -e

CLASSPATH=${ANDROID_HOST_OUT}/framework/currysrc.jar
CHANGE_LOG=$(mktemp --suffix srcgen-change.log)

function get_uncommitted_repackaged_files() {
  git -C "${PROJECT_DIR}" status -s | cut -c4- | grep "^repackaged/"
}

cd ${ANDROID_BUILD_TOP}
build/soong/soong_ui.bash --make-mode currysrc

DEFAULT_CONSTRUCTORS_FILE=${PROJECT_DIR}/srcgen/default-constructors.txt
CORE_PLATFORM_API_FILE=${PROJECT_DIR}/srcgen/core-platform-api.txt
INTRA_CORE_API_FILE=${PROJECT_DIR}/srcgen/intra-core-api.txt
UNSUPPORTED_APP_USAGE_FILE=${PROJECT_DIR}/srcgen/unsupported-app-usage.json

TAB_SIZE=${TAB_SIZE-4}

REPACKAGE_ARGS=""
SEP=""
for i in ${PACKAGE_TRANSFORMATIONS}
do
  REPACKAGE_ARGS="${REPACKAGE_ARGS}${SEP}--package-transformation ${i}"
  SEP=" "
done

if [[ -f "${DEFAULT_CONSTRUCTORS_FILE}" ]]; then
  echo "Adding default constructors from ${DEFAULT_CONSTRUCTORS_FILE}"
  REPACKAGE_ARGS="${REPACKAGE_ARGS}${SEP}--default-constructors-file ${DEFAULT_CONSTRUCTORS_FILE}"
  SEP=" "
fi

if [[ -f "${CORE_PLATFORM_API_FILE}" ]]; then
  echo "Adding CorePlatformApi annotations from ${CORE_PLATFORM_API_FILE}"
  REPACKAGE_ARGS="${REPACKAGE_ARGS}${SEP}--core-platform-api-file ${CORE_PLATFORM_API_FILE}"
  SEP=" "
fi

if [[ -f "${INTRA_CORE_API_FILE}" ]]; then
  echo "Adding IntraCoreApi annotations from ${INTRA_CORE_API_FILE}"
  REPACKAGE_ARGS="${REPACKAGE_ARGS}${SEP}--intra-core-api-file ${INTRA_CORE_API_FILE}"
  SEP=" "
fi

if [[ -f "${UNSUPPORTED_APP_USAGE_FILE}" ]]; then
  echo "Adding UnsupportedAppUsage annotations from ${UNSUPPORTED_APP_USAGE_FILE}"
  REPACKAGE_ARGS="${REPACKAGE_ARGS}${SEP}--unsupported-app-usage-file ${UNSUPPORTED_APP_USAGE_FILE}"
  SEP=" "
  if [[ -n "${UNSUPPORTED_APP_USAGE_CLASS}" ]]; then
    REPACKAGE_ARGS="${REPACKAGE_ARGS}${SEP}--unsupported-app-usage-class ${UNSUPPORTED_APP_USAGE_CLASS}"
  fi
fi

if [[ -n "${TAB_SIZE}" ]]; then
  echo "Using tab size of ${TAB_SIZE}"
  REPACKAGE_ARGS="${REPACKAGE_ARGS}${SEP}--tab-size ${TAB_SIZE}"
  SEP=" "
fi

function do_transform() {
  local SRC_IN_DIR=$1
  local SRC_OUT_DIR=$2

  rm -rf ${SRC_OUT_DIR}
  mkdir -p ${SRC_OUT_DIR}

  java -cp ${CLASSPATH} com.google.currysrc.aosp.RepackagingTransform \
       --source-dir ${SRC_IN_DIR} \
       --target-dir ${SRC_OUT_DIR} \
       --change-log ${CHANGE_LOG} \
       ${REPACKAGE_ARGS}

  # Restore TEST_MAPPING files that may have been removed from the source directory
  (cd $SRC_OUT_DIR; git checkout HEAD $(git status --short | grep -E "^ D .*/TEST_MAPPING$" | cut -c4-))
}

REPACKAGED_DIR=${PROJECT_DIR}/repackaged
for i in ${MODULE_DIRS}
do
  MODULE_DIR=${PROJECT_DIR}/${i}
  if [[ ! -d ${MODULE_DIR} ]]; then
    echo "Module directory ${MODULE_DIR} does not exist" >&2
    exit 1;
  fi

  for s in ${SOURCE_DIRS}
  do
    IN=${MODULE_DIR}/${s}
    if [[ -d ${IN} ]]; then
      OUT=${REPACKAGED_DIR}/${i}/${s}
      do_transform ${IN} ${OUT}
    fi
  done
done

# Check to ensure that the entries in the change log are correct
typeset -i ERROR=0
function checkChangeLog {
  local IN="$1"
  local TAG="$2"
  local MSG="$3"
  DIFF=$(comm -23 "${IN}" <(grep -P "^\Q$TAG\E:" ${CHANGE_LOG} | cut -f2- -d: | sort -u))
  if [[ -n "${DIFF}" ]]; then
    ERROR=1
    echo -e "\nERROR: ${MSG}" >&2
    for i in ${DIFF}
    do
      echo "  $i" >&2
    done
    echo >&2
  fi
}

if [[ -f "${DEFAULT_CONSTRUCTORS_FILE}" ]]; then
  # Check to ensure that all the requested default constructors were added.
  checkChangeLog <(sort -u "${DEFAULT_CONSTRUCTORS_FILE}" | grep -v '^#') "AddDefaultConstructor" \
      "Default constructors were not added at the following locations from ${DEFAULT_CONSTRUCTORS_FILE}:"
fi

if [[ -f "${CORE_PLATFORM_API_FILE}" ]]; then
  # Check to ensure that all the requested annotations were added.
  checkChangeLog <(sort -u "${CORE_PLATFORM_API_FILE}" | grep -v '^#') "@libcore.api.CorePlatformApi" \
      "CorePlatformApi annotations were not added at the following locations from ${CORE_PLATFORM_API_FILE}:"
fi

if [[ -f "${INTRA_CORE_API_FILE}" ]]; then
  # Check to ensure that all the requested annotations were added.
  checkChangeLog <(sort -u "${INTRA_CORE_API_FILE}" | grep -v '^#') "@libcore.api.IntraCoreApi" \
      "IntraCoreApi annotations were not added at the following locations from ${INTRA_CORE_API_FILE}:"
fi

if [[ -f "${UNSUPPORTED_APP_USAGE_FILE}" ]]; then
  # Check to ensure that all the requested annotations were added.
  checkChangeLog <(grep @location "${UNSUPPORTED_APP_USAGE_FILE}" | grep -vE "[[:space:]]*//" | cut -f4 -d\" | sort -u) \
      "@android.compat.annotation.UnsupportedAppUsage" \
      "UnsupportedAppUsage annotations were not added at the following locations from ${UNSUPPORTED_APP_USAGE_FILE}:"
fi

if [[ $ERROR = 1 ]]; then
  echo "Errors found during transformation, see above.\n" >&2
  exit 1
fi
