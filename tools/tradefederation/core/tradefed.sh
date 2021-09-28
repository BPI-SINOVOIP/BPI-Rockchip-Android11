#!/bin/bash

# Copyright (C) 2015 The Android Open Source Project
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

# A helper script that launches Trade Federation

shdir=`dirname $0`/
source "${shdir}/script_help.sh"
# At this point, we're guaranteed to have the right Java version, and the
# following env variables will be set, if appropriate:
# JAVA_VERSION, RDBG_FLAG, TF_PATH, TRADEFED_OPTS
checkPath adb

# Allow to specify another java entry point
CONSOLE_CLASS="com.android.tradefed.command.Console"
if [ ! -z "${ENTRY_CLASS}" ]; then
  CONSOLE_CLASS=${ENTRY_CLASS}
fi

# Note: must leave $RDBG_FLAG and $TRADEFED_OPTS unquoted so that they go away
# when unset
exec ${TF_JAVA} $ADD_OPENS_FLAG $RDBG_FLAG -XX:+HeapDumpOnOutOfMemoryError \
  -XX:-OmitStackTraceInFastThrow $TRADEFED_OPTS \
  -cp "${TF_PATH}" -DTF_JAR_DIR=${TF_JAR_DIR} $CONSOLE_CLASS "$@"
