#
# Copyright (C) 2016 The Android Open Source Project
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

# for drm tests
vts_test_lib_packages += \
    libvtswidevine \

# for fuzz tests
vts_test_lib_packages += \
    libclang_rt.asan-arm-android \
    libclang_rt.asan-aarch64-android \
    libclang_rt.asan-i686-android \
    libclang_rt.asan-x86_64-android \
    libvts_func_fuzzer_utils \
    libvts_proto_fuzzer \
    libvts_proto_fuzzer_proto \

# for HAL interface hash test
vts_test_lib_packages += \
    libhidl-gen-hash \
