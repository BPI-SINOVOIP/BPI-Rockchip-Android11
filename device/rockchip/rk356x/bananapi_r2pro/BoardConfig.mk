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
BUILD_WITH_GO_OPT := false
BOARD_SELINUX_ENFORCING ?= false

# AB image definition
BOARD_USES_AB_IMAGE := false
BOARD_ROCKCHIP_VIRTUAL_AB_ENABLE := false

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    include device/rockchip/common/BoardConfig_AB.mk
    TARGET_RECOVERY_FSTAB := device/rockchip/rk356x/rk3566_r/recovery.fstab_AB
endif

PRODUCT_UBOOT_CONFIG := bananapi_r2pro
PRODUCT_KERNEL_DTS := bananapi-r2pro-dtbs
PRODUCT_KERNEL_CONFIG := bananapi_r2pro_defconfig android-11.config
PRODUCT_KERNEL_DIR := kernel

BOARD_GSENSOR_MXC6655XA_SUPPORT := true
BOARD_CAMERA_SUPPORT_EXT := true
BOARD_HS_ETHERNET := true
BOARD_QUECTEL_RIL := true
BOARD_HAVE_DONGLE := true

BOARD_CUSTOM_BT_CONFIG := $(TARGET_DEVICE_DIR)/vnd_$(TARGET_DEVICE).txt

# primary panel orientation
SF_PRIMARY_DISPLAY_ORIENTATION := 0

TARGET_ROCKCHIP_PCBATEST := false
BOARD_HAS_FACTORY_TEST := false

# increase super partition size for system, system_ext, vendor, product and odm
# must be a multiple of its block size(65536)
#ifeq ($(PRODUCT_USE_DYNAMIC_PARTITIONS), true)
#    BOARD_SUPER_PARTITION_SIZE := 5169676288
#endif
