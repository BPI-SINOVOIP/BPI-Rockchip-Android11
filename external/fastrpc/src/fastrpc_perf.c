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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "HAP_farf.h"
#include "verify.h"
#include "remote.h"
#include "rpcmem.h"
#include "AEEstd.h"
#include "adsp_perf.h"
#include "fastrpc_perf.h"
#include "fastrpc_internal.h"
#include "fastrpc_apps_user.h"


#ifdef ANDROID_P
#define PERF_KEY_KERNEL "vendor.fastrpc.perf.kernel"
#define PERF_KEY_ADSP   "vendor.fastrpc.perf.adsp"
#define PERF_KEY_FREQ   "vendor.fastrpc.perf.freq"
#else
#define PERF_KEY_KERNEL "fastrpc.perf.kernel"
#define PERF_KEY_ADSP   "fastrpc.perf.adsp"
#define PERF_KEY_FREQ   "fastrpc.perf.freq"
#endif

#define PERF_MODE 2
#define PERF_OFF 0
#define PERF_KERNEL_MASK (0x1)
#define PERF_ADSP_MASK (0x2)
#define PERF_KEY_STR_MAX (2*1024)
#define PERF_MAX_NUM_KEYS 64

#define PERF_NS_TO_US(n) ((n)/1000)

#define IS_KEY_ENABLED(name) (!std_strncmp((name), "perf_invoke_count", 17) || \
                              !std_strncmp((name), "perf_mod_invoke", 15) || \
                              !std_strncmp((name), "perf_rsp", 8) || \
                              !std_strncmp((name), "perf_hdr_sync_flush", 19) || \
                              !std_strncmp((name), "perf_sync_flush", 15) || \
                              !std_strncmp((name), "perf_hdr_sync_inv", 17) || \
                              !std_strncmp((name), "perf_sync_inv", 13)) \

struct perf_keys {
   int64 data[PERF_MAX_NUM_KEYS];
   int numKeys;
   int maxLen;
   int enable;
   char *keys;
};

struct fastrpc_perf {
   int count;
   int freq;
   int perf_on;
   struct perf_keys kernel;
   struct perf_keys dsp;
};
struct fastrpc_perf gperf;

static int perf_kernel_getkeys(int dev) {
   int nErr = 0;
bail:
   return nErr;
}

static void get_perf_kernel(int dev, remote_handle handle, uint32_t sc) {
bail:
   return;
}

static void get_perf_adsp(remote_handle handle, uint32_t sc) {
   int nErr = 0;
   struct fastrpc_perf *p = &gperf;
   struct perf_keys *pdsp = &gperf.dsp;
   int ii;
   char *token;

   char *keystr = pdsp->keys;
   VERIFY(0 == adsp_perf_get_usecs(pdsp->data, PERF_MAX_NUM_KEYS));
   VERIFY(pdsp->maxLen < PERF_KEY_STR_MAX);
   VERIFY(pdsp->numKeys < PERF_MAX_NUM_KEYS);
   FARF(ALWAYS, "\nFastRPC dsp perf for handle 0x%x sc 0x%x\n", handle, sc);
   for(ii = 0; ii < pdsp->numKeys; ii++) {
      token = keystr;
      keystr += strlen(token) + 1;
      VERIFY(token);
      if (!pdsp->data[ii])
         continue;
      if (!std_strncmp(token, "perf_invoke_count",17)) {
         FARF(ALWAYS, "fastrpc.dsp.%-20s : %lld \n", token, pdsp->data[ii]);
      } else {
         FARF(ALWAYS, "fastrpc.dsp.%-20s : %lld us\n", token, pdsp->data[ii]);
      }
   }
bail:
   return;
}

void fastrpc_perf_update(int dev, remote_handle handle, uint32_t sc) {
   int nErr = 0;
   struct fastrpc_perf *p = &gperf;

   if (!(p->perf_on && !IS_STATIC_HANDLE(handle) && p->freq > 0))
      return;

   p->count++;
   if (p->count % p->freq != 0)
      return;

   if (p->kernel.enable)
      get_perf_kernel(dev, handle, sc);

   if (p->dsp.enable)
      get_perf_adsp(handle, sc);
bail:
   return;
}

static int perf_dsp_enable(void) {
   int nErr = 0;
   int numKeys = 0, maxLen = 0;
   char *keys = NULL;
   int ii;

   keys = (char *)rpcmem_alloc_internal(0, RPCMEM_HEAP_DEFAULT, PERF_KEY_STR_MAX);
   VERIFY(gperf.dsp.keys = keys);
   std_memset(keys, 0, PERF_KEY_STR_MAX);
   
   VERIFY(0 == adsp_perf_get_keys(keys, PERF_KEY_STR_MAX, &maxLen, &numKeys));
   VERIFY(maxLen < PERF_KEY_STR_MAX);
   gperf.dsp.maxLen = maxLen;
   gperf.dsp.numKeys = numKeys;
   for(ii = 0; ii < numKeys; ii++) {
      char *name = keys;
      keys += strlen(name) + 1;
      if (IS_KEY_ENABLED(name))
         VERIFY(0 == adsp_perf_enable(ii));
   }
   FARF(HIGH, "keys enable done maxLen %d numKeys %d", maxLen, numKeys);
bail:
   return nErr;
}

int fastrpc_perf_init(int dev) {
   int nErr = 0;
   struct fastrpc_perf *p = &gperf;
   struct perf_keys *pk = &gperf.kernel;
   struct perf_keys *pd = &gperf.dsp;

   pk->enable = FASTRPC_PROPERTY_GET_INT32(PERF_KEY_KERNEL, 0);
   pd->enable = FASTRPC_PROPERTY_GET_INT32(PERF_KEY_ADSP, 0);
   p->perf_on = (pk->enable || pd->enable) ? PERF_MODE : PERF_OFF;
   p->freq = FASTRPC_PROPERTY_GET_INT32(PERF_KEY_FREQ, 1000);
   VERIFY(p->freq > 0);

   p->count = 0;
   if (pk->enable) {
      //VERIFY(!ioctl(dev, FASTRPC_IOCTL_SETMODE, PERF_MODE));
      VERIFY(NULL != (pk->keys = (char *)calloc(sizeof(char), PERF_KEY_STR_MAX)));
      VERIFY(0 == perf_kernel_getkeys(dev));
   }

   if (pd->enable)
      perf_dsp_enable();
bail:
   if (nErr) {
      FARF(HIGH, "fastrpc perf init failed");
      p->perf_on = 0;
   }
   return nErr;
}

void fastrpc_perf_deinit(void) {
   struct fastrpc_perf *p = &gperf;
   if (p->kernel.keys){
      free(p->kernel.keys);
      p->kernel.keys = NULL;
   }
   if (p->dsp.keys){
      rpcmem_free_internal(p->dsp.keys);
      p->dsp.keys = NULL;
   }
   return;
}

