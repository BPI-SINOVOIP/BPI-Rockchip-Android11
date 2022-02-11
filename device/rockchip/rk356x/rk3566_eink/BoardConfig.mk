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
include device/rockchip/rk356x/BoardConfig.mk

DONT_UNCOMPRESS_PRIV_APPS_DEXS := false

# AB image definition
BOARD_USES_AB_IMAGE := false
BOARD_ROCKCHIP_VIRTUAL_AB_ENABLE := false
BOARD_SELINUX_ENFORCING := true
BOARD_PLAT_PUBLIC_SEPOLICY_DIR := device/rockchip/rk356x/sepolicy_ebook_public

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    include device/rockchip/common/BoardConfig_AB.mk
    TARGET_RECOVERY_FSTAB := device/rockchip/rk356x/rk3566_eink/recovery.fstab_AB
endif
PRODUCT_UBOOT_CONFIG := rk3566-eink
PRODUCT_KERNEL_DTS := rk3566-rk817-eink-w103
PRODUCT_FSTAB_TEMPLATE := device/rockchip/rk356x/rk3566_eink/fstab_eink.in

BOARD_GSENSOR_MXC6655XA_SUPPORT := true
BOARD_CAMERA_SUPPORT_EXT := true

#Config RK EBOOK
BUILD_WITH_RK_EBOOK := true
SF_PRIMARY_DISPLAY_ORIENTATION := 270

PRODUCT_HAVE_OPTEE := false
