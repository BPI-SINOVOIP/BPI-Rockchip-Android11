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

TARGET_BOARD_PLATFORM_PRODUCT := box

# First lunching is R, api_level is 30
PRODUCT_SHIPPING_API_LEVEL := 30
PRODUCT_DTBO_TEMPLATE := $(LOCAL_PATH)/dt-overlay.in
PRODUCT_SDMMC_DEVICE := fe2b0000.dwmmc

include device/rockchip/common/build/rockchip/DynamicPartitions.mk
include device/rockchip/rk356x_box/bananapi_r2pro_box/BoardConfig.mk
include device/rockchip/common/BoardConfig.mk
$(call inherit-product, device/rockchip/rk356x_box/device.mk)
$(call inherit-product, device/rockchip/common/device.mk)
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

#DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/../overlay
DEVICE_MANIFEST_FILE := device/rockchip/rk356x_box/bananapi_r2pro_box/manifest.xml

PRODUCT_FSTAB_TEMPLATE := device/rockchip/rk356x_box/fstab_box.in

PRODUCT_CHARACTERISTICS := tv

PRODUCT_NAME := bananapi_r2pro_box
PRODUCT_DEVICE := bananapi_r2pro_box
PRODUCT_BRAND := Bananapi
PRODUCT_MODEL := bananapi_r2pro_box
PRODUCT_MANUFACTURER := Sinovoip
PRODUCT_AAPT_PREF_CONFIG := tvdpi
#
## add Rockchip properties
#
PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=213

# TV Input HAL
PRODUCT_PACKAGES += \
    android.hardware.tv.input@1.0-impl

# Display
TARGET_BASE_PARAMETER_IMAGE := device/rockchip/rk356x_box/etc/baseparameter_auto.img

# Disable bluetooth because of continuous driver crashes
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += config.disable_bluetooth=true

# tmp compile needed
BOARD_WITH_RKTOOLBOX := false

# Google TV Service and frp overlay
PRODUCT_USE_PREBUILT_GTVS := no
BUILD_WITH_GOOGLE_FRP := false

BOARD_WITH_SPECIAL_PARTITIONS := baseparameter:1M,logo:16M

# Get the long list of APNs
PRODUCT_COPY_FILES += vendor/rockchip/common/phone/etc/apns-full-conf.xml:system/etc/apns-conf.xml
PRODUCT_COPY_FILES += vendor/rockchip/common/phone/etc/spn-conf.xml:system/etc/spn-conf.xml

PRODUCT_PACKAGES += \
    RKTvLauncher \
    libcrypto_vendor.vendor \
    displayd \
    libion \
    MediaCenter \
    RockchipPinnerService

PRODUCT_PACKAGES += \
    android.hardware.memtrack@1.0-service \
    android.hardware.memtrack@1.0-impl \
    memtrack.$(TARGET_BOARD_PLATFORM)

PRODUCT_PACKAGE_OVERLAYS += device/rockchip/rk356x_box/bananapi_r2pro_box/overlay

# GTVS add the Client ID (provided by Google)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.com.google.clientidbase=android-rockchip-tv

# copy input keylayout and device config
PRODUCT_COPY_FILES += \
    device/rockchip/rk356x_box/remote_config/fdd70030_pwm.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/fdd70030_pwm.kl \

# Vendor seccomp policy files for media components:
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/seccomp_policy/mediacodec.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediacodec.policy

PRODUCT_COPY_FILES += \
    frameworks/av/media/libeffects/data/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml \

BOARD_HS_ETHERNET := true

#
#add Rockchip properties here
#
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.machinetype=356x_box \
    wifi.interface=wlan0 \
    ro.audio.monitorOrientation=true \
    persist.vendor.rk_vulkan=true \
    sf.power.control=2073600 \
    ro.tether.denied=false \
    sys.resolution.changed=false \
    ro.product.usbfactory=rockchip_usb \
    wifi.supplicant_scan_interval=15 \
    ro.kernel.android.checkjni=0 \
    ro.vendor.nrdp.modelgroup=NEXUSPLAYERFUGU \
    vendor.hwc.device.primary=HDMI-A,TV \
    persist.vendor.framebuffer.main=1920x1080@60 \
    ro.vendor.sdkversion=RK356x_ANDROID11.0_BOX_V1.0.8 \


PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.opengles.version=131072 \
    ro.hwui.drop_shadow_cache_size=4.0 \
    ro.hwui.gradient_cache_size=0.8 \
    ro.hwui.layer_cache_size=32.0 \
    ro.hwui.path_cache_size=24.0 \
    ro.hwui.text_large_cache_width=2048 \
    ro.hwui.text_large_cache_height=1024 \
    ro.hwui.text_small_cache_width=1024 \
    ro.hwui.text_small_cache_height=512 \
    ro.hwui.texture_cache_flushrate=0.4 \
    ro.hwui.texture_cache_size=72.0 \
    debug.hwui.use_partial_updates=false

