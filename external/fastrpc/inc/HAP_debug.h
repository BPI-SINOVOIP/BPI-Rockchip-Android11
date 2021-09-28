#ifndef HAP_DEBUG_H
#define HAP_DEBUG_H
/**
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "AEEStdDef.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAP_LEVEL_LOW       0
#define HAP_LEVEL_MEDIUM    1
#define HAP_LEVEL_HIGH      2
#define HAP_LEVEL_ERROR     3
#define HAP_LEVEL_FATAL     4

#define HAP_LEVEL_RUNTIME   (1 << 5)

//Add a weak reference so shared objects work with older images
#pragma weak HAP_debug_v2

//Add a weak reference so runtime FARFs are ignored on older images
#pragma weak HAP_debug_runtime

/**************************************************************************
    These HAP_debug* functions are not meant to be called directly.
    Please use the FARF() macros to call them instead
**************************************************************************/
void HAP_debug_v2(int level, const char* file, int line, const char* format, ...);
void HAP_debug_runtime(int level, const char* file, int line, const char* format, ...);
int HAP_setFARFRuntimeLoggingParams(unsigned int mask, const char* files[],
                                    unsigned short numberOfFiles);

// Keep these around to support older shared objects and older images
void HAP_debug(const char *msg, int level, const char *filename, int line);

static __inline void _HAP_debug_v2(int level, const char* file, int line,
                          const char* format, ...){
   char buf[256];
   va_list args;
   va_start(args, format);
   vsnprintf(buf, sizeof(buf), format, args);
   va_end(args);
   HAP_debug(buf, level, file, line);
}

/*!
This function is called to log an accumlated log entry. If logging is
enabled for the entry by the external device, then the entry is copied
into the diag allocation manager and commited.

    [in] log_code_type    ID of the event to be reported
    [in] *data            data points to the log which is to be submitted
    [in] dataLen          The length of the data to be logged.

Returns
    TRUE if log is submitted successfully into diag buffers
    FALSE if there is no space left in the buffers.

*/
boolean HAP_log_data_packet(unsigned short log_code_type, unsigned int dataLen,
                    byte* data);

#define HAP_DEBUG_TRACEME 0

long HAP_debug_ptrace(int req, unsigned int pid, void* addr, void* data);

#ifdef __cplusplus
}
#endif

#endif // HAP_DEBUG_H

