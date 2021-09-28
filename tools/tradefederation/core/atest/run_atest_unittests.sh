#!/bin/bash

# Copyright (C) 2017 The Android Open Source Project
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

# A simple helper script that runs all of the atest unit tests.
# There are 2 situations that we take care of:
#   1. User wants to invoke this script directly.
#   2. PREUPLOAD hook invokes this script.

ATEST_DIR=$(dirname $0)
[ "$(uname -s)" == "Darwin" ] && { realpath(){ echo "$(cd $(dirname $1);pwd -P)/$(basename $1)"; }; }
ATEST_REAL_PATH=$(realpath $ATEST_DIR)
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color
COVERAGE=false

function get_pythonpath() {
    echo "$ATEST_REAL_PATH:$PYTHONPATH"
}

function print_summary() {
    local test_results=$1
    if [[ $COVERAGE == true ]]; then
        coverage report -m
        coverage html
    fi
    if [[ $test_results -eq 0 ]]; then
        echo -e "${GREEN}All unittests pass${NC}!"
    else
        echo -e "${RED}There was a unittest failure${NC}"
    fi
}

function run_atest_unittests() {
    echo "Running tests..."
    local run_cmd="python"
    local rc=0
    if [[ $COVERAGE == true ]]; then
        # Clear previously coverage data.
        python -m coverage erase
        # Collect coverage data.
        run_cmd="coverage run --source $ATEST_REAL_PATH --append"
    fi

    for test_file in $(find $ATEST_DIR -name "*_unittest.py"); do
        if ! PYTHONPATH=$(get_pythonpath) $run_cmd $test_file; then
          rc=1
          echo -e "${RED}$t failed${NC}"
        fi
    done
    echo
    print_summary $rc
    return $rc
}

# Let's check if anything is passed in, if not we assume the user is invoking
# script, but if we get a list of files, assume it's the PREUPLOAD hook.
read -ra PREUPLOAD_FILES <<< "$@"
if [[ ${#PREUPLOAD_FILES[@]} -eq 0 ]]; then
    run_atest_unittests; exit $?
elif [[ "${#PREUPLOAD_FILES[@]}" -eq 1 && "${PREUPLOAD_FILES}" == "coverage" ]]; then
    COVERAGE=true run_atest_unittests; exit $?
else
    for f in ${PREUPLOAD_FILES[@]}; do
        # We only want to run this unittest if atest files have been touched.
        if [[ $f == atest/* ]]; then
            run_atest_unittests; exit $?
        fi
    done
fi
