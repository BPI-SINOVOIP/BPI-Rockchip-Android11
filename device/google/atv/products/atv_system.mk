#
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# This makefile contains the system partition contents for
# a generic TV device.
$(call inherit-product, $(SRC_TARGET_DIR)/product/media_system.mk)

$(call inherit-product-if-exists, frameworks/base/data/fonts/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/dancing-script/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/carrois-gothic-sc/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/coming-soon/fonts.mk)
$(call inherit-product-if-exists, external/google-fonts/cutive-mono/fonts.mk)
$(call inherit-product-if-exists, external/noto-fonts/fonts.mk)
$(call inherit-product-if-exists, external/roboto-fonts/fonts.mk)
$(call inherit-product-if-exists, external/hyphenation-patterns/patterns.mk)
$(call inherit-product-if-exists, frameworks/base/data/keyboards/keyboards.mk)
$(call inherit-product-if-exists, frameworks/webview/chromium/chromium.mk)

PRODUCT_IS_ATV := true

PRODUCT_PACKAGES += \
    TvProvider

# Media tuner jni
PRODUCT_PACKAGES += \
    libmedia_tv_tuner

# From build/target/product/core.mk
PRODUCT_PACKAGES += \
    BasicDreams \
    Bluetooth \
    CalendarProvider \
    CaptivePortalLogin \
    CertInstaller \
    clatd \
    clatd.conf \
    ExternalStorageProvider \
    FusedLocation \
    InputDevices \
    KeyChain \
    librs_jni \
    PacProcessor \
    PrintSpooler \
    ProxyHandler \
    SharedStorageBackup \
    screenrecord \
    UserDictionaryProvider \
    VpnDialogs \
    com.android.media.tv.remoteprovider

# Traceur for debug only
PRODUCT_PACKAGES_DEBUG += \
    Traceur

# PRODUCT_SUPPORTS_CAMERA: Whether the product supports cameras at all
# (built-in or external USB camera). When 'false', we drop cameraserver, which
# saves ~3 MiB of RAM. When 'true', additional settings are required for
# external webcams to work, see "External USB Cameras" documentation.
#
# Defaults to true to mimic legacy behaviour.
PRODUCT_SUPPORTS_CAMERA ?= true
ifeq ($(PRODUCT_SUPPORTS_CAMERA),true)
    PRODUCT_PACKAGES += cameraserver
else
    # When cameraserver is not included, we need to configure Camera API to not
    # connect to it.
    PRODUCT_PROPERTY_OVERRIDES += config.disable_cameraservice=true
endif

# SDK builds needs to build layoutlib-legacy that depends on debug info
ifneq ($(PRODUCT_IS_ATV_SDK),true)
    # Strip the local variable table and the local variable type table to reduce
    # the size of the system image. This has no bearing on stack traces, but will
    # leave less information available via JDWP.
    # From //build/make/target/product/go_defaults_common.mk
    PRODUCT_MINIMIZE_JAVA_DEBUG_INFO := true

    # Do not generate libartd.
    # From //build/make/target/product/go_defaults_common.mk
    PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD := false
endif

DEVICE_PACKAGE_OVERLAYS += \
    device/google/atv/overlay

# Enable frame-exact AV sync
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.media.avsync=true

# Copy .kl file for generic voice remotes
PRODUCT_COPY_FILES += \
    device/google/atv/Generic.kl:system/usr/keylayout/Generic.kl

PRODUCT_COPY_FILES += \
    device/google/atv/permissions/tv_core_hardware.xml:system/etc/permissions/tv_core_hardware.xml

PRODUCT_COPY_FILES += \
    frameworks/av/media/libeffects/data/audio_effects.conf:system/etc/audio_effects.conf
