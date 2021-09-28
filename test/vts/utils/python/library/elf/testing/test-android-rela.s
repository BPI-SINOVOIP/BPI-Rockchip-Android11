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
/*
To generate sleb128 encoded bytes:
  llvm-mc -filetype=asm

.section test.rela,"a",@0x60000002

.ascii "APS2"

.sleb128 6          // Number of relocations
.sleb128 0x200000   // Offset

// Group 1
.sleb128 2
.sleb128 0

.sleb128 8              // Offset delta
.sleb128 (1 << 32) | 1  // Symbol index 1 R_X86_64_64
.sleb128 8              // Offset delta
.sleb128 (2 << 32) | 1  // Symbol index 2 R_X86_64_64

// Group 2
.sleb128 4
.sleb128 9              // Group by info & group has addend
.sleb128 8              // R_X86_RELATIVE

.sleb128 16             // Offset delta
.sleb128 128            // Addend delta
.sleb128 16             // Offset delta
.sleb128 8              // Addend delta
.sleb128 16             // Offset delta
.sleb128 16             // Addend delta
.sleb128 16             // Offset delta
.sleb128 32             // Addend delta
*/

.section test.rela,"a",@0x60000002

.ascii "APS2"

.byte 6
.ascii "\200\200\200\001"

.byte 2
.byte 0

.byte 8
.ascii "\201\200\200\200\020"
.byte 8
.ascii "\201\200\200\200 "

.byte 4
.byte 9
.byte 8

.byte 16
.ascii "\200\001"
.byte 16
.byte 8
.byte 16
.byte 16
.byte 16
.byte 32
