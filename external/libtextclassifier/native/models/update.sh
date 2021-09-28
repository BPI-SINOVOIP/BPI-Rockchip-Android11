#!/bin/bash
# Updates the set of model with the most recent ones.

set -e

ANNOTATOR_BASE_URL=https://www.gstatic.com/android/text_classifier/q/live
ACTIONS_BASE_URL=https://www.gstatic.com/android/text_classifier/actions/q/live
LANGID_BASE_URL=https://www.gstatic.com/android/text_classifier/langid/q/live

download() {
	echo "$1/FILELIST"
	for f in $(wget -O- "$1/FILELIST"); do
		destination="$(basename -- $f)"
		wget "$1/$f" -O "$destination"
	done
}

cd "$(dirname "$0")"

download $ANNOTATOR_BASE_URL
download $ACTIONS_BASE_URL
download $LANGID_BASE_URL

echo "You may want to edit the file name of downloaded files, see external/libtextclassifier/Android.bp"
