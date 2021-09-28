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

# For rockchip rk3036 rk312x rk3288 rk3368 rk3366 rk3399 rk3399pro platforms
ifneq ($(filter rk%, $(strip $(TARGET_BOARD_PLATFORM))), )

PRODUCT_HAVE_IPP ?= true
PRODUCT_HAVE_RKVPU ?= true
PRODUCT_HAVE_NAND ?= true
PRODUCT_HAVE_RKWIFI ?= true
PRODUCT_HAVE_RFTESTTOOL ?= false
PRODUCT_HAVE_GPS ?= true
PRODUCT_HAVE_RKPHONE_FEATURES ?= true
PRODUCT_HAVE_RKAPPS ?= true
PRODUCT_HAVE_RKEBOOK ?= false
PRODUCT_HAVE_DATACLONE ?= false
PRODUCT_HAVE_ADBLOCK ?= true
PRODUCT_HAVE_WEBKIT_DEBUG ?= false
PRODUCT_HAVE_RKTOOLS ?= true

else

# For sofia platforms
PRODUCT_HAVE_RKVPU ?= true
PRODUCT_HAVE_RKTOOLS ?= true
PRODUCT_HAVE_PLUGINSVC ?= true

endif
