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
#ifndef VERIFY_PRINT_ERROR
#define VERIFY_PRINT_ERROR
#endif /* VERIFY_PRINT_ERROR */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include "apps_mem.h"
#include "remote64.h"
#include "rpcmem.h"
#include "verify.h"
#include "rpcmem.h"
#include "AEEQList.h"
#include "AEEstd.h"
#include "AEEStdErr.h"
#include "fastrpc_apps_user.h"
#include "platform_libs.h"

#define ADSP_MMAP_HEAP_ADDR 4
#define ADSP_MMAP_REMOTE_HEAP_ADDR 8
#define ADSP_MMAP_ADD_PAGES   0x1000

static QList memlst;
static pthread_mutex_t memmt;

struct mem_info {
   QNode qn;
   uint64 vapps;
   uint64 vadsp;
   int32 size;
   int32 mapped;
};

/*
These should be called in some static constructor of the .so that
uses rpcmem.

I moved them into fastrpc_apps_user.c because there is no gurantee in
the order of when constructors are called.
*/

static int apps_mem_init(void) {
   QList_Ctor(&memlst);
   pthread_mutex_init(&memmt, 0);
   return AEE_SUCCESS;
}

void apps_mem_deinit(void) {
   QNode *pn;
   while ((pn = QList_PopZ(&memlst)) != NULL) {
      struct mem_info *mfree = STD_RECOVER_REC(struct mem_info, qn, pn);

      if (mfree->vapps) {
         if(mfree->mapped) {
            munmap((void*)(uintptr_t)mfree->vapps, mfree->size);
         } else {
            rpcmem_free_internal((void *)(uintptr_t)mfree->vapps);
         }
      }
      free(mfree);
      mfree = NULL;
   }
   pthread_mutex_destroy(&memmt);
}
PL_DEFINE(apps_mem, apps_mem_init, apps_mem_deinit);

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_mem_request_map64)(int heapid, uint32 lflags, uint32 rflags, uint64 vin, int64 len, uint64* vapps, uint64* vadsp) __QAIC_IMPL_ATTRIBUTE {
   struct mem_info *minfo = 0;
   int nErr = 0;
   void* buf = 0;
   uint64_t pbuf;
   int fd = -1;

   (void)vin;
   VERIFYC(NULL != (minfo = malloc(sizeof(*minfo))), AEE_ENOMEMORY);
   QNode_CtorZ(&minfo->qn);
   *vadsp = 0;
   if (rflags == ADSP_MMAP_HEAP_ADDR || rflags == ADSP_MMAP_REMOTE_HEAP_ADDR) {
	VERIFY(AEE_SUCCESS == (nErr = remote_mmap64(-1, rflags, 0, len, (uint64_t*)vadsp)));
	*vapps = 0;
	minfo->vapps = 0;
   } else {
	if ((rflags != ADSP_MMAP_ADD_PAGES) ||
		((rflags == ADSP_MMAP_ADD_PAGES) && !is_kernel_alloc_supported(-1, -1))) {
		VERIFYC(NULL != (buf = rpcmem_alloc_internal(heapid, lflags, len)), AEE_ENORPCMEMORY);
		fd = rpcmem_to_fd_internal(buf);
		VERIFYC(fd > 0, AEE_EINVALIDFD);
	}
	VERIFY(AEE_SUCCESS == (nErr = remote_mmap64(fd, rflags, (uint64_t)buf, len, (uint64_t*)vadsp)));
	pbuf = (uint64_t)buf;
	*vapps = pbuf;
	minfo->vapps = *vapps;
   }
   minfo->vadsp = *vadsp;
   minfo->size = len;
   minfo->mapped = 0;
   pthread_mutex_lock(&memmt);
   QList_AppendNode(&memlst, &minfo->qn);
   pthread_mutex_unlock(&memmt);
bail:
   if(nErr) {
      if(buf) {
         rpcmem_free_internal(buf);
         buf = NULL;
      }
      if(minfo) {
         free(minfo);
         minfo = NULL;
      }
      VERIFY_EPRINTF("Error %x: apps_mem_request_mmap64 failed\n", nErr);
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_mem_request_map)(int heapid, uint32 lflags, uint32 rflags, uint32 vin, int32 len, uint32* vapps, uint32* vadsp) __QAIC_IMPL_ATTRIBUTE {
   uint64 vin1, vapps1, vadsp1;
   int64 len1;
   int nErr = AEE_SUCCESS;
   vin1 = (uint64)vin;
   len1 = (int64)len;
   nErr = apps_mem_request_map64(heapid, lflags, rflags, vin1, len1, &vapps1, &vadsp1);
   *vapps = (uint32)vapps1;
   *vadsp = (uint32)vadsp1;
   if(nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: apps_mem_request_map failed\n", nErr);
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_mem_request_unmap64)(uint64 vadsp, int64 len) __QAIC_IMPL_ATTRIBUTE {
   int nErr = AEE_SUCCESS;
   struct mem_info *minfo, *mfree = 0;
   QNode *pn, *pnn;
   VERIFY(0 == (nErr = remote_munmap64((uint64_t)vadsp, len)));
   pthread_mutex_lock(&memmt);
   QLIST_NEXTSAFE_FOR_ALL(&memlst, pn, pnn) {
      minfo = STD_RECOVER_REC(struct mem_info, qn, pn);
      if(minfo->vadsp == vadsp) {
         mfree = minfo;
         QNode_Dequeue(&minfo->qn);
         break;
      }
   }
   pthread_mutex_unlock(&memmt);
   VERIFYC(mfree, AEE_ENOSUCHMAP);
   if(mfree->mapped) {
      munmap((void*)(uintptr_t)mfree->vapps, mfree->size);
   } else {
      if (mfree->vapps)
         rpcmem_free_internal((void *)(uintptr_t)mfree->vapps);
   }
   free(mfree);
   mfree = NULL;
bail:
   if(nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: apps_mem_request_unmap64 failed\n", nErr);
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_mem_request_unmap)(uint32 vadsp, int32 len) __QAIC_IMPL_ATTRIBUTE {
   uint64 vadsp1 = (uint64)vadsp;
   int64 len1 = (int64)len;
   int nErr = apps_mem_request_unmap64(vadsp1, len1);
   if(nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: apps_mem_request_unmap failed\n", nErr);
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_mem_share_map)(int fd, int size, uint64* vapps, uint64* vadsp) __QAIC_IMPL_ATTRIBUTE {
   struct mem_info *minfo = 0;
   int nErr = AEE_SUCCESS;
   void* buf = 0;
   uint64_t pbuf;
   VERIFYC(0 != (minfo = malloc(sizeof(*minfo))), AEE_ENOMEMORY);
   QNode_CtorZ(&minfo->qn);
   VERIFYC(fd > 0, AEE_EINVALIDFD);
   *vadsp = 0;
   VERIFYC(MAP_FAILED != (buf = (void *)mmap(NULL, size,
                           PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)), AEE_EMMAP);
   VERIFY(AEE_SUCCESS == (nErr = remote_mmap64(fd, 0, (uint64_t)buf, size, (uint64_t*)vadsp)));
   pbuf = (uint64_t)buf;
   *vapps = pbuf;
   minfo->vapps = *vapps;
   minfo->vadsp = *vadsp;
   minfo->size = size;
   minfo->mapped = 1;
   pthread_mutex_lock(&memmt);
   QList_AppendNode(&memlst, &minfo->qn);
   pthread_mutex_unlock(&memmt);
bail:
   if(nErr) {
      if(buf) {
         munmap(buf, size);
      }
      if(minfo) {
         free(minfo);
         minfo = NULL;
      }
      VERIFY_EPRINTF("Error %x: apps_mem_share_map failed\n", nErr);
   }
   return nErr;
}

__QAIC_IMPL_EXPORT int __QAIC_IMPL(apps_mem_share_unmap)(uint64 vadsp, int size) __QAIC_IMPL_ATTRIBUTE {
   int64 len1 = (int64)size;
   int nErr = AEE_SUCCESS;
   nErr = apps_mem_request_unmap64(vadsp, len1);
   if(nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: apps_mem_share_unmap failed\n", nErr);
   }
   return nErr;
}
