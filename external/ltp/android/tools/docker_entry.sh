#!/usr/bin/env bash
#
# Copyright 2019 - The Android Open Source Project
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
#

set -e

# /src is the original source tree mounted to the host, in order to not
# generate files that will need to be cleaned into it, copy it to $LTP_ROOT and
# execute configure / make from there.
#
# This also ensures that the parsing scripts will get paths that make sense on
# the host system.
cp -R /src/* $LTP_ROOT/

/src/android/tools/dump_make_dryrun.sh
