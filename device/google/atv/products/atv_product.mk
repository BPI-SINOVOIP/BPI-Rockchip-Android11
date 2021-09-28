#
# Copyright (C) 2020 The Android Open Source Project
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
# This makefile contains the product partition contents for
# a generic TV device.
$(call inherit-product, $(SRC_TARGET_DIR)/product/media_product.mk)

$(call inherit-product-if-exists, frameworks/base/data/sounds/AudioTv.mk)

PRODUCT_PACKAGES += \
    SettingsIntelligence \
    SystemUI \
    TvSettings

# Do not include the Live Channels app if USE_OEM_TV_APP flag is set.
# The feature com.google.android.tv.installed is used to tell whether a device
# has the pre-installed Live Channels app. This is necessary for the Play Store
# to identify the compatible devices that can install later updates of the app.
ifneq ($(USE_OEM_TV_APP),true)
PRODUCT_PACKAGES += LiveTv

PRODUCT_COPY_FILES += \
    device/google/atv/permissions/com.google.android.tv.installed.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/permissions/com.google.android.tv.installed.xml
endif
