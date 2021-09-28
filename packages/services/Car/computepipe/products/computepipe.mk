# Copyright (C) 2019 The Android Open Source Project

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

# Enable computepipe interface
PRODUCT_PACKAGES += android.automotive.computepipe.router@1.0-impl

# Enable computepipe router
PRODUCT_PACKAGES += android.automotive.computepipe.router@1.0

# Enable computepipe runner engine library
PRODUCT_PACKAGES += computepipe_runner_engine

# Enable computepipe runner engine client interface library
PRODUCT_PACKAGES += computepipe_client_interface

# Enable computepipe runner engine prebuilt graph library
PRODUCT_PACKAGES += computepipe_prebuilt_graph


# Selinux public policies for computepipe services
PRODUCT_PUBLIC_SEPOLICY_DIRS += packages/services/Car/computepipe/sepolicy/public

# Selinux private policies for computepipe services
PRODUCT_PRIVATE_SEPOLICY_DIRS += packages/services/Car/computepipe/sepolicy/private
