#!/bin/bash
if [ -z "$ANDROID_PRODUCT_OUT" ]; then
    echo "ANDROID_PRODUCT_OUT not set"
    exit
fi
DISP_ID=1
if [[ $# -eq 1 ]]; then
  echo "$1"
  DISP_ID=$1
fi
echo "Use display:$DISP_ID"

adb root
# Check always crashing one
echo "Start AlwaysCrashingActivity in fixed mode"
adb shell dumpsys car_service start-fixed-activity-mode $DISP_ID com.google.android.car.kitchensink com.google.android.car.kitchensink.AlwaysCrashingActivity
sleep 1
read -p "AlwaysCrashingAvtivity should not be tried any more. Press Enter"
# package update
echo "Will try package update:"
adb install -r -g $ANDROID_PRODUCT_OUT/system/priv-app/EmbeddedKitchenSinkApp/EmbeddedKitchenSinkApp.apk
read -p "AlwaysCrashingAvtivity should have been retried. Press Enter"
# suspend-resume
echo "Check retry for suspend - resume"
adb shell setprop android.car.garagemodeduration 1
adb shell dumpsys car_service suspend
adb shell dumpsys car_service resume
read -p "AlwaysCrashingAvtivity should have been retried. Press Enter"
# starting other Activity
echo "Switch to no crash Activity"
adb shell dumpsys car_service start-fixed-activity-mode $DISP_ID com.google.android.car.kitchensink com.google.android.car.kitchensink.NoCrashActivity
read -p "NoCrashAvtivity should have been shown. Press Enter"
# stating other non-fixed Activity
adb shell am start-activity --display $DISP_ID -n com.google.android.car.kitchensink/.EmptyActivity
read -p "NoCrashAvtivity should be shown after showing EmptyActivity. Press Enter"
# package update
echo "Will try package update:"
adb install -r -g $ANDROID_PRODUCT_OUT/system/priv-app/EmbeddedKitchenSinkApp/EmbeddedKitchenSinkApp.apk
read -p "NoCrashActivity should be shown. Press Enter"
# stop the mode
echo "Stop fixed activity mode"
adb shell dumpsys car_service stop-fixed-activity-mode $DISP_ID
adb shell am start-activity --display $DISP_ID -n com.google.android.car.kitchensink/.EmptyActivity
read -p "EmptyActivity should be shown. Press Enter"
