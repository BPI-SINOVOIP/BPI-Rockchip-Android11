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

PRODUCT_HARDWARE := blueline

$(call inherit-product, packages/services/Car/car_product/build/car.mk)
include device/google_car/blueline_car/device-common.mk

PRODUCT_COPY_FILES += \
    device/google/crosshatch/init.insmod.blueline.cfg:$(TARGET_COPY_OUT_VENDOR)/etc/init.insmod.cfg

DEVICE_PACKAGE_OVERLAYS += device/google/crosshatch/blueline/overlay

# Audio XMLs
PRODUCT_COPY_FILES += \
    device/google/crosshatch/mixer_paths_tavil_b1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_paths_tavil_b1.xml \
    device/google/crosshatch/audio_policy_volumes_b1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    device/google/crosshatch/audio_platform_info_tavil_b1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_platform_info_tavil_b1.xml

PRODUCT_COPY_FILES += \
    device/google/crosshatch/nfc/libnfc-nxp.blueline.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-nxp.conf

PRODUCT_PACKAGES += \
    NoCutoutOverlay

PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=250
