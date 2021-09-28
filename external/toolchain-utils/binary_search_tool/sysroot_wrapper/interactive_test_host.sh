#!/bin/bash -u
#
# Copyright 2017 Google Inc. All Rights Reserved.
#
# This script is intended to be used by binary_search_state.py, as
# part of the binary search triage on ChromeOS package and object files for a
# host package. It waits for the test setup script to build the image, then asks
# the user if the image is good or not. (Since this is a host package, there is
# no 'install' phase needed.)  This script should return '0' if the test succeeds
# (the image is 'good'); '1' if the test fails (the image is 'bad'); and '125'
# if it could not determine (does not apply in this case).
#

source common/common.sh

while true; do
    read -p "Is this a good ChromeOS image?" yn
    case $yn in
        [Yy]* ) exit 0;;
        [Nn]* ) exit 1;;
        * ) echo "Please answer yes or no.";;
    esac
done

exit 125
