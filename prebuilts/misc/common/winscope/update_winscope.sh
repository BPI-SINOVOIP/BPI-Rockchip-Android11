#!/bin/bash
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
  echo "usage: $0 [--out <output path>]"
  echo "  --out: publish to output path. Otherwise, publishes to current directory."
  false
fi

if [ -t 1 ] && [ $(tput colors) -ge 8 ]; then
    style_reset="$(tput sgr0)"
    style_red="$(tput setaf 1)"
    style_green="$(tput setaf 2)"
    style_bold="$(tput bold)"
fi
OK="${style_bold}OK${style_reset}"
FAIL="${style_bold}${style_red}FAIL${style_reset}"
INFO="${style_bold}${style_green}INFO${style_reset}"
WARN="${style_bold}${style_red}WARN${style_reset}"
SUCCESS="${style_bold}${style_green}SUCCESS${style_reset}"

WINSCOPE_OUT_PATH="${PWD}/winscope.html"
if [ "$1" == "--out" ]; then
  WINSCOPE_OUT_PATH="$2"
fi

YARN="${YARN:-$(which yarn || true)}"
WINSCOPE_PATH="${ANDROID_BUILD_TOP}/development/tools/winscope"
WINSCOPE_BUILD_PATH="${WINSCOPE_PATH}/dist/index.html"

[ -x "$YARN" ] || { echo -e "$FAIL: could not run yarn.\n$INFO: looking for yarn at YARN='${YARN:-\$(which yarn)}'. You can override this by setting the YARN environment variable."; false; }
[ -x "$ANDROID_BUILD_TOP" ] || { echo -e "$FAIL: $ANDROID_BUILD_TOP variable is not set. \n$INFO: did you run setup/buildenv.sh?"; false; }
"$YARN" --cwd "$WINSCOPE_PATH" install || { echo -e "$FAIL: installing winscope dependencies failed.\n$INFO: command was: \"$YARN\" --cwd \"$WINSCOPE_PATH\" install"; false; }
"$YARN" --cwd "$WINSCOPE_PATH" run build || { echo -e "$FAIL: building winscope failed.\n$INFO: command was: \"$YARN\" --cwd \"$WINSCOPE_PATH\" run build"; false; }
install --mode 664 "$WINSCOPE_BUILD_PATH" "$WINSCOPE_OUT_PATH" || { echo -e "$FAIL: publishing ${WINSCOPE_OUT_PATH} failed."; false; }
echo -e "$SUCCESS: published to ${WINSCOPE_OUT_PATH}"
