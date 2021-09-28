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

PRODUCT_PACKAGES += \
    Launcher3QuickStep \
    Settings \
    SystemUI \
    ModuleMetadata \
    frameworks-base-overlays \
    LatinIME \
    Provision \

# Default AOSP sounds
$(call inherit-product-if-exists, frameworks/base/data/sounds/AllAudio.mk)

# Additional settings used in all AOSP builds
PRODUCT_PRODUCT_PROPERTIES += \
    ro.config.ringtone=Ring_Synth_04.ogg \
    ro.config.notification_sound=pixiedust.ogg \
    ro.com.android.dataroaming=true \

# Telephony:
#   Provide a APN configuration to GSI product
PRODUCT_COPY_FILES += \
    device/sample/etc/apns-full-conf.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/apns-conf.xml

# NFC:
#   Provide a libnfc-nci.conf to GSI product
PRODUCT_COPY_FILES += \
    device/generic/common/nfc/libnfc-nci.conf:$(TARGET_COPY_OUT_PRODUCT)/etc/libnfc-nci.conf
