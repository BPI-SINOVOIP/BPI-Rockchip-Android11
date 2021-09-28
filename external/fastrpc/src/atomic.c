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

uint32 atomic_Add(uint32 * volatile puDest, int nAdd) {
   uint32 previous;
   uint32 current;
   do {
      current = *puDest;
      previous = atomic_CompareAndExchange(puDest, current + nAdd, current);
   } while(previous != current);
   return (current + nAdd);
}

uint32 atomic_Exchange(uint32* volatile puDest, uint32 uVal) {
   uint32 previous;
   uint32 current;
   do {
      current = *puDest;
      previous = atomic_CompareAndExchange(puDest, uVal, current);
   } while(previous != current);
   return previous;
}

uint32 atomic_CompareOrAdd(uint32* volatile puDest, uint32 uCompare, int nAdd) {
   uint32 previous;
   uint32 current;
   uint32 result;
   do {
      current = *puDest;
      previous = current;
      result = current;
      if(current != uCompare) {
         previous = atomic_CompareAndExchange(puDest, current + nAdd, current);
         if(previous == current) {
            result = current + nAdd;
         }
      }
   } while(previous != current);
   return result;
}

