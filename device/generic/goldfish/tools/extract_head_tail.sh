#!/bin/bash

set -e

if [ "$#" -ne 3 ]; then
    echo "$0 src.img head.img tail.img" >&2
    exit 1
fi

srcimg=$1
headimg=$2
tailimg=$3

disksize=$(stat -c %s $srcimg)

mycount=`expr $disksize \/ 1024 \/ 1024 - 1`

dd if=$srcimg of=$headimg ibs=1M obs=1M count=1
dd if=$srcimg of=$tailimg ibs=1M obs=1M count=1 skip=$mycount
#dd if=/dev/zero of=file.txt count=3083 bs=1M
