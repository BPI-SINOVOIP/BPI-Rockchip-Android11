#
# Copyright (C) 2020 The Android Open Source Project
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

# The system image of aosp_tv_arm64-userdebug is a GSI for the devices with:
# - ARM 64 bits user space
# - 64 bits binder interface
# - VNDK enforcement
# - compatible property override enabled

#
# All components inherited here go to system image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, device/google/atv/products/atv_mainline_system.mk)

# Enable mainline checking for excat this product name
ifeq (aosp_tv_arm64,$(TARGET_PRODUCT))
PRODUCT_ENFORCE_ARTIFACT_PATH_REQUIREMENTS := relaxed
endif

#
# All components inherited here go to system_ext image
#
$(call inherit-product, device/google/atv/products/atv_system_ext.mk)
# Packages required for ATV GSI
PRODUCT_PACKAGES += \
    FrameworkPackageStubs \
    TvProvision

#
# All components inherited here go to product image
#
$(call inherit-product, device/google/atv/products/atv_product.mk)
# Packages required for ATV GSI
PRODUCT_PACKAGES += \
    LatinIMEGoogleTvPrebuilt \
    TvSampleLeanbackLauncher

#
# All components inherited here go to vendor image
#
$(call inherit-product, device/google/atv/products/atv_emulator_vendor.mk)
$(call inherit-product-if-exists, device/generic/goldfish/arm64-vendor.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/board/generic_arm64/device.mk)

ifeq (aosp_tv_arm64,$(TARGET_PRODUCT))
$(call inherit-product, $(SRC_TARGET_DIR)/product/gsi_release.mk)
endif

PRODUCT_NAME := aosp_tv_arm64
PRODUCT_DEVICE := generic_arm64
PRODUCT_BRAND := Android
PRODUCT_MODEL := AOSP TV on ARM64
