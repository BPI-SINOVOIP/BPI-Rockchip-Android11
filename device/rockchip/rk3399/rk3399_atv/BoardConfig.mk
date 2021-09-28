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
include device/rockchip/rk3399/BoardConfig.mk

#PRODUCT_KERNEL_DTS ?= rk3399-box-rev2-avb
PRODUCT_KERNEL_DTS := rk3399-sapphire-excavator-edp-avb
PRODUCT_KERNEL_CONFIG := rockchip_defconfig

BUILD_WITH_GO_OPT := false
# Google TV Service and frp overlay
PRODUCT_USE_PREBUILT_GTVS := yes
BUILD_WITH_GOOGLE_FRP := true
BOARD_SENSOR_MPU_PAD := false
BUILD_WITH_GOOGLE_GMS_EXPRESS := false
CAMERA_SUPPORT_AUTOFOCUS:= false
PRODUCT_HAVE_RKAPPS := false

# for widevine drm
BOARD_WIDEVINE_OEMCRYPTO_LEVEL := 3

#for microsoft drm
BUILD_WITH_MICROSOFT_PLAYREADY :=true
DEVICE_MANIFEST_FILE := $(TARGET_DEVICE_DIR)/manifest.xml


PRODUCT_KERNEL_CONFIG := rockchip_defconfig android-10.config
BOARD_AVB_ENABLE := true
