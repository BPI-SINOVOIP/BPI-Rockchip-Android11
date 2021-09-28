#!/bin/bash

# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script takes an existing llvm ebuild file and generate a llvm-extra
# ebuild. The newly generated llvm-extra ebuild can be installed as a regular
# host package.
# The new ebuild should be generated in sys-devel/llvm-extra directory.
# The script also copies all the files from files/ directory.
# The generated llvm-extra ebuild is slotted so multiple instances of
# llvm-extra ebuilds can be installed at same time.
# The slot is derived based on the _pre<num> string in the llvm ebuild name.
# e.g. For llvm-7.0_pre331547_p20180529-r8.ebuild, the slot will be
# 7.0_pre331547.
#
# Usage:
#  ./create_llvm_extra.sh /path/to/llvm-7.0_pre331547_p20180529-r8.ebuild
#
# To use the clang installed by llvm-extra, modify the CFLAGS and
# LDFLAGS of a pckage to pass the patch of the clang binary installed by
# the llvm-extra package.
# e.g. append-flags -Xclang-path=/usr/llvm-extra/version/clang
#      append-ldflags -Xclang-path=/usr/llvm-extra/version/clang
#

SCRIPT_DIR=$(realpath $(dirname "$0"))

function check_cmd() {
	if [[ "$#" -ne 1 ]]; then
		echo "Exactly 1 argument expected"
		echo "Usage $0 <path_to_llvm_ebuild>"
		exit 1
	fi
	if [[ ! -f "$1" ]]; then
		echo "$1 is not a file"
		exit 1;
	fi
}

function create_llvm_extra_ebuild() {
	EBUILD_PREFIX=llvm-extra
	EBUILD_DIR=$(dirname "$1")
	EBUILD_FILE_NAME=$(basename "$1")
	NEW_EBUILD_FILE_NAME="${EBUILD_FILE_NAME/llvm/$EBUILD_PREFIX}"
	NEW_EBUILD_FILENAME_NO_EXT="${NEW_EBUILD_FILE_NAME%.*}"
	NEW_EBUILD_DIR="${EBUILD_DIR}/../${EBUILD_PREFIX}"
	NEW_EBUILD_PV="${NEW_EBUILD_FILENAME_NO_EXT#"$EBUILD_PREFIX-"}"
	NEW_EBUILD_SLOT="${NEW_EBUILD_PV%%_p[[:digit:]]*}"

	mkdir -p "${NEW_EBUILD_DIR}"
	if [[ -d "${EBUILD_DIR}/files" ]]; then
		cp -rf "${EBUILD_DIR}/files" "${NEW_EBUILD_DIR}"
	fi

	if [[ -f "${NEW_EBUILD_DIR}/${NEW_EBUILD_FILE_NAME}" ]]; then
		echo "Removing existing ebuild file ${NEW_EBUILD_FILE_NAME}"
		rm -f "${NEW_EBUILD_DIR}/${NEW_EBUILD_FILE_NAME}"
	fi
	# Generate the llvm-extra ebuild file.
	"${SCRIPT_DIR}"/create_ebuild_file.py "$1" "${NEW_EBUILD_DIR}/${NEW_EBUILD_FILE_NAME}"
	if [[ $? -ne 0 ]]; then
		echo "Creation of ${NEW_EBUILD_DIR}/${NEW_EBUILD_FILE_NAME} failed"
		exit 1
	fi
	echo "***"
	echo "***"
	echo "${NEW_EBUILD_DIR}/${NEW_EBUILD_FILE_NAME} has been created."

	echo "***"
	echo "Test if it builds by running \$ sudo emerge ${EBUILD_PREFIX}:${NEW_EBUILD_SLOT}"
	echo "***"
	echo "If it works, Go ahead and submit the newly generated ebuild"\
	     "and any other files in ${NEW_EBUILD_DIR}."
	echo "***"
	echo "Don't forget to add sys-devel/${EBUILD_PREFIX}:${NEW_EBUILD_SLOT} to"\
	     "the dependencies in virtual/target-chromium-os-sdk ebuild."
	echo "***"
	echo "***"
}


set -e
# Sanity checks.
check_cmd "${@}"
# Create llvm-extra ebuild.
create_llvm_extra_ebuild "${@}"
