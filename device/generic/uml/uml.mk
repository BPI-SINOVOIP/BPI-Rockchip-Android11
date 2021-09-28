# Copyright (C) 2017 The Android Open Source Project
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

$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)

PRODUCT_NAME := uml
PRODUCT_DEVICE := uml
PRODUCT_BRAND := Android
PRODUCT_MODEL := UML for x86_64

# default is nosdcard, S/W button enabled in resource
DEVICE_PACKAGE_OVERLAYS := device/generic/x86/overlay
PRODUCT_CHARACTERISTICS := nosdcard

PRODUCT_COPY_FILES += $(LOCAL_PATH)/fstab.uml:root/fstab.uml
PRODUCT_COPY_FILES += $(LOCAL_PATH)/init.uml.rc:root/init.uml.rc
PRODUCT_COPY_FILES += $(LOCAL_PATH)/init.eth0.sh:system/bin/init.eth0.sh

PRODUCT_PACKAGES += \
    adbd \
    adbd.recovery \
    usbd \
    android.hardware.configstore@1.1-service \
    android.hidl.allocator@1.0-service \
    android.hidl.memory@1.0-impl \
    android.hidl.memory@1.0-impl.vendor \
    atrace \
    blank_screen \
    bootanimation \
    bootstat \
    charger \
    cmd \
    crash_dump \
    debuggerd\
    dumpstate \
    dumpsys \
    gralloc.default \
    healthd \
    hwservicemanager \
    init \
    init.environ.rc \
    libEGL \
    libETC1 \
    libFFTEm \
    libGLESv1_CM \
    libGLESv2 \
    libGLESv3 \
    libbinder \
    libc \
    libcutils \
    libdl \
    libgui \
    libhardware \
    libhardware_legacy \
    libjpeg \
    liblog \
    libm \
    libpower \
    libstdc++ \
    libsurfaceflinger \
    libsysutils \
    libui \
    libutils \
    linker \
    linker.recovery \
    lmkd \
    logcat \
    lshal \
    recovery \
    service \
    servicemanager \
    shell_and_utilities \
    storaged \
    thermalserviced \
    tombstoned \
    tzdatacheck \
    vndservice \
    vndservicemanager \

# VINTF stuff for system and vendor (no product / odm / system_ext / etc.)
PRODUCT_PACKAGES += \
    vendor_compatibility_matrix.xml \
    vendor_manifest.xml \
    system_manifest.xml \
    system_compatibility_matrix.xml \

# SELinux packages are added as dependencies of the selinux_policy
# phony package.
PRODUCT_PACKAGES += \
    selinux_policy \

# AID Generation for
# <pwd.h> and <grp.h>
PRODUCT_PACKAGES += \
    passwd \
    group \
    fs_config_files \
    fs_config_dirs

# If there are product-specific adb keys defined, install them on debuggable
# builds.
PRODUCT_PACKAGES_DEBUG += \
    adb_keys

# Ensure that this property is always defined so that bionic_systrace.cpp
# can rely on it being initially set by init.
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    debug.atrace.tags.enableflags=0

PRODUCT_COPY_FILES += \
    system/core/rootdir/init.usb.rc:system/etc/init/hw/init.usb.rc \
    system/core/rootdir/init.usb.configfs.rc:system/etc/init/hw/init.usb.configfs.rc \
    system/core/rootdir/etc/hosts:system/etc/hosts

PRODUCT_HOST_PACKAGES += \
    adb \
    e2fsdroid \
    fastboot \
    make_f2fs \
    mke2fs \
    tzdatacheck \

