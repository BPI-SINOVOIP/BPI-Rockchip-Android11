#!/bin/bash
#
# Copyright 2017 Google Inc. All Rights Reserved.
#
# This is a generic ChromeOS package/image test setup script. It is meant to
# be used for either the object file or package bisection tools. This script
# is intended to be used with host object bisection, to bisect the object
# files in a host package.  Since it deals with a host package, there is no
# building an image or flashing a device -- just building the host package
# itself.
#
# This script is intended to be used by binary_search_state.py, as
# part of the binary search triage on ChromeOS objects and packages. It should
# return '0' if the setup succeeds; and '1' if the setup fails (the image
# could not build or be flashed).
#

export PYTHONUNBUFFERED=1

source common/common.sh


if [[ "${BISECT_MODE}" == "OBJECT_MODE" ]]; then
  echo "EMERGING ${BISECT_PACKAGE}"
  sudo -E emerge ${BISECT_PACKAGE}
  emerge_status=$?

  if [[ ${emerge_status} -ne 0 ]] ; then
    echo "emerging ${BISECT_PACKAGE} returned a non-zero status: $emerge_status"
    exit 1
  fi

  exit 0
fi


exit 0
