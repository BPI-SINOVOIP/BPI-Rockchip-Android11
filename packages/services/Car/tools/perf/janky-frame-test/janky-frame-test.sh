#!/bin/bash
#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Analyze dumpsys output and show the janky frame count.

readonly NON_MAP_ACTIVITY=com.android.car.carlauncher/.AppGridActivity
readonly MAP_PACKAGE=com.google.android.apps.maps
readonly MAP_ACTIVITY=${MAP_PACKAGE}/com.google.android.maps.MapsActivity
readonly CAR_ACTIVITY=com.android.car.carlauncher/.CarLauncher

####################################################
# Analyze dumpsys and returns total/janky frame counts.
# Globals:
#   None
# Arguments:
#   PID of map activity of interest
# Returns:
#   Total and janky frame count in space-saparated string
####################################################
function retrieve_frame() {
  local start_pattern="Graphics info for pid ([0-9]+)"
  local start_pattern_found=false
  local total_frame_pattern="Total frames rendered: ([0-9]+)"
  local janky_frame_pattern="Janky frames: ([0-9]+)"
  local total_frame=0
  local janky_frame=0

  while IFS= read -r line; do
    # Maps runs under two different users (u0 and u10).
    # To get information about the map under the user of interest,
    # the PID is extracted and compared.
    if [[ ${line} =~ ${start_pattern} ]]; then
      if [ ${BASH_REMATCH[1]} = "$1" ]; then
        start_pattern_found=true
      else
        start_pattern_found=false
      fi
    fi
    if [[ ${start_pattern_found} = "true" ]]; then
      if [[ ${line} =~ ${total_frame_pattern} ]]; then
        total_frame=${BASH_REMATCH[1]}
      fi
      if [[ ${line} =~ ${janky_frame_pattern} ]]; then
        janky_frame=${BASH_REMATCH[1]}
      fi
    fi
  done < <(adb shell dumpsys gfxinfo ${MAP_PACKAGE})

  echo "${total_frame} ${janky_frame}"
}

echo "Testing...."

# Launch full-screen map.
# Starting full-screen map directly doesn't work due to some reasons.
# We need to kill the map when no map is shown and re-start map activity.
adb shell am start -n ${NON_MAP_ACTIVITY} &> /dev/null
# Wait until non-map activity is fully started.
sleep 2
adb shell am force-stop ${MAP_PACKAGE} &> /dev/null
adb shell am start -n ${MAP_ACTIVITY} &> /dev/null
# Wait until map activity is fully started.
sleep 7

# Get PID of map under user 10.
readonly PS_INFO=($(adb shell ps -ef | fgrep ${MAP_PACKAGE} | fgrep u10))
readonly MAP_PID=${PS_INFO[1]}

if [[ -z ${MAP_PID} ]]; then
  echo "Map is not found. Test terminates."
  exit 1
fi

RET_VAL=($(retrieve_frame ${MAP_PID}))
readonly OLD_TOTAL_FRAME=${RET_VAL[0]}
readonly OLD_JANKY_FRAME=${RET_VAL[1]}

# Get screen size.
readonly SIZE_PATTERN="Physical size: ([0-9]+)x([0-9]+)"
readonly WM_SIZE=$(adb shell wm size)
if [[ ${WM_SIZE} =~ ${SIZE_PATTERN} ]]; then
  SCREEN_WIDTH=${BASH_REMATCH[1]}
  SCREEN_HEIGHT=${BASH_REMATCH[2]}
else
  echo "Test terminates due to failing to get screen size."
  exit 1
fi

readonly LEFT_POS=$(awk -v width=${SCREEN_WIDTH} 'BEGIN {printf "%d", width * 0.2}')
readonly RIGHT_POS=$(awk -v width=${SCREEN_WIDTH} 'BEGIN {printf "%d", width * 0.8}')
readonly VERTICAL_MID_POS=$(awk -v height=${SCREEN_HEIGHT} 'BEGIN {printf "%d", height * 0.5}')
readonly SWIPE_DURATION=100

# Send input signal to scroll map.
COUNTER=0
while [[ $COUNTER -lt 10 ]]; do
  adb shell input swipe ${LEFT_POS} ${VERTICAL_MID_POS} ${RIGHT_POS} ${VERTICAL_MID_POS} ${SWIPE_DURATION}
  sleep 0.5
  ((COUNTER++))
done

COUNTER=0
while [[ $COUNTER -lt 10 ]]; do
  adb shell input swipe ${RIGHT_POS} ${VERTICAL_MID_POS} ${LEFT_POS} ${VERTICAL_MID_POS} ${SWIPE_DURATION}
  sleep 0.5
  ((COUNTER++))
done

# Make sure that map drawing is finished.
sleep 3

RET_VAL=($(retrieve_frame ${MAP_PID}))
readonly CUR_TOTAL_FRAME=${RET_VAL[0]}
readonly CUR_JANKY_FRAME=${RET_VAL[1]}

if [[ ${CUR_TOTAL_FRAME} = ${OLD_TOTAL_FRAME} ]]; then
  echo "Map has not been updated. Test failed."
  exit 1
fi

readonly TOTAL_COUNT=$((CUR_TOTAL_FRAME - OLD_TOTAL_FRAME))
readonly JANKY_COUNT=$((CUR_JANKY_FRAME - OLD_JANKY_FRAME))

echo "Janky frame count: ${JANKY_COUNT} out of ${TOTAL_COUNT}"

# Let car launcher take the screen.
adb shell am start -n ${CAR_ACTIVITY} &> /dev/null
