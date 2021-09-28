#!/bin/sh
set -e

. build/envsetup.sh >/dev/null && setpaths

OUT_DIR=$2
TARGET_DIR=$OUT_DIR/recovery/root
#TARGET_RAMDISK=ramdisk-recovery
#TMP_MOUNT=_tmp_mount
if [ "$1"x != ""x  ]; then
	TARGET_DIR=$1
fi

echo " generic recovery rootfs content "

rm -rf ${TARGET_DIR}
mkdir -p ${TARGET_DIR}
cd ${TARGET_DIR}
mkdir dev
mkdir lib
mkdir proc
mkdir res
mkdir sbin
mkdir sys
mkdir -p system/bin
mkdir -p system/etc
mkdir tmp
mkdir sdcard
mkdir cache
mkdir data
mkdir flash
# system partition mount point
mkdir system_mount
mkdir etc

cd ../../../../../../

cp bootable/recovery/etc/init.rc ${TARGET_DIR}/
cp ${TARGET_DEVICE_DIR}/recovery.fstab ${TARGET_DIR}/etc/
cp ${OUT_DIR}/root/init ${TARGET_DIR}/
cp ${OUT_DIR}/root/default.prop ${TARGET_DIR}/

cp ${OUT_DIR}/root/sbin/adbd ${TARGET_DIR}/sbin/

cp ${OUT_DIR}/root/sbin/ueventd ${TARGET_DIR}/sbin/

cp ${OUT_DIR}/system/lib/libc.so ${TARGET_DIR}/lib/
cp ${OUT_DIR}/system/lib/libcutils.so ${TARGET_DIR}/lib/
cp ${OUT_DIR}/system/lib/libdl.so ${TARGET_DIR}/lib/
cp ${OUT_DIR}/system/lib/liblog.so ${TARGET_DIR}/lib/
cp ${OUT_DIR}/system/lib/libm.so ${TARGET_DIR}/lib/
cp ${OUT_DIR}/system/lib/libstdc++.so ${TARGET_DIR}/lib/

cp -r bootable/recovery/res/images ${TARGET_DIR}/res/

cp -rf ${OUT_DIR}/system/etc/firmware  ${TARGET_DIR}/system/etc/firmware
cp  ${OUT_DIR}/system/lib/modules/wlan.ko ${TARGET_DIR}/res/images
cp ${OUT_DIR}/system/bin/pretest ${TARGET_DIR}/system/bin/
cp ${OUT_DIR}/system/bin/wlarm_android ${TARGET_DIR}/system/bin/
cp ${OUT_DIR}/system/bin/aplay ${TARGET_DIR}/system/bin/
cp ${OUT_DIR}/system/bin/arec ${TARGET_DIR}/system/bin/

cp ${OUT_DIR}/system/bin/mkdosfs ${TARGET_DIR}/sbin/
cp ${OUT_DIR}/root/sbin/e2fsck ${TARGET_DIR}/sbin/
cp ${OUT_DIR}/root/sbin/mke2fs ${TARGET_DIR}/sbin/
cp ${OUT_DIR}/system/bin/recovery ${TARGET_DIR}/sbin/
ln -s mke2fs ${TARGET_DIR}/sbin/mkfs.ext3

cp ${OUT_DIR}/system/build.prop ${TARGET_DIR}/system/
cp ${OUT_DIR}/system/bin/linker ${TARGET_DIR}/system/bin/
cp ${OUT_DIR}/system/bin/logcat ${TARGET_DIR}/system/bin/
cp ${OUT_DIR}/system/bin/sh ${TARGET_DIR}/system/bin/
cp ${OUT_DIR}/root/rk29xxnand_ko.ko* ${TARGET_DIR}/


cp ${OUT_DIR}/system/bin/busybox ${TARGET_DIR}/system/bin/
ln -s busybox ${TARGET_DIR}/system/bin/ls
ln -s busybox ${TARGET_DIR}/system/bin/cat
ln -s busybox ${TARGET_DIR}/system/bin/chmod
ln -s busybox ${TARGET_DIR}/system/bin/chown
ln -s busybox ${TARGET_DIR}/system/bin/date
ln -s busybox ${TARGET_DIR}/system/bin/dd
ln -s busybox ${TARGET_DIR}/system/bin/df
ln -s busybox ${TARGET_DIR}/system/bin/dmesg
ln -s busybox ${TARGET_DIR}/system/bin/ifconfig
ln -s busybox ${TARGET_DIR}/system/bin/kill
ln -s busybox ${TARGET_DIR}/system/bin/mkdir
ln -s busybox ${TARGET_DIR}/system/bin/mount
ln -s busybox ${TARGET_DIR}/system/bin/reboot
ln -s busybox ${TARGET_DIR}/system/bin/rm
ln -s busybox ${TARGET_DIR}/system/bin/umount
ln -s busybox ${TARGET_DIR}/system/bin/insmod
ln -s busybox ${TARGET_DIR}/system/bin/lsmod
ln -s busybox ${TARGET_DIR}/system/bin/rmmod
ln -s busybox ${TARGET_DIR}/system/bin/mv
ln -s busybox ${TARGET_DIR}/system/bin/ps
ln -s busybox ${TARGET_DIR}/system/bin/cp

#cp ${OUT_DIR}/system/bin/toolbox ${TARGET_DIR}/system/bin/
#ln -s toolbox ${TARGET_DIR}/system/bin/ls
#ln -s toolbox ${TARGET_DIR}/system/bin/cat
#ln -s toolbox ${TARGET_DIR}/system/bin/chmod
#ln -s toolbox ${TARGET_DIR}/system/bin/chown
#ln -s toolbox ${TARGET_DIR}/system/bin/date
#ln -s toolbox ${TARGET_DIR}/system/bin/dd
#ln -s toolbox ${TARGET_DIR}/system/bin/df
#ln -s toolbox ${TARGET_DIR}/system/bin/dmesg
#ln -s toolbox ${TARGET_DIR}/system/bin/ifconfig
#ln -s toolbox ${TARGET_DIR}/system/bin/kill
#ln -s toolbox ${TARGET_DIR}/system/bin/mkdir
#ln -s toolbox ${TARGET_DIR}/system/bin/mount
#ln -s toolbox ${TARGET_DIR}/system/bin/reboot
#ln -s toolbox ${TARGET_DIR}/system/bin/rm
#ln -s toolbox ${TARGET_DIR}/system/bin/umount
#ln -s toolbox ${TARGET_DIR}/system/bin/insmod
#ln -s toolbox ${TARGET_DIR}/system/bin/lsmod
#ln -s toolbox ${TARGET_DIR}/system/bin/rmmod
#ln -s toolbox ${TARGET_DIR}/system/bin/mv
#ln -s toolbox ${TARGET_DIR}/system/bin/ps

#mknod ${TARGET_DIR}/dev/console c 5 1

chmod 777 $TARGET_DIR -R

echo " ==>end "

