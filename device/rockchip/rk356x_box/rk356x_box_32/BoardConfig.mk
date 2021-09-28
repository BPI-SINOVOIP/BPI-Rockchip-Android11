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
include device/rockchip/rk356x_box/CommonBoardConfig.mk
BUILD_WITH_GO_OPT := false

BOARD_SELINUX_ENFORCING := false

# AB image definition
BOARD_USES_AB_IMAGE := false
BOARD_ROCKCHIP_VIRTUAL_AB_ENABLE := false

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    include device/rockchip/common/BoardConfig_AB.mk
    TARGET_RECOVERY_FSTAB := device/rockchip/rk356x_box/rk356x_box/recovery.fstab_AB
endif

PRODUCT_UBOOT_CONFIG := rk3566
PRODUCT_KERNEL_DTS := rk3566-box-demo-v10
BOARD_GSENSOR_MXC6655XA_SUPPORT := true

TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv8-2a
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := cortex-a55
TARGET_CPU_SMP := true

TARGET_2ND_ARCH :=
TARGET_2ND_ARCH_VARIANT :=
TARGET_2ND_CPU_ABI :=
TARGET_2ND_CPU_ABI2 :=
TARGET_2ND_CPU_VARIANT :=

TARGET_PREFER_32_BIT_APPS := true
TARGET_SUPPORTS_64_BIT_APPS := false

BOARD_OVERRIDE_RS_CPU_VARIANT_64 :=

TARGET_USES_64_BIT_BCMDHD := false
TARGET_USES_64_BIT_BINDER := true
MALLOC_SVELTE := true
BOARD_TV_LOW_MEMOPT := true
