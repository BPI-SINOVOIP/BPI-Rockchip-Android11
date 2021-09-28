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
        $(LOCAL_DIR)/rk356x_box/rk356x_box.mk \
        $(LOCAL_DIR)/rk356x_box_32/rk356x_box_32.mk

COMMON_LUNCH_CHOICES := \
    rk356x_box-user \
    rk356x_box-userdebug \
    rk356x_box_32-user \
    rk356x_box_32-userdebug
