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

#$_rbox_$_modify_$_zhengyang: add displayd
PRODUCT_PACKAGES += \
    displayd \
    libion

# enable this for support f2fs with data partion
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs
BOARD_SEPOLICY_DIRS += device/rockchip/rk3326/sepolicy_vendor

# used for fstab_generator, sdmmc controller address
PRODUCT_SDMMC_DEVICE := ff370000.dwmmc

# This ensures the needed build tools are available.
# TODO: make non-linux builds happy with external/f2fs-tool; system/extras/f2fs_utils
ifeq ($(HOST_OS),linux)
  TARGET_USERIMAGES_USE_F2FS := true
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.recovery.rk30board.rc:recovery/root/init.recovery.rk30board.rc \
    vendor/rockchip/common/bin/$(TARGET_ARCH)/busybox:recovery/root/sbin/busybox \

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rk3326.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk3326.rc \
    $(LOCAL_PATH)/init.rk3326.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk3326.usb.rc \
    $(LOCAL_PATH)/wake_lock_filter.xml:system/etc/wake_lock_filter.xml \
    device/rockchip/rk3326/package_performance.xml:$(TARGET_COPY_OUT_ODM)/etc/package_performance.xml \
    $(TARGET_DEVICE_DIR)/media_profiles_default.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml\

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml

$(call inherit-product-if-exists, vendor/rockchip/rk3326/device-vendor.mk)

# Reduces GC frequency of foreground apps by 50%
PRODUCT_PROPERTY_OVERRIDES += \
               dalvik.vm.foreground-heap-growth-multiplier=2.0

# Disable partial updates
PRODUCT_PROPERTY_OVERRIDES += \
    debug.hwui.use_partial_updates=false

# Get the long list of APNs
PRODUCT_COPY_FILES += vendor/rockchip/common/phone/etc/apns-full-conf.xml:system/etc/apns-conf.xml
PRODUCT_COPY_FILES += vendor/rockchip/common/phone/etc/spn-conf.xml:system/etc/spn-conf.xml
PRODUCT_PROPERTY_OVERRIDES += \
    ro.product.version = 1.0.0 \
    ro.product.ota.host = www.rockchip.com:2300 \
    ro.vendor.sdkversion = $(CURRENT_SDK_VERSION) \
    vendor.gralloc.no_afbc_for_fb_target_layer = 1

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

#
## setup oem-content configs.
#
HAVE_PRESET_CONTENT := $(shell test -d $(TARGET_DEVICE_DIR)/pre_set && echo true)
HAVE_PRESET_DEL_CONTENT := $(shell test -d $(TARGET_DEVICE_DIR)/pre_set_del && echo true)

ifeq ($(HAVE_PRESET_DEL_CONTENT), true)
PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(TARGET_DEVICE_DIR)/pre_set_del,$(TARGET_COPY_OUT_ODM)/pre_set_del)

PRODUCT_PROPERTY_OVERRIDES += ro.boot.copy_oem=true
endif

ifeq ($(HAVE_PRESET_CONTENT), true)
PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(TARGET_DEVICE_DIR)/pre_set,$(TARGET_COPY_OUT_ODM)/pre_set)

PRODUCT_PROPERTY_OVERRIDES += ro.boot.copy_oem=true
endif

#
# add Rockchip properties here
#
PRODUCT_PROPERTY_OVERRIDES += \
                ro.ril.ecclist=112,911 \
                ro.opengles.version=196610 \
                persist.vendor.rk_vulkan=true \
                wifi.interface=wlan0 \
                rild.libpath=/system/lib/libril-rk29-dataonly.so \
                rild.libargs=-d /dev/ttyACM0 \
                persist.tegra.nvmmlite = 1 \
                ro.audio.monitorOrientation=true \
                debug.nfc.fw_download=false \
                debug.nfc.se=false \
                ro.rk.screenoff_time=60000 \
                ro.rk.screenshot_enable=true \
                ro.rk.def_brightness=200 \
                ro.rk.homepage_base=http://www.google.com/webhp?client={CID}&amp;source=android-home \
                ro.rk.install_non_market_apps=false \
                vendor.hwc.compose_policy=6 \
                sys.wallpaper.rgb565=0 \
                sf.power.control=2073600 \
                sys.rkadb.root=0 \
                ro.sf.fakerotation=false \
                ro.rk.MassStorage=false \
                ro.rk.systembar.voiceicon=true \
                ro.rk.systembar.tabletUI=false \
                ro.rk.LowBatteryBrightness=false \
                ro.tether.denied=false \
                sys.resolution.changed=false \
                ro.default.size=100 \
                ro.product.usbfactory=rockchip_usb \
                wifi.supplicant_scan_interval=15 \
                ro.factory.tool=0 \
                ro.kernel.android.checkjni=0 \
                ro.build.shutdown_timeout=0 \
                persist.enable_task_snapshots=false
