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

DEVICE_MANIFEST_FILE := device/rockchip/rk3328/manifest.xml

#overlay config
ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT), box)
PRODUCT_PACKAGES += \
    RKTvLauncher
PRODUCT_PACKAGE_OVERLAYS += device/rockchip/rk3328/rk3328_box/overlay
else ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT), atv)
	PRODUCT_PACKAGE_OVERLAYS += device/rockchip/rk3328/rk3328_atv/overlay
else
PRODUCT_PACKAGE_OVERLAYS += device/rockchip/rk3328/overlay
endif

PRODUCT_PACKAGES += \
    libcrypto_vendor.vendor

PRODUCT_PACKAGES += \
    displayd \
    libion \
    memtrack.$(TARGET_BOARD_PLATFORM)

# Default integrate MediaCenter
PRODUCT_PACKAGES += \
    MediaCenter

#enable this for support f2fs with data partion
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs

#box fstab in template
PRODUCT_FSTAB_TEMPLATE := device/rockchip/rk3328/fstab.in

#sdmmc device config
PRODUCT_SDMMC_DEVICE := ff500000.dwmmc

# This ensures the needed build tools are available.
# TODO: make non-linux builds happy with external/f2fs-tool; system/extras/f2fs_utils
ifeq ($(HOST_OS),linux)
  TARGET_USERIMAGES_USE_F2FS := true
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.recovery.rk30board.rc:recovery/root/init.recovery.rk30board.rc \
    vendor/rockchip/common/bin/$(TARGET_ARCH)/busybox:recovery/root/sbin/busybox

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rk3328.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk3328.rc \
    $(LOCAL_PATH)/init.rk3328.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk3328.usb.rc \
    $(LOCAL_PATH)/wake_lock_filter.xml:system/etc/wake_lock_filter.xml \
    device/rockchip/rk3328/package_performance.xml:$(TARGET_COPY_OUT_ODM)/etc/package_performance.xml \
    device/rockchip/$(TARGET_BOARD_PLATFORM)/external_camera_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/external_camera_config.xml \
   $(TARGET_DEVICE_DIR)/media_profiles_default.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml

# copy input keylayout and device config
ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT), box)
PRODUCT_COPY_FILES += \
    device/rockchip/rk3328/remote_config/ff1b0030_pwm.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/ff1b0030_pwm.kl

else
PRODUCT_COPY_FILES += \
    device/rockchip/rk3328/remote_config/110b0030_pwm.kl:system/usr/keylayout/110b0030_pwm.kl \
    device/rockchip/rk3328/remote_config/ff1b0030_pwm.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/ff1b0030_pwm.kl \
    device/rockchip/rk3328/remote_config/ff1b0030_pwm.idc:$(TARGET_COPY_OUT_VENDOR)/usr/idc/ff1b0030_pwm.idc \
    device/rockchip/rk3328/remote_config/HiRemote.kl:system/usr/keylayout/HiRemote.kl \
    device/rockchip/rk3328/remote_config/HiRemote.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/HiRemote.kl \
    device/rockchip/rk3328/remote_config/HiRemote.idc:$(TARGET_COPY_OUT_VENDOR)/usr/idc/HiRemote.idc \
    device/rockchip/rk3328/remote_config/virtual-remote.idc:$(TARGET_COPY_OUT_VENDOR)/usr/idc/virtual-remote.idc
endif


#
## setup boot-shutdown animation configs.
#
HAVE_BOOT_ANIMATION := $(shell test -f $(TARGET_DEVICE_DIR)/bootanimation.zip && echo true)
HAVE_SHUTDOWN_ANIMATION := $(shell test -f $(TARGET_DEVICE_DIR)/shutdownanimation.zip && echo true)

ifeq ($(HAVE_BOOT_ANIMATION), true)
PRODUCT_COPY_FILES += $(TARGET_DEVICE_DIR)/bootanimation.zip:$(TARGET_COPY_OUT_ODM)/media/bootanimation.zip
endif

ifeq ($(HAVE_SHUTDOWN_ANIMATION), true)
PRODUCT_COPY_FILES += $(TARGET_DEVICE_DIR)/shutdownanimation.zip:$(TARGET_COPY_OUT_ODM)/media/shutdownanimation.zip
endif

# setup dalvik vm configs.
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)


$(call inherit-product-if-exists, vendor/rockchip/rk3328/device-vendor.mk)

#for enable optee support
ifeq ($(strip $(PRODUCT_HAVE_OPTEE)),true)

PRODUCT_COPY_FILES += \
       device/rockchip/common/init.optee_verify.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.optee.rc
endif

#tv_core_hardware_3328
ifeq ($(strip $(TARGET_PRODUCT)),rk3328_atv)
PRODUCT_COPY_FILES += \
    device/rockchip/rk3328/permissions/tv_core_hardware_3328.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/tv_core_hardware_3328.xml \
    frameworks/native/data/etc/android.hardware.gamepad.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.gamepad.xml
endif

PRODUCT_PACKAGES += \
    android.hardware.memtrack@1.0-service \
    android.hardware.memtrack@1.0-impl \
    memtrack.$(TARGET_BOARD_PLATFORM)

#
#add Rockchip properties here
#
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    ro.audio.monitorOrientation=true \
    vendor.hwc.compose_policy=6 \
    persist.vendor.rk_vulkan=true \
    sf.power.control=2073600 \
    ro.tether.denied=false \
    sys.resolution.changed=false \
    ro.product.usbfactory=rockchip_usb \
    wifi.supplicant_scan_interval=15 \
    ro.kernel.android.checkjni=0 \
    ro.vendor.nrdp.modelgroup=NEXUSPLAYERFUGU \
    vendor.hwc.device.primary=HDMI-A,TV \
    ro.vendor.sdkversion=RK3328_ANDROID10.0_BOX_V1.0.6 \
    vendor.gralloc.no_afbc_for_fb_target_layer=1

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

# GTVS add the Client ID (provided by Google)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.com.google.clientidbase=android-rockchip-tv

# Vendor seccomp policy files for media components:
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/seccomp_policy/mediacodec.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediacodec.policy

PRODUCT_COPY_FILES += \
    frameworks/av/media/libeffects/data/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml

