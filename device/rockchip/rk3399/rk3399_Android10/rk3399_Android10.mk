#
# Copyright 2014 The Android Open-Source Project
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

# First lunching is Q, api_level is 29
PRODUCT_SHIPPING_API_LEVEL := 29
PRODUCT_FSTAB_TEMPLATE := $(LOCAL_PATH)/fstab.in
PRODUCT_DTBO_TEMPLATE := $(LOCAL_PATH)/dt-overlay.in
PRODUCT_BOOT_DEVICE := fe330000.sdhci
include device/rockchip/common/build/rockchip/DynamicPartitions.mk
include device/rockchip/rk3399/rk3399_Android10/BoardConfig.mk
include device/rockchip/common/BoardConfig.mk
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
# Inherit from those products. Most specific first.
$(call inherit-product, device/rockchip/rk3399/device.mk)
$(call inherit-product, device/rockchip/common/device.mk)

#enable this for support f2fs with data partion
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := ext4

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_NAME := rk3399_Android10
PRODUCT_DEVICE := rk3399_Android10
PRODUCT_BRAND := rockchip
PRODUCT_MODEL := rk3399-Android10
PRODUCT_MANUFACTURER := rockchip
PRODUCT_AAPT_PREF_CONFIG := hdpi

PRODUCT_PACKAGES += \
    SoundRecorder

# Get the long list of APNs
PRODUCT_COPY_FILES += vendor/rockchip/common/phone/etc/apns-full-conf.xml:system/etc/apns-conf.xml
PRODUCT_COPY_FILES += vendor/rockchip/common/phone/etc/spn-conf.xml:system/etc/spn-conf.xml
PRODUCT_PROPERTY_OVERRIDES += \
    ro.product.version = 1.0.0 \
    ro.product.ota.host = www.rockchip.com:2300 \
    ro.sf.lcd_density=280

#PRODUCT_HAVE_OPTEE := true
