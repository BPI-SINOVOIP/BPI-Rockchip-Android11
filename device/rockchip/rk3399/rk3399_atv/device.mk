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
    libcrypto_vendor.vendor

#$_rbox_$_modify_$_zhengyang: add displayd
PRODUCT_PACKAGES += \
    displayd \
    libion

#enable this for support f2fs with data partion
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs

# This ensures the needed build tools are available.
# TODO: make non-linux builds happy with external/f2fs-tool; system/extras/f2fs_utils
ifeq ($(HOST_OS),linux)
  TARGET_USERIMAGES_USE_F2FS := true
endif

PRODUCT_COPY_FILES += \
    device/rockchip/rk3399/init.recovery.rk30board.rc:recovery/root/init.recovery.rk30board.rc \
    vendor/rockchip/common/bin/$(TARGET_ARCH)/busybox:recovery/root/sbin/busybox \

PRODUCT_COPY_FILES += \
    device/rockchip/rk3399/init.rk3399.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk3399.rc \
    device/rockchip/rk3399/wake_lock_filter.xml:system/etc/wake_lock_filter.xml \
    device/rockchip/rk3399/package_performance.xml:$(TARGET_COPY_OUT_ODM)/etc/package_performance.xml \
    device/rockchip/$(TARGET_BOARD_PLATFORM)/media_profiles_default.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml


# Default integrate MediaCenter
PRODUCT_PACKAGES += \
    MediaCenter


PRODUCT_PACKAGE_OVERLAYS += device/rockchip/rk3399/rk3399_atv/overlay

PRODUCT_COPY_FILES += \
    $(TARGET_DEVICE_DIR)/110b0030_pwm.kl:system/usr/keylayout/110b0030_pwm.kl \
    $(TARGET_DEVICE_DIR)/ff1b0030_pwm.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/ff1b0030_pwm.kl \
    $(TARGET_DEVICE_DIR)/ff1b0030_pwm.idc:$(TARGET_COPY_OUT_VENDOR)/usr/idc/ff1b0030_pwm.idc \
    $(TARGET_DEVICE_DIR)/HiRemote.kl:system/usr/keylayout/HiRemote.kl \
    $(TARGET_DEVICE_DIR)/HiRemote.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/HiRemote.kl \
    $(TARGET_DEVICE_DIR)/HiRemote.idc:$(TARGET_COPY_OUT_VENDOR)/usr/idc/HiRemote.idc \
    $(TARGET_DEVICE_DIR)/virtual-remote.idc:$(TARGET_COPY_OUT_VENDOR)/usr/idc/virtual-remote.idc
    
PRODUCT_COPY_FILES += \
    $(TARGET_DEVICE_DIR)/permissions/tv_core_hardware_3399.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/tv_core_hardware_3399.xml \
    frameworks/native/data/etc/android.hardware.gamepad.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.gamepad.xml

ifeq ($(BOARD_CAMERA_SUPPORT_EXT),true)
PRODUCT_COPY_FILES += \
	device/rockchip/$(TARGET_BOARD_PLATFORM)/external_camera_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/external_camera_config.xml \
	frameworks/native/data/etc/android.hardware.camera.external.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.external.xml

PRODUCT_PACKAGES += \
     android.hardware.camera.provider@2.4-external-service
endif
# setup dalvik vm configs.
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)
$(call inherit-product-if-exists, vendor/rockchip/rk3399/device-vendor.mk)
#for enable optee support
ifeq ($(strip $(PRODUCT_HAVE_OPTEE)),true)
#Choose TEE storage type
#auto (storage type decide by storage chip emmc:rpmb nand:rkss)
#rpmb
#rkss
PRODUCT_PROPERTY_OVERRIDES += ro.tee.storage=rkss
PRODUCT_COPY_FILES += \
       device/rockchip/common/init.optee_verify.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.optee.rc
endif
PRODUCT_COPY_FILES += \
    device/rockchip/rk3399/public.libraries.txt:vendor/etc/public.libraries.txt


#fireware for dp
PRODUCT_COPY_FILES += \
    device/rockchip/rk3399/dptx.bin:root/lib/firmware/rockchip/dptx.bin


