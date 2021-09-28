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

# Main function.
function _asuite_main() {
    local T="$(gettop)/tools"
    src_atest="$T/asuite/atest/atest_completion.sh"
    src_acloud="$T/acloud/acloud_completion.sh"
    src_aidegen="$T/asuite/aidegen/aidegen_completion.sh"
    declare -a asuite_srcs=($src_atest $src_acloud $src_aidegen)
    for src in ${asuite_srcs[@]}; do
        # should_add_completion selectively allows disabling tab completion.
        # ENVSETUP_NO_COMPLETION=acloud # -> disable acloud completion.
        # ENVSETUP_NO_COMPLETION=acloud:atest # -> disable acloud and atest.
        if [[ -f $src ]] && should_add_completion $src ; then
            source $src || true
        fi
    done
}

_asuite_main
