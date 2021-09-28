#!/bin/bash

set -e

if [ "$#" -ne 1 ]; then
    echo "$0 path-to-system.img | path-to-vendor.img" >&2
    exit 1
fi

srcimg=$1
if [[ ! -f "$srcimg" ]]; then
    echo "$0: '${srcimg}' does not exist"
    exit 1
fi

base_srcimg=`basename $srcimg`
label="${base_srcimg%.*}"
dir_name=$(dirname $srcimg)
target=${dir_name}/$label-qemu.img

#check if $srcimg is sparse
magic="3aff26ed"
src_magic=`xxd -p -l 4 $srcimg`

if [[ $src_magic == $magic ]]; then
echo "Unsparsing ${srcimg}"
tmpfile=$(mktemp -t unsparse_image.XXXXXX)
${SIMG2IMG:-simg2img} $srcimg $tmpfile > /dev/null 2>&1
srcimg="$tmpfile"
fi

dd if=/dev/zero of=$target ibs=1024k count=1 > /dev/null 2>&1
dd if=$srcimg of=$target conv=notrunc,sync ibs=1024k obs=1024k seek=1 > /dev/null 2>&1
unamestr=`uname`
curdisksize=$(stat -c %s $target)

dd if=/dev/zero of=$target conv=notrunc bs=1 count=1024k seek=$curdisksize > /dev/null 2>&1

disksize=`expr $curdisksize + 1024 \* 1024 `

end=`expr $disksize \/ 512 - 2048 - 1`
${SGDISK:-sgdisk} --clear $target > /dev/null 2>&1
${SGDISK:-sgdisk} --new=1:2048:$end --type=1:8300 --change-name=1:$label $target > /dev/null 2>&1

if [[ -e $tmpfile ]]; then
rm $tmpfile
fi
