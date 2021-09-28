#!/bin/bash

set -e

if [ "$#" -ne 2 ]; then
    echo "$0 src.img target.img" >&2
    exit 1
fi

srcimg=$1
target=$2

disksize=$(stat -c %s $srcimg)

mycount=`expr $disksize \/ 1024 - 2048`

dd if=$srcimg of=$target ibs=1k skip=1024 count=${mycount}

