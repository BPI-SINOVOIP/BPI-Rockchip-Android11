#!/bin/bash
BASEPATH=$(cd `dirname $0`; pwd)
echo $BASEPATH
TIMESTAMP=$(date  +%Y%m%d-%H%M%S)
if [ ! -n "$1" ] ;then
    export STUB_PATCH_PATH="$BASEPATH"/PATCHES_"$TIMESTAMP"
else
    echo "STUB_PATCH_PATH is $1"
    export STUB_PATCH_PATH=$1
fi

mkdir -p $STUB_PATCH_PATH

#Generate patches

.repo/repo/repo forall -c "$BASEPATH/device/rockchip/common/gen_patches_body.sh"
