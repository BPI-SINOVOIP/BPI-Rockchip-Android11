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


#ifndef FASTRPC_PERF_H
#define FASTRPC_PERF_H

#include "remote.h"
#ifdef __ANDROID__
#include "cutils/properties.h"
#else
#define PROPERTY_VALUE_MAX	32
#endif

#if (defined LE_ENABLE)
#define FASTRPC_PROPERTY_GET_INT32(key, defvalue) ((int)0)
#define FASTRPC_PROPERTY_GET_STR(key, buffer, defvalue)     ((int)0)
#else
#if  (defined __ANDROID__)
#define FASTRPC_PROPERTY_GET_INT32(key, defvalue) ((int)property_get_int32((key), (defvalue)))
#define FASTRPC_PROPERTY_GET_STR(key, buffer, defvalue)     ((int)property_get((key), (buffer), (defvalue)))
#else
#define FASTRPC_PROPERTY_GET_INT32(key, defvalue) ((int)0)
#define FASTRPC_PROPERTY_GET_STR(key, buffer, defvalue)     ((int)0)
#endif
#endif

#define FASTRPC_MAX_STATIC_HANDLE (10)
#define IS_STATIC_HANDLE(handle) ((handle) >= 0 && (handle) <= FASTRPC_MAX_STATIC_HANDLE)
extern int fastrpc_perf_init(int dev);
extern void fastrpc_perf_update(int dev, remote_handle handle, uint32_t sc);
extern void fastrpc_perf_deinit(void);

#endif //FASTRPC_PERF_H
