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
# Analyze the output from Android Auto app launch test application.

# ITERATION_COUNT decides how many times the test is repeated.
readonly ITERATION_COUNT=2
# SCALE decides the precision of fractional part in bc.
readonly SCALE=9

####################################################
# Calculate arithmetic mean and standard deviation.
# Globals:
#   None
# Arguments:
#   A sequence of numbers
# Returns:
#   Count, average and stddev in space-saparated string
####################################################
function calc_stat() {
  local sum=0
  local count=0
  local mean=0
  local diff_sqrd_sum=0
  local stddev=0

  # Calculate the mean.
  for val in "$@"; do
    sum=$(echo "scale=${SCALE};${sum} + ${val}" | bc)
    ((count++))
  done
  mean=$(echo "scale=${SCALE};${sum} / ${count}" | bc)

  # Calculate standard deviation.
  for val in "$@"; do
    diff_sqrd_sum=$(echo "scale=${SCALE};${diff_sqrd_sum} + (${mean} - ${val}) ^ 2" | bc)
  done
  stddev=$(echo "scale=${SCALE};sqrt(${diff_sqrd_sum} / ${count})" | bc)

  echo "${count} ${mean} ${stddev}"
}

####################################################
# Execute app launch performance test.
# Globals:
#   None
# Arguments:
#   Type of app launch, cold or hot
#   Whether to press home button, boolean
#   Whether to drop cache, boolean
#   Whether to kill app, boolean
# Outputs:
#   Writes the analysis result to stdout
####################################################
function run_app_launch_test() {
  local stat=
  local package_pattern="INSTRUMENTATION_STATUS: $1_startup_([a-z]+(\.[a-z]+)+)=([0-9]+)"
  local cmd="adb shell am instrument -w -r -e iterations ${ITERATION_COUNT}\
    -e listener android.device.collectors.AppStartupListener\
    -e class 'android.platform.test.scenario.dial.OpenAppMicrobenchmark,android.platform.test.scenario.googleplay.OpenAppMicrobenchmark,android.platform.test.scenario.maps.OpenAppMicrobenchmark,android.platform.test.scenario.radio.OpenAppMicrobenchmark,android.platform.test.scenario.settings.OpenAppMicrobenchmark'\
    -e favor-shell-commands true -e log false -e suite-timeout_msec 36000000\
    -e durationMs 30000 -e press-home $2 -e newRunListenerMode true\
    -e timeout_msec 300000 -e drop-cache $3 -e kill-app $4 android.platform.test.scenario/android.support.test.runner.AndroidJUnitRunner"
  # Example output from ${cmd}
  #
  #   INSTRUMENTATION_STATUS: class=android.platform.test.scenario.maps.OpenAppMicrobenchmark
  #   INSTRUMENTATION_STATUS: current=1
  #   INSTRUMENTATION_STATUS: id=AndroidJUnitRunner
  #   INSTRUMENTATION_STATUS: numtests=25
  #   INSTRUMENTATION_STATUS: stream=
  #   android.platform.test.scenario.maps.OpenAppMicrobenchmark:
  #   INSTRUMENTATION_STATUS: test=testOpen
  #   INSTRUMENTATION_STATUS_CODE: 1
  #   INSTRUMENTATION_STATUS: cold_startup_com.google.android.apps.maps=2286
  #   INSTRUMENTATION_STATUS: cold_startup_count_com.google.android.apps.maps=1
  #   INSTRUMENTATION_STATUS: cold_startup_total_count=1
  #   INSTRUMENTATION_STATUS_CODE: 2
  #   INSTRUMENTATION_STATUS: class=android.platform.test.scenario.maps.OpenAppMicrobenchmark
  #   INSTRUMENTATION_STATUS: current=1
  #   INSTRUMENTATION_STATUS: id=AndroidJUnitRunner
  #   INSTRUMENTATION_STATUS: numtests=25
  #   INSTRUMENTATION_STATUS: stream=.
  #   INSTRUMENTATION_STATUS: test=testOpen
  #   INSTRUMENTATION_STATUS_CODE: 0

  declare -A app_launch_map
  printf "Testing $1 start performance....\n"

  while IFS= read -r line; do
    if [[ ${line} =~ ${package_pattern} ]]; then
      APP_PACKAGE="${BASH_REMATCH[1]}"
      LAUNCH_TIME=${BASH_REMATCH[3]}
      app_launch_map[${APP_PACKAGE}]+="${LAUNCH_TIME} "
    fi
  done < <(${cmd})

  for key in "${!app_launch_map[@]}"; do
    stat=($(calc_stat ${app_launch_map[${key}]}))
    printf "[${key}]\n"
    printf "  Count: ${stat[0]}\n"
    printf "  Average: ${stat[1]}\n"
    printf "  StdDev: ${stat[2]}\n"
    printf "\n"
  done
}

# We test two types of app launch performance: cold and hot.
# Cold start is to launch an app without cache support as if it is executed for the first time.
# Hot start is to make an app go into the foreground while the app is running in the background.
run_app_launch_test cold false true true
echo ""
run_app_launch_test hot true false false
