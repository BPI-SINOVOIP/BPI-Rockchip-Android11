#!/bin/bash
#
# Build benchmark app and run it, mimicking a user-initiated run
#
# Output is logged to a temporary folder and summarized in txt and JSON formats.

MODE="${1:-scoring}"

case "$MODE" in
  scoring)
    CLASS=com.android.nn.benchmark.app.NNScoringTest
    ;;
  inference-stress)
    CLASS=com.android.nn.benchmark.app.NNInferenceStressTest
    ;;
  model-loading-stress)
    CLASS=com.android.nn.benchmark.app.NNModelLoadingStressTest
    ;;
  *)
    echo "Unknown execution mode: $1"
    echo "Known modes: scoring (default), inference-stress, model-loading-stress"
    exit 1
    ;;
esac

if [[ -z "$ANDROID_BUILD_TOP" ]]; then
  echo ANDROID_BUILD_TOP not set, bailing out
  echo you must run lunch before running this script
  exit 1
fi

set -e
cd $ANDROID_BUILD_TOP

# Build and install benchmark app
build/soong/soong_ui.bash --make-mode NeuralNetworksApiBenchmark
if ! adb install -r $OUT/testcases/NeuralNetworksApiBenchmark/arm64/NeuralNetworksApiBenchmark.apk; then
  adb uninstall com.android.nn.benchmark.app
  adb install -r $OUT/testcases/NeuralNetworksApiBenchmark/arm64/NeuralNetworksApiBenchmark.apk
fi


# Should we figure out if we run on release device
if [ -z "$MLTS_RELEASE_DEVICE" ]; then
  BUILD_DESCRIPTION=`adb shell getprop ro.build.description`
  if [[ $BUILD_DESCRIPTION =~ .*release.* ]]
  then
    MLTS_RELEASE_DEVICE=True
  else
    MLTS_RELEASE_DEVICE=False
  fi
fi

# Pass --no-isolated-storage to am instrument?
BUILD_VERSION_RELEASE=`adb shell getprop ro.build.version.release`
AM_INSTRUMENT_FLAGS=""
if [[ $BUILD_VERSION_RELEASE == "Q" ]]; then
  AM_INSTRUMENT_FLAGS+="--no-isolated-storage"
fi

if [[ "$MLTS_RELEASE_DEVICE" == "True" ]]; then
  TEST_EXTENRAL_STORAGE="com.android.nn.benchmark.app/com.android.nn.benchmark.util.TestExternalStorageActivity"
  while ! adb shell "am start -W $TEST_EXTENRAL_STORAGE && rm /sdcard/mlts_write_external_storage" > /dev/null 2>&1; do
     echo "************************************************************"
     echo "Grant External storage write permissions to MLTS to proceed!"
     echo "************************************************************"
     read -n 1 -r -p "Continue? (press any key)"
     echo
  done
else
  adb root
  adb shell "pm grant com.android.nn.benchmark.app android.permission.WRITE_EXTERNAL_STORAGE"
  # Skip setup wizard and remount (read-write)
  if ! adb shell test -f /data/local.prop; then
    adb shell 'echo ro.setupwizard.mode=DISABLED > /data/local.prop'
    adb shell 'chmod 644 /data/local.prop'
    adb shell 'settings put global device_provisioned 1*'
    adb shell 'settings put secure user_setup_complete 1'
    adb disable-verity
    adb reboot
    sleep 5
    adb wait-for-usb-device root
    adb wait-for-usb-device remount
    sleep 5
  fi
  set +e
  # Enable menu key press through adb
  adb shell 'echo testing > /data/local/enable_menu_key'
  # Leave screen on (affects scheduling)
  adb shell settings put system screen_off_timeout 86400000
  # Stop background apps, seem to take ~10% CPU otherwise
  adb shell 'pm disable com.google.android.googlequicksearchbox'
  adb shell 'pm list packages -f' | sed -e 's/.*=//' | sed 's/\r//g' | grep "com.breel.wallpapers" | while read pkg; do adb shell "pm disable $pkg"; done;
  set -e
fi

adb shell setprop debug.nn.cpuonly 0
adb shell setprop debug.nn.vlog 0

# Menukey - make sure screen is on
adb shell "input keyevent 82"
# Show homescreen
adb shell wm dismiss-keyguard

if [[ "$MODE" == "scoring" ]]; then
  LOGDIR=$(mktemp -d)/mlts-logs
  HOST_CSV=$LOGDIR/benchmark.csv
  RESULT_HTML=$LOGDIR/result.html
  DEVICE_CSV=/sdcard/mlts_benchmark.csv

  mkdir -p $LOGDIR
  echo Creating logs in $LOGDIR

  # Remove old benchmark csv data
  adb shell rm -f ${DEVICE_CSV}
fi

# Set the shell pid as a top-app and run tests
time adb shell "echo $$ > /dev/stune/top-app/tasks; am instrument ${AM_INSTRUMENT_FLAGS} -w -e class $CLASS com.android.nn.benchmark.app/androidx.test.runner.AndroidJUnitRunner"

if [[ "$MODE" == "scoring" ]]; then
  adb pull $DEVICE_CSV $HOST_CSV
  echo Benchmark data saved in $HOST_CSV

  $ANDROID_BUILD_TOP/test/mlts/benchmark/results/generate_result.py $HOST_CSV $RESULT_HTML
  echo Results stored  in $RESULT_HTML
  xdg-open $RESULT_HTML
fi
