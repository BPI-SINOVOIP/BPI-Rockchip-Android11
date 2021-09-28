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

PRODUCT_SHIPPING_API_LEVEL := 29
PRODUCT_USE_DYNAMIC_PARTITIONS := true
PRODUCT_FULL_TREBLE_OVERRIDE := true
PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS := false

#
# All components inherited here go to system image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/mainline_system.mk)

#
# All components inherited here go to system_ext image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/handheld_system_ext.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony_system_ext.mk)

#
# All components inherited here go to product image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_product.mk)

#
# All components inherited here go to vendor image
#
$(call inherit-product, $(SRC_TARGET_DIR)/product/media_vendor.mk)

PRODUCT_SOONG_NAMESPACES += device/generic/goldfish

PRODUCT_PACKAGES += \
    android.hardware.audio@2.0-service \
    android.hardware.audio@4.0-impl:32 \
    android.hardware.audio.effect@4.0-impl:32 \
    audio.primary.default \
    audio.r_submix.default \
    android.hardware.drm@1.0-service \
    android.hardware.drm@1.0-impl \
    android.hardware.drm@1.3-service.clearkey \
    android.hardware.gatekeeper@1.0-service.software \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.composer@2.1-impl \
    android.hardware.graphics.mapper@2.0-impl \
    android.hardware.health@2.0-service \
    android.hardware.keymaster@4.0-service \
    android.hardware.keymaster@4.0-impl \
    libEGL_swiftshader \
    libGLESv1_CM_swiftshader \
    libGLESv2_swiftshader \

PRODUCT_PACKAGE_OVERLAYS := device/generic/goldfish/overlay

PRODUCT_NAME := fvp
PRODUCT_DEVICE := fvpbase
PRODUCT_BRAND := Android
PRODUCT_MODEL := AOSP on FVP

PRODUCT_COPY_FILES += \
    device/generic/goldfish/data/etc/handheld_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.ethernet.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.ethernet.xml \
    device/generic/goldfish/fvpbase/fstab.fvpbase:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.fvpbase \
    device/generic/goldfish/fvpbase/fstab.fvpbase.initrd:$(TARGET_COPY_OUT_RAMDISK)/fstab.fvpbase \
    device/generic/goldfish/fvpbase/init.fvpbase.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.fvpbase.rc \
    frameworks/av/services/audiopolicy/config/audio_policy_configuration_generic.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/surround_sound_configuration_5_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/surround_sound_configuration_5_0.xml \

PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.hardware.egl=swiftshader \
    debug.sf.nobootanimation=1 \

PRODUCT_REQUIRES_INSECURE_EXECMEM_FOR_SWIFTSHADER := true

# It's almost always faster to dexopt on the host even in eng builds.
WITH_DEXPREOPT_BOOT_IMG_AND_SYSTEM_SERVER_ONLY := false

DEVICE_MANIFEST_FILE := device/generic/goldfish/fvpbase/manifest.xml

# Normally, the bootloader is supposed to concatenate the Android initramfs
# and the initramfs for the kernel modules and let the kernel combine
# them. However, the bootloader that we're using with FVP (U-Boot) doesn't
# support concatenation, so we implement it in the build system.
droid: $(OUT_DIR)/target/product/$(PRODUCT_DEVICE)/boot/ramdisk.img

$(OUT_DIR)/target/product/$(PRODUCT_DEVICE)/boot/ramdisk.img: $(OUT_DIR)/target/product/$(PRODUCT_DEVICE)/ramdisk.img $(OUT_DIR)/target/product/$(PRODUCT_DEVICE)/boot/initramfs.img
	gzip -dc $^ > $@
