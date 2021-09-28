#!/bin/bash

# This script generate visualizations and metadata json files of the tflite models
# Results are stored in /tmp by default
# Prerequisites:
# Follow the link to run the visualize.py
# https://www.tensorflow.org/lite/guide/faq#how_do_i_inspect_a_tflite_file

if [[ -z "$ANDROID_BUILD_TOP" ]]; then
  echo ANDROID_BUILD_TOP not set, bailing out
  echo you must run lunch before running this script
  exit 1
fi

echo "Follow the link to set up the prerequisites for running visualize.py: \
https://www.tensorflow.org/lite/guide/faq#how_do_i_inspect_a_tflite_file"
read -p "Are you able to run the visualize.py script with bazel? [Y/N]" -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
  MODEL_DIR="$ANDROID_BUILD_TOP/test/mlts/models/assets"
  # The .json files are always output to /tmp by the tflite visualize tool
  HTML_DIR="${1:-/tmp}"
  mkdir -p $HTML_DIR

  set -e
  for file in "$MODEL_DIR"/*.tflite
  do
    if [ -f "$file" ]; then
      filename=`basename $file`
      modelname=${filename%.*}
      bazel run //tensorflow/lite/tools:visualize $file $HTML_DIR/$modelname.html
    fi
  done
else
  echo "Please set up first following https://www.tensorflow.org/lite/guide/faq#how_do_i_inspect_a_tflite_file."
fi

exit