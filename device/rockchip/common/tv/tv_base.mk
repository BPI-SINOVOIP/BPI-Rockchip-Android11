#
# Copyright (C) 2014 The Android Open Source Project
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

PRODUCT_PACKAGES := \
    TvProvider \
    TvSettings \
    SettingsIntelligence \
    tv_input.default

ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
PRODUCT_PACKAGES += Traceur
endif

PRODUCT_COPY_FILES := \
    device/rockchip/common/tv/permissions/tv_core_hardware.xml:system/etc/permissions/tv_core_hardware.xml

DEVICE_PACKAGE_OVERLAYS := \
    device/rockchip/common/tv/overlay

# From build/target/product/core_base.mk
PRODUCT_PACKAGES += \
    ContactsProvider \
    DefaultContainerService \
    UserDictionaryProvider \
    libaudiopreprocessing \
    libfilterpack_imageproc \
    libgabi++ \
    libkeystore \
    libstagefright_soft_aacdec \
    libstagefright_soft_aacenc \
    libstagefright_soft_amrdec \
    libstagefright_soft_amrnbenc \
    libstagefright_soft_amrwbenc \
    libstagefright_soft_avcdec \
    libstagefright_soft_avcenc \
    libstagefright_soft_flacdec \
    libstagefright_soft_flacenc \
    libstagefright_soft_g711dec \
    libstagefright_soft_gsmdec \
    libstagefright_soft_hevcdec \
    libstagefright_soft_mp3dec \
    libstagefright_soft_mpeg2dec \
    libstagefright_soft_mpeg4dec \
    libstagefright_soft_mpeg4enc \
    libstagefright_soft_opusdec \
    libstagefright_soft_rawdec \
    libstagefright_soft_vorbisdec \
    libstagefright_soft_vpxdec \
    libstagefright_soft_vpxenc \
    mdnsd \
    requestsync

# From build/target/product/core.mk
PRODUCT_PACKAGES += \
    BasicDreams \
    Browser \
    CalendarProvider \
    ExactCalculator \
    CaptivePortalLogin \
    CertInstaller \
    ExternalStorageProvider \
    FusedLocation \
    InputDevices \
    KeyChain \
    BoxLatinIME \
    PicoTts \
    PacProcessor \
    PrintSpooler \
    ProxyHandler \
    SharedStorageBackup \
    VpnDialogs \
    com.android.media.tv.remoteprovider \
    com.android.media.tv.remoteprovider.xml

# From build/target/product/generic_no_telephony.mk
PRODUCT_PACKAGES += \
    Bluetooth \
    SystemUI \
    librs_jni \
    audio.primary.default \
    audio_policy.default \
    clatd \
    clatd.conf \
    local_time.default \
    screenrecord \
    Camera2 \
    Provision

# From build/target/product/handheld_system.mk
PRODUCT_PACKAGES += \
    MtpService

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


PRODUCT_COPY_FILES += \
    frameworks/av/media/libeffects/data/audio_effects.conf:system/etc/audio_effects.conf

# Enable frame-exact AV sync
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.media.avsync=true \
    persist.sys.audio.enforce_safevolume=false


# SDK builds needs to build layoutlib-legacy that depends on debug info
# Strip the local variable table and the local variable type table to reduce
# the size of the system image. This has no bearing on stack traces, but will
# leave less information available via JDWP.
# From //build/make/target/product/go_defaults_common.mk
#PRODUCT_MINIMIZE_JAVA_DEBUG_INFO := true

# Do not generate libartd.
# From //build/make/target/product/go_defaults_common.mk
#PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD := false


# Do not include the Live Channels app if USE_OEM_TV_APP flag is set.
# The feature com.google.android.tv.installed is used to tell whether a device
# has the pre-installed Live Channels app. This is necessary for the Play Store
# to identify the compatible devices that can install later updates of the app.
ifneq ($(USE_OEM_TV_APP),true)
    ifneq ($(PRODUCT_IS_ATV_SDK),true)
        PRODUCT_PACKAGES += TV
    else
        PRODUCT_PACKAGES += LiveTv
    endif # if PRODUCT_IS_ATV_SDK

    PRODUCT_COPY_FILES += \
        device/google/atv/permissions/com.google.android.tv.installed.xml:system/etc/permissions/com.google.android.tv.installed.xml
endif

# To enable access to /dev/dvb*
BOARD_SEPOLICY_DIRS += device/google/atv/sepolicy


# Copy .kl file for generic voice remotes
PRODUCT_COPY_FILES += \
    device/google/atv/Generic.kl:system/usr/keylayout/Generic.kl
$(call inherit-product-if-exists, frameworks/base/data/sounds/AllAudio.mk)
$(call inherit-product-if-exists, external/svox/pico/lang/all_pico_languages.mk)
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
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_minimal.mk)
