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

ifeq ($(PRODUCT_HAVE_RKAPPS), true)
ifneq ($(BUILD_WITH_GOOGLE_GMS_EXPRESS), true)
$(call inherit-product-if-exists, vendor/rockchip/common/apps/apps.mk)
endif
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), PVR540)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/PVR540.mk)
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali400)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/Mali400.mk)
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali450)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/Mali450.mk)
endif
ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t760)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/MaliT760.mk)
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t720)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/MaliT720.mk)
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t860)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/MaliT860.mk)
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-tDVx)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/MaliTDVx.mk)
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-G52)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/MaliG52.mk)
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), G6110)
$(call inherit-product-if-exists, vendor/rockchip/common/gpu/G6110.mk)
endif

ifeq ($(PRODUCT_HAVE_IPP), true)
$(call inherit-product-if-exists, vendor/rockchip/common/ipp/ipp.mk)
endif

ifeq ($(PRODUCT_HAVE_RKVPU), true)
$(call inherit-product-if-exists, vendor/rockchip/common/vpu/vpu.mk)
endif

ifeq ($(PRODUCT_HAVE_NAND), true)
$(call inherit-product-if-exists, vendor/rockchip/common/nand/nand.mk)
endif

ifeq ($(PRODUCT_HAVE_RKWIFI), true)
$(call inherit-product-if-exists, vendor/rockchip/common/wifi/wifi.mk)
endif

ifeq ($(PRODUCT_HAVE_RFTESTTOOL), true)
$(call inherit-product-if-exists, vendor/rockchip/common/rftesttool/rftesttool.mk)
endif

ifeq ($(PRODUCT_HAVE_RKTOOLS), true)
$(call inherit-product-if-exists, vendor/rockchip/common/bin/bin.mk)
endif

ifeq ($(PRODUCT_HAVE_WEBKIT_DEBUG), true)
$(call inherit-product-if-exists, vendor/rockchip/common/webkit/webkit.mk)
endif

ifeq ($(strip $(BOARD_HAVE_BLUETOOTH)),true)
$(call inherit-product-if-exists, vendor/rockchip/common/bluetooth/bluetooth.mk)
endif

ifeq ($(PRODUCT_HAVE_GPS), true)
$(call inherit-product-if-exists, vendor/rockchip/common/gps/gps.mk)
endif

ifeq ($(PRODUCT_HAVE_ADBLOCK), true)
$(call inherit-product-if-exists, vendor/rockchip/common/etc/adblock.mk)
endif

# uncomment the line bellow to enable phone functions
ifeq ($(PRODUCT_HAVE_RKPHONE_FEATURES), true)
$(call inherit-product-if-exists, vendor/rockchip/common/phone/phone.mk)
endif

ifeq ($(PRODUCT_HAVE_RKEBOOK)),true)
$(call inherit-product-if-exists, vendor/rockchip/common/app/rkbook.mk)
endif

# for data clone
ifeq ($(PRODUCT_HAVE_DATACLONE)),true)
$(call inherit-product-if-exists, vendor/rockchip/common/data_clone/packdata.mk)
endif

#for HDMI HDCP2
ifeq ($(PRODUCT_HAVE_HDMIHDCP2), true)
$(call inherit-product-if-exists, vendor/rockchip/common/hdcp2/hdcp2.mk)
endif

ifeq ($(PRODUCT_HAVE_EPTZ),true)
$(call inherit-product-if-exists, vendor/rockchip/common/eptz/eptz.mk)
endif

ifeq ($(PRODUCT_HAVE_FEC),true)
$(call inherit-product-if-exists, vendor/rockchip/common/fec/fec.mk)
endif

ifeq ($(PRODUCT_HAVE_PLUGINSVC),true)
$(call inherit-product-if-exists, vendor/rockchip/common/pluginsvc/pluginsvc.mk)
endif

$(call inherit-product-if-exists, vendor/rockchip/common/pppoe/pppoe.mk)

$(call inherit-product-if-exists, vendor/rockchip/common/gpu/gpu_performance/face_detection.mk)
