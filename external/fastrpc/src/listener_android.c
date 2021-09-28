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

//#include "qurt_mutex.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/eventfd.h>

#include "platform_libs.h"
#include "HAP_farf.h"
#include "verify.h"
#include "mod_table.h"
#include "remote_priv.h"
#include "rpcmem.h"
#include "adsp_listener.h"
#include "listener_buf.h"
#include "shared.h"
#include "AEEstd.h"
#include "fastrpc_apps_user.h"
#include "AEEStdErr.h"

#define LOGL(format, ...) VERIFY_PRINT_INFO(format, ##__VA_ARGS__)
#ifndef MALLOC
#define MALLOC malloc
#endif

#ifndef CALLOC
#define CALLOC calloc
#endif

#ifndef FREE
#define FREE free
#endif

#ifndef REALLOC
#define REALLOC realloc
#endif

#ifndef FREEIF
#define FREEIF(pv) \
   do {\
      if(pv) { \
         void* tmp = (void*)pv;\
         pv = 0;\
         FREE(tmp);\
      } \
   } while(0)
#endif

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/eventfd.h>

struct listener {
   pthread_t thread;
   int eventfd;
};

static struct listener linfo[NUM_DOMAINS_EXTEND] =
{ [0 ... NUM_DOMAINS_EXTEND - 1] = { .thread = 0, .eventfd = -1 } };

//TODO: fix this to work over any number of buffers
//      needs qaic to support extra buffers
#define MAX_BUFS 250
struct invoke_bufs {
   adsp_listener_buffer outbufs[MAX_BUFS];
   adsp_listener_buffer inbufs[MAX_BUFS];
   int inbufLenReqs[MAX_BUFS];
   int outbufLenReqs[MAX_BUFS];
   remote_arg args[2*MAX_BUFS];
};

extern void set_thread_context(int domain);

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_remotectl_open)(const char* name, uint32* handle, char* dlStr, int dlerrorLen, int* dlErr) __QAIC_IMPL_ATTRIBUTE
{
   return mod_table_open(name, handle, dlStr, dlerrorLen, dlErr);
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_remotectl_close)(uint32 handle, char* errStr, int errStrLen, int* dlErr) __QAIC_IMPL_ATTRIBUTE
{
   return mod_table_close(handle, errStr, errStrLen, dlErr);
}

#define RPC_FREEIF(heapid, buf) \
do {\
   if(heapid == -1)  {\
      FREEIF(buf);\
   } else {\
      if(buf) {\
         rpcmem_free_internal(buf);\
         buf = 0;\
      }\
   }\
} while (0)

static __inline void* rpcmem_realloc(int heapid, uint32 flags, void* buf, int oldsize, int size) {
   if(heapid == -1) {
      return REALLOC(buf, size);
   } else {
      void* bufnew = rpcmem_alloc_internal(heapid, flags, size);
      if(buf && bufnew) {
         memmove(bufnew, buf, oldsize);
         rpcmem_free_internal(buf);
         buf = NULL;
      }
      return bufnew;
   }
}

static void* listener(void* arg) {
   struct listener* me = (struct listener*)arg;
   int numOutBufs = 0;
   int nErr = AEE_SUCCESS;
   adsp_listener_invoke_ctx ctx = 0;
   struct invoke_bufs* bufs = 0;
   boolean bNeedMore;
   int result = -1;
   adsp_listener_remote_handle handle;
   uint32 sc;
   int ii, inBufsAllocated = 0;
   const char* eheap = getenv("ADSP_LISTENER_HEAP_ID");
   int heapid = eheap == 0 ? 0 : (uint32)atoi(eheap);
   const char* eflags = getenv("ADSP_LISTENER_HEAP_FLAGS");
   uint32 flags = eflags == 0 ? RPCMEM_HEAP_DEFAULT : (uint32)atoi(eflags);

   if(eheap || eflags) {
      FARF(HIGH, "listener using ion heap: %d flags: %x\n", (int)heapid, (int)flags);
   }

   VERIFYC(NULL != (bufs = rpcmem_realloc(heapid, flags, 0, 0, sizeof(*bufs))), AEE_ENORPCMEMORY);
   memset(bufs, 0, sizeof(*bufs));
   set_thread_context((int)(me - &linfo[0]));

   do {
 invoke:
      bNeedMore = FALSE;
      sc = 0xffffffff;
      if(result != AEE_SUCCESS) {
         numOutBufs = 0;
      }
      nErr = __QAIC_HEADER(adsp_listener_next_invoke)(
                        ctx, result, bufs->outbufs, numOutBufs, &ctx,
                        &handle, &sc, bufs->inbufs, inBufsAllocated,
                        bufs->inbufLenReqs, MAX_BUFS, bufs->outbufLenReqs, MAX_BUFS);
      if(nErr) {
         VERIFY_EPRINTF("listener protocol failure %x\n", nErr);
         VERIFY(AEE_SUCCESS == (nErr = __QAIC_HEADER(adsp_listener_next_invoke)(
                        ctx, nErr, 0, 0, &ctx,
                        &handle, &sc, bufs->inbufs, inBufsAllocated,
                        bufs->inbufLenReqs, MAX_BUFS, bufs->outbufLenReqs, MAX_BUFS)));
      }

      if(MAX_BUFS < REMOTE_SCALARS_INBUFS(sc) || MAX_BUFS < REMOTE_SCALARS_OUTBUFS(sc)) {
         result = AEE_EMAXBUFS;
         goto invoke;
      }
      for(ii = 0; ii < (int)REMOTE_SCALARS_INBUFS(sc); ++ii) {
         if(bufs->inbufs[ii].dataLen < bufs->inbufLenReqs[ii]) {
            if(0 != bufs->inbufLenReqs[ii]) {
               bufs->inbufs[ii].data = rpcmem_realloc(heapid, flags, bufs->inbufs[ii].data,  bufs->inbufs[ii].dataLen, bufs->inbufLenReqs[ii]);
               if(0 == bufs->inbufs[ii].data) {
                  bufs->inbufs[ii].dataLen = 0;
                  result = AEE_ENORPCMEMORY;
                  goto invoke;
               }
            }
            bufs->inbufs[ii].dataLen = bufs->inbufLenReqs[ii];
            inBufsAllocated = STD_MAX(inBufsAllocated, ii + 1);
            bNeedMore = TRUE;
         }
         bufs->args[ii].buf.pv = bufs->inbufs[ii].data;
         bufs->args[ii].buf.nLen = bufs->inbufLenReqs[ii];
      }
      for(ii = 0; ii < (int)REMOTE_SCALARS_OUTBUFS(sc); ++ii) {
         if(bufs->outbufs[ii].dataLen < bufs->outbufLenReqs[ii]) {
            if(0 !=  bufs->outbufLenReqs[ii]) {
               bufs->outbufs[ii].data = rpcmem_realloc(heapid, flags, bufs->outbufs[ii].data, bufs->outbufs[ii].dataLen, bufs->outbufLenReqs[ii]);
               if(0 == bufs->outbufs[ii].data) {
                  result = AEE_ENORPCMEMORY;
                  goto invoke;
               }
            }
            bufs->outbufs[ii].dataLen = bufs->outbufLenReqs[ii];
         }
         bufs->args[ii + REMOTE_SCALARS_INBUFS(sc)].buf.pv = bufs->outbufs[ii].data;
         bufs->args[ii + REMOTE_SCALARS_INBUFS(sc)].buf.nLen = bufs->outbufLenReqs[ii];
      }
      numOutBufs = REMOTE_SCALARS_OUTBUFS(sc);
      if(bNeedMore) {
         assert(inBufsAllocated >= REMOTE_SCALARS_INBUFS(sc));
         if(0 != (result = __QAIC_HEADER(adsp_listener_invoke_get_in_bufs)(ctx, bufs->inbufs,
                                                                  REMOTE_SCALARS_INBUFS(sc)))) {
            FARF(HIGH, "adsp_listener_invoke_get_in_bufs failed  %x\n", result);
            goto invoke;
         }
      }

      result = mod_table_invoke(handle, sc, bufs->args);
   } while(1);
bail:
   for(ii = 0; ii < MAX_BUFS && bufs; ++ii) {
      RPC_FREEIF(heapid, bufs->outbufs[ii].data);
      RPC_FREEIF(heapid, bufs->inbufs[ii].data);
   }
   RPC_FREEIF(heapid, bufs);
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: listener thread exiting\n", nErr);
   }
   return (void*)(uintptr_t)nErr;
}

static int listener_start_thread(struct listener* me) {
   return pthread_create(&me->thread, 0, listener, (void*)me);
}
#define MIN_BUF_SIZE 0x1000
#define ALIGNB(sz) ((sz) == 0 ? MIN_BUF_SIZE : _SBUF_ALIGN((sz), MIN_BUF_SIZE))

static void* listener2(void* arg) {
   struct listener* me = (struct listener*)arg;
   int nErr = AEE_SUCCESS;
   adsp_listener_invoke_ctx ctx = 0;
   uint8* outBufs = 0;
   int outBufsLen = 0, outBufsCapacity = 0;
   uint8* inBufs = 0;
   int inBufsLen = 0, inBufsLenReq = 0;
   int result = -1;
   adsp_listener_remote_handle handle = -1;
   uint32 sc = 0;
   const char* eheap = getenv("ADSP_LISTENER_HEAP_ID");
   int heapid = eheap == 0 ? -1 : atoi(eheap);
   const char* eflags = getenv("ADSP_LISTENER_HEAP_FLAGS");
   uint32 flags = eflags == 0 ? 0 : (uint32)atoi(eflags);
   const char* emin = getenv("ADSP_LISTENER_MEM_CACHE_SIZE");
   int cache_size = emin == 0 ? 0 : atoi(emin);
   remote_arg args[512];
   struct sbuf buf;
   eventfd_t event = 0xff;

   memset(args, 0, sizeof(args));
   set_thread_context((int)(me - &linfo[0]));
   if(eheap || eflags || emin) {
      FARF(HIGH, "listener using ion heap: %d flags: %x cache: %lld\n", (int)heapid, (int)flags, cache_size);
   }

   do {
 invoke:
      sc = 0xffffffff;
      if(result != 0) {
         outBufsLen = 0;
      }
      FARF(HIGH, "responding message for %x %x %x %x", ctx, handle, sc, result);
      nErr = __QAIC_HEADER(adsp_listener_next2)(
                        ctx, result, outBufs, outBufsLen,
                        &ctx, &handle, &sc, inBufs, inBufsLen, &inBufsLenReq);
      FARF(HIGH, "got message for %x %x %x %x", ctx, handle, sc, nErr);
      if(nErr) {
         VERIFY_EPRINTF("listener protocol failure %x\n", nErr);
         if (nErr == AEE_EINTERRUPTED) {
            goto invoke;
         }
         VERIFY(0 == (nErr = __QAIC_HEADER(adsp_listener_next2)(
                        ctx, nErr, 0, 0,
                        &ctx, &handle, &sc, inBufs, inBufsLen,
                        &inBufsLenReq)));
      }
      if(ALIGNB(inBufsLenReq * 2) < inBufsLen && inBufsLen > cache_size) {
         void* buf;
         int size = ALIGNB(inBufsLenReq * 2);
         if(NULL == (buf = rpcmem_realloc(heapid, flags, inBufs, inBufsLen, size))) {
            result = AEE_ENORPCMEMORY;
            FARF(HIGH, "rpcmem_realloc shrink failed");
            goto invoke;
         }
         inBufs = buf;
         inBufsLen = size;
      }
      if(inBufsLenReq > inBufsLen) {
         void* buf;
         int req;
         int oldLen = inBufsLen;
         int size = _SBUF_ALIGN(inBufsLenReq, MIN_BUF_SIZE);
         if(AEE_SUCCESS == (buf = rpcmem_realloc(heapid, flags, inBufs, inBufsLen, size))) {
            result = AEE_ENORPCMEMORY;
            FARF(ERROR, "rpcmem_realloc failed");
            goto invoke;
         }
         inBufs = buf;
         inBufsLen = size;
         if(0 != (result = __QAIC_HEADER(adsp_listener_get_in_bufs2)(ctx, oldLen,
                                                                     inBufs + oldLen,
                                                                     inBufsLen - oldLen, &req))) {
            FARF(HIGH, "adsp_listener_invoke_get_in_bufs2 failed  %x", result);
            goto invoke;
         }
         if(req > inBufsLen) {
            result = AEE_EBADSIZE;
            FARF(HIGH, "adsp_listener_invoke_get_in_bufs2 failed  %x", result);
            goto invoke;
         }
      }
      if(REMOTE_SCALARS_INHANDLES(sc) + REMOTE_SCALARS_OUTHANDLES(sc) > 0) {
         result = AEE_EINVARGS;
         goto invoke;
      }

      sbuf_init(&buf, 0, inBufs, inBufsLen);
      unpack_in_bufs(&buf, args,  REMOTE_SCALARS_INBUFS(sc));
      unpack_out_lens(&buf, args + REMOTE_SCALARS_INBUFS(sc), REMOTE_SCALARS_OUTBUFS(sc));

      sbuf_init(&buf, 0, 0, 0);
      pack_out_bufs(&buf, args + REMOTE_SCALARS_INBUFS(sc), REMOTE_SCALARS_OUTBUFS(sc));
      outBufsLen = sbuf_needed(&buf);

      if(ALIGNB(outBufsLen*2) < outBufsCapacity && outBufsCapacity > cache_size) {
         void* buf;
         int size = ALIGNB(outBufsLen*2);
         if(NULL == (buf = rpcmem_realloc(heapid, flags, outBufs, outBufsCapacity, size))) {
            result = AEE_ENORPCMEMORY;
            FARF(HIGH, "listener rpcmem_realloc shrink failed");
            goto invoke;
         }
         outBufs = buf;
         outBufsCapacity = size;
      }
      if(outBufsLen > outBufsCapacity) {
         void* buf;
         int size = ALIGNB(outBufsLen);
         if(NULL == (buf = rpcmem_realloc(heapid, flags, outBufs, outBufsCapacity, size))) {
            result = AEE_ENORPCMEMORY;
            FARF(ERROR, "listener rpcmem_realloc failed");
            goto invoke;
         }
         outBufs = buf;
         outBufsLen = size;
         outBufsCapacity = size;
      }
      sbuf_init(&buf, 0, outBufs, outBufsLen);
      pack_out_bufs(&buf, args + REMOTE_SCALARS_INBUFS(sc), REMOTE_SCALARS_OUTBUFS(sc));

      result = mod_table_invoke(handle, sc, args);
   } while(1);
bail:
   RPC_FREEIF(heapid, outBufs);
   RPC_FREEIF(heapid, inBufs);
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: listener thread exited", nErr);
   }
   eventfd_write(me->eventfd, event);
   dlerror();
   return (void*)(uintptr_t)nErr;
}
static int listener_start_thread2(struct listener* me) {
   return pthread_create(&me->thread, 0, listener2, (void*)me);
}

extern int apps_remotectl_skel_invoke(uint32 _sc, remote_arg* _pra);
extern int apps_std_skel_invoke(uint32 _sc, remote_arg* _pra);
extern int apps_mem_skel_invoke(uint32 _sc, remote_arg* _pra);
extern int adspmsgd_apps_skel_invoke(uint32_t _sc, remote_arg* _pra);

#include "adsp_listener_stub.c"
PL_DEP(mod_table)
PL_DEP(apps_std);

void listener_android_deinit(void) {
   PL_DEINIT(mod_table);
   PL_DEINIT(apps_std);
}

int listener_android_init(void) {
   int nErr = 0;

   VERIFY(AEE_SUCCESS == (nErr = PL_INIT(mod_table)));
   VERIFY(AEE_SUCCESS == (nErr = PL_INIT(apps_std)));
   VERIFY(AEE_SUCCESS == (nErr = mod_table_register_const_handle(0, "apps_remotectl", apps_remotectl_skel_invoke)));
   VERIFY(AEE_SUCCESS == (nErr = mod_table_register_static("apps_std", apps_std_skel_invoke)));
   VERIFY(AEE_SUCCESS == (nErr = mod_table_register_static("apps_mem", apps_mem_skel_invoke)));
   VERIFY(AEE_SUCCESS == (nErr = mod_table_register_static("adspmsgd_apps", adspmsgd_apps_skel_invoke)));
bail:
   if(nErr != AEE_SUCCESS) {
      listener_android_deinit();
      VERIFY_EPRINTF("Error %x: fastrpc listener initialization error", nErr);
   }
   return nErr;
}

void listener_android_domain_deinit(int domain) {
   struct listener* me = &linfo[domain];

   FARF(HIGH, "fastrpc listener joining to exit");
   if(me->thread) {
      pthread_join(me->thread, 0);
      me->thread = 0;
   }
   FARF(HIGH, "fastrpc listener joined");
   if(me->eventfd != -1) {
      close(me->eventfd);
      me->eventfd = -1;
   }
}

int listener_android_domain_init(int domain) {
   struct listener* me = &linfo[domain];
   int nErr = 0;

   VERIFYC(-1 != (me->eventfd = eventfd(0, 0)), AEE_EINVALIDFD);
   nErr = __QAIC_HEADER(adsp_listener_init2)();
   if(AEE_EUNSUPPORTEDAPI == nErr) {
      FARF(HIGH, "listener2 initialization error falling back to listener1 %x", nErr);
      VERIFY(AEE_SUCCESS == (nErr = __QAIC_HEADER(adsp_listener_init)()));
      VERIFY(AEE_SUCCESS == (nErr = listener_start_thread(me)));
   } else if(AEE_SUCCESS == nErr) {
      FARF(HIGH, "listener2 initialized for domain %d", domain);
      VERIFY(AEE_SUCCESS == (nErr = listener_start_thread2(me)));
   }

bail:
   if(nErr != AEE_SUCCESS) {
      VERIFY_EPRINTF("Error %x: listener android domain init failed. domain %d\n", nErr, domain);
      listener_android_domain_deinit(domain);
   }
   return nErr;
}

int listener_android_geteventfd(int domain, int *fd) {
   struct listener* me = &linfo[domain];
   int nErr = 0;

   VERIFYC(-1 != me->eventfd, AEE_EINVALIDFD);
   *fd = me->eventfd;
bail:
   if (nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: listener android getevent file descriptor failed for domain %d\n", nErr, domain);
   }
   return nErr;
}

PL_DEFINE(listener_android, listener_android_init, listener_android_deinit)
