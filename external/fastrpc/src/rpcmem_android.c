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

#include "rpcmem.h"
#include "verify.h"
#include "fastrpc_internal.h"
#include "AEEQList.h"
#include "AEEstd.h"
#include "apps_std.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#define PAGE_SIZE 4096
#define PAGE_MASK ~((uintptr_t)PAGE_SIZE - 1)

static QList rpclst;
static pthread_mutex_t rpcmt;
struct rpc_info
{
	QNode qn;
	void *buf;
	void *aligned_buf;
	int size;
	int fd;
};

extern int open_device_node(int domain);
static int rpcmem_open_dev()
{
	return open_device_node(3);
}

void rpcmem_init()
{
	int fd;
	QList_Ctor(&rpclst);
	pthread_mutex_init(&rpcmt, 0);
}

void rpcmem_deinit()
{
	pthread_mutex_destroy(&rpcmt);
}

int rpcmem_to_fd_internal(void *po) {
	struct rpc_info *rinfo, *rfree = 0;
	QNode *pn, *pnn;

	pthread_mutex_lock(&rpcmt);
	QLIST_NEXTSAFE_FOR_ALL(&rpclst, pn, pnn)
	{
		rinfo = STD_RECOVER_REC(struct rpc_info, qn, pn);
		if (rinfo->aligned_buf == po)
		{
			rfree = rinfo;
			break;
		}
	}
	pthread_mutex_unlock(&rpcmt);

	if (rfree)
		return rfree->fd;

	return -1;
}

int rpcmem_to_fd(void *po) {
	return rpcmem_to_fd_internal(po);
}


void *rpcmem_alloc_internal(int heapid, uint32 flags, int size)
{
	struct rpc_info *rinfo;
	struct fastrpc_alloc_dma_buf buf;
	int nErr = 0;
	(void)heapid;
	(void)flags;
	int dev = rpcmem_open_dev();

	VERIFY(0 != (rinfo = calloc(1, sizeof(*rinfo))));

	buf.size = size + PAGE_SIZE;
	buf.fd = -1;
	buf.flags = 0;

	VERIFY((0 == ioctl(dev, FASTRPC_IOCTL_ALLOC_DMA_BUFF, (unsigned long)&buf)) || errno == ENOTTY);
	VERIFY(0 != (rinfo->buf = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, buf.fd, 0)));
	rinfo->fd = buf.fd;
	rinfo->aligned_buf = (void *)(((uintptr_t)rinfo->buf /*+ PAGE_SIZE*/) & PAGE_MASK);
	rinfo->aligned_buf = rinfo->buf;
	rinfo->size = size;
	pthread_mutex_lock(&rpcmt);
	QList_AppendNode(&rpclst, &rinfo->qn);
	pthread_mutex_unlock(&rpcmt);

	return rinfo->aligned_buf;
bail:
	if (nErr)
	{
		if (rinfo)
		{
			if (rinfo->buf)
			{
				free(rinfo->buf);
			}
			free(rinfo);
		}
	}
	return 0;
}

void rpcmem_free_internal(void *po)
{
	struct rpc_info *rinfo, *rfree = 0;
	QNode *pn, *pnn;
	int nErr = 0;

	pthread_mutex_lock(&rpcmt);
	QLIST_NEXTSAFE_FOR_ALL(&rpclst, pn, pnn)
	{
		rinfo = STD_RECOVER_REC(struct rpc_info, qn, pn);
		if (rinfo->aligned_buf == po)
		{
			rfree = rinfo;
			QNode_Dequeue(&rinfo->qn);
			break;
		}
	}
	pthread_mutex_unlock(&rpcmt);
	if (rfree)
	{
		int dev = rpcmem_open_dev();

		munmap(rfree->buf, rfree->size);
		free(rfree);
	}
bail:
	return;

}

void rpcmem_free(void* po) {
    rpcmem_free_internal(po);
}

void* rpcmem_alloc(int heapid, uint32 flags, int size) {
	return rpcmem_alloc_internal(heapid, flags, size);
}
