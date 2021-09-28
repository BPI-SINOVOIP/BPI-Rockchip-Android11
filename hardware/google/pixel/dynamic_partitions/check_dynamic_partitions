#!/system/bin/sh

#
# Copyright (C) 2018 The Android Open Source Project
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

# This script will run as an environment check for regular dynamic partitions
# update. In particular, a regular update with dynamic partitions enabled
# should not be applied on a build that does not have dynamic partitions.

DP_PROPERTY_NAME="ro.boot.dynamic_partitions"
DP_RETROFIT_PROPERTY_NAME="ro.boot.dynamic_partitions_retrofit"

DP_PROPERTY=$(getprop ${DP_PROPERTY_NAME})
DP_RETROFIT_PROPERTY=$(getprop ${DP_RETROFIT_PROPERTY_NAME})

if [ "${DP_PROPERTY}" != "true" ] || [ "${DP_RETROFIT_PROPERTY}" != "true" ] ; then
    echo "Error: applied non-retrofit update on build without dynamic partitions."
    echo "${DP_PROPERTY_NAME}=${DP_PROPERTY}"
    echo "${DP_RETROFIT_PROPERTY_NAME}=${DP_RETROFIT_PROPERTY}"
    exit 1
fi
