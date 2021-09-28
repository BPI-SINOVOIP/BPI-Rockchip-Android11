/*
 *
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBTEEUI_INCLUDE_TEEUI_INCFONT_H_
#define LIBTEEUI_INCLUDE_TEEUI_INCFONT_H_

#ifdef __ASSEMBLY__
.macro TEEUI_ASM_INCFONT fontname, filename
.section .rodata
.align 4
.globl \fontname
\fontname:
.incbin "\filename"
.section .rodata
.align 1
\fontname\()_end:
.section .rodata
.align 4;
.globl \fontname\()_length
\fontname\()_length:
.long \fontname\()_end - \fontname - 1
.endmacro

#define TEEUI_INCFONT(fontname, filename) TEEUI_ASM_INCFONT fontname, filename
#else

#define TEEUI_INCFONT(fontname, ...)                                                               \
    extern unsigned char fontname[];                                                               \
    extern unsigned int fontname##_length

#endif

#endif  // LIBTEEUI_INCLUDE_TEEUI_INCFONT_H_
