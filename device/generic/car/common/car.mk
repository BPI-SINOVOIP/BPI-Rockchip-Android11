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
#

# Auto modules
# TODO: Add broadcastradio@.2.0 back once it's stable b/145694104
PRODUCT_PACKAGES += \
    android.hardware.automotive.vehicle@2.0-service \
    android.hardware.automotive.audiocontrol@2.0-service \
    android.frameworks.automotive.display@1.0-service \

# Emulator configuration
PRODUCT_COPY_FILES += \
    device/generic/car/common/config.ini:config.ini

# Car init.rc
PRODUCT_COPY_FILES += \
    packages/services/Car/car_product/init/init.bootstat.rc:root/init.bootstat.rc \
    packages/services/Car/car_product/init/init.car.rc:root/init.car.rc

# Copy car_core_hardware and overwrite handheld_core_hardware.xml with a disable config.
# Overwrite goldfish related xml with a disable config.
PRODUCT_COPY_FILES += \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml \
    device/generic/car/common/car_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/car_core_hardware.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.ar.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.autofocus.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.concurrent.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.full.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.any.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.raw.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml \
    device/generic/car/common/android.hardware.disable.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml \

# Overwrite goldfish fstab.ranchu to turn off adoptable_storage
PRODUCT_COPY_FILES += \
    device/generic/car/common/fstab.ranchu.car:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.ranchu

# Enable landscape
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.screen.landscape.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.screen.landscape.xml

# Used to embed a map in an activity view
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.activities_on_secondary_displays.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.activities_on_secondary_displays.xml

# Permission for Wi-Fi passpoint support
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.wifi.passpoint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.passpoint.xml

# Additional permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.broadcastradio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.broadcastradio.xml \
    frameworks/native/data/etc/android.hardware.type.automotive.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.type.automotive.xml \

# Sensor features
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.gyroscope.xml \

# Copy APN configs
PRODUCT_COPY_FILES += \
    device/generic/goldfish/data/etc/apns-conf.xml:system/etc/apns-conf.xml \
    device/sample/etc/old-apns-conf.xml:system/etc/old-apns-conf.xml

# Whitelisted packages per user type
PRODUCT_COPY_FILES += \
  device/generic/car/common/preinstalled-packages-product-car-emulator.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/sysconfig/preinstalled-packages-product-car-emulator.xml

# Multi-user properties
PRODUCT_SYSTEM_DEFAULT_PROPERTIES := \
    android.car.number_pre_created_users=1 \
    android.car.number_pre_created_guests=1 \
    android.car.user_hal_enabled=true

# Additional selinux policy
BOARD_SEPOLICY_DIRS += device/generic/car/common/sepolicy

#
# Special settings for GSI releasing
#
ifneq (,$(filter aosp_car_x86_64 aosp_car_arm64,$(TARGET_PRODUCT)))
$(call inherit-product, $(SRC_TARGET_DIR)/product/gsi_release.mk)
endif

$(call inherit-product, packages/services/Car/evs/sepolicy/evs.mk)
$(call inherit-product, packages/services/Car/car_product/build/car.mk)
