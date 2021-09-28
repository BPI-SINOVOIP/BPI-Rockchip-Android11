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
PRODUCT_KERNEL_DTS := rk3328-box-liantong-avb
PRODUCT_KERNEL_CONFIG ?= rockchip_defconfig

# AB image definition
BOARD_USES_AB_IMAGE := false

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    include device/rockchip/common/BoardConfig_AB.mk
    TARGET_RECOVERY_FSTAB := device/rockchip/rk3326/rk3326_r/recovery.fstab_AB
endif
