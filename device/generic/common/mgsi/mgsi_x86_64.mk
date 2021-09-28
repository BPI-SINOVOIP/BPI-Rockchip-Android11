#
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

#
# All components inherited here go to system image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/mainline_system.mk)

# Enable mainline checking
PRODUCT_ENFORCE_ARTIFACT_PATH_REQUIREMENTS := relaxed

#
# All components inherited here go to product image
#
$(call inherit-product, device/generic/common/mgsi/mgsi_product.mk)

#
# Special settings for GSI releasing
#
$(call inherit-product, device/generic/common/mgsi/mgsi_release.mk)

PRODUCT_BUILD_SUPER_PARTITION := false

PRODUCT_SOONG_NAMESPACES += device/generic/goldfish

PRODUCT_NAME := mgsi_x86_64
PRODUCT_DEVICE := dummy_x86_64
PRODUCT_BRAND := Android
PRODUCT_MODEL := Minimal GSI on x86_64
