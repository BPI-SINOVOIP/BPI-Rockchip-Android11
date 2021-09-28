#!/usr/bin/env bash
#
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

# Warning and exit when failed to meet the requirements.
[ "$(uname -s)" != "Darwin" ] && { echo "This program runs on Darwin only."; exit 0; }
[ "$UID" -eq 0 ] && { echo "Running with root user is not supported."; exit 0; }

function usage() {
    echo "###########################################"
    echo "Usage: $prog [-U|-e|-n|-o||-l|-f|-h]"
    echo "  -U: The PATH of the search root."
    echo "  -e: The PATH that unwanted to be searched."
    echo "  -n: The name of directories that won't be cached."
    echo "  -o: The PATH of the generated database."
    echo "  -l: No effect. For compatible with Linux mlocate."
    echo "  -f: Filesystems which should not search for."
    echo "  -h: This usage helper."
    echo
    echo "################ [EXAMPLE] ################"
    echo "$prog -U \$ANDROID_BUILD_TOP -n .git -l 0 \\"
    echo " -e \"\$ANDROID_BUILD_TOP/out \$ANDROID_BUILD_TOP/.repo\" \\"
    echo " -o \"\$ANDROID_HOST_OUT/locate.database\""
    echo
    echo "locate -d \$ANDROID_HOST_OUT/locate.database atest.py"
    echo "locate -d \$ANDROID_HOST_OUT/locate.database contrib/res/config"
}

function mktempdir() {
    TMPDIR=/tmp
    if ! TMPDIR=`mktemp -d $TMPDIR/locateXXXXXXXXXX`; then
        exit 1
    fi
    temp=$TMPDIR/_updatedb$$
}

function _updatedb_main() {
    # 0. Disable default features of bash.
    set -o noglob   # Disable * expension before passing arguments to find.
    set -o errtrace # Sub-shells inherit error trap.

    # 1. Get positional arguments and set variables.
    prog=$(basename $0)
    while getopts 'U:n:e:o:l:f:h' option; do
        case $option in
            U) SEARCHROOT="$OPTARG";; # Search root.
            e) PRUNEPATHS="$OPTARG";; # Paths to be excluded.
            n) PRUNENAMES="$OPTARG";; # Dirnames to be pruned.
            o) DATABASE="$OPTARG";;   # the output of the DB.
            l) ;;                     # No effect.
            f) PRUNEFS="$OPTARG";;    # Disallow network filesystems.
            *) usage; exit 0;;
        esac
    done

    : ${SEARCHROOT:="$ANDROID_BUILD_TOP"}
    if [ -z "$SEARCHROOT" ]; then
        echo 'Either $SEARCHROOT or $ANDROID_BUILD_TOP is required.'
        exit 0
    fi

    if [ -n "$ANDROID_BUILD_TOP" ]; then
        PRUNEPATHS="$PRUNEPATHS $ANDROID_BUILD_TOP/out"
    fi

    PRUNENAMES="$PRUNENAMES *.class *.pyc .gitignore"
    : ${DATABASE:=/tmp/locate.database}
    : ${PRUNEFS:="nfs afp smb"}

    # 2. Assemble excludes strings.
    excludes=""
    or=""
    sortarg="-presort"
    for fs in $PRUNEFS; do
        excludes="$excludes $or -fstype $fs -prune"
        or="-o"
    done
    for path in $PRUNEPATHS; do
        excludes="$excludes $or -path $path -prune"
    done
    for file in $PRUNENAMES; do
        excludes="$excludes $or -name $file -prune"
    done

    # 3. Find and create locate database.
    # Delete $temp when trapping specified return values.
    mktempdir
    trap 'rm -rf $temp $TMPDIR; exit' 0 1 2 3 5 10 15
    if find -s $SEARCHROOT $excludes $or -print 2>/dev/null -true |
        /usr/libexec/locate.mklocatedb $sortarg > $temp 2>/dev/null; then
            case x"`find $temp -size 257c -print`" in
                x) cat $temp > $DATABASE;;
                *) echo "$prog: database $temp is found empty."
                   exit 1;;
            esac
    fi
}

_updatedb_main "$@"
