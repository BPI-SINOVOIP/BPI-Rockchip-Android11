# !/bin/sh
source build/envsetup.sh

export PROJECT_TOP=`gettop`
export DATE=$(date  +%Y%m%d.%H%M)

if [ ! -n "$1" ] ;then
    export PATCHES_PATH=$(pwd)/PATCHES
else
    export PATCHES_PATH=`readlink -f $1`
fi
echo
echo
echo "*****************************"
echo "use ./restore_patches.sh [path_to_your_patches]"
echo "Patches path is $PATCHES_PATH"
echo "*****************************"

.repo/repo/repo forall -pc "$PROJECT_TOP/system/tools/release_tools/restore_patches_body.sh"

echo "********  Done!  ************"
