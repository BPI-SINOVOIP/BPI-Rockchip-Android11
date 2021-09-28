# Copyright (C) 2019 The Android Open Source Project
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

TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_VARIANT := generic
TARGET_CPU_ABI := arm64-v8a

TARGET_2ND_ARCH := arm
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_VARIANT := generic

include build/make/target/board/BoardConfigMainlineCommon.mk

TARGET_NO_KERNEL := true

BOARD_USES_SYSTEM_OTHER_ODEX :=

BUILD_QEMU_IMAGES := true
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true

BOARD_BUILD_SUPER_IMAGE_BY_DEFAULT := true

BOARD_SUPER_PARTITION_SIZE := 3229614080
BOARD_SUPER_PARTITION_GROUPS := fvp_dynamic_partitions
BOARD_FVP_DYNAMIC_PARTITIONS_SIZE := 3221225472
BOARD_FVP_DYNAMIC_PARTITIONS_PARTITION_LIST := system vendor
TARGET_COPY_OUT_PRODUCT := system/product
TARGET_COPY_OUT_SYSTEM_EXT := system/system_ext

BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4

BOARD_USERDATAIMAGE_PARTITION_SIZE := 576716800

BOARD_SEPOLICY_DIRS += device/generic/goldfish/fvpbase/sepolicy

TARGET_EXPERIMENTAL_MTE := true
