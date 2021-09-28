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
-include vendor/rockchip/rk3326/BoardConfigVendor.mk

CURRENT_SDK_VERSION := RK3326_ANDROID10.0_MID_V1.0

TARGET_PREBUILT_KERNEL := kernel/arch/arm64/boot/Image
BOARD_PREBUILT_DTBIMAGE_DIR := kernel/arch/arm64/boot/dts/rockchip

PRODUCT_UBOOT_CONFIG ?= rk3326
PRODUCT_KERNEL_ARCH ?= arm64
PRODUCT_KERNEL_DTS ?= rk3326-863-lp3-v10-rkisp1
PRODUCT_KERNEL_CONFIG ?= rockchip_defconfig
#BOARD_AVB_ENABLE := true
SF_PRIMARY_DISPLAY_ORIENTATION := 0

# Disable emulator for "make dist" until there is a 64-bit qemu kernel
BUILD_EMULATOR := false
TARGET_BOARD_PLATFORM := rk3326
TARGET_BOARD_PLATFORM_GPU := mali-tDVx
TARGET_RK_GRALLOC_VERSION := 4
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

# Sensors
BOARD_SENSOR_ST := true
BOARD_SENSOR_MPU_VR := false
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

#for optee support
#PRODUCT_HAVE_OPTEE ?= true
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
ALLOW_MISSING_DEPENDENCIES=true

# Config GO Optimization
BUILD_WITH_GO_OPT := true

# enable SVELTE malloc
MALLOC_SVELTE := true

#Config omx to support codec type.
BOARD_SUPPORT_VP9 := false
BOARD_SUPPORT_VP6 := false
BOARD_MEMTRACK_SUPPORT := true
