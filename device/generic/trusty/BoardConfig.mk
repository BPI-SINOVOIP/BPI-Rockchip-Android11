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

# The generic product target doesn't have any hardware-specific pieces.
TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := true
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_VARIANT := generic
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_BOOTLOADER_BOARD_NAME := trusty_$(TARGET_ARCH)

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := generic

BOARD_SEPOLICY_DIRS += device/generic/trusty/sepolicy

TARGET_USES_64_BIT_BINDER := true

# We want goldfish build configuration information, but not the resulting
# QEMU images. QEMU_CUSTOMIZATIONS turns this on without building the images
# like BUILD_QEMU_IMAGES would imply.
QEMU_CUSTOMIZATIONS := true

TARGET_USERIMAGES_USE_EXT4 := true
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 536870912 # 512M
BOARD_USERDATAIMAGE_PARTITION_SIZE := 67108864 # 64M
TARGET_COPY_OUT_VENDOR := vendor
# ~100 MB vendor image. Please adjust system image / vendor image sizes
# when finalizing them.
BOARD_VENDORIMAGE_PARTITION_SIZE := 4194304 # 4M
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_FLASH_BLOCK_SIZE := 512
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true
DEVICE_MATRIX_FILE   := device/generic/goldfish/compatibility_matrix.xml

BOARD_PROPERTY_OVERRIDES_SPLIT_ENABLED := true
BOARD_SEPOLICY_DIRS += build/target/board/generic/sepolicy

# Enable A/B update
TARGET_NO_RECOVERY := true
BOARD_BUILD_SYSTEM_ROOT_IMAGE := true

# Specify HALs
DEVICE_MANIFEST_FILE := device/generic/trusty/manifest.xml
