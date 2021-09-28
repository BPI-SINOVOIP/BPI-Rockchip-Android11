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

#ifndef _ADSP_PERF_H
#define _ADSP_PERF_H
#include "AEEStdDef.h"
#ifndef __QAIC_HEADER
#define __QAIC_HEADER(ff) ff
#endif //__QAIC_HEADER

#ifndef __QAIC_HEADER_EXPORT
#define __QAIC_HEADER_EXPORT
#endif // __QAIC_HEADER_EXPORT

#ifndef __QAIC_HEADER_ATTRIBUTE
#define __QAIC_HEADER_ATTRIBUTE
#endif // __QAIC_HEADER_ATTRIBUTE

#ifndef __QAIC_IMPL
#define __QAIC_IMPL(ff) ff
#endif //__QAIC_IMPL

#ifndef __QAIC_IMPL_EXPORT
#define __QAIC_IMPL_EXPORT
#endif // __QAIC_IMPL_EXPORT

#ifndef __QAIC_IMPL_ATTRIBUTE
#define __QAIC_IMPL_ATTRIBUTE
#endif // __QAIC_IMPL_ATTRIBUTE
#ifdef __cplusplus
extern "C" {
#endif
/**
 * Interface for querying the adsp for counter data
 * For example, to enable all the perf numbers:
 * 
 *     int perf_on(void) {
 *       int nErr = 0;
 *       int numKeys = 0, maxLen = 0, ii;
 *       char keys[512];
 *       char* buf = &keys[0];
 *       VERIFY(0 == adsp_perf_get_keys(keys, 512, &maxLen, &numKeys)); 
 *       assert(maxLen < 512);
 *       for(ii = 0; ii < numKeys; ++ii) {
 *          char* name = buf;
 *          buf += strlen(name) + 1;
 *          printf("perf on: %s\n", name);
 *          VERIFY(0 == adsp_perf_enable(ii));
 *       }
 *    bail:
 *       return nErr;
 *    }
 *
 * To read all the results:
 *
 *    int rpcperf_perf_result(void) {
 *       int nErr = 0;
 *       int numKeys, maxLen, ii;
 *       char keys[512];
 *       char* buf = &keys[0];
 *       long long usecs[16];
 *       VERIFY(0 == adsp_perf_get_keys(keys, 512, &maxLen, &numKeys)); 
 *       printf("perf keys: %d\n", numKeys);
 *       VERIFY(0 == adsp_perf_get_usecs(usecs, 16));
 *       assert(maxLen < 512);
 *       assert(numKeys < 16);
 *       for(ii = 0; ii < numKeys; ++ii) {
 *          char* name = buf;
 *          buf += strlen(name) + 1;
 *          printf("perf result: %s %lld\n", name, usecs[ii]);
 *       }
 *    bail:
 *       return nErr;
 *    }
 */
#define _const_adsp_perf_handle 6
__QAIC_HEADER_EXPORT int __QAIC_HEADER(adsp_perf_enable)(int ix) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(adsp_perf_get_usecs)(int64* dst, int dstLen) __QAIC_HEADER_ATTRIBUTE;
__QAIC_HEADER_EXPORT int __QAIC_HEADER(adsp_perf_get_keys)(char* keys, int keysLen, int* maxLen, int* numKeys) __QAIC_HEADER_ATTRIBUTE;
#ifdef __cplusplus
}
#endif
#endif //_ADSP_PERF_H
