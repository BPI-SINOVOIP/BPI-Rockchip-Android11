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
        $(LOCAL_DIR)/PX30_Android11/PX30_Android11.mk \
        $(LOCAL_DIR)/rk3326_pie/rk3326_pie.mk \
        $(LOCAL_DIR)/rk3326_q/rk3326_q.mk \
        $(LOCAL_DIR)/rk3326_rgo/rk3326_rgo.mk \
        $(LOCAL_DIR)/rk3326_r/rk3326_r.mk \

COMMON_LUNCH_CHOICES := \
    rk3326_pie-userdebug \
    rk3326_pie-user \
    PX30_Android11-userdebug \
    PX30_Android11-user \
    rk3326_q-userdebug \
    rk3326_q-user \
    rk3326_rgo-userdebug \
    rk3326_rgo-user \
    rk3326_r-userdebug \
    rk3326_r-user \

