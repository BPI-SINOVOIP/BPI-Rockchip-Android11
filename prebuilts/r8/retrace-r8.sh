#!/bin/bash
#
# Copyright (C) 2019 The Android Open Source Project
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

# Call retrace with the r8 map file.
#
# Usage:
#
#     retrace-r8.sh [-verbose] [<stacktrace_file>]
#
# Read from stdin if stacktrace file is not specified.

# Set up prog to be the path of this script, including following symlinks,
# and set up progdir to be the fully-qualified pathname of its directory.
prog="$0"
while [ -h "${prog}" ]; do
    newProg=`/bin/ls -ld "${prog}"`
    newProg=`expr "${newProg}" : ".* -> \(.*\)$"`
    if expr "x${newProg}" : 'x/' >/dev/null; then
        prog="${newProg}"
    else
        progdir=`dirname "${prog}"`
        prog="${progdir}/${newProg}"
    fi
done
oldwd=`pwd`
progdir=`dirname "${prog}"`
cd "${progdir}"
progdir=`pwd`
prog="${progdir}"/`basename "${prog}"`
cd "${oldwd}"

retracedir="${progdir}/retrace"
retracejar="retrace.jar"
proguardjar="proguard.jar"

if [ ! -r "${retracedir}/${retracejar}" ]; then
    echo `basename "$prog"`": can't find ${retracejar}"
    exit 1
fi

if [ ! -r "${retracedir}/${proguardjar}" ]; then
    echo `basename "$prog"`": can't find ${proguardjar}"
    exit 1
fi

mapfile="r8.jar.map"

if [ ! -r "${progdir}/${mapfile}" ]; then
    echo `basename "${prog}"`": can't find ${mapfile}"
    exit 1
fi

if [ "$OSTYPE" = "cygwin" ]; then
    # For Cygwin, convert the scriptfile path into native Windows style.
    mappath=`cygpath -w "${progdir}/${mapfile}"`
    retracepath=`cygpath -w "${retracedir}/${retracejar}"`
else
    mappath="${progdir}/${mapfile}"
    retracepath="${retracedir}/${retracejar}"
fi

exec java -jar "${retracepath}" "${mappath}" "$@"
