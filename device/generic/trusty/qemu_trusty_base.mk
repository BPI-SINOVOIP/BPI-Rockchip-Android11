#
# Copyright (C) 2018-2019 The Android Open Source Project
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

# This file contains the definitions needed for a _really_ minimal system
# image to be run under emulation under upstream QEMU (www.qemu.org), once
# it supports a few Android virtual devices. Note that this is _not_ the
# same as running under the Android emulator.

PRODUCT_SOONG_NAMESPACES += device/generic/goldfish

PRODUCT_PACKAGES += \
    com.android.adbd \
    android.hidl.allocator@1.0-service \
    apexd \
    com.android.art \
    com.android.i18n \
    com.android.runtime \
    dhcpclient \
    gatekeeperd \
    hwservicemanager \
    init_system \
    init_vendor \
    init.environ.rc \
    libc.bootstrap \
    libdl.bootstrap \
    libdl_android.bootstrap \
    libm.bootstrap \
    linker \
    linker64 \
    logcat \
    logd \
    logwrapper \
    mdnsd \
    reboot \
    servicemanager \
    sh \
    su \
    toolbox \
    toybox \
    vndservicemanager \
    vold \

# VINTF stuff for system and vendor (no product / odm / system_ext / etc.)
PRODUCT_PACKAGES += \
    system_compatibility_matrix.xml \
    system_manifest.xml \
    vendor_compatibility_matrix.xml \
    vendor_manifest.xml \

# Ensure boringssl NIAP check won't reboot us
PRODUCT_PACKAGES += \
    com.android.conscrypt \
    boringssl_self_test \

# SELinux packages are added as dependencies of the selinux_policy
# phony package.
PRODUCT_PACKAGES += \
    selinux_policy \

PRODUCT_HOST_PACKAGES += \
    adb \
    e2fsdroid \
    make_f2fs \
    mdnsd \
    mke2fs \
    sload_f2fs \
    toybox \

PRODUCT_COPY_FILES += \
    system/core/rootdir/init.usb.rc:system/etc/init/hw/init.usb.rc \
    system/core/rootdir/init.usb.configfs.rc:system/etc/init/hw/init.usb.configfs.rc \
    system/core/rootdir/etc/hosts:system/etc/hosts \

PRODUCT_FULL_TREBLE_OVERRIDE := true

PRODUCT_COPY_FILES += \
    device/generic/trusty/fstab.ranchu:root/fstab.qemu_trusty \
    device/generic/trusty/init.qemu_trusty.rc:root/init.qemu_trusty.rc \
    device/generic/trusty/ueventd.qemu_trusty.rc:root/ueventd.qemu_trusty.rc \

PRODUCT_COPY_FILES += \
    device/generic/goldfish/data/etc/config.ini:config.ini \
    device/generic/trusty/advancedFeatures.ini:advancedFeatures.ini \

# for Trusty
$(call inherit-product, system/core/trusty/trusty-base.mk)
$(call inherit-product, system/core/trusty/trusty-storage.mk)

# Test Utilities
PRODUCT_PACKAGES += \
    tipc-test \
    trusty-ut-ctrl \
    VtsHalGatekeeperV1_0TargetTest \
    VtsHalKeymasterV3_0TargetTest \
    VtsHalKeymasterV4_0TargetTest \

PRODUCT_BOOT_JARS := \
    $(ART_APEX_JARS) \
    ext \
    framework-minus-apex \
    telephony-common \
    voip-common \
    ims-common \
    android.test.base \

PRODUCT_UPDATABLE_BOOT_JARS := \
    com.android.conscrypt:conscrypt \
    com.android.tethering:framework-tethering \

