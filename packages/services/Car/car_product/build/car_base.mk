#
# Copyright (C) 2016 The Android Open-Source Project
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

# Base platform for car builds
# car packages should be added to car.mk instead of here

ifeq ($(DISABLE_CAR_PRODUCT_CONFIG_OVERLAY),)
PRODUCT_PACKAGE_OVERLAYS += packages/services/Car/car_product/overlay
endif

ifeq ($(DISABLE_CAR_PRODUCT_VISUAL_OVERLAY),)
PRODUCT_PACKAGE_OVERLAYS += packages/services/Car/car_product/overlay-visual
endif

PRODUCT_COPY_FILES += \
    packages/services/Car/car_product/build/component-overrides.xml:$(TARGET_COPY_OUT_VENDOR)/etc/sysconfig/component-overrides.xml \

PRODUCT_PACKAGES += \
    com.android.wifi \
    Home \
    BasicDreams \
    CaptivePortalLogin \
    CertInstaller \
    DocumentsUI \
    DownloadProviderUi \
    FusedLocation \
    InputDevices \
    KeyChain \
    Keyguard \
    LatinIME \
    Launcher2 \
    ManagedProvisioning \
    PacProcessor \
    PrintSpooler \
    ProxyHandler \
    Settings \
    SharedStorageBackup \
    VpnDialogs \
    MmsService \
    ExternalStorageProvider \
    atrace \
    libandroidfw \
    libaudioutils \
    libmdnssd \
    libnfc_ndef \
    libpowermanager \
    libspeexresampler \
    libvariablespeed \
    libwebrtc_audio_preprocessing \
    A2dpSinkService \
    PackageInstaller \
    car-bugreportd \

# EVS resources
PRODUCT_PACKAGES += android.automotive.evs.manager@1.1
# The following packages, or their vendor specific equivalents should be include in the device.mk
#PRODUCT_PACKAGES += evs_app
#PRODUCT_PACKAGES += evs_app_default_resources
#PRODUCT_PACKAGES += android.hardware.automotive.evs@1.0-service

# EVS manager overrides cameraserver on automotive implementations so
# we need to configure Camera API to not connect to it
PRODUCT_PROPERTY_OVERRIDES += config.disable_cameraservice=true

# Device running Android is a car
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.type.automotive.xml:system/etc/permissions/android.hardware.type.automotive.xml

# Default permission grant exceptions
PRODUCT_COPY_FILES += \
    packages/services/Car/car_product/build/default-car-permissions.xml:system/etc/default-permissions/default-car-permissions.xml \
    packages/services/Car/car_product/build/preinstalled-packages-product-car-base.xml:system/etc/sysconfig/preinstalled-packages-product-car-base.xml

$(call inherit-product, $(SRC_TARGET_DIR)/product/core_minimal.mk)

# Default dex optimization configurations
PRODUCT_PROPERTY_OVERRIDES += \
     pm.dexopt.disable_bg_dexopt=true

# Required init rc files for car
PRODUCT_COPY_FILES += \
    packages/services/Car/car_product/init/init.bootstat.rc:system/etc/init/init.bootstat.car.rc \
    packages/services/Car/car_product/init/init.car.rc:system/etc/init/init.car.rc

# Enable car watchdog
include packages/services/Car/watchdog/product/carwatchdog.mk
