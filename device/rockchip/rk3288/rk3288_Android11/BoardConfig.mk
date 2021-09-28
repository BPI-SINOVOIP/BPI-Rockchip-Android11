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
include device/rockchip/rk3288/BoardConfig.mk
BUILD_WITH_GO_OPT := false

TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := cortex-a15
TARGET_CPU_SMP := true

BOARD_SENSOR_ST := true
BOARD_SENSOR_COMPASS_AK8963-64 := false
BOARD_SENSOR_MPU_PAD := false
BOARD_COMPASS_SENSOR_SUPPORT := true
BOARD_GYROSCOPE_SENSOR_SUPPORT := true

PRODUCT_KERNEL_DTS := rk3288-evb-android-rk808-edp-avb
# AB image definition
BOARD_USES_AB_IMAGE := false
BOARD_ROCKCHIP_VIRTUAL_AB_ENABLE := false

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    include device/rockchip/common/BoardConfig_AB.mk
    TARGET_RECOVERY_FSTAB := device/rockchip/rk3288/rk3288_Android11/recovery.fstab_AB
endif

# Allow deprecated BUILD_ module types to get DDK building
BUILD_BROKEN_USES_BUILD_COPY_HEADERS := true
BUILD_BROKEN_USES_BUILD_HOST_EXECUTABLE := true
BUILD_BROKEN_USES_BUILD_HOST_SHARED_LIBRARY := true
BUILD_BROKEN_USES_BUILD_HOST_STATIC_LIBRARY := true
