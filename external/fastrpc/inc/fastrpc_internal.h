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

#ifndef FASTRPC_INTERNAL_H
#define FASTRPC_INTERNAL_H

#include <linux/types.h>
#include "remote64.h"
#include "verify.h"
#include "AEEstd.h"

#define FASTRPC_IOCTL_ALLOC_DMA_BUFF 	_IOWR('R', 1, struct fastrpc_alloc_dma_buf)
#define FASTRPC_IOCTL_FREE_DMA_BUFF 	_IOWR('R', 2, uint32_t)
#define FASTRPC_IOCTL_INVOKE        	_IOWR('R', 3, struct fastrpc_invoke)
#define FASTRPC_IOCTL_INIT_ATTACH 		_IO('R', 4)
#define FASTRPC_IOCTL_INIT_CREATE 		_IOWR('R', 5, struct fastrpc_init_create)
#define FASTRPC_IOCTL_MMAP				_IOWR('R', 6, struct fastrpc_ioctl_mmap)
#define FASTRPC_IOCTL_MUNMAP			_IOWR('R', 7, struct fastrpc_ioctl_munmap)


#define DEVICE_NAME "adsprpc-smd"

#if !(defined __qdsp6__) && !(defined __hexagon__)
static __inline uint32 Q6_R_cl0_R(uint32 num) {
   int ii;
   for(ii = 31; ii >= 0; --ii) {
      if(num & (1 << ii)) {
         return 31 - ii;
      }
   }
   return 0;
}
#else
#include "hexagon_protos.h"
#include <types.h>
#endif

#define FASTRPC_INFO_SMMU   (1 << 0)

/* struct fastrpc_invoke_args {
	__u64 ptr;
	__u64 length;
	__s32 fd;
	__u32 attrs;
	__u32 crc;
}; */

struct fastrpc_invoke_args {
	__u64 ptr;
	__u64 length;
	__s32 fd;
	__u32 reserved;
};

struct fastrpc_invoke {
	__u32 handle;
	__u32 sc;
	__u64 args;
};

#define FASTRPC_ATTR_NOVA (1)
#define FASTRPC_ATTR_NOMAP (16)

#define GUEST_OS   			0
#define USER_PD   			-1
#define STATIC_USER_PD  	1
#define ATTACH_SENSORS_PD  	2
#define GUEST_OS_SHARED  	3

struct fastrpc_init_create {
	__u32 filelen;	/* elf file length */
	__s32 filefd;	/* fd for the file */
	__u32 attrs;
	__u32 siglen;
	__u64 file;	/* pointer to elf file */
};

#define FASTRPC_ATTR_DEBUG_PROCESS (1)

struct fastrpc_alloc_dma_buf {
	__s32 fd;	/* fd */
	__u32 flags;	/* flags to map with */
	__u64 size;	/* size */
};

struct fastrpc_ioctl_mmap {
	__s32 fd;	/* fd */
	__u32 flags;	/* flags for dsp to map with */
	__u64 vaddrin;	/* optional virtual address */
	__u64 size;	/* size */
	__u64 vaddrout;	/* dsps virtual address */
};

struct fastrpc_ioctl_munmap {
	__u64 vaddrout;	/* address to unmap */
	__u64 size;	/* size */
};

#define FASTRPC_CONTROL_LATENCY	(1)
struct fastrpc_ctrl_latency {
	uint32_t enable;	//!latency control enable
	uint32_t level;		//!level of control
};
#define FASTRPC_CONTROL_SMMU	(2)
struct fastrpc_ctrl_smmu {
	uint32_t sharedcb;
};

#define FASTRPC_CONTROL_KALLOC	(3)
struct fastrpc_ctrl_kalloc {
	uint32_t kalloc_support;
};

struct fastrpc_ioctl_control {
	uint32_t req;
	union {
		struct fastrpc_ctrl_latency lp;
		struct fastrpc_ctrl_smmu smmu;
		struct fastrpc_ctrl_kalloc kalloc;
	};
};

#define FASTRPC_SMD_GUID "fastrpcsmd-apps-dsp"

struct smq_null_invoke32 {
   uint32_t ctx;         //! invoke caller context
   remote_handle handle; //! handle to invoke
   uint32_t sc;          //! scalars structure describing the rest of the data
};

struct smq_null_invoke {
   uint64_t ctx;         //! invoke caller context
   remote_handle handle; //! handle to invoke
   uint32_t sc;          //! scalars structure describing the rest of the data
};

typedef uint32_t smq_invoke_buf_phy_addr;

struct smq_phy_page {
   uint64_t addr; //! physical address
   int64_t size;  //! size
};

struct smq_phy_page32 {
   uint32_t addr; //! physical address
   uint32_t size; //! size
};

struct smq_invoke_buf {
   int num;
   int pgidx;
};

struct smq_invoke32 {
   struct smq_null_invoke32 header;
   struct smq_phy_page32 page;   //! remote arg and list of pages address
};

struct smq_invoke {
   struct smq_null_invoke header;
   struct smq_phy_page page;     //! remote arg and list of pages address
};

struct smq_msg32 {
   uint32_t pid;
   uint32_t tid;
   struct smq_invoke32 invoke;
};

struct smq_msg {
   uint32_t pid;
   uint32_t tid;
   struct smq_invoke invoke;
};

struct smq_msg_u {
   union {
      struct smq_msg32 msg32;
      struct smq_msg msg64;
   } msg;
   int size;
};

struct smq_invoke_rsp32 {
   uint32_t ctx;                 //! invoke caller context
   int nRetVal;                  //! invoke return value
};

struct smq_invoke_rsp {
   uint64_t ctx;                 //! invoke caller context
   int nRetVal;                  //! invoke return value
};

struct smq_invoke_rsp_u {
   union {
      struct smq_invoke_rsp32 rsp32;
      struct smq_invoke_rsp rsp64;
   } rsp;
   int size;
};

static __inline void to_smq_msg(uint32 mode, struct smq_msg_u* msg, struct smq_msg* msg64) {
   if(0 == mode) {
      msg64->pid = msg->msg.msg32.pid;
      msg64->tid = msg->msg.msg32.tid;
      msg64->invoke.header.ctx = msg->msg.msg32.invoke.header.ctx;
      msg64->invoke.header.handle = msg->msg.msg32.invoke.header.handle;
      msg64->invoke.header.sc = msg->msg.msg32.invoke.header.sc;
      msg64->invoke.page.addr = msg->msg.msg32.invoke.page.addr;
      msg64->invoke.page.size = msg->msg.msg32.invoke.page.size;
   } else {
      std_memmove(msg64, &msg->msg.msg64, sizeof(*msg64));
   }
}

static __inline void to_smq_invoke_rsp(uint32 mode, uint64 ctx, int nRetVal, struct smq_invoke_rsp_u* rsp) {
   if (0 == mode) {
      rsp->rsp.rsp32.ctx = (uint32)ctx;
      rsp->rsp.rsp32.nRetVal = nRetVal;
      rsp->size = sizeof(rsp->rsp.rsp32);
   } else {
      rsp->rsp.rsp64.ctx = ctx;
      rsp->rsp.rsp64.nRetVal = nRetVal;
      rsp->size = sizeof(rsp->rsp.rsp64);
   }
}

static __inline struct smq_invoke_buf* to_smq_invoke_buf_start(uint32 mode, void* virt, uint32 sc) {
   struct smq_invoke_buf* buf;
   int len = REMOTE_SCALARS_LENGTH(sc);
   if(0 == mode) {
      remote_arg* pra = (remote_arg*)virt;
      buf = (struct smq_invoke_buf*)(&pra[len]);
   } else {
      remote_arg64* pra = (remote_arg64*)virt;
      buf = (struct smq_invoke_buf*)(&pra[len]);
   }
   return buf;
}

static __inline struct smq_invoke_buf* smq_invoke_buf_start(remote_arg64 *pra, uint32 sc) {
   int len = REMOTE_SCALARS_LENGTH(sc);
   return (struct smq_invoke_buf*)(&pra[len]);
}

static __inline struct smq_phy_page* smq_phy_page_start(uint32 sc, struct smq_invoke_buf* buf) {
   int nTotal =  REMOTE_SCALARS_INBUFS(sc) + REMOTE_SCALARS_OUTBUFS(sc);
   return (struct smq_phy_page*)(&buf[nTotal]);
}

//! size of the out of band data
static __inline int smq_data_size(uint32 sc, int nPages) {
   struct smq_invoke_buf* buf = smq_invoke_buf_start(0, sc);
   struct smq_phy_page* page = smq_phy_page_start(sc, buf);
   return (int)(uintptr_t)(&(page[nPages]));
}

static __inline void to_smq_data(uint32 mode, uint32 sc, int nPages, void* pv, remote_arg64* rpra) {
   if(0 == mode) {
      struct smq_phy_page* page;
      struct smq_phy_page32* page32;
      remote_arg *pra = (remote_arg*)pv;
      int ii, len;
      len = REMOTE_SCALARS_LENGTH(sc);
      for(ii = 0; ii < len; ++ii) {
         rpra[ii].buf.pv = (uint64)(uintptr_t)pra[ii].buf.pv;
         rpra[ii].buf.nLen = pra[ii].buf.nLen;
      }
      len = REMOTE_SCALARS_INBUFS(sc) + REMOTE_SCALARS_OUTBUFS(sc);
      std_memmove(&rpra[ii], &pra[ii], len * sizeof(struct smq_invoke_buf));
      page = (struct smq_phy_page*)((struct smq_invoke_buf*)&rpra[ii] + len);
      page32 = (struct smq_phy_page32*)((struct smq_invoke_buf*)&pra[ii] + len);
      for(ii = 0; ii < nPages; ++ii) {
         page[ii].addr = page32[ii].addr;
         page[ii].size = page32[ii].size;
      }
   } else {
      std_memmove(rpra, pv, smq_data_size(sc, nPages));
   }
}

#endif // FASTRPC_INTERNAL_H
