#
# Copyright (C) 2019 The Android Open-Source Project
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

#
# The makefile contains the special settings for MGSI.
# This makefile is used for the products/targets to release MGSI.
#
# For example:
# - MGSI contains skip_mount.cfg to skip mounting prodcut paritition
# - MGSI contains more VNDK packages to support old version vendors
# - etc.
#

# Exclude all files under system/product and system/system_ext
PRODUCT_ARTIFACT_PATH_REQUIREMENT_ALLOWED_LIST += \
    system/product/% \
    system/system_ext/%

# apex is not available before Q
# Properties set in system (here) take precedence over those in vendor.
PRODUCT_PRODUCT_PROPERTIES += \
    ro.apex.updatable=false

# Split selinux policy
PRODUCT_FULL_TREBLE_OVERRIDE := true

# Enable dynamic partition size
PRODUCT_USE_DYNAMIC_PARTITION_SIZE := true

# Needed by Pi newly launched device to pass VtsTrebleSysProp on MGSI
PRODUCT_COMPATIBLE_PROPERTY_OVERRIDE := true

# MGSI specific tasks on boot
PRODUCT_PACKAGES += \
    gsi_skip_mount.cfg \
    init.gsi.rc \

# Support additional P and Q VNDK packages
PRODUCT_EXTRA_VNDK_VERSIONS := 28 29
