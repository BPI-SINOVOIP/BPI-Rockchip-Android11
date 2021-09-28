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
#include "AEEatomic.h"

#ifdef _WIN32
#include "Windows.h"
uint32 atomic_CompareAndExchange(uint32 * volatile puDest, uint32 uExchange, uint32 uCompare) {
   C_ASSERT(sizeof(LONG) == sizeof(uint32));
   return (uint32)InterlockedCompareExchange((LONG*)puDest, (LONG)uExchange, (LONG)uCompare);
}
uintptr_t atomic_CompareAndExchangeUP(uintptr_t * volatile puDest, uintptr_t uExchange, uintptr_t uCompare) {
   C_ASSERT(sizeof(uintptr_t) == sizeof(void*));
   return (uintptr_t)InterlockedCompareExchangePointer((void**)puDest, (void*)uExchange, (void*)uCompare);
}
#elif __hexagon__

#ifndef C_ASSERT
#define C_ASSERT(test) \
    switch(0) {\
      case 0:\
      case test:;\
    }
#endif

static inline unsigned int
qurt_atomic_compare_val_and_set(unsigned int* target,
                                unsigned int old_val,
                                unsigned int new_val)
{
   unsigned int current_val;

   __asm__ __volatile__(
       "1:     %0 = memw_locked(%2)\n"
       "       p0 = cmp.eq(%0, %3)\n"
       "       if !p0 jump 2f\n"
       "       memw_locked(%2, p0) = %4\n"
       "       if !p0 jump 1b\n"
       "2:\n"
       : "=&r" (current_val),"+m" (*target)
       : "r" (target), "r" (old_val), "r" (new_val)
       : "p0");

   return current_val;
}
uint32 atomic_CompareAndExchange(uint32 * volatile puDest, uint32 uExchange, uint32 uCompare) {
   return (uint32)qurt_atomic_compare_val_and_set((unsigned int*)puDest, uCompare, uExchange);
}
uintptr_t atomic_CompareAndExchangeUP(uintptr_t * volatile puDest, uintptr_t uExchange, uintptr_t uCompare) {
   C_ASSERT(sizeof(uintptr_t) == sizeof(uint32));
   return (uint32)atomic_CompareAndExchange((uint32*)puDest, (uint32)uExchange, (uint32)uCompare);
}
#elif __GNUC__
uint32 atomic_CompareAndExchange(uint32 * volatile puDest, uint32 uExchange, uint32 uCompare) {
   return __sync_val_compare_and_swap(puDest, uCompare, uExchange);
}
uint64 atomic_CompareAndExchange64(uint64 * volatile puDest, uint64 uExchange, uint64 uCompare) {
   return __sync_val_compare_and_swap(puDest, uCompare, uExchange);
}
uintptr_t atomic_CompareAndExchangeUP(uintptr_t * volatile puDest, uintptr_t uExchange, uintptr_t uCompare) {
   return __sync_val_compare_and_swap(puDest, uCompare, uExchange);
}
#endif //compare and exchange
