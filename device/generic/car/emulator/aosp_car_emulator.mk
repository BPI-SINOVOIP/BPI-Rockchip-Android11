#
# Copyright (C) 2019 The Android Open Source Project
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

$(call inherit-product, device/generic/car/common/car.mk)
# This overrides device/generic/car/common/car.mk
$(call inherit-product, device/generic/car/emulator/audio/car_emulator_audio.mk)
$(call inherit-product, device/generic/car/emulator/rotary/car_rotary.mk)

ifeq (true,$(BUILD_EMULATOR_CLUSTER_DISPLAY))
PRODUCT_PRODUCT_PROPERTIES += \
    hwservicemanager.external.displays=1,1080,600,120,0 \
    persist.service.bootanim.displays=8140900251843329
endif

# Define the host tools and libs that are parts of the SDK.
$(call inherit-product, sdk/build/product_sdk.mk)
$(call inherit-product, development/build/product_sdk.mk)
