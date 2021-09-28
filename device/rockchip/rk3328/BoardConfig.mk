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

# Use the non-open-source parts, if they're present

TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a53
TARGET_CPU_SMP := true

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := cortex-a53

BOARD_AVB_ENABLE := false

CURRENT_SDK_VERSION := RK3328_ANDROID11.0_BOX_V1.0

BOARD_WITH_SPECIAL_PARTITIONS := baseparameter:1M,logo:16M

BOARD_BOOT_HEADER_VERSION := 1

# Enable Dex compile opt as default
WITH_DEXPREOPT := true

PRODUCT_UBOOT_CONFIG ?= rk3328
PRODUCT_KERNEL_ARCH ?= arm64
PRODUCT_KERNEL_DTS ?= rk3328-box-liantong-avb
PRODUCT_KERNEL_CONFIG ?= rockchip_defconfig

TARGET_PREBUILT_KERNEL := kernel/arch/arm64/boot/Image
BOARD_PREBUILT_DTBIMAGE_DIR := kernel/arch/arm64/boot/dts/rockchip
# Disable emulator for "make dist" until there is a 64-bit qemu kernel
BUILD_EMULATOR := false
TARGET_BOARD_PLATFORM := rk3328
TARGET_BOARD_PLATFORM_GPU := mali450
BOARD_USE_DRM := true

# RenderScript
# OVERRIDE_RS_DRIVER := libnvRSDriver.so
BOARD_OVERRIDE_RS_CPU_VARIANT_32 := cortex-a53
BOARD_OVERRIDE_RS_CPU_VARIANT_64 := cortex-a53
# DISABLE_RS_64_BIT_DRIVER := true

TARGET_USES_64_BIT_BCMDHD := true
TARGET_USES_64_BIT_BINDER := true

# HACK: Build apps as 64b for volantis_64_only
ifneq (,$(filter ro.zygote=zygote64, $(PRODUCT_DEFAULT_PROPERTY_OVERRIDES)))
TARGET_PREFER_32_BIT_APPS :=
TARGET_SUPPORTS_64_BIT_APPS := true
endif

ENABLE_CPUSETS := true

BOARD_CAMERA_SUPPORT := true
BOARD_CAMERA_SUPPORT_EXT := true
BOARD_NFC_SUPPORT := false
BOARD_HAS_GPS := false

BOARD_GRAVITY_SENSOR_SUPPORT := false
BOARD_COMPASS_SENSOR_SUPPORT := false
BOARD_GYROSCOPE_SENSOR_SUPPORT := false
BOARD_PROXIMITY_SENSOR_SUPPORT := false
BOARD_LIGHT_SENSOR_SUPPORT := false
BOARD_PRESSURE_SENSOR_SUPPORT := false
BOARD_TEMPERATURE_SENSOR_SUPPORT := false
BOARD_USB_HOST_SUPPORT := true
BOARD_USER_FAKETOUCH := false

PRODUCT_HAVE_RKAPPS := true

# for optee support
PRODUCT_HAVE_OPTEE := true

BOARD_USE_SPARSE_SYSTEM_IMAGE := true

# Google Service and frp overlay
BUILD_WITH_GOOGLE_MARKET := false
BUILD_WITH_GOOGLE_MARKET_ALL := false
BUILD_WITH_GOOGLE_FRP := false
# Add widevine L3 support
BOARD_WIDEVINE_OEMCRYPTO_LEVEL := 3

#for microsoft drm
BUILD_WITH_MICROSOFT_PLAYREADY :=true

# Config GO Optimization
#BUILD_WITH_GO_OPT := true
ALLOW_MISSING_DEPENDENCIES=true

# enable SVELTE malloc
#MALLOC_SVELTE := true

#Config omx to support codec type.
BOARD_SUPPORT_VP9 := true
BOARD_SUPPORT_VP6 := false

TARGET_RK_GRALLOC_VERSION := 1

# memtrack support
BOARD_MEMTRACK_SUPPORT := true

#only box and atv using our audio policy(write by rockchip)
USE_CUSTOM_AUDIO_POLICY := 1

TARGET_ROCKCHIP_PCBATEST := false

