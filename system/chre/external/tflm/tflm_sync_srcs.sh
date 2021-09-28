#!/bin/bash
# This script syncs latest TFLM code to the `latest` folder.

DEST_PATH=`dirname "${BASH_SOURCE[0]}"`/latest

# Quit if a command fails.
set -e

# Option to remove DEST_PATH before syncing. This helps to identify files
# that are checked in but no longer needed by the nanoapp.
read -p "Do you want to remove destination $DEST_PATH before proceeding? y/n "
if [ $REPLY == "y" ]
then
  rm -rfv $DEST_PATH
fi

REAL_DEST_PATH=`realpath $DEST_PATH`

pushd /tmp

# Remove previous checkout if any
rm -rf tflm

# Check out tensorflow
git clone https://github.com/tensorflow/tensorflow.git --depth=1 tflm

# Generate chre related files
cd tflm
make -f tensorflow/lite/micro/tools/make/Makefile TARGET=chre generate_hello_world_make_project
rm -rf tensorflow/lite/micro/tools/make/gen/chre_x86_64/prj/hello_world/make/tensorflow/lite/micro/examples

# Remove the destination folder
rm -rf $REAL_DEST_PATH

# Copy files over
cp -r tensorflow/lite/micro/tools/make/gen/chre_x86_64/prj/hello_world/make $REAL_DEST_PATH

# Done
echo "TFLM code sync'ed"

popd