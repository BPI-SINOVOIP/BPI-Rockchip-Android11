#!/bin/bash

# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script modifies the original GSI to match the vendor version and
# the security patch level.
#
# Usage: change_security_patch_ver.sh <system.img> [<output_system.img> \
#                 [<new_security_patch_level> [<input_file_contexts.bin>]]] \
#                 [-v <vendor_version>]
#
# Examples:
# change_security_patch_ver.sh system.img
#   - Shows current version information.
# change_security_patch_ver.sh system.img new_system.img 2018-04-05
#   - Make new_system.img that has replaced SPL with 2018-04-05.
# change_security_patch_ver.sh system.img new_system.img -v 8.1.0
#   - Make new_system.img that includes the patches for vendor version 8.1.0.
# change_security_patch_ver.sh system.img new_system.img 2018-04-05 -v 8.1.0
#   - Make new_system.img that has both new SPL and vendor version.

function unmount() {
  echo "Unmounting..."
  sudo umount "${MOUNT_POINT}/"
}

SCRIPT_NAME=$(basename $0)

declare -a SUPPORTED_VENDOR_VERSIONS=(
  8.1.0
  9
)
SUPPORTED_VENDOR_VERSIONS="${SUPPORTED_VENDOR_VERSIONS[@]}"

param_count=0
while [[ $# -gt 0 ]]
do
case $1 in
-v|--vendor) # set vendor version
  VENDOR_VERSION=$2
  shift
  shift
  ;;
*) # set the ordered parameters
  ((param_count++))
  case $param_count in
  1) # The input file name for original GSI
    SYSTEM_IMG=$1
    shift
    ;;
  2) # The output file name for modified GSI
    OUTPUT_SYSTEM_IMG=$1
    shift
    ;;
  3) # New Security Patch Level to be written. It must be YYYY-MM-DD format.
    NEW_SPL=$1
    shift
    ;;
  4) # Selinux file context
    FILE_CONTEXTS_BIN=$1
    shift
    ;;
  *)
    ERROR=true
    break
    ;;
  esac
  ;;
esac
done

if ((param_count == 0)) || [ "$ERROR" == "true" ]; then
  echo "Usage: $SCRIPT_NAME <system.img> [<output_system.img> [<new_security_patch_level> [<input_file_contexts.bin>]]] [-v <vendor_version>]"
  exit 1
fi

# SPL must have YYYY-MM-DD format
if ((param_count >= 3)) && [[ ! ${NEW_SPL} =~ ^[0-9]{4}-(0[0-9]|1[012])-([012][0-9]|3[01])$ ]]; then
  echo "<new_security_patch_level> must have YYYY-MM-DD format"
  exit 1
fi

if [ "$VENDOR_VERSION" != "" ] && [[ ! ${VENDOR_VERSION} =~ ^(${SUPPORTED_VENDOR_VERSIONS// /\|})$ ]]; then
  echo "Available vendor_version: $SUPPORTED_VENDOR_VERSIONS"
  exit 1
fi

if [ "$VENDOR_VERSION" != "" ] && [ "$OUTPUT_SYSTEM_IMG" == "" ]; then
  echo "<output_system.img> must be provided to set vendor version"
  exit 1
fi

REQUIRED_BINARIES_LIST=(
  "img2simg"
  "simg2img"
)
if [ ! -z "${FILE_CONTEXTS_BIN}" ]; then
  REQUIRED_BINARIES_LIST+=("mkuserimg_mke2fs")
fi

# number of binaries to find.
BIN_COUNT=${#REQUIRED_BINARIES_LIST[@]}

# use an associative array to store binary path
declare -A BIN_PATH
for bin in ${REQUIRED_BINARIES_LIST[@]}; do
  BIN_PATH[${bin}]=""
done

# check current PATH environment first
for bin in ${REQUIRED_BINARIES_LIST[@]}; do
  if command -v ${bin} >/dev/null 2>&1; then
    echo "found ${bin} in PATH."
    BIN_PATH[${bin}]=${bin}
    ((BIN_COUNT--))
  fi
done

if [ ${BIN_COUNT} -gt 0 ]; then
  # listed in the recommended order.
  PATH_LIST=("${PWD}")
  if [ "${PWD##*/}" == "testcases" ] && [ -d "${PWD}/../bin" ]; then
    PATH_LIST+=("${PWD}/../bin")
  fi
  if [ -d "${ANDROID_HOST_OUT}" ]; then
    PATH_LIST+=("${ANDROID_HOST_OUT}/bin")
  fi

  for dir in ${PATH_LIST[@]}; do
    for bin in ${REQUIRED_BINARIES_LIST[@]}; do
      if [ -z "${BIN_PATH[${bin}]}" ] && [ -f "${dir}/${bin}" ]; then
        echo "found ${bin} in ${dir}."
        BIN_PATH[${bin}]=${dir}/${bin}
        ((BIN_COUNT--))
        if [ ${BIN_COUNT} -eq 0 ]; then break; fi
      fi
    done
  done
fi

if [ ${BIN_COUNT} -gt 0 ]; then
  echo "Cannot find the required binaries. Need lunch; or run in a correct path."
  exit 1
fi
echo "Found all binaries."

MOUNT_POINT="${PWD}/temp_mnt"
SPL_PROPERTY_NAME="ro.build.version.security_patch"
RELEASE_VERSION_PROPERTY_NAME="ro.build.version.release"
VNDK_VERSION_PROPERTY="ro.vndk.version"
VNDK_VERSION_PROPERTY_OMR1="${VNDK_VERSION_PROPERTY}=27"

UNSPARSED_SYSTEM_IMG="${SYSTEM_IMG}.raw"
SYSTEM_IMG_MAGIC="$(xxd -g 4 -l 4 "$SYSTEM_IMG" | head -n1 | awk '{print $2}')"
if [ "$SYSTEM_IMG_MAGIC" = "3aff26ed" ]; then
  echo "Unsparsing ${SYSTEM_IMG}..."
  ${BIN_PATH["simg2img"]} "$SYSTEM_IMG" "$UNSPARSED_SYSTEM_IMG"
else
  echo "Copying unsparse input system image ${SYSTEM_IMG}..."
  cp "$SYSTEM_IMG" "$UNSPARSED_SYSTEM_IMG"
fi

IMG_SIZE=$(stat -c%s "$UNSPARSED_SYSTEM_IMG")

echo "Mounting..."
mkdir -p "$MOUNT_POINT"
sudo mount -t ext4 -o loop "$UNSPARSED_SYSTEM_IMG" "${MOUNT_POINT}/"

# check the property file path
BUILD_PROP_PATH_LIST=(
  "/system/build.prop"  # layout of A/B support
  "/build.prop"         # layout of non-A/B support
)
BUILD_PROP_MOUNT_PATH=""
BUILD_PROP_PATH=""

echo "Finding build.prop..."
for path in ${BUILD_PROP_PATH_LIST[@]}; do
  if [ -f "${MOUNT_POINT}${path}" ]; then
    BUILD_PROP_MOUNT_PATH="${MOUNT_POINT}${path}"
    BUILD_PROP_PATH=${path}
    echo "  ${path}"
    break
  fi
done

PROP_DEFAULT_PATH_LIST=(
  "/system/etc/prop.default"  # layout of A/B support
  "/etc/prop.default"         # layout of non-A/B support
)

if [ "$BUILD_PROP_MOUNT_PATH" != "" ]; then
  if [ "$OUTPUT_SYSTEM_IMG" != "" ]; then
    echo "Replacing..."
  fi
  CURRENT_SPL=`sudo sed -n -r "s/^${SPL_PROPERTY_NAME}=(.*)$/\1/p" ${BUILD_PROP_MOUNT_PATH}`
  CURRENT_VERSION=`sudo sed -n -r "s/^${RELEASE_VERSION_PROPERTY_NAME}=(.*)$/\1/p" ${BUILD_PROP_MOUNT_PATH}`
  echo "  Current security patch level: ${CURRENT_SPL}"
  echo "  Current release version: ${CURRENT_VERSION}"

  # Update SPL to <new_security_patch_level>
  if [[ "$OUTPUT_SYSTEM_IMG" != "" && "$NEW_SPL" != "" ]]; then
    if [[ "$CURRENT_SPL" == "" ]]; then
      echo "ERROR: Cannot find ${SPL_PROPERTY_NAME} in ${BUILD_PROP_PATH}"
    else
      echo "  New security patch level: ${NEW_SPL}"
      seek=$(sudo grep --byte-offset "${SPL_PROPERTY_NAME}=" "${BUILD_PROP_MOUNT_PATH}" | cut -d':' -f 1)
      seek=$(($seek + ${#SPL_PROPERTY_NAME} + 1))   # 1 is for '='
      echo "${NEW_SPL}" | sudo dd of="${BUILD_PROP_MOUNT_PATH}" seek="$seek" bs=1 count=10 conv=notrunc
    fi
  fi

  # Update release version to <vendor_version>
  if [[ "$OUTPUT_SYSTEM_IMG" != "" && "$VENDOR_VERSION" != "" ]]; then
    if [[ "$CURRENT_VERSION" == "" ]]; then
      echo "ERROR: Cannot find ${RELEASE_VERSION_PROPERTY_NAME} in ${BUILD_PROP_PATH}"
    else
      echo "  New release version for vendor.img: ${VENDOR_VERSION}"
      sudo sed -i -e "s/^${RELEASE_VERSION_PROPERTY_NAME}=.*$/${RELEASE_VERSION_PROPERTY_NAME}=${VENDOR_VERSION}/" ${BUILD_PROP_MOUNT_PATH}
    fi

    if [[ "$VENDOR_VERSION" == "8.1.0" ]]; then
      # add ro.vndk.version for O-MR1
      echo "Finding prop.default..."
      for path in ${PROP_DEFAULT_PATH_LIST[@]}; do
        if [ -f "${MOUNT_POINT}${path}" ]; then
          PROP_DEFAULT_PATH=${path}
          echo "  ${path}"
          break
        fi
      done

      if [[ "$PROP_DEFAULT_PATH" != "" ]]; then
        CURRENT_VNDK_VERSION=`sudo sed -n -r "s/^${VNDK_VERSION_PROPERTY}=(.*)$/\1/p" ${MOUNT_POINT}${PROP_DEFAULT_PATH}`
        if [[ "$CURRENT_VNDK_VERSION" != "" ]]; then
          echo "WARNING: ${VNDK_VERSION_PROPERTY} is already set to ${CURRENT_VNDK_VERSION} in ${PROP_DEFAULT_PATH}"
        else
          echo "  Add \"${VNDK_VERSION_PROPERTY_OMR1}\" to ${PROP_DEFAULT_PATH} for O-MR1 vendor image."
          sudo sed -i -e "\$a\#\n\# FOR O-MR1 DEVICES\n\#\n${VNDK_VERSION_PROPERTY_OMR1}" ${MOUNT_POINT}${PROP_DEFAULT_PATH}
        fi
      else
        echo "ERROR: Cannot find prop.default."
      fi
    fi
  fi
else
  echo "ERROR: Cannot find build.prop."
fi

if [ "$OUTPUT_SYSTEM_IMG" != "" ]; then
  if [ "$FILE_CONTEXTS_BIN" != "" ]; then
    echo "Writing ${OUTPUT_SYSTEM_IMG}..."

    (cd $ANDROID_BUILD_TOP
     if [[ "$(whereis mkuserimg_mke2fs | wc -w)" < 2 ]]; then
       make mkuserimg_mke2fs -j
     fi
     NON_AB=$(expr "$BUILD_PROP_PATH" == "/build.prop")
     if [ $NON_AB -eq 1 ]; then
       sudo /bin/bash -c "PATH=out/host/linux-x86/bin/:\$PATH mkuserimg_mke2fs -s ${MOUNT_POINT} $OUTPUT_SYSTEM_IMG ext4 system $IMG_SIZE -D ${MOUNT_POINT} -L system $FILE_CONTEXTS_BIN"
     else
       sudo /bin/bash -c "PATH=out/host/linux-x86/bin/:\$PATH mkuserimg_mke2fs -s ${MOUNT_POINT} $OUTPUT_SYSTEM_IMG ext4 / $IMG_SIZE -D ${MOUNT_POINT}/system -L / $FILE_CONTEXTS_BIN"
     fi)

    unmount
  else
    unmount

    echo "Writing ${OUTPUT_SYSTEM_IMG}..."
    ${BIN_PATH["img2simg"]} "$UNSPARSED_SYSTEM_IMG" "$OUTPUT_SYSTEM_IMG"
  fi
else
  unmount
fi

echo "Done."
