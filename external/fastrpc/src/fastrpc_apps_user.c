/*
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>

#define FARF_ERROR 1
//#define FARF_HIGH 1
#include "HAP_farf.h"
#include "verify.h"
#include "remote_priv.h"
#include "shared.h"
#include "fastrpc_internal.h"
#include "fastrpc_apps_user.h"
#include "adsp_current_process.h"
#include "adsp_current_process1.h"
#include "adspmsgd_adsp1.h"
#include "remotectl.h"
#include "rpcmem.h"
#include "AEEstd.h"
#include "AEEStdErr.h"
#include "AEEQList.h"
#include "apps_std.h"
#include "platform_libs.h"
#include "fastrpc_perf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#ifndef _WIN32
#include <pthread.h>
#include <sys/inotify.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <sys/mman.h>
#endif // __WIN32

#ifndef INT_MAX
#define INT_MAX (int)(-1)
#endif

#define ADSPRPC_DEVICE "/dev/fastrpc-adsp"
#define SDSPRPC_DEVICE "/dev/fastrpc-sdsp"
#define MDSPRPC_DEVICE "/dev/fastrpc-mdsp"
#define CDSPRPC_DEVICE "/dev/fastrpc-cdsp"

/* Secure and default device nodes */
#define SECURE_DEVICE "/dev/fastrpc-adsp-secure"
#define DEFAULT_DEVICE "/dev/fastrpc-adsp"

#define INVALID_DOMAIN_ID -1
#define INVALID_HANDLE (remote_handle64)(-1)
#define INVALID_KEY    (pthread_key_t)(-1)

#define MAX_DMA_HANDLES 256

#define FASTRPC_TRACE_INVOKE_START "fastrpc_trace_invoke_start"
#define FASTRPC_TRACE_INVOKE_END   "fastrpc_trace_invoke_end"
#define FASTRPC_TRACE_LOG(k, handle, sc) if(fastrpc_trace == 1 && !IS_STATIC_HANDLE(handle)) { \
                                            FARF(ALWAYS, "%s: sc 0x%x", (k), (sc)); } \

#define FASTRPC_MODE_DEBUG			(0x1)
#define FASTRPC_MODE_PTRACE			(0x2)
#define FASTRPC_MODE_CRC			(0x4)
#define FASTRPC_MODE_ADAPTIVE_QOS	(0x10)

#define FASTRPC_DISABLE_QOS		0
#define FASTRPC_PM_QOS			1
#define FASTRPC_ADAPTIVE_QOS	2

/* FastRPC mode for Unsigned module */
#define FASTRPC_MODE_UNSIGNED_MODULE (0x8)

#define M_CRCLIST (64)
#define IS_DEBUG_MODE_ENABLED(var) (var & FASTRPC_MODE_DEBUG)
#define IS_CRC_CHECK_ENABLED(var)  (var & FASTRPC_MODE_CRC)
#define POLY32 0x04C11DB7	// G(x) = x^32+x^26+x^23+x^22+x^16+x^12
                                // +x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1

#define FASTRPC_LATENCY_START      (1)
#define FASTRPC_LATENCY_STOP       (0)
#define FASTRPC_LATENCY_EXIT       (2)
#define FASTRPC_LATENCY_VOTE_ON    (1)
#define FASTRPC_LATENCY_VOTE_OFF   (0)
#define FASTRPC_LATENCY_WAIT_TIME  (1)

#ifdef ANDROID_P
#define FASTRPC_PROP_PROCESS  "vendor.fastrpc.process.attrs"
#define FASTRPC_PROP_TRACE    "vendor.fastrpc.debug.trace"
#define FASTRPC_PROP_TESTSIG  "vendor.fastrpc.debug.testsig"
#else
#define FASTRPC_PROP_PROCESS  "fastrpc.process.attrs"
#define FASTRPC_PROP_TRACE    "fastrpc.debug.trace"
#define FASTRPC_PROP_TESTSIG  "fastrpc.debug.testsig"
#endif

#define DEFAULT_UTHREAD_PRIORITY	0xC0
#define DEFAULT_UTHREAD_STACK_SIZE	16*1024

/* Shell prefix for signed and unsigned */
const char* const SIGNED_SHELL = "fastrpc_shell_";
const char* const UNSIGNED_SHELL = "fastrpc_shell_unsigned_";

struct fastrpc_latency {
	int adaptive_qos;
	int state;
	int exit;
	int invoke;
	int vote;
	int dev;
	int wait_time;
	int latency;
	pthread_t thread;
	pthread_mutex_t mut;
	pthread_mutex_t wmut;
	pthread_cond_t cond;
};

struct fastrpc_thread_params {
	uint32_t prio;
	uint32_t stack_size;
	int reqID;
	pthread_t thread;
};

struct mem_to_fd {
   QNode qn;
   void* buf;
   int size;
   int fd;
   int nova;
   int attr;
   int refcount;
};

struct mem_to_fd_list {
   QList ql;
   pthread_mutex_t mut;
};

struct dma_handle_info {
   int fd;
   int len;
   int used;
   uint32_t attr;
};

struct handle_info {
   QNode qn;
   struct handle_list *hlist;
   remote_handle64 local;
   remote_handle64 remote;
};

struct handle_list {
	QList ql;
	pthread_mutex_t mut;
	pthread_mutex_t init;
	int dsppd;
	char *dsppdname;
	int domainsupport;
	int nondomainsupport;
	int kmem_support;
	int dev;
	int initialized;
	int setmode;
	uint32_t mode;
	uint32_t info;
	void* pdmem;
	remote_handle64 cphandle;
	remote_handle64 msghandle;
	int procattrs;
	struct fastrpc_latency qos;
	struct fastrpc_thread_params th_params;
	int unsigned_module;
};

static struct mem_to_fd_list fdlist;
static struct handle_list *hlist = 0;
static struct dma_handle_info dhandles[MAX_DMA_HANDLES];
static int dma_handle_count = 0;
static pthread_key_t tlsKey = INVALID_KEY;

static int fastrpc_trace = 0;

extern int listener_android_domain_init(int domain);
extern void listener_android_domain_deinit(int domain);
extern int initFileWatcher(int domain);
extern void deinitFileWatcher(int domain);
static int open_dev(int domain);
static void domain_deinit(int domain);
static int __attribute__((constructor)) fastrpc_init_once(void);
remote_handle64 get_adsp_current_process1_handle(int domain);
remote_handle64 get_adspmsgd_adsp1_handle(int domain);
static int remote_unmap_fd(void *buf, int size, int fd, int attr);

static uint32_t crc_table[256];

static void GenCrc32Tab(uint32_t GenPoly, uint32_t *crctab)
{
   uint32_t crc;
   int i, j;

   for (i = 0; i < 256; i++) {
       crc = i<<24;
       for (j = 0; j <8; j++) {
           crc = (crc << 1) ^ (crc & 0x80000000 ? GenPoly : 0);
       }
       crctab[i] = crc;
   }
}

static uint32_t crc32_lut(unsigned char *data, int nbyte, uint32_t *crctab)
{
   uint32_t crc = 0;
   if (!data || !crctab)
      return 0;

   while(nbyte--) {
       crc = (crc<<8) ^ crctab[(crc>>24) ^ *data++];
   }
   return crc;
}

int fastrpc_latency_refinc(struct fastrpc_latency *qp) {
    int nErr = 0;

    if (qp == NULL || qp->state == FASTRPC_LATENCY_STOP)
       goto bail;
    qp->invoke++;
    if (qp->vote == FASTRPC_LATENCY_VOTE_OFF) {
       pthread_mutex_lock(&qp->wmut);
       pthread_cond_signal(&qp->cond);
       pthread_mutex_unlock(&qp->wmut);
    }
bail:
    return 0;
}

static void* fastrpc_latency_thread_handler(void* arg) {
   FARF(ALWAYS, "Unsupported: rpc latency thread exited");
   return NULL;
}

int fastrpc_latency_init(int dev, struct fastrpc_latency *qos) {
   int i, nErr = 0;

   VERIFY(qos && dev != -1);

   qos->dev = dev;
   qos->state = FASTRPC_LATENCY_STOP;
   qos->thread = 0;
   qos->wait_time = FASTRPC_LATENCY_WAIT_TIME;
   pthread_mutex_init(&qos->mut, 0);
   pthread_mutex_init(&qos->wmut, 0);
   pthread_cond_init(&qos->cond, NULL);
bail:
   return nErr;
}

int fastrpc_latency_deinit(struct fastrpc_latency *qos) {
   int nErr = 0;

   VERIFY(qos);
   if (qos->state == FASTRPC_LATENCY_START) {
      pthread_mutex_lock(&qos->wmut);
      qos->exit = FASTRPC_LATENCY_EXIT;
      pthread_cond_signal(&qos->cond);
      pthread_mutex_unlock(&qos->wmut);
      if(qos->thread) {
         pthread_join(qos->thread, 0);
         qos->thread = 0;
         FARF(ALWAYS, "latency thread joined");
      }
      qos->state = FASTRPC_LATENCY_STOP;
      pthread_mutex_destroy(&qos->mut);
      pthread_mutex_destroy(&qos->wmut);
   }
bail:
   return 0;
}

/* Thread function that will be invoked to update remote user PD parameters */
static void *fastrpc_set_remote_uthread_params(void *arg)
{
	int nErr = AEE_SUCCESS, paramsLen = 2;
	struct fastrpc_thread_params *th_params = (struct fastrpc_thread_params*)arg;

	VERIFY(th_params != NULL);
	VERIFY(AEE_SUCCESS == (nErr = remotectl_set_param(th_params->reqID, (uint32_t*)th_params, paramsLen)));
bail:
	if (nErr != AEE_SUCCESS)
		FARF(ERROR, "Error 0x%x: setting remote user thread parameters failed !", nErr);
	return NULL;
}

void *remote_register_fd_attr(int fd, int size, int attr) {
   int nErr = AEE_SUCCESS;
   void *po = NULL;
   void *buf = (void*)-1;
   struct mem_to_fd* tofd = 0;

   VERIFY(!fastrpc_init_once());
   VERIFYC(NULL != (tofd = calloc(1, sizeof(*tofd))), AEE_ENOMEMORY);
   QNode_CtorZ(&tofd->qn);
   VERIFYC((void*)-1 != (buf = mmap(0, size, PROT_NONE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)), AEE_EMMAP);
   tofd->buf = buf;
   tofd->size = size;
   tofd->fd = fd;
   tofd->nova = 1;
   tofd->attr = attr;

   pthread_mutex_lock(&fdlist.mut);
   QList_AppendNode(&fdlist.ql, &tofd->qn);
   pthread_mutex_unlock(&fdlist.mut);

   tofd = 0;
   po = buf;
   buf = (void*)-1;
bail:
   if(buf != (void*)-1)
      munmap(buf, size);
   if(tofd)
   {
      free(tofd);
      tofd = NULL;
   }
   if(nErr != AEE_SUCCESS) {
	FARF(ERROR,"Error %x: remote register fd fails for fd %x, size %x\n", nErr, fd, size);
   }
   return po;
}

void *remote_register_fd(int fd, int size) {
   return remote_register_fd_attr(fd, size, 0);
}

static void remote_register_buf_common(void* buf, int size, int fd, int attr) {
   int nErr = 0;
   VERIFY(!fastrpc_init_once());
   if(fd != -1) {
      struct mem_to_fd* tofd;
      int fdfound = 0;
      QNode* pn, *pnn;

      pthread_mutex_lock(&fdlist.mut);
      QLIST_NEXTSAFE_FOR_ALL(&fdlist.ql, pn, pnn) {
         tofd = STD_RECOVER_REC(struct mem_to_fd, qn, pn);
         if(tofd->buf == buf && tofd->size == size && tofd->fd == fd) {
            fdfound = 1;
            if(attr)
               tofd->attr = attr;
            tofd->refcount++;
            break;
         }
      }
      pthread_mutex_unlock(&fdlist.mut);
      if(!fdfound) {
         VERIFYC(NULL != (tofd = calloc(1, sizeof(*tofd))), AEE_ENOMEMORY);
         QNode_CtorZ(&tofd->qn);
         tofd->buf = buf;
         tofd->size = size;
         tofd->fd = fd;
         if (attr)
            tofd->attr = attr;
         tofd->refcount++;
         pthread_mutex_lock(&fdlist.mut);
         QList_AppendNode(&fdlist.ql, &tofd->qn);
         pthread_mutex_unlock(&fdlist.mut);
      }
   } else {
      QNode* pn, *pnn;
      pthread_mutex_lock(&fdlist.mut);
      QLIST_NEXTSAFE_FOR_ALL(&fdlist.ql, pn, pnn) {
         struct mem_to_fd* tofd = STD_RECOVER_REC(struct mem_to_fd, qn, pn);
         if(tofd->buf == buf && tofd->size == size) {
            tofd->refcount--;
            if(tofd->refcount <= 0) {
               QNode_DequeueZ(&tofd->qn);
	       if (tofd->attr & FASTRPC_ATTR_KEEP_MAP) {
		  remote_unmap_fd(tofd->buf, tofd->size, tofd->fd, tofd->attr);
               }
               if(tofd->nova) {
                  munmap(tofd->buf, tofd->size);
               }
               free(tofd);
               tofd = NULL;
            }
            break;
         }
      }
      pthread_mutex_unlock(&fdlist.mut);
   }
bail:
   if(nErr != AEE_SUCCESS) {
      FARF(ERROR, "Error %x: remote_register_buf failed buf %p, size %d, fd %x", nErr, buf, size, fd);
   }
   return;
}

void remote_register_buf(void* buf, int size, int fd) {
   return remote_register_buf_common(buf, size, fd, 0);
}

void remote_register_buf_attr(void* buf, int size, int fd, int attr) {
   return remote_register_buf_common(buf, size, fd, attr);
}

int remote_register_dma_handle_attr(int fd, uint32_t len, uint32_t attr) {
	int nErr = AEE_SUCCESS, i;
	int fd_found = 0;

	if (attr && attr != FASTRPC_ATTR_NOMAP) {
		FARF(ERROR, "Error: %s failed, unsupported attribute 0x%x", __func__, attr);
		return AEE_EBADPARM;
	}
	VERIFY(!fastrpc_init_once());

	pthread_mutex_lock(&fdlist.mut);
	for(i = 0; i < dma_handle_count; i++) {
		if(dhandles[i].used && dhandles[i].fd == fd) {
			/* If fd already present in handle list, then just update attribute only if its zero */
			if(!dhandles[i].attr) {
				dhandles[i].attr = attr;
			}
			fd_found = 1;
			break;
		}
	}
	pthread_mutex_unlock(&fdlist.mut);

	if (fd_found) {
		return AEE_SUCCESS;
	}

	pthread_mutex_lock(&fdlist.mut);
	for(i = 0; i < dma_handle_count; i++) {
		if(!dhandles[i].used) {
			dhandles[i].fd = fd;
			dhandles[i].len = len;
			dhandles[i].used = 1;
			dhandles[i].attr = attr;
			break;
		}
	}
	if(i == dma_handle_count) {
		if(dma_handle_count >= MAX_DMA_HANDLES) {
			FARF(ERROR, "Error: %s: DMA handle list is already full (count %d)", __func__, dma_handle_count);
			nErr = AEE_EOUTOFHANDLES;
		} else {
			dhandles[dma_handle_count].fd = fd;
			dhandles[dma_handle_count].len = len;
			dhandles[dma_handle_count].used = 1;
			dhandles[dma_handle_count].attr = attr;
			dma_handle_count++;
		}
	}
	pthread_mutex_unlock(&fdlist.mut);

bail:
	if(nErr) {
		FARF(ERROR, "Error 0x%x: %s failed for fd 0x%x, len %d, attr 0x%x", nErr, __func__, fd, len, attr);
	}
	return nErr;
}

int remote_register_dma_handle(int fd, uint32_t len) {
	return remote_register_dma_handle_attr(fd, len, 0);
}

static void unregister_dma_handle(int fd, uint32_t *len, uint32_t *attr) {
	int i, last_used = 0;

	*len = 0;
	*attr = 0;

	pthread_mutex_lock(&fdlist.mut);
	for(i = 0; i < dma_handle_count; i++) {
		if(dhandles[i].used) {
			if(dhandles[i].fd == fd) {
				dhandles[i].used = 0;
				*len = dhandles[i].len;
				*attr = dhandles[i].attr;
				if(i == (dma_handle_count - 1)) {
					dma_handle_count = last_used + 1;
				}
				break;
			} else {
				last_used = i;
			}
		}
	}
	pthread_mutex_unlock(&fdlist.mut);
}

static int fdlist_fd_from_buf(void* buf, int bufLen, int* nova, void** base, int* attr, int* ofd) {
   QNode* pn;
   int fd = -1;
   pthread_mutex_lock(&fdlist.mut);
   QLIST_FOR_ALL(&fdlist.ql, pn) {
      if(fd != -1) {
         break;
      } else {
         struct mem_to_fd* tofd = STD_RECOVER_REC(struct mem_to_fd, qn, pn);
         if(STD_BETWEEN(buf, tofd->buf, (unsigned long)tofd->buf + tofd->size)) {
            if(STD_BETWEEN((unsigned long)buf + bufLen -1, tofd->buf, (unsigned long)tofd->buf + tofd->size)) {
               fd = tofd->fd;
               *nova = tofd->nova;
               *base = tofd->buf;
               *attr = tofd->attr;
            } else {
               pthread_mutex_unlock(&fdlist.mut);
			   FARF(ERROR,"Error %x: Mismatch in buffer address(%p) or size(%x) to the registered FD(%x), address(%p) and size(%x)\n", AEE_EBADPARM, buf, bufLen, tofd->fd, tofd->buf, tofd->size);
               return AEE_EBADPARM;
            }
         }
      }
   }
   *ofd = fd;
   pthread_mutex_unlock(&fdlist.mut);
   return 0;
}

static int verify_local_handle(remote_handle64 local) {
	struct handle_info* hinfo = (struct handle_info*)(uintptr_t)local;
	int nErr = AEE_SUCCESS;

	VERIFYC(hinfo, AEE_EMEMPTR);
	VERIFYC((hinfo->hlist >= &hlist[0]) && (hinfo->hlist < &hlist[NUM_DOMAINS_EXTEND]), AEE_EMEMPTR);
	VERIFYC(QNode_IsQueuedZ(&hinfo->qn), AEE_ENOSUCHHANDLE);
bail:
	if (nErr != AEE_SUCCESS) {
		FARF(HIGH, "Error %x: verify local handle failed. handle %p\n", nErr, &local);
	}
	return nErr;
}

static int get_domain_from_handle(remote_handle64 local, int *domain) {
	struct handle_info *hinfo = (struct handle_info*)(uintptr_t)local;
	int dom, nErr = AEE_SUCCESS;

	VERIFY(AEE_SUCCESS == (nErr = verify_local_handle(local)));
	dom = (int)(hinfo->hlist - &hlist[0]);
	VERIFYC((dom >= 0) && (dom < NUM_DOMAINS_EXTEND), AEE_EINVALIDDOMAIN);
	*domain = dom;
bail:
	if (nErr != AEE_SUCCESS) {
		FARF(HIGH, "Error %x: get domain from handle failed. handle %p\n", nErr, &local);
	}
	return nErr;
}

static int get_domain_from_name(const char *uri) {
   int domain = DEFAULT_DOMAIN_ID;

   if(uri) {
      if(std_strstr(uri, ADSP_DOMAIN)) {
         domain = ADSP_DOMAIN_ID;
      } else if(std_strstr(uri, MDSP_DOMAIN)) {
         domain = MDSP_DOMAIN_ID;
      } else if(std_strstr(uri, SDSP_DOMAIN)) {
         domain = SDSP_DOMAIN_ID;
      } else if(std_strstr(uri, CDSP_DOMAIN)) {
         domain = CDSP_DOMAIN_ID;
      } else {
         domain = INVALID_DOMAIN_ID;
         FARF(ERROR, "invalid domain uri: %s\n", uri);
      }
      if (std_strstr(uri, FASTRPC_SESSION_URI)) {
         domain = domain | FASTRPC_SESSION_ID1;
      }
   }
   VERIFY_IPRINTF("get_domain_from_name: %d\n", domain);
   return domain;
}

static int alloc_handle(int domain, remote_handle64 remote, struct handle_info **info) {
   struct handle_info* hinfo;
   int nErr = AEE_SUCCESS;

   VERIFYC(NULL != (hinfo = malloc(sizeof(*hinfo))), AEE_ENOMEMORY);
   hinfo->local = (remote_handle64)(uintptr_t)hinfo;
   hinfo->remote = remote;
   hinfo->hlist = &hlist[domain];
   QNode_CtorZ(&hinfo->qn);
   pthread_mutex_lock(&hlist[domain].mut);
   QList_PrependNode(&hlist[domain].ql, &hinfo->qn);
   pthread_mutex_unlock(&hlist[domain].mut);
   *info = hinfo;

bail:
   if (nErr != AEE_SUCCESS) {
	FARF(ERROR, "Error %x: alloc handle failed. domain %d\n", nErr, domain); 
   }
   return nErr;
}

#define IS_CONST_HANDLE(h)  (((h) < 0xff) ? 1 : 0)
static int is_last_handle(int domain) {
   QNode* pn;
   int nErr = AEE_SUCCESS, empty = 0;

   pthread_mutex_lock(&hlist[domain].mut);
   if(!(hlist[domain].domainsupport && !hlist[domain].nondomainsupport)){
       VERIFY_IPRINTF("Error %x: hlist[domain].domainsupport && !hlist[domain].nondomainsupport\n",AEE_EBADDOMAIN);
       goto bail;
   }
   empty = 1;
   if (!QList_IsEmpty(&hlist[domain].ql)) {
      empty = 1;
      QLIST_FOR_ALL(&hlist[domain].ql, pn) {
         struct handle_info* hi = STD_RECOVER_REC(struct handle_info, qn, pn);
         empty = empty & IS_CONST_HANDLE(hi->remote);
         if(!empty)
            break;
      }
   }
bail:
   pthread_mutex_unlock(&hlist[domain].mut);
   if (nErr != AEE_SUCCESS) {
       VERIFY_IPRINTF("Error %x: is_last_handle %d failed\n", nErr, domain);
   }
   return empty;
}

static int free_handle(remote_handle64 local) {
   struct handle_info* hinfo = (struct handle_info*)(uintptr_t)local;
   int nErr = AEE_SUCCESS;

   VERIFY(AEE_SUCCESS == (nErr = verify_local_handle(local)));
   pthread_mutex_lock(&hinfo->hlist->mut);
   QNode_DequeueZ(&hinfo->qn);
   pthread_mutex_unlock(&hinfo->hlist->mut);
   free(hinfo);
   hinfo = NULL;
bail:
   if (nErr != AEE_SUCCESS) {
	FARF(ERROR, "Error %x: free handle failed %p\n", nErr, &local);
   }
   return nErr;
}

static int get_handle_remote(remote_handle64 local, remote_handle64 *remote) {
   struct handle_info* hinfo = (struct handle_info*)(uintptr_t)local;
   int nErr = AEE_SUCCESS;

   VERIFY(AEE_SUCCESS == (nErr = verify_local_handle(local)));
   *remote = hinfo->remote;
bail:
   if (nErr != AEE_SUCCESS) {
        FARF(ERROR, "Error %x: get handle remote failed %p\n", nErr, &local);
   }
   return nErr;
}

void set_thread_context(int domain) {
   if(tlsKey != INVALID_KEY) {
      pthread_setspecific(tlsKey, (void*)&hlist[domain]);
   }
}

int get_domain_id() {
   int domain;
   struct handle_list* list;
   list = (struct handle_list*)pthread_getspecific(tlsKey);
   if(list && hlist){
      domain = (int)(list - &hlist[0]);
   }else{
      domain = DEFAULT_DOMAIN_ID;
   }
   return domain;
}

int is_smmu_enabled(void) {
	struct handle_list* list;
	int domain, nErr = 0;

	list = (struct handle_list*)pthread_getspecific(tlsKey);
	if (list) {
		domain = (int)(list - &hlist[0]);
		VERIFY((domain >= 0) && (domain < NUM_DOMAINS_EXTEND));
		return hlist[domain].info & FASTRPC_INFO_SMMU;
	}
bail:
	return 0;
}

static int fdlist_fd_to_buf(void *buf)
{
	QNode *pn;
	int fd = -1;
	pthread_mutex_lock(&fdlist.mut);
	QLIST_FOR_ALL(&fdlist.ql, pn)
	{
		if (fd != -1)
		{
			break;
		}
		else
		{
			struct mem_to_fd *tofd = STD_RECOVER_REC(struct mem_to_fd, qn, pn);
			if (STD_BETWEEN(buf, tofd->buf, (unsigned long)tofd->buf + tofd->size))
			{
				fd = tofd->fd;
			}
		}
	}
	pthread_mutex_unlock(&fdlist.mut);
	return fd;
}

int remote_handle_invoke_domain(int domain, remote_handle handle, uint32_t sc, remote_arg* pra) {
    struct fastrpc_invoke invoke;
	struct fastrpc_invoke_args *args;
	int bufs, i, req, nErr = 0;
	int dev;
	VERIFY(dev != -1);
	invoke.handle = handle;
	invoke.sc = sc;
	struct handle_list* list;
	
   VERIFYC(-1 != (dev = open_dev(domain)), AEE_EINVALIDDEVICE);
   list = &hlist[domain];
   if(0 == pthread_getspecific(tlsKey)) {
      pthread_setspecific(tlsKey, (void*)list);
   }
	bufs = REMOTE_SCALARS_LENGTH(sc);

	args = malloc(bufs * sizeof(*args));
	if (!args)
		return -ENOMEM;

	invoke.args = (__u64)(uintptr_t)args;

	for (i = 0; i < bufs; i++)
	{
		args[i].reserved = 0;
		args[i].length = pra[i].buf.nLen;
		args[i].ptr = (__u64)(uintptr_t)pra[i].buf.pv;
		

		if (pra[i].buf.nLen)
		{
			FARF(HIGH,"debug:sc:%x,handle:%x,len:%llx\n",sc,pra[i].buf.nLen);
			args[i].fd = fdlist_fd_to_buf(pra[i].buf.pv);
		}
		else
		{
			args[i].fd = -1;
		}
	}
	req = FASTRPC_IOCTL_INVOKE;

	if (0 == pthread_getspecific(tlsKey))
	{
		pthread_setspecific(tlsKey, (void *)1);
	}
    FARF(HIGH,"debug:sc:%x,handle:%x\n",sc,handle);
	nErr = ioctl(dev, req, (unsigned long)&invoke);
	free(args);
bail:
	return nErr;
}

int remote_handle_invoke(remote_handle handle, uint32_t sc, remote_arg* pra) {
	struct handle_list* list;
	int domain = DEFAULT_DOMAIN_ID, nErr = AEE_SUCCESS;

	VERIFYC(handle != (remote_handle)-1, AEE_EBADHANDLE);
	list = (struct handle_list*)pthread_getspecific(tlsKey);

	if(list) {
		domain = (int)(list - &hlist[0]);
		VERIFYC((domain >= 0) && (domain < NUM_DOMAINS_EXTEND), AEE_EINVALIDDOMAIN);
	} else {
		domain = DEFAULT_DOMAIN_ID;
	}
	VERIFY(AEE_SUCCESS == (nErr = remote_handle_invoke_domain(domain, handle, sc, pra)));
bail:
	if (nErr != AEE_SUCCESS) {
		FARF(HIGH, "Error %x: remote handle invoke failed. domain %d, handle %x, sc %x, pra %p\n", nErr, domain, handle, sc, pra);
	}
	return nErr;
}

int remote_handle64_invoke(remote_handle64 local, uint32_t sc, remote_arg* pra) {
	remote_handle64 remote;
	int nErr = AEE_SUCCESS, domain = DEFAULT_DOMAIN_ID;

	VERIFYC(local != (remote_handle64)-1, AEE_EBADHANDLE);
	VERIFY(AEE_SUCCESS == (nErr = get_domain_from_handle(local, &domain)));
	VERIFY(AEE_SUCCESS == (nErr = get_handle_remote(local, &remote)));
	VERIFY(AEE_SUCCESS == (nErr = remote_handle_invoke_domain(domain, remote, sc, pra)));
bail:
	if (nErr != AEE_SUCCESS) {
		FARF(HIGH, "Error %x: remote handle64 invoke failed. domain %d, handle %p, sc %x, pra %p\n", nErr, domain, &local, sc, pra);
	}
	return nErr;
}

int listener_android_geteventfd(int domain, int *fd);
int remote_handle_open_domain(int domain, const char* name, remote_handle *ph)
{
   char dlerrstr[255];
   int dlerr = 0, nErr = AEE_SUCCESS;
   if (!std_strncmp(name, ITRANSPORT_PREFIX "geteventfd", std_strlen(ITRANSPORT_PREFIX "geteventfd"))) {
      FARF(HIGH, "getting event fd \n");
      return listener_android_geteventfd(domain, (int *)ph);
   }
   if (!std_strncmp(name, ITRANSPORT_PREFIX "attachguestos", std_strlen(ITRANSPORT_PREFIX "attachguestos"))) {
      FARF(HIGH, "setting attach mode to guestos : %d\n", domain);
      VERIFY(AEE_SUCCESS == (nErr = fastrpc_init_once()));
      hlist[domain].dsppd = GUEST_OS;
      return AEE_SUCCESS;
   }
   if (!std_strncmp(name, ITRANSPORT_PREFIX "createstaticpd", std_strlen(ITRANSPORT_PREFIX "createstaticpd"))) {
      FARF(HIGH, "creating static pd on domain: %d\n", domain);
      VERIFY(AEE_SUCCESS == (nErr = fastrpc_init_once()));
      const char *pdName = name + std_strlen(ITRANSPORT_PREFIX "createstaticpd:");
      hlist[domain].dsppdname = (char *)malloc((std_strlen(pdName) + 1)*(sizeof(char)));
      VERIFYC(NULL != hlist[domain].dsppdname, AEE_ENOMEMORY);
      std_strlcpy(hlist[domain].dsppdname, pdName, std_strlen(pdName) + 1);
      if (!std_strncmp(pdName, "audiopd", std_strlen("audiopd"))) {
         hlist[domain].dsppd = STATIC_USER_PD;
      } else if (!std_strncmp(pdName, "sensorspd", std_strlen("sensorspd"))) {
         hlist[domain].dsppd = ATTACH_SENSORS_PD;
      } else if (!std_strncmp(pdName, "rootpd", std_strlen("rootpd"))) {
         hlist[domain].dsppd = GUEST_OS_SHARED;
      }
      return AEE_SUCCESS;
   }
   if (std_strbegins(name, ITRANSPORT_PREFIX "attachuserpd")) {
      FARF(HIGH, "setting attach mode to userpd : %d\n", domain);
      VERIFY(AEE_SUCCESS == (nErr = fastrpc_init_once()));
      hlist[domain].dsppd = USER_PD;
      return AEE_SUCCESS;
   }
   VERIFYC(-1 != open_dev(domain), AEE_EINVALIDDEVICE);
   FARF(HIGH, "Name of the shared object to open %s\n", name);
   VERIFY(AEE_SUCCESS == (nErr = remotectl_open(name, (int*)ph, dlerrstr, sizeof(dlerrstr), &dlerr)));
   VERIFY(AEE_SUCCESS == (nErr = dlerr));

bail:
   if(dlerr != 0) {
      FARF(ERROR, "Error %x: remote handle open domain failed. domain %d, name %s, dlerror %s\n", nErr, domain, name, dlerrstr);
   }
   if (nErr != 0)
      if (hlist[domain].dsppdname != NULL)
      {
         free(hlist[domain].dsppdname);
         hlist[domain].dsppdname = NULL;
      }
   return nErr;
}

int remote_handle_open(const char* name, remote_handle *ph) {
   int nErr = 0, domain;
   domain = DEFAULT_DOMAIN_ID;
   VERIFY(!remote_handle_open_domain(domain, name, ph));
   hlist[domain].nondomainsupport = 1;
bail:
   return nErr;
}

int remote_handle64_open(const char* name, remote_handle64 *ph)
{
   struct handle_info* hinfo = 0;
   remote_handle h = 0;
   int domain, nErr = 0;

   domain = get_domain_from_name(name);
   VERIFYC(domain >= 0, AEE_EINVALIDDOMAIN);
   VERIFY(AEE_SUCCESS == (nErr = fastrpc_init_once()));
   VERIFY(AEE_SUCCESS == (nErr = remote_handle_open_domain(domain, name, &h)));
   hlist[domain].domainsupport = 1;
   VERIFY(AEE_SUCCESS == (nErr = alloc_handle(domain, h, &hinfo)));
   *ph = hinfo->local;
bail:
   if(nErr) {
      if(h)
         remote_handle_close(h);
      FARF(HIGH, "Error %x: remote handle64 open failed. name %s\n", nErr, name);
   }
   return nErr;
}

int remote_handle_close(remote_handle h)
{
   char dlerrstr[255];
   int dlerr = 0, nErr = AEE_SUCCESS;

   VERIFY(AEE_SUCCESS == (nErr = remotectl_close(h, dlerrstr, sizeof(dlerrstr), &dlerr)));
   VERIFY(AEE_SUCCESS == (nErr = dlerr));
bail:
   if (nErr != AEE_SUCCESS) {
	FARF(HIGH, "Error %x: remote handle close failed. error %s\n", nErr, dlerrstr);
   }
   return nErr;
}

int remote_handle64_close(remote_handle64 handle) {
   remote_handle64 remote;
   int domain, nErr = AEE_SUCCESS;

   VERIFY(AEE_SUCCESS == (nErr = get_domain_from_handle(handle, &domain)));
   VERIFY(AEE_SUCCESS == (nErr = get_handle_remote(handle, &remote)));
   set_thread_context(domain);
   VERIFY(AEE_SUCCESS == (nErr = remote_handle_close((remote_handle)remote)));
bail:
   free_handle(handle);
   if (is_last_handle(domain)) {
      domain_deinit(domain);
   }
   if (nErr != AEE_SUCCESS) {
	FARF(HIGH, "Error %x: remote handle64 close failed.\n", nErr);
   }
   return nErr;
}

int manage_pm_qos(int domain, remote_handle64 h, uint32_t enable, uint32_t latency) {
	int nErr = AEE_SUCCESS;
	struct fastrpc_latency *qos;
	int state = 0;

	if (h == -1) {
		/* Handle will be -1 in non-domains invocation. Create session if necessary  */
		if (!hlist || (hlist && hlist[domain].dev == -1))
			VERIFYC(-1 != open_dev(domain), AEE_EINVALIDDEVICE);
	} else {
		/* If the multi-domain handle is valid, then verify that session is created already */
		VERIFY(hlist[domain].dev != -1);
	}
	qos = &hlist[domain].qos;
	VERIFY(qos);
	if (qos->exit == FASTRPC_LATENCY_EXIT)
		goto bail;
	pthread_mutex_lock(&qos->mut);
	state = qos->state;
	qos->latency = latency;
	pthread_mutex_unlock(&qos->mut);

	if (!enable && state == FASTRPC_LATENCY_START) {
		qos->exit = FASTRPC_LATENCY_EXIT;
		pthread_mutex_lock(&qos->wmut);
		pthread_cond_signal(&qos->cond);
		pthread_mutex_unlock(&qos->wmut);
	}

	if (enable && state == FASTRPC_LATENCY_STOP) {
		qos->state = FASTRPC_LATENCY_START;
		VERIFY(AEE_SUCCESS == (nErr = pthread_create(&qos->thread, 0, fastrpc_latency_thread_handler, (void*)qos)));
	}
bail:
	return nErr;
}

int manage_adaptive_qos(int domain, uint32_t enable) {
	int nErr = AEE_SUCCESS;

	VERIFY(AEE_SUCCESS == (nErr = fastrpc_init_once()));

	/* If adaptive QoS is already enabled/disabled, then just return */
	if ((enable && hlist[domain].qos.adaptive_qos) || (!enable && !hlist[domain].qos.adaptive_qos))
		return nErr;

	if (hlist[domain].dev != -1) {
		/* If session is already open on DSP, then make rpc call directly to user PD */
		nErr = remotectl_set_param(FASTRPC_ADAPTIVE_QOS, &enable, 1);
		if (nErr) {
			FARF(ERROR, "Error: %s: remotectl_set_param failed to reset adaptive QoS on DSP to %d on domain %d",
							__func__, enable, domain);
			goto bail;
		} else {
			hlist[domain].qos.adaptive_qos = ((enable == FASTRPC_ADAPTIVE_QOS)? 1:0);
		}
	} else {
		/* If session is not created already, then just process attribute */
		hlist[domain].qos.adaptive_qos = ((enable == FASTRPC_ADAPTIVE_QOS)? 1:0);
	}

	if (enable) 
		FARF(ALWAYS, "%s: Successfully enabled adaptive QoS on domain %d", __func__, domain);
	else
		FARF(ALWAYS, "%s: Disabled adaptive QoS on domain %d", __func__, domain);
bail:
	return nErr;
}

int remote_handle_control_domain(int domain, remote_handle64 h, uint32_t req, void* data, uint32_t len) {
	int nErr = AEE_SUCCESS;

	switch (req) {
		case DSPRPC_CONTROL_LATENCY:
		{
			struct remote_rpc_control_latency *lp = (struct remote_rpc_control_latency*)data;
			VERIFYC(lp, AEE_EBADPARM);
			VERIFYC(len == sizeof(struct remote_rpc_control_latency), AEE_EBADPARM);

			switch(lp->enable) {
				/* Only one of PM QoS or adaptive QoS can be enabled */
				case FASTRPC_DISABLE_QOS:
				{
					VERIFY(AEE_SUCCESS == (nErr = manage_adaptive_qos(domain, FASTRPC_DISABLE_QOS)));
					VERIFY(AEE_SUCCESS == (nErr = manage_pm_qos(domain, h, FASTRPC_DISABLE_QOS, lp->latency)));
					break;
				}
				case FASTRPC_PM_QOS:
				{
					VERIFY(AEE_SUCCESS == (nErr = manage_adaptive_qos(domain, FASTRPC_DISABLE_QOS)));
					VERIFY(AEE_SUCCESS == (nErr = manage_pm_qos(domain, h, FASTRPC_PM_QOS, lp->latency)));
					break;
				}
				case FASTRPC_ADAPTIVE_QOS:
				{
					/* Disable PM QoS if enabled and then enable adaptive QoS */
					VERIFY(AEE_SUCCESS == (nErr = manage_pm_qos(domain, h, FASTRPC_DISABLE_QOS, lp->latency)));
					VERIFY(AEE_SUCCESS == (nErr = manage_adaptive_qos(domain, FASTRPC_ADAPTIVE_QOS)));
					break;
				}
				default:
					nErr = AEE_EBADPARM;
					FARF(ERROR, "Error: %s: Bad enable parameter %d passed for QoS control", __func__, lp->enable);
					break;
			}
			break;
		}
		default:
			nErr = AEE_EUNSUPPORTEDAPI;
			FARF(ERROR, "Error: %s: remote handle control called with unsupported request ID %d", __func__, req);
			break;
	}
bail:
	if (nErr != AEE_SUCCESS)
		FARF(ERROR, "Error 0x%x: %s failed for request ID %d on domain %d", nErr, __func__, req, domain);
	return nErr;
}

int remote_handle_control(uint32_t req, void* data, uint32_t len) {
	struct handle_list* list;
	int domain = DEFAULT_DOMAIN_ID, nErr = AEE_SUCCESS;

	VERIFY(AEE_SUCCESS == (nErr = remote_handle_control_domain(domain, -1, req, data, len)));
bail:
	if (nErr != AEE_SUCCESS)
		FARF(ERROR, "Error 0x%x: %s failed for request ID %d", nErr, __func__, req);
	return nErr;
}

int remote_handle64_control(remote_handle64 handle, uint32_t req, void* data, uint32_t len) {
	int nErr = AEE_SUCCESS, domain = 0;

	VERIFY(AEE_SUCCESS == (nErr = get_domain_from_handle(handle, &domain)));
	VERIFY(AEE_SUCCESS == (nErr = remote_handle_control_domain(domain, handle, req, data, len)));

bail:
	if (nErr != AEE_SUCCESS)
		FARF(ERROR, "Error 0x%x: %s failed for request ID %d", nErr, __func__, req);
	return nErr;
}

static int store_domain_thread_params(int domain, struct remote_rpc_thread_params *params, uint32_t req)
{
	int nErr = AEE_SUCCESS;

	if (hlist[domain].dev != -1) {
		nErr = AEE_ENOTALLOWED;
		FARF(ERROR, "%s: Session already open on domain %d ! Set parameters before making any RPC calls",
					__func__, domain);
		goto bail;
	}
	if (params->prio != -1) {
		/* Valid QuRT thread priorities are 1 to 255 */
		unsigned int min_prio = 1, max_prio = 255;

		if ((params->prio < min_prio) || (params->prio > max_prio)) {
			nErr = AEE_EBADPARM;
			FARF(ERROR, "%s: Priority %d is invalid! Should be between %d and %d",
						__func__, params->prio, min_prio, max_prio);
			goto bail;
		} else
			hlist[domain].th_params.prio = (uint32_t) params->prio;
	}
	if (params->stack_size != -1) {
		/* Stack size passed by user should be between 16 KB and 8 MB */
		unsigned int min_stack_size = 16*1024, max_stack_size = 8*1024*1024;

		if ((params->stack_size < min_stack_size) || (params->stack_size > max_stack_size)) {
			nErr = AEE_EBADPARM;
			FARF(ERROR, "%s: Stack size %d is invalid! Should be between %d and %d",
						__func__, params->stack_size, min_stack_size, max_stack_size);
			goto bail;
		} else
			hlist[domain].th_params.stack_size = (uint32_t) params->stack_size;
	}
	hlist[domain].th_params.reqID = req;
bail:
	if (nErr != AEE_SUCCESS)
		FARF(ERROR, "Error 0x%x: %s failed for domain %d", nErr, __func__, domain);
	return nErr;
}

/* Set remote session parameters like thread stack size, running on unsigned PD etc */
int remote_session_control(uint32_t req, void *data, uint32_t datalen)
{
	int nErr = AEE_SUCCESS;

	VERIFY(AEE_SUCCESS == (nErr = fastrpc_init_once()));

	switch (req) {
		case FASTRPC_THREAD_PARAMS:
		{
			struct remote_rpc_thread_params *params = (struct remote_rpc_thread_params*)data;
			if (!params) {
				nErr = AEE_EBADPARM;
				FARF(ERROR, "%s: Thread params struct passed is %p", __func__, params);
				goto bail;
			}
			VERIFYC(datalen == sizeof(struct remote_rpc_thread_params), AEE_EINVALIDFORMAT);
			if (params->domain != -1) {
				if ((params->domain < 0) || (params->domain >= NUM_DOMAINS_EXTEND)) {
					nErr = AEE_EINVALIDDOMAIN;
					FARF(ERROR, "%s: Invalid domain ID %d passed", __func__, params->domain);
					goto bail;
				}
				VERIFY(AEE_SUCCESS == (nErr = store_domain_thread_params(params->domain, params, req)));
			} else {
				/* If domain is -1, then set parameters for all domains */
				for (int i = 0; i < NUM_DOMAINS_EXTEND; i++) {
					VERIFY(AEE_SUCCESS == (nErr = store_domain_thread_params(i, params, req)));
				}
			}
			break;
		}
		case DSPRPC_CONTROL_UNSIGNED_MODULE:
		{
			// Handle the unsigned module offload request
			struct remote_rpc_control_unsigned_module *um = (struct remote_rpc_control_unsigned_module*)data;
			VERIFYC(datalen == sizeof(struct remote_rpc_control_unsigned_module), AEE_EINVALIDFORMAT);
			VERIFY(um != NULL);
			FARF (HIGH, "%s Unsigned module offload enable %d for domain %d", __func__, um->enable, um->domain);
			if (um->domain != -1) {
				VERIFYC((um->domain >= 0) && (um->domain < NUM_DOMAINS_EXTEND), AEE_EINVALIDDOMAIN);
				hlist[um->domain].unsigned_module = um->enable? 1 :0 ;
			} else {
				for (int ii = 0; ii < NUM_DOMAINS_EXTEND; ii++) {
					hlist[ii].unsigned_module = um->enable? 1: 0;
				}
			}
		}
		break;
		default:
			nErr = AEE_EUNSUPPORTEDAPI;
			FARF(ERROR, "%s: Unsupported request ID %d", __func__, req);
			break;
	}
bail:
	if (nErr != AEE_SUCCESS) {
		FARF(ERROR, "Error 0x%x: %s failed for request ID %d", nErr, __func__, req);
	}
	return nErr;
}

int remote_mmap64(int fd, uint32_t flags, uint64_t vaddrin, int64_t size, uint64_t* vaddrout) {
	struct handle_list* list;
	struct fastrpc_ioctl_mmap mmap;
	int dev, domain, nErr = AEE_SUCCESS;

	list = (struct handle_list*)pthread_getspecific(tlsKey);
	VERIFYC(NULL != list, AEE_EMEMPTR);
	domain = (int)(list - &hlist[0]);
	VERIFYC((domain >= 0) && (domain < NUM_DOMAINS_EXTEND), AEE_EINVALIDDOMAIN);
	VERIFYC(-1 != (dev = open_dev(domain)), AEE_EINVALIDDEVICE);
	mmap.fd  = fd;
	mmap.flags  = flags;
	mmap.vaddrin = vaddrin;
	mmap.size = size;
	FARF(HIGH, "Entering %s : fd %d, vaddrin %llx, size %llx ioctl %x\n", __func__, fd, vaddrin, size, FASTRPC_IOCTL_MMAP);
	VERIFY(AEE_SUCCESS == (nErr = ioctl(dev, FASTRPC_IOCTL_MMAP, (unsigned long)&mmap)));
	*vaddrout = mmap.vaddrout;
bail:
	if (nErr != AEE_SUCCESS) {
		FARF(ERROR, "Error %x: remote mmap64 failed. fd %x, flags %x, vaddrin %llx, size %zx\n", nErr, fd, flags, vaddrin, size);
	}
	return nErr;
}

int remote_mmap(int fd, uint32_t flags, uint32_t vaddrin, int size, uint32_t* vaddrout) {
	return remote_mmap64(fd, flags, (uintptr_t)vaddrin, (int64_t)size, (uint64_t*)vaddrout);
}

int remote_munmap64(uint64_t vaddrout, int64_t size) {
	struct handle_list* list;
	struct fastrpc_ioctl_munmap munmap;
	int dev, domain, nErr = AEE_SUCCESS;

	list = (struct handle_list*)pthread_getspecific(tlsKey);
	VERIFYC(NULL != list, AEE_EMEMPTR);
	domain = (int)(list - &hlist[0]);
	VERIFYC((domain >= 0) && (domain < NUM_DOMAINS_EXTEND), AEE_EINVALIDDOMAIN);
	VERIFYC(-1 != (dev = open_dev(domain)), AEE_EINVALIDDEVICE);
	VERIFY(list->dev > 0);
	munmap.vaddrout = vaddrout;
	munmap.size = size;
	FARF(HIGH, "Entering %s : vaddrin %llx, size %llx\n", __func__, vaddrout, size);
	VERIFY(AEE_SUCCESS == (nErr = ioctl(dev, FASTRPC_IOCTL_MUNMAP, (unsigned long)&munmap)));
bail:
	if (nErr != AEE_SUCCESS) {
		FARF(ERROR, "Error %x: remote munmap64 failed. vaddrout %p, size %zx\n", nErr, vaddrout, size);
	}
	return nErr;
}

int remote_munmap(uint32_t vaddrout, int size) {
	return remote_munmap64((uintptr_t)vaddrout, (int64_t)size);
}

static int remote_unmap_fd(void *buf, int size, int fd, int attr) {
   int nErr = 0;
   int i;
   struct fastrpc_ioctl_munmap map;

   VERIFY(hlist);
   map.vaddrout = (uintptr_t) buf;
   map.size = size;
   for (i = 0; i < NUM_DOMAINS; i++) {
      pthread_mutex_lock(&hlist[i].mut);
      if (hlist[i].dev != -1) {
        nErr = ioctl(hlist[i].dev, FASTRPC_IOCTL_MUNMAP, (unsigned long)&map);
	if (nErr)
	   FARF(LOW, "unmap_fd: device found %d for domain %d returned %d", hlist[i].dev, i, nErr);
      }
      pthread_mutex_unlock(&hlist[i].mut);
   }
bail:
   return nErr;
}


int remote_set_mode(uint32_t mode) {
   int i;
   for(i = 0; i < NUM_DOMAINS_EXTEND; i++) {
      hlist[i].mode = mode;
      hlist[i].setmode = 1;
   }
   return AEE_SUCCESS;
}

#ifdef __ANDROID__
#include <android/log.h>
extern const char* __progname;
void HAP_debug(const char *msg, int level, const char *filename, int line) {
   __android_log_print(level, __progname, "%s:%d: %s", filename, line, msg);
}
#else
extern const char* __progname;
void HAP_debug(const char *msg, int level, const char *filename, int line) {
   printf("hello %s - %s:%d: %s", __progname, filename, line, msg);
}    
#endif

PL_DEP(fastrpc_apps_user);
PL_DEP(gpls);
PL_DEP(apps_mem);
PL_DEP(apps_std);
PL_DEP(rpcmem);
PL_DEP(listener_android);

static int attach_guestos(int domain) {
   int attach;

   switch(domain & DOMAIN_ID_MASK) {
      case MDSP_DOMAIN_ID:
      case ADSP_DOMAIN_ID:
          attach = USER_PD;
          break;
      case CDSP_DOMAIN_ID:
          attach = USER_PD;
          break;
      default:
          attach = GUEST_OS;
          break;
   }
   return attach;
}

static void domain_deinit(int domain) {
   QNode *pn;
   remote_handle64 handle;

   if(!hlist) {
      return;
   }

   pthread_mutex_lock(&hlist[domain].mut);
   FARF(HIGH, "domain_deinit for domain %d: dev %d \n", domain, hlist[domain].dev);
   if(hlist[domain].dev != -1) {
      handle = get_adsp_current_process1_handle(domain);
      if(handle != INVALID_HANDLE) {
         adsp_current_process1_exit(handle);
      } else {
         adsp_current_process_exit();
      }

      listener_android_domain_deinit(domain);
      deinitFileWatcher(domain);
      fastrpc_perf_deinit();
      fastrpc_latency_deinit(&hlist[domain].qos);
      while((pn = QList_Pop(&hlist[domain].ql))) {
         struct handle_info* hi = STD_RECOVER_REC(struct handle_info, qn, pn);
         free(hi);
         hi = NULL;
      }
      hlist[domain].cphandle = 0;
      hlist[domain].msghandle = 0;
      hlist[domain].domainsupport = 0;
      hlist[domain].nondomainsupport = 0;
      hlist[domain].initialized = 0;
      hlist[domain].dsppd = attach_guestos(domain);
      if (hlist[domain].dsppdname != NULL)
      {
         free(hlist[domain].dsppdname);
         hlist[domain].dsppdname = NULL;
      }

      FARF(HIGH, "exit: closing %d, rpc errors are expected.\n", domain);

      if (close(hlist[domain].dev))
	      FARF(ERROR, "exit: failed to close file descriptor for domain %d\n", domain);

      hlist[domain].dev = -1;
   }
   if(hlist[domain].pdmem) {
      rpcmem_free_internal(hlist[domain].pdmem);
      hlist[domain].pdmem = NULL;
   }
   pthread_mutex_unlock(&hlist[domain].mut);
}

#define ALIGN_B(p, a)	      (((p) + ((a) - 1)) & ~((a) - 1))

static const char* get_domain_name(int domain_id) {
   const char* name;
   int domain = domain_id & DOMAIN_ID_MASK;

   switch (domain) {
      case ADSP_DOMAIN_ID:
         name = ADSPRPC_DEVICE;
         break;
      case SDSP_DOMAIN_ID:
         name = SDSPRPC_DEVICE;
         break;
      case MDSP_DOMAIN_ID:
         name = MDSPRPC_DEVICE;
         break;
      case CDSP_DOMAIN_ID:
         name = CDSPRPC_DEVICE;
         break;
      default:
         name = DEFAULT_DEVICE;
         break;
   }
   return name;
}

/* Returns the name of the domain based on the following
 ADSP/SLPI/MDSP - Return Secure node
 CDSP - Return default node
 */
static const char* get_secure_domain_name(int domain_id) {
   const char* name;
   int domain = domain_id & DOMAIN_ID_MASK;

   switch (domain) {
      case ADSP_DOMAIN_ID:
      case SDSP_DOMAIN_ID:
      case MDSP_DOMAIN_ID:
         name = SECURE_DEVICE;
         break;
      case CDSP_DOMAIN_ID:
         // Intentional fallthrough
      default:
         name = DEFAULT_DEVICE;
         break;
   }
   return name;
}

/* Opens device node based on the domain
 This function takes care of the backward compatibility to open
 approriate device for following configurations of the device nodes
 1. 4 different device nodes
 2. 1 device node (adsprpc-smd)
 3. 2 device nodes (adsprpc-smd, adsprpc-smd-secure)
 Algorithm
 For ADSP, SDSP, MDSP domains:
   Open secure device node fist
     if no secure device, open actual device node
     if still no device, open default node
     if failed to open the secure node due to permission,
     open default node
 For CDSP domain:
   Open actual device node ("cdsprpc-smd")
   if no device, open secure / default device node
*/
static int open_device_node_internal (int domain_id) {
  int dev = -1;
  int domain = domain_id & DOMAIN_ID_MASK;

  switch (domain) {
    case ADSP_DOMAIN_ID:
    case SDSP_DOMAIN_ID:
    case MDSP_DOMAIN_ID:
      dev = open(get_secure_domain_name(domain), O_NONBLOCK);
      if((dev < 0) && (errno == ENOENT)) {
        FARF(HIGH, "Device node %s open failed for domain %d (errno %s),\n"
                    "falling back to node %s \n",
                    get_secure_domain_name(domain), domain, strerror(errno),
                    get_domain_name(domain));
        dev = open(get_domain_name(domain), O_NONBLOCK);
        if((dev < 0) && (errno == ENOENT)) {
          FARF(HIGH, "Device node %s open failed for domain %d (errno %s),"
                    "falling back to node %s \n",
                    get_domain_name(domain), domain, strerror(errno),
                    DEFAULT_DEVICE);
          dev = open(DEFAULT_DEVICE, O_NONBLOCK);
        }
      } else if ((dev < 0) && (errno == EACCES)) {
         // Open the default device node if unable to open the
         // secure device node due to permissions
         FARF(HIGH, "Device node %s open failed for domain %d (errno %s),"
                   "falling back to node %s \n",
                   get_secure_domain_name(domain), domain, strerror(errno),
                   DEFAULT_DEVICE);
         dev = open(DEFAULT_DEVICE, O_NONBLOCK);
      }
      break;
    case CDSP_DOMAIN_ID:
       dev = open(get_domain_name(domain), O_NONBLOCK);
       if((dev < 0) && (errno == ENOENT)) {
         FARF(HIGH, "Device node %s open failed for domain %d (errno %s),"
                   "falling back to node %s \n",
                   get_domain_name(domain), domain, strerror(errno),
                   get_secure_domain_name(domain));
         dev = open(get_secure_domain_name(domain), O_NONBLOCK);
       }
       break;
    default:
      break;
  }

  if (dev < 0)
    FARF(ERROR, "Error: Device node open failed for domain %d (errno %s)",
         domain, strerror(errno));

  return dev;
}


static int get_process_attrs(int domain) {
	int nErr = 0;
	uint64 len = 0;
	int attrs = 0;

	attrs = FASTRPC_PROPERTY_GET_INT32(FASTRPC_PROP_PROCESS, 0);
	if (!attrs) {
		const char *env = getenv("ADSP_PROCESS_ATTRS");
		attrs = env == 0 ? 0 : (int)atoi(env);
	}
	fastrpc_trace = FASTRPC_PROPERTY_GET_INT32(FASTRPC_PROP_TRACE, 0);
	attrs |= hlist[domain].qos.adaptive_qos ? FASTRPC_MODE_ADAPTIVE_QOS : 0;
	attrs |= hlist[domain].unsigned_module ? FASTRPC_MODE_UNSIGNED_MODULE : 0;
	return attrs;
}

static void get_process_testsig(apps_std_FILE *fp, uint64 *ptrlen) {
   int nErr = 0;
   uint64 len = 0;
   char testsig[PROPERTY_VALUE_MAX];

   if (fp == NULL || ptrlen == NULL)
      return;

   if (FASTRPC_PROPERTY_GET_STR(FASTRPC_PROP_TESTSIG, testsig, NULL)) {
      FARF(HIGH, "testsig file loading is %s", testsig);
      nErr = apps_std_fopen_with_env("ADSP_LIBRARY_PATH", ";", testsig, "r", fp);
      if (nErr == AEE_SUCCESS && *fp != -1)
         nErr = apps_std_flen(*fp, &len);
   }
bail:
   if (nErr)
      len = 0;
   *ptrlen = len;
   return;
}

int is_kernel_alloc_supported(int dev, int domain) {
	return 1;
}

static int open_shell(int domain_id, apps_std_FILE *fh, int unsigned_shell) {
   char *absName = NULL;
   char *shell_absName = NULL;
   char *domain_str = NULL;
   uint16 shell_absNameLen = 0, absNameLen = 0;;
   int nErr = AEE_SUCCESS;
   int domain = domain_id & DOMAIN_ID_MASK;
   const char* shell_name = SIGNED_SHELL;

   if (1 == unsigned_shell) {
     shell_name = UNSIGNED_SHELL;
   }

   if (domain == MDSP_DOMAIN_ID) {
      return nErr;
   }
   VERIFYC(NULL != (domain_str = (char*)malloc(sizeof(domain))), AEE_ENOMEMORY);
   snprintf(domain_str, sizeof(domain), "%d",domain);


   shell_absNameLen = std_strlen(shell_name) + std_strlen(domain_str) + 1;

   VERIFYC(NULL != (shell_absName = (char*)malloc(sizeof(char) * shell_absNameLen)), AEE_ENOMEMORY);
   std_strlcpy(shell_absName, shell_name, shell_absNameLen);

   std_strlcat(shell_absName, domain_str, shell_absNameLen);

   absNameLen = std_strlen("/usr/lib/") + shell_absNameLen + 1;
   VERIFYC(NULL != (absName = (char*)malloc(sizeof(char) * absNameLen)), AEE_ENOMEMORY);
   std_strlcpy(absName, "/usr/lib/",absNameLen);
   std_strlcat(absName, shell_absName, absNameLen);

   nErr = apps_std_fopen(absName, "r", fh);
   if (nErr) {
      absNameLen = std_strlen("/vendor/dsp/") + shell_absNameLen + 1;
      VERIFYC(NULL != (absName = (char*)realloc(absName, sizeof(char) * absNameLen)), AEE_ENOMEMORY);
      std_strlcpy(absName, "/vendor/dsp/",absNameLen);
      std_strlcat(absName, shell_absName, absNameLen);

      nErr = apps_std_fopen(absName, "r", fh);
      if (nErr) {
        FARF(HIGH, "Searching for %s%d ...", shell_name, domain);
        nErr = apps_std_fopen_with_env("ADSP_LIBRARY_PATH", ";", shell_absName, "r", fh);
      }
   }
   FARF(HIGH, "fopen for shell returned %d", nErr);
bail:
   if(domain_str){
       free(domain_str);
       domain_str = NULL;
   }
   if(shell_absName){
       free(shell_absName);
       shell_absName = NULL;
   }
   if(absName){
       free(absName);
       absName = NULL;
   }
   if (nErr != AEE_SUCCESS) {
       if (domain == SDSP_DOMAIN_ID && fh != NULL) {
           nErr = AEE_SUCCESS;
           *fh = -1;
       } else {
           FARF(ERROR, "open_shell failed with err %d domain %d\n", nErr, domain);
       }
   }
   return nErr;
}

int open_device_node(int domain) {
	int nErr=0;

	VERIFY(!fastrpc_init_once());
	
	pthread_mutex_lock(&hlist[domain].mut);
	if(hlist[domain].dev == -1) {
		hlist[domain].dev = open_device_node_internal(domain);
		/* the domain was opened but not apps initialized */
		hlist[domain].initialized = 0;
	}
	pthread_mutex_unlock(&hlist[domain].mut);
bail:
	return hlist[domain].dev;
}

static int apps_dev_init(int domain) {
	int nErr = AEE_SUCCESS;
	struct fastrpc_init_create uproc = {0};
	apps_std_FILE fh = -1;
	int battach;
	uint32_t info = domain & DOMAIN_ID_MASK;

	FARF(HIGH, "starting %s for domain %d", __func__, domain);
	pthread_mutex_lock(&hlist[domain].mut);
	pthread_setspecific(tlsKey, (void*)&hlist[domain]);
	battach = hlist[domain].dsppd;
	if(!hlist[domain].initialized) {
		if (hlist[domain].dev == -1)
			hlist[domain].dev = open_device_node_internal(domain);

		VERIFYC(hlist[domain].dev >= 0, AEE_EFOPEN);
		FARF(HIGH, "%s: device %d opened with info 0x%x (attach %d)", __func__, hlist[domain].dev, hlist[domain].info, battach);
		hlist[domain].initialized = 1;
		//keep the memory we used to allocate
		if (battach == GUEST_OS || battach == GUEST_OS_SHARED) {
			FARF(HIGH, "%s: attaching to guest OS for domain %d", __func__, domain);
			VERIFY(!ioctl(hlist[domain].dev, FASTRPC_IOCTL_INIT_ATTACH) || errno == ENOTTY);
		} else if (battach == USER_PD) {
			uint64 len = 0;
			uint64 filelen = 0;
			int readlen = 0, eof;
			int procattr = 0;
			apps_std_FILE fsig = -1;
			uint64 siglen = 0;

			VERIFY(0 == open_shell(domain, &fh, hlist[domain].unsigned_module));

			hlist[domain].procattrs = get_process_attrs(domain);
			if (IS_DEBUG_MODE_ENABLED(hlist[domain].procattrs))
				get_process_testsig(&fsig, &siglen);

			if (fh != -1) {
				VERIFY(AEE_SUCCESS == (nErr = apps_std_flen(fh, &len)));
				filelen = len + siglen;
				VERIFYC(filelen < INT_MAX, AEE_EBADSIZE);
				pthread_mutex_unlock(&hlist[domain].mut);
				FARF(HIGH,"debug:file len:%llx",filelen);
				FARF(HIGH,"debug:file len to rpc malloc:%x",filelen);
				uproc.file = (__u64)rpcmem_alloc_internal(0, RPCMEM_HEAP_DEFAULT, (int)(filelen));
				pthread_mutex_lock(&hlist[domain].mut);
				VERIFYC(uproc.file, AEE_ENORPCMEMORY);
				VERIFY(AEE_SUCCESS == (nErr = apps_std_fread(fh, (void *)uproc.file, len, &readlen, &eof)));
				VERIFYC((int)len == readlen, AEE_EFREAD);
				uproc.filefd = rpcmem_to_fd_internal((void *)uproc.file);
				uproc.filelen = (int)len;
				VERIFYC(uproc.filefd != -1, AEE_EINVALIDFD);
			} else {
				FARF(ERROR, "Unable to open shell file\n");
			}
			uproc.attrs = hlist[domain].procattrs;
			if(siglen && fsig != -1) {
				VERIFY(AEE_SUCCESS == (nErr = apps_std_fread(fsig, (byte*)(uproc.file + len), siglen, &readlen, &eof)));
				VERIFYC(siglen == (uint64)readlen, AEE_EFREAD);
				uproc.siglen = siglen;
				uproc.filelen = len + siglen;
			}
			nErr = ioctl(hlist[domain].dev, FASTRPC_IOCTL_INIT_CREATE, (unsigned long)&uproc);
			if (nErr == AEE_SUCCESS) {
				FARF(HIGH, "Successfully created user PD on domain %d (attrs 0x%x)", domain, hlist[domain].procattrs);
			}
		} else {
			FARF(ERROR, "Error: %s called for unknown mode %d", __func__, battach);
		}
	}
bail:
	pthread_mutex_unlock(&hlist[domain].mut);
	if(uproc.file) {
		rpcmem_free_internal((void*)uproc.file);
	}
	if(fh != -1) {
		apps_std_fclose(fh);
	}
	if(nErr != AEE_SUCCESS) {
		domain_deinit(domain);
		FARF(ERROR, "Error 0x%x: %s failed for domain %d, errno %s\n", nErr, __func__, domain, strerror(errno));
	}
	FARF(HIGH, "Done with %s, err: 0x%x, dev: %d", __func__, nErr, hlist[domain].dev);
	return nErr;
}

__attribute__((destructor))
static void close_dev(void) {
	int i;
	for(i = 0; i < NUM_DOMAINS_EXTEND; i++) {
		domain_deinit(i);
	}
	pl_deinit();
	PL_DEINIT(fastrpc_apps_user);
}

remote_handle64 get_adsp_current_process1_handle(int domain) {
   struct handle_info* hinfo;
   int nErr = AEE_SUCCESS;

   VERIFYC(hlist[domain].domainsupport, AEE_EBADDOMAIN);
   if(hlist[domain].cphandle) {
      return hlist[domain].cphandle;
   }
   VERIFY(AEE_SUCCESS == (nErr = alloc_handle(domain, _const_adsp_current_process1_handle, &hinfo)));
   hlist[domain].cphandle = hinfo->local;
   return hlist[domain].cphandle;
bail:
   if (nErr != AEE_SUCCESS) {
        if (hlist[domain].domainsupport)
		FARF(ERROR, "Error %x: adsp current process handle failed. domain %d\n", nErr, domain);
	else if (!hlist[domain].nondomainsupport)
		FARF(ERROR, "Error %x: adsp current process handle failed. domain %d\n", nErr, domain);
   }
   return INVALID_HANDLE;
}

remote_handle64 get_adspmsgd_adsp1_handle(int domain) {
   struct handle_info* hinfo;
   int nErr = AEE_SUCCESS;

   VERIFYC(hlist[domain].domainsupport, AEE_EBADDOMAIN);
   if(hlist[domain].msghandle) {
      return hlist[domain].msghandle;
   }
   VERIFY(AEE_SUCCESS == (nErr = alloc_handle(domain, _const_adspmsgd_adsp1_handle, &hinfo)));
   hlist[domain].msghandle = hinfo->local;
   return hlist[domain].msghandle;
bail:
   if (nErr != AEE_SUCCESS) {
        FARF(ERROR,"Error %x: get adsp msgd handle failed. domain %d\n", nErr, domain);
   }
   return INVALID_HANDLE;
}

static int open_dev(int domain) {
   static pthread_once_t pl = PTHREAD_ONCE_INIT;
   int init = 0, nErr = AEE_SUCCESS;

   if(hlist && hlist[domain].dev != -1 && hlist[domain].initialized) {
      if(0 == pthread_getspecific(tlsKey)) {
         pthread_setspecific(tlsKey, (void*)&hlist[domain]);
      }
      goto bail;
   }
   VERIFY(AEE_SUCCESS == (nErr = fastrpc_init_once()));
   VERIFY(AEE_SUCCESS == (nErr = pthread_once(&pl, (void*)pl_init)));
   init = 1;
   pthread_mutex_lock(&hlist[domain].init);
   if(hlist && hlist[domain].dev != -1 && hlist[domain].initialized) {
      goto bail;
   }
   VERIFY(AEE_SUCCESS == (nErr = apps_dev_init(domain)));
   VERIFY(AEE_SUCCESS == (nErr = listener_android_domain_init(domain)));
   initFileWatcher(domain); // Ignore errors
   if(hlist){
      fastrpc_perf_init(hlist[domain].dev);
      VERIFY(AEE_SUCCESS == (nErr = fastrpc_latency_init(hlist[domain].dev, &hlist[domain].qos)));
   }
   if (hlist[domain].th_params.prio != DEFAULT_UTHREAD_PRIORITY || hlist[domain].th_params.stack_size != DEFAULT_UTHREAD_STACK_SIZE) {
      struct fastrpc_thread_params *uthread_params = &hlist[domain].th_params;

      VERIFY(AEE_SUCCESS == (nErr = pthread_create(&uthread_params->thread, NULL, fastrpc_set_remote_uthread_params, (void*)uthread_params)));
      VERIFY(AEE_SUCCESS == (nErr = pthread_join(uthread_params->thread, NULL)));
	  FARF(ALWAYS, "%s: Successfully set remote user thread priority to %d and stack size to %d",
				__func__, uthread_params->prio, uthread_params->stack_size);
   }

bail:
   if(init) {
      pthread_mutex_unlock(&hlist[domain].init);
   }
   if(nErr != AEE_SUCCESS) {
      domain_deinit(domain);
      if(hlist)
	  FARF(ERROR, "Error %x: open dev %d for domain %d failed\n", nErr, hlist[domain].dev, domain);
      return -1;
   }
   if(hlist){
       FARF(HIGH, "done open dev %d err %d", hlist[domain].dev, nErr);
       return hlist[domain].dev;
   } else {
       return -1;
   }
}

static void fastrpc_apps_user_deinit(void) {
   QNode *pn;
   int i;
   if(tlsKey != INVALID_KEY) {
      pthread_key_delete(tlsKey);
      tlsKey = INVALID_KEY;
   }
   PL_DEINIT(apps_mem);
   PL_DEINIT(apps_std);
   PL_DEINIT(rpcmem);
   if(hlist) {
      for (i = 0; i < NUM_DOMAINS_EXTEND; i++) {
         while((pn = QList_Pop(&hlist[i].ql))) {
            struct handle_info* h = STD_RECOVER_REC(struct handle_info, qn, pn);
            free(h);
            h = NULL;
         }
         pthread_mutex_destroy(&hlist[i].mut);
         pthread_mutex_destroy(&hlist[i].init);
      }
      free(hlist);
      hlist = NULL;
   }
   pthread_mutex_destroy(&fdlist.mut);
   return;
}

static void exit_thread(void *value)
{
   remote_handle64 handle;
   struct handle_list* list = (struct handle_list*)value;
   int domain;

   if(!hlist) {
      return;
   }
   domain = (int)(list - &hlist[0]);
   if(hlist[domain].dev != -1) {
      FARF(HIGH, "exiting thread domain: %d", domain);
      if((domain < NUM_DOMAINS_EXTEND) &&
         (handle = get_adsp_current_process1_handle(domain)) != INVALID_HANDLE) {
         (void)adsp_current_process1_thread_exit(handle);
      } else if (domain == DEFAULT_DOMAIN_ID) {
         (void)adsp_current_process_thread_exit();
      }
   }
}

static int fastrpc_apps_user_init() {
	int nErr = AEE_SUCCESS, i;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&fdlist.mut, 0);
	QList_Ctor(&fdlist.ql);
	std_memset(dhandles, 0, sizeof(dhandles));
	VERIFYC(NULL != (hlist = calloc(NUM_DOMAINS_EXTEND, sizeof(*hlist))), AEE_ENOMEMORY);
	for (i = 0; i < NUM_DOMAINS_EXTEND; i++) {
		hlist[i].dev = -1;
		hlist[i].domainsupport = 0;
		hlist[i].nondomainsupport = 0;
		hlist[i].kmem_support = 0;
		hlist[i].th_params.prio = DEFAULT_UTHREAD_PRIORITY;
		hlist[i].th_params.stack_size = DEFAULT_UTHREAD_STACK_SIZE;
		hlist[i].th_params.reqID = 0;
		hlist[i].dsppd = attach_guestos(i);
		hlist[i].dsppdname = NULL;
		pthread_mutex_init(&hlist[i].mut, &attr);
		pthread_mutex_init(&hlist[i].init, 0);
		QList_Ctor(&hlist[i].ql);
	}
	pthread_mutexattr_destroy(&attr);
	VERIFY(AEE_SUCCESS == (nErr = pthread_key_create(&tlsKey, exit_thread)));
	VERIFY(AEE_SUCCESS == (nErr = PL_INIT(rpcmem)));
	VERIFY(AEE_SUCCESS == (nErr = PL_INIT(apps_mem)));
	VERIFY(AEE_SUCCESS == (nErr = PL_INIT(apps_std)));
	GenCrc32Tab(POLY32, crc_table);
bail:
	if(nErr) {
		FARF(ERROR, "Error %x: fastrpc_apps_user_init failed\n", nErr);
		fastrpc_apps_user_deinit();
	}
	return nErr;
}

PL_DEFINE(fastrpc_apps_user, fastrpc_apps_user_init, fastrpc_apps_user_deinit);

static void frpc_init(void) {
   PL_INIT(fastrpc_apps_user);
}

static int fastrpc_init_once(void) {
   static pthread_once_t frpc = PTHREAD_ONCE_INIT;
   int nErr = AEE_SUCCESS;
   VERIFY(AEE_SUCCESS == (nErr = pthread_once(&frpc, (void*)frpc_init)));
bail:
   if(nErr != AEE_SUCCESS) {
	FARF(ERROR, "Error %x: fastrpc init once failed\n", nErr);
   }
   return nErr == AEE_SUCCESS ? _pl_fastrpc_apps_user()->nErr : nErr;
}

static int rpcmem_init_me(void) {
   rpcmem_init();
   return AEE_SUCCESS;
}
PL_DEFINE(rpcmem, rpcmem_init_me, rpcmem_deinit);
