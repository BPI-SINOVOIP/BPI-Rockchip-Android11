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

PRODUCT_USE_DYNAMIC_PARTITIONS := true
PRODUCT_RETROFIT_DYNAMIC_PARTITIONS := true

BOARD_SUPER_PARTITION_GROUPS := rockchip_dynamic_partitions
BOARD_ROCKCHIP_DYNAMIC_PARTITIONS_SIZE ?= 3263168512
BOARD_ROCKCHIP_DYNAMIC_PARTITIONS_PARTITION_LIST := system vendor odm
BOARD_SYSTEMIMAGE_PARTITION_RESERVED_SIZE := 52428800
BOARD_VENDORIMAGE_PARTITION_RESERVED_SIZE := 52428800
BOARD_ODMIMAGE_PARTITION_RESERVED_SIZE := 52428800
