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
# Test duplicating section names:

.section test.dup,"",@note
nop

# Test path name of program interpreter:

.section .interp,"a",@progbits
.string "/lib64/ld-linux-x86-64.so.2"

# Test android identifier section:

.section .note.android.ident,"a",@note
.balign 4
# Size of NAME
.long ANDROID_API - NAME
# Size of ANDROID_API
.long NOTE_ANDROID_IDENT_END - ANDROID_API
# Type is NT_VERSION
.long 1
NAME:
.ascii "Android\0"
ANDROID_API:
.long 28
NOTE_ANDROID_IDENT_END:
