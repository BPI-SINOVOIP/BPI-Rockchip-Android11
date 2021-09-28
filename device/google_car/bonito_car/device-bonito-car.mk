#
# Copyright 2020 The Android Open Source Project
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

PRODUCT_HARDWARE := bonito

$(call inherit-product, packages/services/Car/car_product/build/car.mk)
include device/google_car/bonito_car/device-common.mk
include packages/services/Car/computepipe/products/computepipe.mk

PRODUCT_COPY_FILES += \
    device/google/bonito/init.insmod.bonito.cfg:$(TARGET_COPY_OUT_VENDOR)/etc/init.insmod.cfg

DEVICE_PACKAGE_OVERLAYS += device/google/bonito/bonito/overlay

# Audio XMLs
PRODUCT_COPY_FILES += \
    device/google/bonito/mixer_paths_intcodec_b4.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_paths_intcodec_b4.xml \
    device/google/bonito/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    device/google/bonito/audio_platform_info_intcodec_b4.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_platform_info_intcodec_b4.xml

PRODUCT_COPY_FILES += \
    device/google/bonito/nfc/libnfc-nxp.bonito.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-nxp.conf

PRODUCT_PACKAGES += \
    NoCutoutOverlay

# TODO: property below is set on other _car projects, but it doesn't seem to be
# needed - looks like 250 is already the default value
# PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=250
