# Copyright 2018, The Android Open Source Project
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

ATEST_REL_DIR="tools/asuite/atest"

_atest_completion_ready() {
    # Not support completion on systems of which the out-of-box bash version
    # is too old to fully support required built-in commands/functions.
    # For MacOS users, atest tab completion can be enabled via:
    #    brew install bash bash-completion@2
    # and configure code below in ~/.bashrc
    #    completion_file="/usr/local/etc/profile.d/bash_completion.sh"
    #    [[ -r "$completion_file" ]] && source "$completion_file"
    # Open a new terminal, source/lunch and try again.
    reqs=(compopt _get_comp_words_by_ref __ltrim_colon_completions)
    for _cmd in "${reqs[@]}"; do
        if ! type "$_cmd" >/dev/null 2>&1; then
            return 1
        fi
    done
}

_fetch_testable_modules() {
    [[ -z $ANDROID_BUILD_TOP ]] && return 0
    export ATEST_DIR="$ANDROID_BUILD_TOP/$ATEST_REL_DIR"
    $PYTHON - << END
import os
import pickle
import sys

sys.path.append(os.getenv('ATEST_DIR'))
import constants

if os.path.isfile(constants.MODULE_INDEX):
    with open(constants.MODULE_INDEX, 'rb') as cache:
        try:
            print("\n".join(pickle.load(cache, encoding="utf-8")))
        except:
            print("\n".join(pickle.load(cache)))
else:
    print("")
END
    unset ATEST_DIR
}

# This function invoke get_args() and return each item
# of the list for tab completion candidates.
_fetch_atest_args() {
    [[ -z $ANDROID_BUILD_TOP ]] && return 0
    export ATEST_DIR="$ANDROID_BUILD_TOP/$ATEST_REL_DIR"
    $PYTHON - << END
import os
import sys

atest_dir = os.path.join(os.getenv('ATEST_DIR'))
sys.path.append(atest_dir)

import atest_arg_parser

parser = atest_arg_parser.AtestArgParser()
parser.add_atest_args()
print("\n".join(parser.get_args()))
END
    unset ATEST_DIR
}

# This function returns devices recognised by adb.
_fetch_adb_devices() {
    while read dev; do echo $dev | awk '{print $1}'; done < <(adb devices | egrep -v "^List|^$"||true)
}

# This function returns all paths contain TEST_MAPPING.
_fetch_test_mapping_files() {
    [[ -z $ANDROID_BUILD_TOP ]] && return 0
    find -maxdepth 5 -type f -name TEST_MAPPING |sed 's/^.\///g'| xargs dirname 2>/dev/null
}

# The main tab completion function.
_atest() {
    local cur prev
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    _get_comp_words_by_ref -n : cur prev || true

    case "$cur" in
        -*)
            COMPREPLY=($(compgen -W "$(_fetch_atest_args)" -- $cur))
            ;;
        */*)
            ;;
        *)
            local candidate_args=$(ls; _fetch_testable_modules)
            COMPREPLY=($(compgen -W "$candidate_args" -- $cur))
            ;;
    esac

    case "$prev" in
        --iterations|--retry-any-failure|--rerun-until-failure)
            COMPREPLY=(10) ;;
        --list-modules|-L)
            # TODO: genetate the list automately when the API is available.
            COMPREPLY=($(compgen -W "cts vts" -- $cur)) ;;
        --serial|-s)
            local adb_devices="$(_fetch_adb_devices)"
            if [ -n "$adb_devices" ]; then
                COMPREPLY=($(compgen -W "$(_fetch_adb_devices)" -- $cur))
            else
                # Don't complete files/dirs when there'is no devices.
                compopt -o nospace
                COMPREPLY=("")
            fi ;;
        --test-mapping|-p)
            local mapping_files="$(_fetch_test_mapping_files)"
            if [ -n "$mapping_files" ]; then
                COMPREPLY=($(compgen -W "$mapping_files" -- $cur))
            else
                # Don't complete files/dirs when TEST_MAPPING wasn't found.
                compopt -o nospace
                COMPREPLY=("")
            fi ;;
    esac
    __ltrim_colon_completions "$cur" "$prev" || true
    return 0
}

function _atest_main() {
    # Only use this in interactive mode.
    # Warning: below check must be "return", not "exit". "exit" won't break the
    # build in interactive shell(e.g VM), but will result in build breakage in
    # non-interactive shell(e.g docker container); therefore, using "return"
    # adapts both conditions.
    [[ ! $- =~ 'i' ]] && return 0

    # If required functions are not available, exit gracefully.
    if ! _atest_completion_ready; then
        return 0
    fi

    # Use Py3 as the default interpreter. This script is aiming for being
    # compatible with both Py2 and Py3.
    if [ -x "$(which python3)" ]; then
        PYTHON=$(which python3)
    elif [ -x "$(which python2)" ]; then
        PYTHON=$(which python2)
    else
        PYTHON="/usr/bin/env python3"
    fi

    # Complete file/dir name first by using option "nosort".
    # BASH version <= 4.3 doesn't have nosort option.
    # Note that nosort has no effect for zsh.
    local _atest_comp_options="-o default -o nosort"
    local _atest_executables=(atest atest-dev atest-src atest-py3)
    for exec in "${_atest_executables[*]}"; do
        complete -F _atest $_atest_comp_options $exec 2>/dev/null || \
        complete -F _atest -o default $exec
    done

    # Install atest-src for the convenience of debugging.
    local atest_src="$(gettop)/$ATEST_REL_DIR/atest.py"
    [[ -f "$atest_src" ]] && alias atest-src="$atest_src"
}

_atest_main
