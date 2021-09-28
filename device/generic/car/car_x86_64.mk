#
# Copyright (C) 2017 The Android Open Source Project
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

PRODUCT_PACKAGE_OVERLAYS := device/generic/car/common/overlay

$(call inherit-product, device/generic/car/emulator/aosp_car_emulator.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_x86_64.mk)
$(call inherit-product, device/generic/car/emulator/audio/car_emulator_audio.mk)

PRODUCT_SDK_ADDON_NAME := car_emu_x86_64

# Extra files needed by the addon.
PRODUCT_SDK_ADDON_COPY_FILES := \
    device/generic/goldfish/data/etc/encryptionkey.img:images/${TARGET_CPU_ABI}/encryptionkey.img\
    device/generic/car/car_x86_64/manifest.ini:manifest.ini

PRODUCT_SDK_ADDON_COPY_MODULES := \
    com.android.future.usb.accessory:libs/usb.jar

DEVICE_PACKAGE_OVERLAYS += device/generic/car/car_x86_64/overlay

EMULATOR_VENDOR_NO_SOUND := true
PRODUCT_NAME := car_x86_64
PRODUCT_DEVICE := generic_car_x86_64
PRODUCT_BRAND := Google
PRODUCT_MODEL := Car on x86_64 emulator
PRODUCT_SDK_ADDON_NAME := car_x86_64

PRODUCT_SDK_ADDON_SYS_IMG_SOURCE_PROP := \
    device/generic/car/car_x86_64/source.properties
