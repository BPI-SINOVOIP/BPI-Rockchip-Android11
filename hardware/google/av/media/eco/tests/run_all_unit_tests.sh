#!/bin/bash
if [ -z "$ANDROID_BUILD_TOP" ]; then
    echo "Android build environment not set"
    exit -1
fi

echo "waiting for device"
adb root && adb wait-for-device remount && adb sync

adb shell /data/nativetest/EcoDataTest/EcoDataTest
adb shell /data/nativetest/EcoSessionTest/EcoSessionTest
#ECOService test lives in vendor side.
adb shell data/nativetest/vendor/EcoServiceTest/EcoServiceTest
