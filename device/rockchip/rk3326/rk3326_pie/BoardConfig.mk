#
# Copyright 2014 The Android Open-Source Project
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
include device/rockchip/rk3326/BoardConfig.mk
BUILD_WITH_GO_OPT := false

TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := cortex-a53
TARGET_CPU_SMP := true

BOARD_BOOT_HEADER_VERSION := 1

# AB image definition
BOARD_USES_AB_IMAGE := false

ifneq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    BOARD_RECOVERY_MKBOOTIMG_ARGS := --second $(TARGET_PREBUILT_RESOURCE) --header_version 1
    # Android Q use odm instead of oem, but for upgrading to Q, partation list cant be changed, odm will mount at /dev/block/by-name/oem
    BOARD_ODMIMAGE_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py device/rockchip/rk3326/rk3326_pie/parameter.txt oem)
endif

# No need to place dtb into boot.img for the device upgrading to Q.
BOARD_INCLUDE_DTB_IN_BOOTIMG :=
BOARD_PREBUILT_DTBIMAGE_DIR :=

ifneq ($(strip $(BOARD_USES_AB_LEGACY_RETROFIT)), true)
    # Need to build system as root for the device upgrading to Q.
    BOARD_BUILD_SYSTEM_ROOT_IMAGE := true
endif

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    ifeq ($(strip $(BOARD_USES_AB_LEGACY_RETROFIT)), true)
        PRODUCT_FSTAB_TEMPLATE := device/rockchip/rk3326/rk3326_pie/fstab_ab_retrofit.in
        PRODUCT_DTBO_TEMPLATE := device/rockchip/rk3326/rk3326_pie/dt-overlay_ab_retrofit.in
    else
        PRODUCT_FSTAB_TEMPLATE := device/rockchip/rk3326/rk3326_pie/fstab_ab.in
        PRODUCT_DTBO_TEMPLATE := device/rockchip/rk3326/rk3326_pie/dt-overlay_ab.in
    endif
    include device/rockchip/common/BoardConfig_AB.mk
    ifeq ($(strip $(BOARD_USES_AB_LEGACY_RETROFIT)), true)
        TARGET_RECOVERY_FSTAB := device/rockchip/rk3326/rk3326_pie/recovery.fstab_AB_retrofit
    else
        TARGET_RECOVERY_FSTAB := device/rockchip/rk3326/rk3326_pie/recovery.fstab_AB
    endif
endif
