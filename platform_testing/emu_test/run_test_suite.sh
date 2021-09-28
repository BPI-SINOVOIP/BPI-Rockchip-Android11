#!/bin/bash
# It is to be used with BYOB setup to run CTS tests.
#
# It will return 0 if it is able to execute tests, otherwise
# it will return 1.
#
# Owner: mattwachakagrawal@google.com

# Echo vars to the log
set -x

DIST_DIR=$1
# Build ID is used for identifying the builds during upload
BUILD_ID=$2
# Module list file
MODULE_LIST_FILE=$3
# Emulator GPU option
GPU_FLAG=$4
# Directory containing the system image to run the tests against. Default is gphone_x86_64-user
PRODUCT_DIR=${5:-gphone_x86_64-user}

# Kill any emulators that might still be active from older sessions.
killall qemu-system-x86_64-headless

# Working files for the testing process
WORK_DIR="$DIST_DIR/tradefed-make"
TEST_DIR="$DIST_DIR/tradefed-test"
rm -rf $WORK_DIR
mkdir -p $WORK_DIR

function cleanup_dirs {
  echo "Cleanup prebuilts"
  rm -rf /buildbot/prebuilt/*
  rm -rf $WORK_DIR
  rm -rf $TEST_DIR/common
  # Extra files that may sometimes be of use, but in general seem
  # to create a lot of artifact clutter.
  find $TEST_DIR \( \
    -name 'adbkey*' -o \
    -name '*.cache' -o \
    -name '*.conf' -o \
    -name 'cts.dynamic' -o \
    -name '*.data' -o \
    -name '*.json' -o \
    -name '*.lock' -o \
    -name 'modem-nv-ram-*' -o \
    -name '*.pb' -o \
    -name '*.png' -o \
    -name '*.protobuf' -o \
    -name 'sepolicy' -o \
    -name '*.zip' \
    \) -print0 | xargs -0 -n 10 rm
}
# Always remove working files, even on error
trap cleanup_dirs EXIT
# Exit on errors.
set -e

function die {
  echo "run_test_suite.sh: $1">&2
  exit 1
}

function fetch_latest_emulator {
  local emu_dir=$1
  mkdir -p $emu_dir
  local fetch_stdout=$(fetch_artifacts.py \
    -build_target linux-sdk_tools_linux \
    -branch aosp-emu-master-dev \
    -image_path gs://android-build-emu/builds \
    -dest $emu_dir)
  # extract build_id from fetch fetch_artifacts.py stdout
  # stdout looks like:
  #   Fetching latest build 5800753 for linux-sdk_tools_linux
  echo $(echo $fetch_stdout | grep "Fetching latest build" | awk '{ print $4 }')
}

function find_zip_in_dir {
  local target_name=$1
  local zip_dir=$2
  [[ -d $zip_dir ]] || die "Could not find $target_name dir: $zip_dir"
  local zip_path=$zip_dir/$(ls $zip_dir)
  [[ -f $zip_path ]] || die "Could not find $target_name zip file: $zip_path"
  [[ "$zip_path" == *.zip ]] || die "Bad image $target_name zip pathname: $zip_path"
  echo $zip_path
}

# Check that we have the expected version of java.
EXPECTED_VERSION=9.0.4
export PATH=~/jdk-${EXPECTED_VERSION}/bin:$PATH
java --version | grep $EXPECTED_VERSION  # Fails if version string not found.

MODULE_LIST_PATH=$(dirname ${BASH_SOURCE[0]})/$MODULE_LIST_FILE
[[ -f $MODULE_LIST_PATH ]] || die "The module list path $MODULE_LIST_PATH was not found"

# Directory where tradefed-make tools are cloned
TRADEFED_MAKE_DIR="$WORK_DIR/tradefed-make"
git clone \
  https://team.googlesource.com/android-devtools-emulator/tradefed-make \
  $TRADEFED_MAKE_DIR

# The emulator requires files in the platforms directory
PLATFORMS_DIR="${HOME}/Android_sys_image/sdk/platforms/android-28"

# Platform tools contain core tools, like adb
PLATFORM_TOOLS_DIR="${HOME}/Android_sys_image/sdk/platform-tools"

# More tools dependencies.
SDK_TOOLS_DIR="${HOME}/Android_sys_image/sdk/build-tools/27.0.3"

# Where to put the testing configuration file
CONFIG_PATH="$WORK_DIR/tradefed-make-config.yaml"

# Fetch the latest emulator
EMU_DIR=$WORK_DIR/emulator
EMU_BUILD_ID=$(fetch_latest_emulator $EMU_DIR)
EMU_ZIP=$(find_zip_in_dir emulator $EMU_DIR)
ls -l $EMU_ZIP

# Directory where system images, and cts can be found
BUILD_DIR=out/prebuilt_cached/builds

IMAGE_DIR=$BUILD_DIR/$PRODUCT_DIR
IMAGE_ZIP=$(find_zip_in_dir image $IMAGE_DIR)
ls -l $IMAGE_ZIP

if [[ -f "$BUILD_DIR/test_suite/android-cts.zip" ]]; then
  TEST_SUITE=cts
  IMAGE_FLAVOR=user
elif [[ -f "$BUILD_DIR/test_suite/android-gts.zip" ]]; then
  TEST_SUITE=gts
  IMAGE_FLAVOR=user
elif [[ -f "$BUILD_DIR/test_suite/android-vts.zip" ]]; then
  TEST_SUITE=vts
  IMAGE_FLAVOR=userdebug
  export VTS_PYPI_PATH=$WORK_DIR/venv
  pip install --user virtualenv
  virtualenv $VTS_PYPI_PATH
  curl https://android.googlesource.com/platform/test/vts/+/master/script/pip_requirements.txt?format=TEXT | base64 -d > $WORK_DIR/pip_requirements.txt
  pip download -d $VTS_PYPI_PATH -r $WORK_DIR/pip_requirements.txt --no-binary protobuf,grpcio,matplotlib,numpy,Pillow,scipy==1.2.2
else
  die "Could not find android-cts.zip, android-gts.zip or android-vts.zip in $BUILD_DIR/test_suite"
fi


# Setup the testing configuration
$TRADEFED_MAKE_DIR/make-config \
  $TRADEFED_MAKE_DIR/config.yaml \
  $CONFIG_PATH \
  --override \
    config.tradefed.ape_api_key=/home/android-build/gts-android-emulator.json \
    vars.emulator.files.download.build_id=$EMU_BUILD_ID \
    vars.emulator.files.local_zip_path=$EMU_ZIP \
    vars.emulator.flags.feature=PlayStoreImage,GLAsyncSwap,GLESDynamicVersion \
    vars.emulator.flags.gpu=$GPU_FLAG \
    vars.image.files.local_zip_path.${IMAGE_FLAVOR}=$IMAGE_ZIP \
    vars.image.files.download.branch=git_rvc-release \
    vars.image.files.download.build_id=$BUILD_ID \
    vars.image.flavor.default=user \
    vars.root_dir=$TEST_DIR \
    vars.tradefed.timeout_seconds=10000 \
    vars.tools.android_level=28 \
    vars.tools.files.download.build_id=0 \
    vars.tools.files.local_dir.platforms=$PLATFORMS_DIR \
    vars.tools.files.local_dir.platform_tools=$PLATFORM_TOOLS_DIR \
    vars.tools.files.local_dir.sdk_tools=$SDK_TOOLS_DIR \
    vars.tradefed.files.download.build_id=$BUILD_ID \
    vars.tradefed.files.local_zip_path.$TEST_SUITE=$BUILD_DIR/test_suite/android-$TEST_SUITE.zip \
  --add \
    vars.emulator.flags.no-window=True \

if [[ "$GPU_FLAG" == "host" ]]; then
  $TRADEFED_MAKE_DIR/make-config \
  $CONFIG_PATH \
  --inline \
  --override \
    config.emulator.command_prefix='vglrun +v -c proxy' \

fi

# Start the tests
set +x
set -e
# Override the jdk's built-in certs to use the system ones.
export RDBG_FLAG=-Djavax.net.ssl.trustStore=/etc/ssl/certs/java/cacerts
export DISPLAY=:0
$TRADEFED_MAKE_DIR/tradefed-make $CONFIG_PATH -j4 prepare.$TEST_SUITE.all_modules
$TRADEFED_MAKE_DIR/tradefed-make $CONFIG_PATH -j4 $(cat $MODULE_LIST_PATH | sed 's/^/run./')
$TRADEFED_MAKE_DIR/tradefed-make $CONFIG_PATH stop.emulator.$TEST_SUITE

# TODO: Further analyze the results.

exit 0
