#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the License);
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

# Target tuple format:
# primary_arch:arch:arch_variant:binder_bitness
vts_vndk_abi_dump_target_tuple_list := \
  arm:arm:armv7-a-neon:32 \
  arm:arm:armv7-a-neon:64 \
  arm64:arm64:armv8-a:64 \
  arm64:arm:armv8-a:64 \
  x86:x86:x86:32 \
  x86:x86:x86:64 \
  x86_64:x86_64:x86_64:64 \
  x86_64:x86:x86_64:64 \
