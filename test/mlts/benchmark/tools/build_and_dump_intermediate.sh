#!/bin/bash

# This script build and run DumpIntermediateTensors activity
# The results will be pulled to /tmp/intermediate by default.
# Usage
# ./test/mlts/benchmark/tools/build_and_dump_intermediate.sh -o /tmp -r intermediate_test -p -m fssd_100_8bit_gray_v1,fssd_100_8bit_v1,fssd_25_8bit_gray_v1,fssd_25_8bit_v1

if [[ -z "$ANDROID_BUILD_TOP" ]]; then
  echo ANDROID_BUILD_TOP not set, bailing out
  echo you must run lunch before running this script
  exit 1
fi

# Default output directory: /tmp/intermediate_currentdate
INTERMEDIATE_OUTPUT_DIR="/tmp"
CURRENTDATE=`date +"%m%d%y"`
RENAME="intermediate_$CURRENTDATE"
BUILD_MODE=true
RUN_PYTHON=false
MODEL_LIST=""

while getopts 'o:r:m:nph' flag; do
  case "${flag}" in
    o) INTERMEDIATE_OUTPUT_DIR="${OPTARG}" ;;
    r) RENAME="${OPTARG}" ;;
    m) MODEL_LIST="modelName ${OPTARG}" ;;
    n) BUILD_MODE=false ;;
    p) RUN_PYTHON=true ;;
    h)
      echo "Optional flags:"
      echo "  -h                  Display this help message."
      echo "  -o <output_dir>     Set destination directory for the output folder."
      echo "  -r <output_name>    Name of the output folder."
      echo "  -m <model_list>     A list of target model names separated by comma(,) e.g. asr_float,tts_float."
      echo "  -n                  If set, skipping build and installation to save time."
      echo "  -p                  If set, run Python script to generate visualization html."
      exit 0
      ;;
    *)
      error "Unexpected option ${flag}, please run with -h to see the options"
      exit 1
      ;;
  esac
done

cd $ANDROID_BUILD_TOP

if [[ "$BUILD_MODE" == true ]]; then
  # Build and install benchmark app
  build/soong/soong_ui.bash --make-mode NeuralNetworksApiBenchmark
  if ! adb install -r $OUT/testcases/NeuralNetworksApiBenchmark/arm64/NeuralNetworksApiBenchmark.apk; then
    adb uninstall com.android.nn.benchmark.app
    adb install -r $OUT/testcases/NeuralNetworksApiBenchmark/arm64/NeuralNetworksApiBenchmark.apk
  fi
fi

# Default to run all public models in DumpIntermediateTensors
adb shell am start -n com.android.nn.benchmark.app/com.android.nn.benchmark.util.DumpIntermediateTensors \
--es "$MODEL_LIST" inputAssetIndex 0 &&
# Wait for the files to finish writing.
# TODO(veralin): find a better way to wait, maybe some sort of callback
sleep 13 &&

mkdir -p $INTERMEDIATE_OUTPUT_DIR &&
cd $INTERMEDIATE_OUTPUT_DIR &&
rm -rf intermediate &&
adb pull /data/data/com.android.nn.benchmark.app/files/intermediate/ &&
rsync -a --delete intermediate/ $RENAME/ &&
echo "Results pulled to $INTERMEDIATE_OUTPUT_DIR/$RENAME"

if [[ "$RUN_PYTHON" == true ]]; then
  cd $ANDROID_BUILD_TOP &&
  python test/mlts/benchmark/tools/tensor_utils.py $ANDROID_BUILD_TOP $INTERMEDIATE_OUTPUT_DIR/$RENAME
fi

exit