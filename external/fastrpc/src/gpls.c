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
#include "HAP_farf.h"
#include "pls.h"
#include "HAP_pls.h"
#include "adsp_pls.h"
#include "platform_libs.h"
#include "version.h"

static struct pls_table gpls;
const char pls_version[] = VERSION_STRING ;
int gpls_init(void) {
   pls_ctor(&gpls, 1);
   return 0;
}

void gpls_deinit(void) {
   pls_thread_deinit(&gpls);
}

int HAP_pls_add(uintptr_t type, uintptr_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void*), void** ppo) {
   return pls_add(&gpls, type, key, size, ctor, ctx, dtor, ppo);
}

int HAP_pls_add_lookup(uintptr_t type, uintptr_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void*), void** ppo) {
   return pls_add_lookup_singleton(&gpls, type, key, size, ctor, ctx, dtor, ppo);
}

int HAP_pls_lookup(uintptr_t type, uintptr_t key, void** ppo) {
   return pls_lookup(&gpls, type, key, ppo);
}

int adsp_pls_add(uintptr_t type, uintptr_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void*), void** ppo) {
   return pls_add(&gpls, type, key, size, ctor, ctx, dtor, ppo);
}

int adsp_pls_add_lookup(uintptr_t type, uintptr_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void*), void** ppo) {
   return pls_add_lookup_singleton(&gpls, type, key, size, ctor, ctx, dtor, ppo);
}

int adsp_pls_lookup(uintptr_t type, uintptr_t key, void** ppo) {
   return pls_lookup(&gpls, type, key, ppo);
}


PL_DEFINE(gpls, gpls_init, gpls_deinit)
