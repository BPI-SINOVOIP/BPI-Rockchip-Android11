# Copyright (C) 2020 The Android Open Source Project

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

# Add carwatchdogd to product package
PRODUCT_PACKAGES += carwatchdogd

# SELinux public policies for car watchdog services
PRODUCT_PUBLIC_SEPOLICY_DIRS += packages/services/Car/watchdog/sepolicy/public

# SELinux private policies for car watchdog services
PRODUCT_PRIVATE_SEPOLICY_DIRS += packages/services/Car/watchdog/sepolicy/private

# Include carwatchdog testclient if the build is userdebug or eng
ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
    PRODUCT_PACKAGES += carwatchdog_testclient
    BOARD_SEPOLICY_DIRS += packages/services/Car/watchdog/testclient/sepolicy
endif
