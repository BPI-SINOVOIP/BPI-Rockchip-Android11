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

# First lunching is R, api_level is 30
PRODUCT_SHIPPING_API_LEVEL := 30
PRODUCT_DTBO_TEMPLATE := $(LOCAL_PATH)/dt-overlay.in
DTBO_DEVICETREE := dt-overlay i2c5 uart0 uart0_cts_rts uart7 uart9 spi3
PRODUCT_SDMMC_DEVICE := fe2b0000.dwmmc

include device/rockchip/common/build/rockchip/DynamicPartitions.mk
include device/rockchip/rk356x/bananapi_r2pro/BoardConfig.mk
include device/rockchip/common/BoardConfig.mk
$(call inherit-product, device/rockchip/rk356x/device.mk)
$(call inherit-product, device/rockchip/common/device.mk)
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)
$(call inherit-product-if-exists, vendor/bananapi/apps/apps.mk)

# copy input keylayout and device config
PRODUCT_COPY_FILES += \
	device/rockchip/rk356x_box/remote_config/fdd70030_pwm.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/fdd70030_pwm.kl \
	device/rockchip/common/adc-keys.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/adc-keys.kl

#For RK3568 EC20
ifeq ($(strip $(BOARD_QUECTEL_RIL)),true)
PRODUCT_PACKAGES += rild

PRODUCT_COPY_FILES += \
	vendor/bananapi/modem/libquectel-ril/arm64-v8a/libreference-ril.so:vendor/lib64/libquectel-ril.so \
	vendor/bananapi/modem/libquectel-ril/arm64-v8a/chat:system/bin/chat \
	vendor/bananapi/modem/libquectel-ril/arm64-v8a/ip-up:system/bin/ip-up \
	vendor/bananapi/modem/libquectel-ril/arm64-v8a/ip-down:system/bin/ip-down \
	vendor/bananapi/modem/apns-conf.xml:system/etc/apns-conf.xml

PRODUCT_PROPERTY_OVERRIDES += \
	ro.telephony.default_network=9 \
	rild.libpath=/vendor/lib64/libquectel-ril.so \
	rild.libargs=-d /dev/ttyUSB0

DEVICE_MANIFEST_FILE += vendor/bananapi/modem/manifest.xml
endif

DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/overlay $(LOCAL_PATH)/../overlay

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_NAME := bananapi_r2pro
PRODUCT_DEVICE := bananapi_r2pro
PRODUCT_BRAND := Bananapi
PRODUCT_MODEL := bananapi_r2pro
PRODUCT_MANUFACTURER := Sinovoip
PRODUCT_AAPT_PREF_CONFIG := mdpi

ifeq ($(BOARD_HAS_FACTORY_TEST),true)
PRODUCT_LOCALES := zh_CN en_US
endif

#
## add Rockchip properties
#
PRODUCT_PROPERTY_OVERRIDES += \
	ro.sf.lcd_density=213 \
	ro.wifi.sleep.power.down=true \
	persist.wifi.sleep.delay.ms=0 \
	persist.bt.power.down=true

PRODUCT_PROPERTY_OVERRIDES += \
	ro.net.eth_primary=eth1 \
	ro.net.eth_secondary=eth0

# 0-dhcp, 1-static
PRODUCT_PROPERTY_OVERRIDES += \
	persist.net.eth0.mode=1 \
	persist.net.eth0.staticinfo=172.16.1.1,24,172.16.1.1,114.114.114.114,8.8.8.8 \
	persist.dhcpserver.enable=1 \
	persist.dhcpserver.start=172.16.1.100 \
	persist.dhcpserver.end=172.16.1.150

# navigation bar power and voice button
PRODUCT_PROPERTY_OVERRIDES += \
	ro.rk.systembar.voiceicon=true \
	ro.rk.systembar.powericon=true \
	ro.rk.systembar.rotateicon=false

# secondary screen rotation(0=0, 1=90, 2=180, 3=270)
PRODUCT_PROPERTY_OVERRIDES += \
	persist.sys.rotation.einit-1=0

# secondary screen full display
PRODUCT_PROPERTY_OVERRIDES += \
	persist.sys.rotation.efull-1=false

# primary/secondary screen mouse control
PRODUCT_PROPERTY_OVERRIDES += \
	sys.mouse.presentation=1
