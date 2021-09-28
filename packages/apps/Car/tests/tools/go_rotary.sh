#!/bin/bash

# Copyright (C) 2020 The Android Open Source Project
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

TMP_OUTDIR="/tmp/rotary"
ME=`basename "$0"`

function help {
    echo "A simple helper script that runs the Trade Federation unit tests"
    echo "to print this message: packages/apps/Car/tests/tools/$ME"
    echo "to build: packages/apps/Car/tests/tools/$ME b"
    echo "to install: packages/apps/Car/tests/tools/$ME i"
    echo "to run only: packages/apps/Car/tests/tools/$ME r"
    echo "the apks and jar are in $TMP_OUTDIR"
}

function build {
    echo
    echo "Building the apks"
    . build/envsetup.sh ; lunch aosp_car_x86-userdebug; make CarRotaryController RotaryPlayground android.car -j32
    ANDROID_OUT=$ANDROID_BUILD_TOP/out
    rm -r $TMP_OUTDIR
    mkdir -p $TMP_OUTDIR
    cp $ANDROID_PRODUCT_OUT/system/app/CarRotaryController/CarRotaryController.apk $TMP_OUTDIR
    cp $ANDROID_PRODUCT_OUT/system/app/RotaryPlayground/RotaryPlayground.apk $TMP_OUTDIR
    cp $ANDROID_OUT/target/common/obj/JAVA_LIBRARIES/android.car_intermediates/classes.jar $TMP_OUTDIR/android.car.jar
}

function install {
    echo
    echo "Installing the apks"
    adb install -g $TMP_OUTDIR/CarRotaryController.apk
    adb install -g $TMP_OUTDIR/RotaryPlayground.apk
}

function run {
    echo
    echo "Starting Rotary service and playground app"
    adb shell settings put secure enabled_accessibility_services com.android.car.rotary/com.android.car.rotary.RotaryService
    adb shell am start -n com.android.car.rotaryplayground/com.android.car.rotaryplayground.RotaryActivity
}

ACTION=$1

if [[ $ACTION == "b" ]]; then
    SECONDS=0
    build
    echo "Build time: $SECONDS sec."
    ACTION="i"
fi

if [[ $ACTION == "i" ]]; then
    install
    ACTION="r"
fi

if [[ $ACTION == "r" ]]; then
    run
    exit
fi

help
