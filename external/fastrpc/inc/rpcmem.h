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

#ifndef RPCMEM_H
#define RPCMEM_H

#include "AEEStdDef.h"

/**
 * RPCMEM_DEFAULT_HEAP
 * Dynamicaly select the heap to use.  This should be ok for most usecases.
 */
#define RPCMEM_DEFAULT_HEAP -1


/**
 * RPCMEM_DEFAULT_FLAGS should allocate memory with the same properties
 * as the ION_FLAG_CACHED flag
 */
#ifdef ION_FLAG_CACHED
#define RPCMEM_DEFAULT_FLAGS ION_FLAG_CACHED
#else
#define RPCMEM_DEFAULT_FLAGS 1
#endif

/**
 * RPCMEM_FLAG_UNCACHED
 * ION_FLAG_CACHED should be defined as 1
 */
#define RPCMEM_FLAG_UNCACHED 0
#define RPCMEM_FLAG_CACHED RPCMEM_DEFAULT_FLAGS

/**
 * examples:
 *
 * heap 22, uncached, 1kb
 *    rpcmem_alloc(22, 0, 1024);
 *    rpcmem_alloc(22, RPCMEM_FLAG_UNCACHED, 1024);
 *
 * heap 21, cached, 2kb
 *    rpcmem_alloc(21, RPCMEM_FLAG_CACHED, 2048);
 *    #include <ion.h>
 *    rpcmem_alloc(21, ION_FLAG_CACHED, 2048);
 *
 * just give me the defaults, 2kb
 *    rpcmem_alloc(RPCMEM_DEFAULT_HEAP, RPCMEM_DEFAULT_FLAGS, 2048);
 *    rpcmem_alloc_def(2048);
 *
 * give me the default flags, but from heap 18, 4kb
 *    rpcmem_alloc(18, RPCMEM_DEFAULT_FLAGS, 4096);
 *
 */
#define ION_SECURE_FLAGS    ((1 << 31) | (1 << 19))
#ifdef __cplusplus
extern "C" {
#endif

/**
 * call once to initialize the library
 * Note: should not call this if rpcmem is linked from libadsprpc.so
 * /libcdsprpc.so/libmdsprpc.so/libsdsprpc.so
 */
void rpcmem_init(void);
/**
 * call once for cleanup
 * Note: should not call this if rpcmem is linked from libadsprpc.so
 * /libcdsprpc.so/libmdsprpc.so/libsdsprpc.so
 */
void rpcmem_deinit(void);

/**
 * Allocate via ION a buffer of size
 * @heapid, the heap id to use
 * @flags, ion flags to use to when allocating
 * @size, the buffer size to allocate
 * @retval, 0 on failure, pointer to buffer on success
 *
 * For example:
 *    buf = rpcmem_alloc(RPCMEM_DEFAULT_HEAP, RPCMEM_DEFAULT_FLAGS, size);
 */

void* rpcmem_alloc(int heapid, uint32 flags, int size);

/**
 * allocate with default settings
 */
 #if !defined(WINNT) && !defined (_WIN32_WINNT)
__attribute__((unused))
#endif
static __inline void* rpcmem_alloc_def(int size) {
   return rpcmem_alloc(RPCMEM_DEFAULT_HEAP, RPCMEM_DEFAULT_FLAGS, size);
}

/**
 * free buffer, ignores invalid buffers
 */
void rpcmem_free(void* po);

/**
 * returns associated fd
 */
int rpcmem_to_fd(void* po);

#ifdef __cplusplus
}
#endif

#define RPCMEM_HEAP_DEFAULT     0x80000000
#define RPCMEM_HEAP_NOREG       0x40000000
#define RPCMEM_HEAP_UNCACHED    0x20000000
#define RPCMEM_HEAP_NOVA        0x10000000
#define RPCMEM_HEAP_NONCOHERENT 0x08000000

#endif //RPCMEM_H
