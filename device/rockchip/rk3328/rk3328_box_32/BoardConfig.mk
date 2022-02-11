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
include device/rockchip/rk3328/BoardConfig.mk
BUILD_WITH_GO_OPT := false
PRODUCT_UBOOT_CONFIG ?= rk3328
PRODUCT_KERNEL_ARCH ?= arm64
PRODUCT_KERNEL_DTS := rk3328-evb-android-avb
PRODUCT_KERNEL_CONFIG ?= rockchip_defconfig android-11.config

TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := cortex-a53
TARGET_CPU_SMP := true

TARGET_2ND_ARCH :=
TARGET_2ND_ARCH_VARIANT :=
TARGET_2ND_CPU_ABI :=
TARGET_2ND_CPU_ABI2 :=
TARGET_2ND_CPU_VARIANT :=
TARGET_PREBUILT_KERNEL := kernel/arch/arm64/boot/Image

# RenderScript
# OVERRIDE_RS_DRIVER := libnvRSDriver.so
BOARD_OVERRIDE_RS_CPU_VARIANT_32 := cortex-a53
BOARD_OVERRIDE_RS_CPU_VARIANT_64 :=
# DISABLE_RS_64_BIT_DRIVER := true

TARGET_USES_64_BIT_BCMDHD := false
TARGET_USES_64_BIT_BINDER := true

TARGET_PREFER_32_BIT_APPS := true
TARGET_SUPPORTS_64_BIT_APPS := false

BOARD_OVERRIDE_RS_CPU_VARIANT_64 :=

MALLOC_SVELTE := true
BOARD_TV_LOW_MEMOPT := true# AB image definition

TARGET_USES_64_BIT_BCMDHD := false
TARGET_USES_64_BIT_BINDER := true

BOARD_USES_AB_IMAGE := false

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    include device/rockchip/common/BoardConfig_AB.mk
    TARGET_RECOVERY_FSTAB := device/rockchip/rk3326/rk3326_r/recovery.fstab_AB
endif
