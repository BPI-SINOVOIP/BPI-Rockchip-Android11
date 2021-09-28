#!/bin/sh
#
# Copyright (C) 2019 The Android Open-Source Project
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

usage() {
    echo "Usage: $0 [--all] [--apk-info=file] [--apk-dir=dir]"
    echo ""
    echo "Options:"
    echo -e "  -a, --all \t\trun all tests"
    echo -e "  -h, --help \t\tprint this help"
    echo -e "  --apk-info=file \tAn XML file describing the list of APKs for qualifications."
    echo -e "  --apk-dir=dir \tDirectory containing the APKs for qualifications.  If --apk-info is"
    echo -e "                \tnot specified and a file named 'apk-info.xml' exists in --apk-dir,"
    echo -e "                \tthat file will be used as the apk-info."
    exit 1;
}

OPTS=`getopt -o ah -l all,help,apk-info:,apk-dir: -- "$@"` || usage
eval set -- "$OPTS"

CONFIG="certification-tests.xml"
APK_INFO=""
APK_DIR=""
while [ $# -ne 0 ]; do
    case "$1" in
        --apk-info)
            APK_INFO="--apk-info $2"; shift 2;;
        --apk-dir)
            APK_DIR="--apk-dir $2"; shift 2;;
        -a|--all)
            CONFIG="AndroidTest.xml"; shift;;
        -h|--help)
            usage;;
        --) shift; break;;
        *)
            # Should be unreachable.
            echo "Internal error: getopt ${OPTS}"
            usage
    esac
done

ANDROID_TARGET_OUT_TESTCASES=$PWD/testcases ./bin/tradefed.sh run commandAndExit ${CONFIG} ${APK_INFO} ${APK_DIR} "$@"
