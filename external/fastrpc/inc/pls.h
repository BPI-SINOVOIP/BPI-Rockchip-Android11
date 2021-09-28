#ifndef PLS_H
#define PLS_H

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

#include <stdlib.h>
#include "AEEStdDef.h"
#include "AEEatomic.h"
#include "verify.h"
#include "HAP_farf.h"

struct PLS;

struct plskey {
   uintptr_t type;
   uintptr_t key;
};

struct PLS {
   struct PLS* next;
   struct plskey key;
   void (*dtor)(void* data);
   uint64_t data[1];
};


struct pls_table {
   struct PLS* lst;
   uint32_t uRefs;
   uint32_t primThread;
};

/**
 * initialize on every thread and stick the pls_thread_deinit
 * function into the threads tls
 */
static __inline int pls_thread_init(struct pls_table* me, uintptr_t tid) {
   if(tid == me->primThread) {
      return 0;
   }
   if(0 == atomic_CompareOrAdd(&me->uRefs, 0, 1)) {
      return -1;
   }
   return 0;
}

/* call this constructor before the first thread creation with the
 * first threads id
 */
static __inline void pls_ctor(struct pls_table* me, uintptr_t primThread) {
   me->uRefs = 1;
   me->primThread = primThread;
}

static __inline struct pls_table* pls_thread_deinit(struct pls_table* me) {
   if(me && 0 != me->uRefs && 0 == atomic_Add(&me->uRefs, -1)) {
      struct PLS* lst, *next;
      lst = me->lst;
      me->lst = 0;
      while(lst) {
         next = lst->next;
         if(lst->dtor) {
            FARF(HIGH, "pls dtor %p", lst->dtor);
            lst->dtor((void*)lst->data);
         }
         free(lst);
         lst = next;
      }
      return me;
   }
   return 0;
}

/**
 * adds a new key to the local storage, overriding
 * any previous value at the key.  Overriding the key
 * does not cause the destructor to run.
 *
 * @param type, type part of the key to be used for lookup,
          these should be static addresses.
 * @param key, the key to be used for lookup
 * @param size, the size of the data
 * @param ctor, constructor that takes a context and memory of size
 * @param ctx, constructor context passed as the first argument to ctor
 * @param dtor, destructor to run at pls shutdown
 * @param ppo, output data
 * @retval, 0 for success
 */

static __inline int pls_add(struct pls_table* me, uintptr_t type, uintptr_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void* data), void** ppo) {
   int nErr = 0;
   struct PLS* pls = 0;
   struct PLS* prev;
   VERIFY(me->uRefs != 0);
   VERIFY(0 != (pls = (struct PLS*)calloc(1, size + sizeof(*pls) - sizeof(pls->data))));
   if(ctor) {
      VERIFY(0 == ctor(ctx, (void*)pls->data));
   }
   pls->dtor = dtor;
   pls->key.type = type;
   pls->key.key = key;
   do {
      pls->next = me->lst;
      prev = (struct PLS*)atomic_CompareAndExchangeUP((uintptr_t*)&me->lst, (uintptr_t)pls, (uintptr_t)pls->next);
   } while(prev != pls->next);
   if(ppo) {
      *ppo = (void*)pls->data;
   }
   FARF(HIGH, "pls added %p", dtor);
bail:
   if(nErr && pls) {
      free(pls);
   }
   return nErr;
}

static __inline int pls_lookup(struct pls_table* me, uintptr_t type, uintptr_t key, void** ppo);

/**
 * like add, but will only add 1 item if two threads try to add at the same time.  returns
 * item if its already there, otherwise tries to add.
 * ctor may be called twice
 * callers should avoid calling pls_add which will override the singleton
 */
static __inline int pls_add_lookup_singleton(struct pls_table* me, uintptr_t type, uintptr_t key, int size, int (*ctor)(void* ctx, void* data), void* ctx, void (*dtor)(void* data), void** ppo) {
   int nErr = 0;
   struct PLS* pls = 0;
   struct PLS* prev;
   if(0 == pls_lookup(me, type, key, ppo)) {
      return 0;
   }
   VERIFY(me->uRefs != 0);
   VERIFY(0 != (pls = (struct PLS*)calloc(1, size + sizeof(*pls) - sizeof(pls->data))));
   if(ctor) {
      VERIFY(0 == ctor(ctx, (void*)pls->data));
   }
   pls->dtor = dtor;
   pls->key.type = type;
   pls->key.key = key;
   do {
      pls->next = me->lst;
      if(0 == pls_lookup(me, type, key, ppo)) {
         if(pls->dtor) {
            pls->dtor((void*)pls->data);
         }
         free(pls);
         return 0;
      }
      prev = (struct PLS*)atomic_CompareAndExchangeUP((uintptr_t*)&me->lst, (uintptr_t)pls, (uintptr_t)pls->next);
   } while(prev != pls->next);
   if(ppo) {
      *ppo = (void*)pls->data;
   }
   FARF(HIGH, "pls added %p", dtor);
bail:
   if(nErr && pls) {
      free(pls);
   }
   return nErr;
}


/**
 * finds the last data pointer added for key to the local storage
 *
 * @param key, the key to be used for lookup
 * @param ppo, output data
 * @retval, 0 for success
 */

static __inline int pls_lookup(struct pls_table* me, uintptr_t type, uintptr_t key, void** ppo) {
   struct PLS* lst;
   for(lst = me->lst; me->uRefs != 0 && lst != 0; lst = lst->next) {
      if(lst->key.type == type && lst->key.key == key) {
         if(ppo) {
            *ppo = lst->data;
         }
         return 0;
      }
   }
   return -1;
}

#endif //PLS_H


