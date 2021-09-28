/*
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

/*
=======================================================================

FILE:         std_mem.c

SERVICES:     apiOne std lib memory operations stuff

*/

#include <string.h>
#include "AEEstd.h"
#include "AEEStdErr.h"

#if defined __hexagon__
#include "stringl/stringl.h"

//Add a weak reference so shared objects work with older images
#pragma weak memscpy
#pragma weak memsmove
#endif /*__hexagon__*/

void* std_memset(void* p, int c, int nLen)
{
   if (nLen < 0) {
      return p;
   }
   return memset(p, c, (size_t)nLen);
}

void* std_memmove(void* pTo, const void* cpFrom, int nLen)
{
   if (nLen <= 0) {
      return pTo;
   }
#ifdef __hexagon__
   std_memsmove(pTo, (size_t)nLen, cpFrom, (size_t)nLen);
   return pTo;
#else
   return memmove(pTo, cpFrom, (size_t)nLen);
#endif
}

int std_memscpy(void *dst, int dst_size, const void *src, int src_size){
    size_t copy_size = 0;

    if(dst_size <0 || src_size <0){
        return AEE_EBADSIZE;
    }

#if defined (__hexagon__)
    if (memscpy){
        return memscpy(dst,dst_size,src,src_size);
    }
#endif

    copy_size = (dst_size <= src_size)? dst_size : src_size;
    memcpy(dst, src, copy_size);
    return copy_size;
}

int std_memsmove(void *dst, int dst_size, const void *src, int src_size){
    size_t copy_size = 0;

    if(dst_size <0 || src_size <0){
        return AEE_EBADSIZE;
    }

#if defined (__hexagon__)
    if (memsmove){
        return memsmove(dst,dst_size,src,src_size);
    }
#endif

    copy_size = (dst_size <= src_size)? dst_size : src_size;
    memmove(dst, src, copy_size);
    return copy_size;
}

