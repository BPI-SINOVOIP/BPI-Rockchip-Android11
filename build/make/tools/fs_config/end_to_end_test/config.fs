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

# This file is used to define the properties of the filesystem
# images generated by build tools (mkbootfs and mkyaffs2image) and
# by the device side of adb.

[AID_VENDOR_NEW_SERVICE]
value: 2900

[AID_VENDOR_NEW_SERVICE_TWO]
value:2902

[vendor/bin/service1]
mode: 0755
user: AID_SYSTEM
group: AID_VENDOR_NEW_SERVICE
caps: CHOWN DAC_OVERRIDE

[vendor/bin/service2]
mode: 0755
user: AID_VENDOR_NEW_SERVICE_TWO
group: AID_SYSTEM
caps: AUDIT_READ CHOWN SYS_ADMIN

[system/vendor/bin/service3]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: AUDIT_READ CHOWN SYS_ADMIN

[vendor/dir/]
mode: 0755
user: AID_VENDOR_NEW_SERVICE_TWO
group: AID_SYSTEM
caps: 0

[system/vendor/dir2/]
mode: 0755
user: AID_VENDOR_NEW_SERVICE_TWO
group: AID_SYSTEM
caps: 0

[product/bin/service1]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: 0x34

[product/bin/service2]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: NET_BIND_SERVICE WAKE_ALARM

[system/product/bin/service3]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: NET_BIND_SERVICE WAKE_ALARM

[product/dir/]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: 0

[system/product/dir/]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: 0

[system/bin/service]
mode: 0755
user: AID_SYSTEM
group: AID_RADIO
caps: NET_BIND_SERVICE

[system/dir/]
mode: 0755
user: AID_SYSTEM
group: AID_RADIO
caps: 0

[root_file]
mode: 0755
user: AID_SYSTEM
group: AID_RADIO
caps: 0

[root_dir/]
mode: 0755
user: AID_SYSTEM
group: AID_RADIO
caps: 0
