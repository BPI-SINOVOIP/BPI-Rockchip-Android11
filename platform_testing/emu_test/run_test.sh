#!/bin/bash
# It is to be used with BYOB setup to run tests on cloud VMs.
# It will run UI and boot tests on them.
#
# It takes 3 command line arguments.
# DIST_DIR => Absolute path for the distribution directory.
# API => API number for the system image
# ORI => branch code for the system image
#
# It will return 0 if it is able to execute tests, otherwise
# it will return 1.
#
# For the test results please refer to go/dashboard-adt
#
# Owner: akagrawal@google.com

DIST_DIR=$1
API=$2
ORI=$3

function run_with_timeout () {
   ( $1 $2 $3 $4 ) & pid=$!
   ( sleep $5 && kill -HUP $pid ) 2>/dev/null & watcher=$!
   if wait $pid 2>/dev/null; then
      pkill -HUP -P $watcher
      wait $watcher
   else
      echo "Test time out."
      exit 1
   fi
}

echo "Checkout adt-infra repo"
# $ADT_INFRA has to be set on the build machine. It should have absolute path
# where adt-infra needs to be checked out.
rm -rf $ADT_INFRA
git clone https://android.googlesource.com/platform/external/adt-infra -b emu-master-dev $ADT_INFRA

BUILD_DIR="out/prebuilt_cached/builds"

export ANDROID_HOME=$SDK_SYS_IMAGE
export ANDROID_SDK_ROOT=$SDK_SYS_IMAGE

echo "Setup builds"
$ADT_INFRA/emu_test/utils/setup_builds.sh $BUILD_DIR $API

echo "Run Boot tests from $ADT_INFRA"
cmd="$ADT_INFRA/emu_test/utils/run_boot_test.sh"
run_with_timeout $cmd $DIST_DIR $ORI $API 5400

# Skip UI tests for presubmit build which has a build number starts with P.
if [[ $BUILD_NUMBER != P* ]]; then
    echo "Run UI tests from $ADT_INFRA"
    cmd="$ADT_INFRA/emu_test/utils/run_ui_test.sh"
    run_with_timeout $cmd $DIST_DIR $ORI $API 10800
fi

echo "Cleanup prebuilts"
rm -rf /buildbot/prebuilt/*

exit 0
