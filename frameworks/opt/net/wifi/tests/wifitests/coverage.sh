#!/bin/sh

# A shell script to generate a coverage report for opt/net/wifi

if [[ ! ($# == 1) ]]; then
  echo "$0: usage: coverage.sh OUTPUT_DIR"
  exit 1
fi

if [ -z $ANDROID_BUILD_TOP ]; then
  echo "You need to source and lunch before you can use this script"
  exit 1
fi

# Make the output directory and get its full name
OUTPUT_DIR="$1"
mkdir -p $OUTPUT_DIR || exit 1
OUTPUT_DIR="`(cd $OUTPUT_DIR && pwd)`"
BUILD_OUT_DIR=$OUTPUT_DIR/out
echo "Output dir: $OUTPUT_DIR"

cd "$(dirname $0)" #cd to directory containing this script

echo "Checking jacoco patterns"
class_patterns_from_filenames () {
  sed -e 's/[.]java$//' -e 's@/@.@g' |
    while read x; do
      printf '            "%s",\n' "$x"
      printf '            "%s$*",\n' "$x"
      printf '            "%s.**",\n' "$x"
    done
}

generate_new_bp () (
  sed -n -e p -e '/include_filter:/q' < Android.bp
  (cd ../../service/java && find * -name \*.java) |
    LC_ALL=C sort |
    class_patterns_from_filenames
  tail -n 3 Android.bp
)

generate_new_bp > $OUTPUT_DIR/bp

diff -u Android.bp $OUTPUT_DIR/bp || {
  mv $OUTPUT_DIR/bp Android.bp
  echo "Android.bp has been updated. Please review and check in the new version"
  exit 1
}
rm -f $OUTPUT_DIR/bp

REMOTE_COVERAGE_OUTPUT_FILE=/data/data/com.android.server.wifi.test/files/coverage.ec
COVERAGE_OUTPUT_FILE=$OUTPUT_DIR/wifi_coverage.ec

# Note - the $VARs in the following are expanded by the here-file redirection!
echo "Building for coverage report"
bash <<END_OF_BUILD_SCRIPT || { exit 1; }
  cd $ANDROID_BUILD_TOP
  source build/make/envsetup.sh
  lunch ${TARGET_PRODUCT}-${TARGET_BUILD_VARIANT}
  export OUT_DIR=${BUILD_OUT_DIR}
  export EMMA_INSTRUMENT=true
  export EMMA_INSTRUMENT_FRAMEWORK=false
  export EMMA_INSTRUMENT_STATIC=true
  export ANDROID_COMPILE_WITH_JACK=false
  export SKIP_BOOT_JARS_CHECK=true
  m FrameworksWifiTests jacoco-cli
END_OF_BUILD_SCRIPT

APK_NAME="$(find $BUILD_OUT_DIR/target -name FrameworksWifiTests.apk)"
REPORTER_JAR="$(find $BUILD_OUT_DIR/host -name jacoco-cli.jar)"

echo "Running tests and generating coverage report"

set -e # fail early
set -x # print commands
test -f "$APK_NAME"
test -f "$REPORTER_JAR"

adb root
adb wait-for-device

adb shell rm -f $REMOTE_COVERAGE_OUTPUT_FILE

adb install -r -g "$APK_NAME"

adb shell am instrument -e coverage true --no-hidden-api-checks \
  -w 'com.android.server.wifi.test/com.android.server.wifi.CustomTestRunner'

adb pull $REMOTE_COVERAGE_OUTPUT_FILE $COVERAGE_OUTPUT_FILE

java -jar $REPORTER_JAR \
  report \
  --html $OUTPUT_DIR \
  --classfiles $BUILD_OUT_DIR/target/common/obj/APPS/FrameworksWifiTests_intermediates/jacoco-report-classes.jar \
  --sourcefiles $ANDROID_BUILD_TOP/frameworks/opt/net/wifi/service/java \
  --name wifi-coverage \
  $COVERAGE_OUTPUT_FILE
set +x

echo Created report at file://$OUTPUT_DIR/index.html

