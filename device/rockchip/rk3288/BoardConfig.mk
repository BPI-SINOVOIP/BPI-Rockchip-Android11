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

TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_VARIANT := cortex-a15
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
ENABLE_CPUSETS := true
TARGET_CPU_SMP := true

CURRENT_SDK_VERSION := RK3288_ANDROID11.0_MID_V1.0

TARGET_PREBUILT_KERNEL := kernel/arch/arm/boot/zImage
BOARD_PREBUILT_DTBIMAGE_DIR := kernel/arch/arm/boot/dts

PRODUCT_UBOOT_CONFIG ?= rk3288
PRODUCT_KERNEL_ARCH ?= arm
PRODUCT_KERNEL_DTS ?= rk3288-evb-android-rk808-edp-avb
PRODUCT_KERNEL_CONFIG ?= rockchip_defconfig
BOARD_AVB_ENABLE := false
SF_PRIMARY_DISPLAY_ORIENTATION := 0

# Disable emulator for "make dist" until there is a 64-bit qemu kernel
BUILD_EMULATOR := false
TARGET_BOARD_PLATFORM := rk3288
TARGET_BOARD_PLATFORM_GPU := mali-t760
TARGET_RK_GRALLOC_VERSION := 4
BOARD_USE_DRM := true

# RenderScript
# OVERRIDE_RS_DRIVER := libnvRSDriver.so
BOARD_OVERRIDE_RS_CPU_VARIANT_32 := cortex-a15
# DISABLE_RS_64_BIT_DRIVER := false

TARGET_USES_64_BIT_BCMDHD := false
TARGET_USES_64_BIT_BINDER := true

# HACK: Build apps as 64b for volantis_64_only
ifneq (,$(filter ro.zygote=zygote64, $(PRODUCT_DEFAULT_PROPERTY_OVERRIDES)))
TARGET_PREFER_32_BIT_APPS :=
TARGET_SUPPORTS_64_BIT_APPS := true
endif

# Sensors
BOARD_SENSOR_ST := true
BOARD_SENSOR_MPU_PAD := false

BOARD_USES_GENERIC_INVENSENSE := false


ifneq ($(filter %box, $(TARGET_PRODUCT)), )
TARGET_BOARD_PLATFORM_PRODUCT ?= box
else
 ifneq ($(filter %vr, $(TARGET_PRODUCT)), )
   TARGET_BOARD_PLATFORM_PRODUCT ?= vr
else
TARGET_BOARD_PLATFORM_PRODUCT ?= tablet
endif
endif

ENABLE_CPUSETS := true

# Enable Dex compile opt as default
WITH_DEXPREOPT := true

BOARD_NFC_SUPPORT := false
BOARD_HAS_GPS := false

BOARD_GRAVITY_SENSOR_SUPPORT := true
BOARD_COMPASS_SENSOR_SUPPORT := false
BOARD_GYROSCOPE_SENSOR_SUPPORT := false
BOARD_PROXIMITY_SENSOR_SUPPORT := false
BOARD_LIGHT_SENSOR_SUPPORT := false
BOARD_PRESSURE_SENSOR_SUPPORT := false
BOARD_TEMPERATURE_SENSOR_SUPPORT := false
BOARD_USB_HOST_SUPPORT := true

# Enable optee service
PRODUCT_HAVE_OPTEE ?= true
BOARD_USE_SPARSE_SYSTEM_IMAGE := true

# Google Service and frp overlay
BUILD_WITH_GOOGLE_MARKET := false
BUILD_WITH_GOOGLE_MARKET_ALL := false
BUILD_WITH_GOOGLE_FRP := false
BUILD_WITH_GOOGLE_GMS_EXPRESS := false

# Add widevine L3 support
BOARD_WIDEVINE_OEMCRYPTO_LEVEL := 3

# camera enable
BOARD_CAMERA_SUPPORT := true
BOARD_CAMERA_SUPPORT_EXT := true
ALLOW_MISSING_DEPENDENCIES=true

# enable SVELTE malloc
MALLOC_SVELTE := true

#Config omx to support codec type.
BOARD_SUPPORT_HEVC := true
BOARD_SUPPORT_VP9 := false
BOARD_SUPPORT_VP6 := false
BOARD_MEMTRACK_SUPPORT := true

#for camera autofocus support
CAMERA_SUPPORT_AUTOFOCUS=true

# ANDROID HDMI
BOARD_SHOW_HDMI_SETTING := true

# for ethernet
BOARD_HS_ETHERNET := true
# Allow deprecated BUILD_ module types to get DDK building
BUILD_BROKEN_USES_BUILD_COPY_HEADERS := true
BUILD_BROKEN_USES_BUILD_HOST_EXECUTABLE := true
BUILD_BROKEN_USES_BUILD_HOST_SHARED_LIBRARY := true
BUILD_BROKEN_USES_BUILD_HOST_STATIC_LIBRARY := true
