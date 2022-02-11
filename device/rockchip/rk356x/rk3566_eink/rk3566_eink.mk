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

# First lunching is R, api_level is 30
PRODUCT_SHIPPING_API_LEVEL := 30
PRODUCT_DTBO_TEMPLATE := $(LOCAL_PATH)/dt-overlay.in
PRODUCT_SDMMC_DEVICE := fe2b0000.dwmmc

include device/rockchip/common/build/rockchip/DynamicPartitions.mk
include device/rockchip/rk356x/rk3566_eink/BoardConfig.mk
include device/rockchip/common/BoardConfig.mk

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/wake_lock_filter.xml:system/etc/wake_lock_filter.xml \
    $(LOCAL_PATH)/init.rk356x.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk356x.rc

$(call inherit-product, device/rockchip/rk356x/device.mk)
$(call inherit-product, device/rockchip/common/device.mk)
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

BOARD_PLAT_PRIVATE_SEPOLICY_DIR += device/rockchip/rk356x/sepolicy_ebook_system
BOARD_SEPOLICY_DIRS += device/rockchip/rk356x/sepolicy_ebook

DEVICE_PACKAGE_OVERLAYS := $(LOCAL_PATH)/overlay device/rockchip/common/overlay

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_NAME := rk3566_eink
PRODUCT_DEVICE := rk3566_eink
PRODUCT_BRAND := rockchip
PRODUCT_MODEL := rk3566_eink
PRODUCT_MANUFACTURER := rockchip
PRODUCT_AAPT_PREF_CONFIG := mdpi

PRODUCT_KERNEL_CONFIG += rk356x_eink.config

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/eink_logo/poweroff_logo/poweroff.png:$(TARGET_COPY_OUT_VENDOR)/media/poweroff.png \
    $(LOCAL_PATH)/eink_logo/poweroff_logo/poweroff_nopower.png:$(TARGET_COPY_OUT_VENDOR)/media/poweroff_nopower.png \
    $(LOCAL_PATH)/eink_logo/standby_logo/standby.png:$(TARGET_COPY_OUT_VENDOR)/media/standby.png \
    $(LOCAL_PATH)/eink_logo/standby_logo/standby_lowpower.png:$(TARGET_COPY_OUT_VENDOR)/media/standby_lowpower.png \
    $(LOCAL_PATH)/eink_logo/standby_logo/standby_charge.png:$(TARGET_COPY_OUT_VENDOR)/media/standby_charge.png \
    $(LOCAL_PATH)/eink_logo/android_logo/bootanimation.zip:$(TARGET_COPY_OUT_ODM)/media/bootanimation.zip \
    $(LOCAL_PATH)/android.software.eink.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.eink.xml \
    frameworks/native/data/etc/android.software.picture_in_picture.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.picture_in_picture.xml

PRODUCT_PACKAGES += \
    NoNavigationBarModeGestural \
    NoteDemo

PRODUCT_SYSTEM_EXT_PROPERTIES += ro.lockscreen.disable.default=true

#
## add Rockchip properties
#
PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=320 \
                              ro.vendor.eink=true \
                              sys.eink.mode=7 \
                              sys.eink.rgba2y4_by_rga=1 \
                              persist.sys.idle-wakeup=false \
                              persist.sys.idle-delay=5000 \
                              persist.sys.gamma.level=0.5 \
                              sys.eink.recovery.eink_fb = true
