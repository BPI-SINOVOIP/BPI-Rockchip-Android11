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

PRODUCT_MAKEFILES := \
        $(LOCAL_DIR)/rk3566_rgo/rk3566_rgo.mk \
        $(LOCAL_DIR)/rk3566_32bit/rk3566_32bit.mk \
        $(LOCAL_DIR)/rk3566_r/rk3566_r.mk \
        $(LOCAL_DIR)/rk3568_r/rk3568_r.mk \
        $(LOCAL_DIR)/rk3566_eink/rk3566_eink.mk \
        $(LOCAL_DIR)/rk3566_einkw6/rk3566_einkw6.mk

COMMON_LUNCH_CHOICES := \
    rk3566_32bit-userdebug \
    rk3566_32bit-user \
    rk3566_rgo-userdebug \
    rk3566_rgo-user \
    rk3566_r-userdebug \
    rk3566_r-user \
    rk3568_r-userdebug \
    rk3568_r-user \
    rk3566_eink-userdebug \
    rk3566_eink-user \
    rk3566_einkw6-userdebug \
    rk3566_einkw6-user


