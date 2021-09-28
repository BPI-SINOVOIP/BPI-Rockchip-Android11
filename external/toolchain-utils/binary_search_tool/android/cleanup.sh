#!/bin/bash
#
# Copyright 2016 Google Inc. All Rights Reserved.
#
# This script is part of the Android binary search triage process.
# It should be the last script called by the user, after the user has
# successfully run the bisection tool and found their bad items. This script
# will perform all necessary cleanup for the bisection tool.
#

rm android/common.sh
# Remove build command script if pass_bisect enabled
rm -f android/cmd_script.sh
# Remove tmp IR file used for ir_diff in pass beisction
rm -f /tmp/bisection_bad_item.o
