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

PRODUCT_PACKAGES += \
    WallpaperPicker \
    Launcher3

PRODUCT_PACKAGES += \
    RockchipPinnerService

#$_rbox_$_modify_$_zhengyang: add displayd
PRODUCT_PACKAGES += \
    displayd \
    libion

# Enable this for support f2fs with data partion
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs

# This ensures the needed build tools are available.
# TODO: make non-linux builds happy with external/f2fs-tool; system/extras/f2fs_utils
ifeq ($(HOST_OS), linux)
  TARGET_USERIMAGES_USE_F2FS := true
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rk3288.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk3288.rc \
    $(LOCAL_PATH)/init.rk3288.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk3288.usb.rc \
    $(LOCAL_PATH)/wake_lock_filter.xml:system/etc/wake_lock_filter.xml \
    device/rockchip/$(TARGET_BOARD_PLATFORM)/package_performance.xml:$(TARGET_COPY_OUT_ODM)/etc/package_performance.xml \
    device/rockchip/$(TARGET_BOARD_PLATFORM)/media_profiles_default.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml

ifeq ($(BOARD_CAMERA_SUPPORT),true)
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.camera.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.xml \
	frameworks/native/data/etc/android.hardware.camera.front.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml

PRODUCT_PACKAGES += \
	librkisp_aec \
	librkisp_awb \
	librkisp_af
endif

ifeq ($(BOARD_CAMERA_SUPPORT_EXT),true)
PRODUCT_COPY_FILES += \
	device/rockchip/$(TARGET_BOARD_PLATFORM)/external_camera_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/external_camera_config.xml \
	frameworks/native/data/etc/android.hardware.camera.external.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.external.xml

PRODUCT_PACKAGES += \
     android.hardware.camera.provider@2.4-external-service
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.recovery.rk30board.rc:recovery/root/init.recovery.rk30board.rc \
    vendor/rockchip/common/bin/$(TARGET_ARCH)/busybox:recovery/root/sbin/busybox \

PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml

ifeq ($(BUILD_WITH_GOOGLE_MARKET),false)
# copy xml files for Vulkan features.
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.vulkan.level-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level-0.xml \
	frameworks/native/data/etc/android.hardware.vulkan.level-1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level-1.xml \
	frameworks/native/data/etc/android.hardware.vulkan.version-1_0_3.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version-1_0_3.xml
endif

# update realtek bluetooth configs
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/bluetooth/rtkbt.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/rtkbt.conf

# setup dalvik vm configs.
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

$(call inherit-product-if-exists, vendor/rockchip/$(TARGET_BOARD_PLATFORM)/device-vendor.mk)

# For enable optee support
ifeq ($(strip $(PRODUCT_HAVE_OPTEE)), true)
PRODUCT_COPY_FILES += \
       device/rockchip/common/init.optee_verify.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.optee.rc

# Choose TEE storage type
PRODUCT_PROPERTY_OVERRIDES += ro.tee.storage=rkss
endif

PRODUCT_COPY_FILES += \
    device/rockchip/rk3288/public.libraries.txt:vendor/etc/public.libraries.txt

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

BOARD_SEPOLICY_DIRS += device/rockchip/rk3288/sepolicy_vendor

#
# add Rockchip properties here
#
PRODUCT_PROPERTY_OVERRIDES += \
                ro.ril.ecclist=112,911 \
                ro.opengles.version=196610 \
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
                ro.sf.hwrotation=0 \
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
                ro.sf.lcd_density=320 \
                ro.build.shutdown_timeout=6 \
                persist.enable_task_snapshots=false \
                ro.adb.secure=0 \
                ro.rk.displayd.enable=false
