#!/usr/bin/env bash
# Copyright 2019, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color
[ "$(uname -s)" == "Darwin" ] && { realpath(){ echo "$(cd $(dirname $1);pwd -P)/$(basename $1)"; }; }
AIDEGEN_DIR=$(dirname $(realpath $0))
ASUITE_DIR="$(dirname $AIDEGEN_DIR)"
CORE_DIR="$(dirname $ASUITE_DIR)/tradefederation/core"
ATEST_DIR="$CORE_DIR/atest"
RC_FILE=${AIDEGEN_DIR}/.coveragerc
MOD_COVERAGE='coverage:import coverage'
MOD_PROTOBUF='protobuf:from google import protobuf'

function get_python_path() {
    echo "$PYTHONPATH:$CORE_DIR:$ATEST_DIR:$ASUITE_DIR"
}

function print_summary() {
    local test_results=$1
    local tmp_dir=$(mktemp -d)
    python3 -m coverage report
    python3 -m coverage html -d $tmp_dir --rcfile=$RC_FILE
    echo "Coverage report available at file://${tmp_dir}/index.html"

    if [[ $test_results -eq 0 ]]; then
        echo -e "${GREEN}All unittests pass${NC}!"
    else
        echo -e "${RED}Unittest failure found${NC}!"
        exit 1
    fi
}

function run_unittests() {
    local specified_tests=$@
    local rc=0

    # Get all unit tests under tools/acloud.
    local all_tests=$(find $AIDEGEN_DIR -type f -name "*_unittest.py");
    local tests_to_run=$all_tests

    python3 -m coverage erase
    for t in $tests_to_run; do
        echo "Testing" $t
        if ! PYTHONPATH=$(get_python_path) python3 -m coverage run --append --rcfile=$RC_FILE $t; then
            rc=1
            echo -e "${RED}$t failed${NC}"
        fi
    done

    print_summary $rc
    cleanup
}

function install_module() {
    local FLAG="--user"
    local module=$(echo $1| awk -F: '{print $1}')
    local action=$(echo $1| awk -F: '{print $2}')
    if ! python3 -c "$action" 2>/dev/null; then
        echo -en "Module $RED$module$NC is missing. Start to install..."
        cmd="python3 -m pip install $module $FLAG"
        eval "$cmd" >/dev/null || { echo Failed to install $module; exit 1; }
        echo " Done."
    fi
}

function check_env() {
    if [ -z "$ANDROID_BUILD_TOP" ]; then
        echo "Missing ANDROID_BUILD_TOP env variable. Run 'lunch' first."
        exit 1
    fi
    # Append more necessary modules if needed.
    for mod in "$MOD_COVERAGE" "$MOD_PROTOBUF"; do
        install_module "$mod"
    done
}

function cleanup() {
    # Search for *.pyc and delete them.
    find $AIDEGEN_DIR -name "*.pyc" -exec rm -f {} \;
}

check_env
cleanup
run_unittests "$@"
