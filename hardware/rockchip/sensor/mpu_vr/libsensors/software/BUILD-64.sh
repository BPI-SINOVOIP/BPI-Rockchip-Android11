#!/bin/bash

# This is a sample of the command line make used to build
#   the libraries and binaries for the Pandaboard.
# Please customize this path to match the location of your
#   Android source tree. Other variables may also need to
#   be customized such as:
#     $CROSS, $PRODUCT, $KERNEL_ROOT

export ANDROID_BASE=/home/lyx/rk3399-8.1

make -C build/android \
	VERBOSE=0 \
	TARGET=android \
	ANDROID_ROOT=${ANDROID_BASE} \
	KERNEL_ROOT=${ANDROID_BASE}/kernel \
	CROSS=${ANDROID_BASE}/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android- \
	PRODUCT=rk3399 \
	MPL_LIB_NAME=mplmpu \
	echo_in_colors=echo \
	ARCH=arm64 \
	-f shared.mk

