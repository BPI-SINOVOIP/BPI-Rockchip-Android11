#
# Copyright 2019 The Android Open Source Project
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

DEVICE_FRAMEWORK_MANIFEST_FILE += device/google_car/coral_car/manifest.xml

#
# All components inherited here go to system image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/mainline_system.mk)

# mainline_system.mk sets 'PRODUCT_ENFORCE_RRO_TARGETS := *'
# but this breaks coral_car. So undo it here.
PRODUCT_ENFORCE_RRO_TARGETS :=

# Enable mainline checking
PRODUCT_ENFORCE_ARTIFACT_PATH_REQUIREMENTS :=


#
# All components inherited here go to system_ext image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/handheld_system_ext.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony_system_ext.mk)

#
# All components inherited here go to product image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_product.mk)

# Auto modules
PRODUCT_PACKAGES += \
            android.hardware.broadcastradio@2.0-service \
            android.hardware.automotive.vehicle@2.0-service

# Additional selinux policy
BOARD_SEPOLICY_DIRS += device/google_car/common/sepolicy

# Car init.rc
PRODUCT_COPY_FILES += \
            packages/services/Car/car_product/init/init.bootstat.rc:root/init.bootstat.rc \
            packages/services/Car/car_product/init/init.car.rc:root/init.car.rc

# Override heap growth limit due to high display density on device
PRODUCT_PROPERTY_OVERRIDES += \
            dalvik.vm.heapgrowthlimit=256m

PRODUCT_PACKAGE_OVERLAYS += device/google_car/coral_car/overlay

# Pre-create users
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    android.car.number_pre_created_users=1 \
    android.car.number_pre_created_guests=1 \
    android.car.user_hal_enabled=true

# Enable landscape
PRODUCT_COPY_FILES += \
            frameworks/native/data/etc/android.hardware.screen.landscape.xml:system/etc/permissions/android.hardware.screen.landscape.xml


TARGET_USES_CAR_FUTURE_FEATURES := true

PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/car_core_hardware.xml:system/etc/permissions/car_core_hardware.xml \
        frameworks/native/data/etc/android.hardware.type.automotive.xml:system/etc/permissions/android.hardware.type.automotive.xml \
        frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
        frameworks/native/data/etc/android.hardware.telephony.cdma.xml:system/etc/permissions/android.hardware.telephony.cdma.xml \
        frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
        frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
        frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
        frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
        frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
        frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
        frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
        frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
        frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
        frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
        frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml

# broadcast radio feature
 PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/android.hardware.broadcastradio.xml:system/etc/permissions/android.hardware.broadcastradio.xml

# EVS v1.1
PRODUCT_PACKAGES += android.automotive.evs.manager@1.1 \
                    android.hardware.automotive.evs@1.1-sample \
                    evs_app
PRODUCT_PRODUCT_PROPERTIES += persist.automotive.evs.mode=0

# Automotive display service
PRODUCT_PACKAGES += android.frameworks.automotive.display@1.0-service

#
# All components inherited here go to vendor image
#
# TODO(b/136525499): move *_vendor.mk into the vendor makefile later
$(call inherit-product, $(SRC_TARGET_DIR)/product/handheld_vendor.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony_vendor.mk)

$(call inherit-product, device/google_car/coral_car/device-coral-car.mk)
$(call inherit-product-if-exists, vendor/google_devices/coral/proprietary/device-vendor.mk)
$(call inherit-product-if-exists, vendor/google_devices/coral/prebuilts/device-vendor-coral.mk)

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml \

# Don't build super.img.
PRODUCT_BUILD_SUPER_PARTITION := false

# b/113232673 STOPSHIP deal with Qualcomm stuff later
# PRODUCT_RESTRICT_VENDOR_FILES := all

PRODUCT_MANUFACTURER := Google
PRODUCT_BRAND := Android
PRODUCT_NAME := aosp_coral_car
PRODUCT_DEVICE := coral
PRODUCT_MODEL := AOSP on coral
