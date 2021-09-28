#
# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
PRODUCT_IS_ATV_SDK := true

# ATV SDK is not designed to have a camera by default
PRODUCT_SUPPORTS_CAMERA ?= false

QEMU_USE_SYSTEM_EXT_PARTITIONS := true

$(call inherit-product, device/google/atv/products/aosp_tv_x86.mk)

# Define the host tools and libs that are parts of the SDK.
$(call inherit-product, sdk/build/product_sdk.mk)
$(call inherit-product, development/build/product_sdk.mk)

# keep this apk for sdk targets for now
PRODUCT_PACKAGES += \
    EmulatorSmokeTests

# Overrides
PRODUCT_BRAND := Android
PRODUCT_NAME := sdk_atv_x86
PRODUCT_DEVICE := generic_x86
