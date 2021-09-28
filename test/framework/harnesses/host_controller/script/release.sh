#!/bin/bash
# VTS lab release script
# Usage: <this script> prod|test
# Caution: only used by a VTS lab release engineer
rm out/host/linux-x86/vtslab/android-vtslab/testcases/tmp/ -rf
. build/envsetup.sh
lunch aosp_arm64
make WifiUtil -j
mkdir -p out/host/linux-x86/vtslab/android-vtslab/testcases/DATA/app
cp out/target/product/generic_arm64/testcases/WifiUtil/ out/host/linux-x86/vtslab/android-vtslab/testcases/DATA/app/ -rf
make vtslab -j

if [ $# -eq 1 ]; then
  ls -al out/host/linux-x86/vtslab/android-vtslab.zip
  echo "To upload, please run:"
  echo gsutil cp out/host/linux-x86/vtslab/android-vtslab.zip gs://vtslab-release/$1/android-vtslab-$(date +%F).zip
fi
