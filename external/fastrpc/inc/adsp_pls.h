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

#ifndef ADSP_PLS_H
#define ADSP_PLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * internal header
 */

/**
 * adsp process local storage is local storage for the fastrpc hlos
 * process context.

 * When used from within a fastrpc started thread this will attach
 * desturctors to the lifetime of the hlos process that is making the
 * rpc calls.  Users can use this to store context for the lifetime of
 * the calling process on the hlos.
 */

/**
 * adds a new key to the local storage, overriding
 * any previous value at the key.  Overriding the key
 * does not cause the destructor to run.
 *
 * @param type, type part of the key to be used for lookup,
 *        these should be static addresses, like the address of a function.
 * @param key, the key to be used for lookup
 * @param size, the size of the data
 * @param ctor, constructor that takes a context and memory of size
 * @param ctx, constructor context passed as the first argument to ctor
 * @param dtor, destructor to run at pls shutdown
 * @param ppo, output data
 * @retval, 0 for success
 */
int adsp_pls_add(uintptr_t type, uintptr_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void*), void** ppo);

/**
 * Like add, but will only add 1 item, and return the same item on the
 * next add.  If two threads try to call this function at teh same time
 * they will both receive the same value as a result, but the constructors
 * may be called twice.
 * item if its already there, otherwise tries to add.
 * ctor may be called twice
 * callers should avoid calling pls_add which will override the singleton
 */
int adsp_pls_add_lookup(uintptr_t type, uintptr_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void*), void** ppo);

/**
 * finds the last data pointer added for key to the local storage
 *
 * @param key, the key to be used for lookup
 * @param ppo, output data
 * @retval, 0 for success
 */
int adsp_pls_lookup(uintptr_t type, uintptr_t key, void** ppo);

/**
 * force init/deinit
 */
int gpls_init(void);
void gpls_deinit(void);

#ifdef __cplusplus
}
#endif
#endif //ADSP_PLS_H
