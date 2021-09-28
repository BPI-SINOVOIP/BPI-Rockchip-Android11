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

#ifndef FASTRPC_ANDROID_USER_H
#define FASTRPC_ANDROID_USER_H

#include <assert.h>
#include <fcntl.h>
#include <asm/ioctl.h>
#include <errno.h>

/*
 * API to check if kernel supports remote memory allocation
 * Returns 0 if NOT supported
 *
 */
int is_kernel_alloc_supported(int dev, int domain);

/*
 * API to allocate ION memory for internal purposes
 * Returns NULL if allocation fails
 *
 */
void* rpcmem_alloc_internal(int heapid, uint32 flags, int size);

/*
 * API to free internally allocated ION memory
 *
 */
void rpcmem_free_internal(void* po);

/*
 * API to get fd of internally allocated ION buffer
 * Returns valid fd on success and -1 on failure
 *
 */
int rpcmem_to_fd_internal(void *po);

#endif //FASTRPC_ANDROID_USER_H
