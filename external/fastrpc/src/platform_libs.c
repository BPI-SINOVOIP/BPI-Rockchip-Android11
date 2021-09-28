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
#define FARF_ERROR 1
#include "HAP_farf.h"
#include "platform_libs.h"
#include "AEEatomic.h"
#include "AEEstd.h"
#include "AEEStdErr.h"
#include <stdio.h>
#include <assert.h>
#include "verify.h"

extern struct platform_lib* (*pl_list[])(void);
static uint32 atomic_IfNotThenAdd(uint32* volatile puDest, uint32 uCompare, int nAdd);

int pl_lib_init(struct platform_lib* (*plf)(void)) {
   int nErr = AEE_SUCCESS;
   struct platform_lib* pl = plf();
   if(1 == atomic_Add(&pl->uRefs, 1)) {
      if(pl->init) {
         FARF(HIGH, "calling init for %s",pl->name);
         nErr = pl->init();
         FARF(HIGH, "init for %s returned %x",pl->name, nErr);
      }
      pl->nErr = nErr;
   }
   if(pl->nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: %s init failed", nErr, pl->name);
   }
   return pl->nErr;
}

void pl_lib_deinit(struct platform_lib* (*plf)(void)) {
   struct platform_lib* pl = plf();
   if(1 == atomic_IfNotThenAdd(&pl->uRefs, 0, -1)) {
      if(pl->deinit && pl->nErr == 0) {
         pl->deinit();
      }
   }
   return;
}

static int pl_init_lst(struct platform_lib* (*lst[])(void)) {
   int nErr = AEE_SUCCESS;
   int ii;
   for(ii = 0; lst[ii] != 0; ++ii) {
      nErr = pl_lib_init(lst[ii]);
      if(nErr != 0) {
         break;
      }
   }
   if(nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: plinit failed\n", nErr);
   }
   return nErr;

}
int pl_init(void) {
   int nErr = pl_init_lst(pl_list);
   return nErr;
}

static void pl_deinit_lst(struct platform_lib* (*lst[])(void)) {
   int size, ii;
   for(size = 0; lst[size] != 0; ++size) {;}
   for(ii = size - 1; ii >= 0; --ii) {
      pl_lib_deinit(lst[ii]);
   }
   return;
}


void pl_deinit(void) {
   pl_deinit_lst(pl_list);
   return;
}

static uint32 atomic_IfNotThenAdd(uint32* volatile puDest, uint32 uCompare, int nAdd)
{
   uint32 uPrev;
   uint32 uCurr;
   do {
      //check puDest
      uCurr = *puDest;
      uPrev = uCurr;
      //see if we need to update it
      if(uCurr != uCompare) {
         //update it
         uPrev = atomic_CompareAndExchange(puDest, uCurr + nAdd, uCurr);
      }
      //verify that the value was the same during the update as when we decided to update
   } while(uCurr != uPrev);
   return uPrev;
}

