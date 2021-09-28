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
PRODUCT_BOOT_DEVICE := ff390000.dwmmc,ff3b0000.nandc

include device/rockchip/common/build/rockchip/DynamicPartitions.mk
include device/rockchip/rk3326/PX30_Android11/BoardConfig.mk
include device/rockchip/common/BoardConfig.mk
$(call inherit-product, device/rockchip/rk3326/device-common.mk)
$(call inherit-product, device/rockchip/common/device.mk)

DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/overlay

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_NAME := PX30_Android11
PRODUCT_DEVICE := PX30_Android11
PRODUCT_BRAND := rockchip
PRODUCT_MODEL := PX30_Android11
PRODUCT_MANUFACTURER := rockchip
PRODUCT_AAPT_PREF_CONFIG := mdpi

# Enable DM file preopting to reduce first boot time
PRODUCT_DEX_PREOPT_GENERATE_DM_FILES := true
PRODUCT_DEX_PREOPT_DEFAULT_COMPILER_FILTER := verify

# Save space for Go-device
DONT_UNCOMPRESS_PRIV_APPS_DEXS := true
#
## add Rockchip properties
#
PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcd_density=160
# Reduces GC frequency of foreground apps by 50%
PRODUCT_PROPERTY_OVERRIDES += dalvik.vm.foreground-heap-growth-multiplier=2.0
# set zygote
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.zygote=zygote32
