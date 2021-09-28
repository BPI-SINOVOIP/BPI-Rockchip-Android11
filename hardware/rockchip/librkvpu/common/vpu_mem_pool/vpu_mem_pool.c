/*
 *
 * Copyright 2014 Rockchip Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
   * File:
   * vpu_mem_pool.c
   * Description:
   *
   * Author:
   *     Alpha Lin
   * Date:
   *    2014-1-23
 */

#include <malloc.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>

#include <cutils/log.h>
#define LOG_TAG "vpu_mem_pool"

#include "list.h"
#include "atomic.h"
#include "classmagic.h"
#include "vpu_mem_pool.h"
#include "tsemaphore.h"
#include "vpu_mem.h"
#include "vpu_dma_buf.h"
#include <errno.h>
struct vpu_memory_block {
    struct list_head lnk_status; // link to used or free list
    int mem_hdl;
    atomic_t ref;
#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    VPUMemLinear_t *dmabuf;
#endif
    int buff_size;
};

static struct vpu_dmabuf_dev *dmabuf_dev;

//#define VPU_MBLK_DEBUG
#define VPU_MBLK_WARNING
#define VPU_MBLK_INFO
#define VPU_MBLK_ERROR

#ifdef VPU_MBLK_DEBUG
#define MBLK_DBG(fmt, args...) ALOGD("%s:%d: " fmt, __func__, __LINE__, ## args)
#else
#define MBLK_DBG(fmt, args...)
#endif

#ifdef VPU_MBLK_ERROR
#define MBLK_ERR(fmt, args...) ALOGE("ERROR, pid %d, %s:%d: " fmt, getpid(), __func__, __LINE__, ## args)
#else
#define MBLK_ERR(fmt, args...)
#endif

#ifdef VPU_MBLK_WARNING
#define MBLK_WRN(fmt, args...) ALOGW("WARNING, pid %d, %s:%d: " fmt, getpid(), __func__, __LINE__, ## args);
#else
#define MBLK_WRN(fmt, args...)
#endif

#ifdef VPU_MBLK_INFO
#define MBLK_INF(fmt, args...) ALOGI("pid %d, %s:%d: " fmt, getpid(), __func__, __LINE__, ## args);
#else
#define MBLK_INF(fmt, args...)
#endif

#define VPU_MEM_POOL_INIT_MAGIC		0x4C4A46
#define TOTAL_VPUMEM_SIZE		(280*1024*1024)

typedef struct vpu_display_mem_pool_impl {
    vpu_display_mem_pool_FIELDS
    struct list_head used_list;
    struct list_head free_list;
    struct list_head abort_list;
    pthread_mutex_t list_mutex;
    atomic_t used_cnt;
    atomic_t abort_cnt;
    atomic_t init;
    tsem_t acq_sem;
#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    int size;
    pthread_t td;
    bool run_flag;
    tsem_t alloc_sem;
    int ion_client;
    bool wait_recalim_flag;
    tsem_t reclaim_sem;
#endif
#if VPU_MEMORY_POOL_MANAGER_ENABLE
    struct list_head mgr_link;
#endif
} vpu_display_mem_pool_impl;

#if VPU_MEMORY_POOL_MANAGER_ENABLE
typedef struct vpu_memory_pool_manager {
    struct list_head pend_pool_list;
    atomic_t init;
    atomic_t pend_cnt;
    atomic_t total_mem_size;
    pthread_t manager_thread;
    pthread_t observer_thread;
} vpu_memory_pool_manager;

vpu_memory_pool_manager pool_manager;
#endif

static struct vpu_memory_block* create_memblk_from_hdl(int mem_hdl)
{
    struct vpu_memory_block *memblk =
        (struct vpu_memory_block*)calloc(1, sizeof(struct vpu_memory_block));

    if (memblk == NULL) {
        MBLK_ERR("malloc memory block failed\n");
        return NULL;
    }

    atomic_set(&memblk->ref, 0);
    memblk->mem_hdl = mem_hdl;

    INIT_LIST_HEAD(&memblk->lnk_status);

    return memblk;
}

static int get_free_memory_handle(vpu_display_mem_pool *p)
{
    struct vpu_memory_block *mblk, *n;
    int trycnt = 0;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;

    assert(p_mempool);

    MBLK_DBG("In\n");

    if (atomic_read(&p_mempool->init) != VPU_MEM_POOL_INIT_MAGIC) {
        MBLK_ERR("memory pool pre-init\n");
        return -1;
    }

    if (ETIMEDOUT == tsem_timed_down(&p_mempool->acq_sem, 1000)) {
        MBLK_ERR("timeout when acquire memory handle\n");
        return -1;
    }

    pthread_mutex_lock(&p_mempool->list_mutex);
    list_for_each_entry_safe(mblk, n, &p_mempool->free_list, lnk_status) {
        if (mblk) {
            MBLK_DBG("memory block (fd = %d) available\n", mblk->mem_hdl);

            list_del_init(&mblk->lnk_status);
            list_add_tail(&mblk->lnk_status, &p_mempool->used_list);

            atomic_inc(&p_mempool->used_cnt);

            atomic_inc(&mblk->ref);
            pthread_mutex_unlock(&p_mempool->list_mutex);
            return mblk->mem_hdl;
        }
    }

    pthread_mutex_unlock(&p_mempool->list_mutex);

    MBLK_WRN("close vpu memory pool when acquire memory handle\n");

    return -1;
}

static int inc_used_memory_handle_ref(vpu_display_mem_pool *p, int mem_hdl)
{
    struct vpu_memory_block *mblk, *n;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;

    assert(p_mempool);

    pthread_mutex_lock(&p_mempool->list_mutex);

    if(atomic_read(&p_mempool->abort_cnt)){
            list_for_each_entry_safe(mblk, n, &p_mempool->abort_list, lnk_status) {
               if (mblk->mem_hdl == mem_hdl) {
                MBLK_DBG("inc used memory block (fd = %d, ref = %d) find out in rest list\n",
                    mblk->mem_hdl, atomic_read(&mblk->ref));

                atomic_inc(&mblk->ref);

                pthread_mutex_unlock(&p_mempool->list_mutex);

                return 0;
            }
        }
    }
    list_for_each_entry_safe(mblk, n, &p_mempool->used_list, lnk_status) {
        if (mblk->mem_hdl == mem_hdl) {
            MBLK_DBG("used memory block (fd = %d, ref = %d) find out in list\n",
                mblk->mem_hdl, atomic_read(&mblk->ref));

            atomic_inc(&mblk->ref);

            pthread_mutex_unlock(&p_mempool->list_mutex);

            return 0;
        }
    }

    pthread_mutex_unlock(&p_mempool->list_mutex);

    MBLK_ERR("used memory block (fd = %d) absent in list\n", mem_hdl);

    return -1;
}

static int put_used_memory_handle(vpu_display_mem_pool *p, int mem_hdl)
{
    struct vpu_memory_block *mblk, *n;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;

    assert(p_mempool);

    pthread_mutex_lock(&p_mempool->list_mutex);
    if(atomic_read(&p_mempool->abort_cnt)){
        list_for_each_entry_safe(mblk, n, &p_mempool->abort_list, lnk_status) {
            MBLK_DBG("used memory block ( mblk fd = %d, mem_hdl fd = %d)\n",mblk->mem_hdl,mem_hdl);
            if (mblk->mem_hdl == mem_hdl) {
                MBLK_DBG("used memory block (fd = %d, ref = %d) find out in list\n",
                mblk->mem_hdl, atomic_read(&mblk->ref));
                atomic_dec(&mblk->ref);
                if (0 == atomic_read(&mblk->ref)) {
                    MBLK_DBG("memory block (fd = %d) exhause\n", mblk->mem_hdl);
                    list_del_init(&mblk->lnk_status);
                    atomic_dec(&p_mempool->abort_cnt);
                    dmabuf_dev->unmap(dmabuf_dev, mblk->dmabuf);
                    MBLK_DBG("fd colse 0x%x",mblk->mem_hdl);
                    close(mblk->mem_hdl);
                    free(mblk);
                }
                pthread_mutex_unlock(&p_mempool->list_mutex);
                return 0;
            }
        }
    }

    list_for_each_entry_safe(mblk, n, &p_mempool->used_list, lnk_status) {
        if (mblk->mem_hdl == mem_hdl) {
            MBLK_DBG("used memory block (fd = %d, ref = %d) find out in list\n",
                mblk->mem_hdl, atomic_read(&mblk->ref));
            atomic_dec(&mblk->ref);
            if (0 == atomic_read(&mblk->ref)) {
                MBLK_DBG("memory block (fd = %d) exhause\n", mblk->mem_hdl);
                list_del_init(&mblk->lnk_status);
                if((mblk->buff_size < p->buff_size) && (p_mempool->version == 2)){
                    MBLK_DBG("memory block (fd = %d) exhause free dirctly\n", mblk->mem_hdl);
                    atomic_dec(&p_mempool->used_cnt);
                        dmabuf_dev->unmap(dmabuf_dev, mblk->dmabuf);
                            // a pool with internal allocator.
                    MBLK_DBG("fd colse 0x%x",mblk->mem_hdl);
                            close(mblk->mem_hdl);
#if VPU_MEMORY_POOL_MANAGER_ENABLE
                        atomic_sub(mblk->buff_size, &pool_manager.total_mem_size);
		                MBLK_INF("vpu memory pool size (%d)\n", atomic_read(&pool_manager.total_mem_size));
#endif
                        free(mblk);
			}else{
            	list_add_tail(&mblk->lnk_status, &p_mempool->free_list);

            	atomic_dec(&p_mempool->used_cnt);

				if (p_mempool->wait_recalim_flag)
					tsem_up(&p_mempool->reclaim_sem);
                tsem_up(&p_mempool->acq_sem);
                }
            }

            pthread_mutex_unlock(&p_mempool->list_mutex);

            return 0;
        }
    }

    pthread_mutex_unlock(&p_mempool->list_mutex);

    MBLK_ERR("used memory block (fd = %d) absent in list\n", mem_hdl);

    return -1;
}

static int get_free_memory_num(vpu_display_mem_pool *p)
{
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;
    int num = 0;
    assert(p_mempool);
    pthread_mutex_lock(&p_mempool->list_mutex);
    if(p_mempool->version == 1){
    	num = p_mempool->acq_sem.semval;
    }else{
        num = 1;
    }
    pthread_mutex_unlock(&p_mempool->list_mutex);
    return num;
}
#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
static struct vpu_memory_block* create_memblk_from_vpumem(VPUMemLinear_t *dmabuf);
#endif
static int commit_memory_handle(vpu_display_mem_pool *p, int mem_hdl, int size)
{
    struct vpu_memory_block *mblk;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;
    VPUMemLinear_t *dmabuf = NULL;
    int ret = 0;

    MBLK_DBG("In\n");

    assert(p_mempool);

#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    if(p_mempool->version == 1){
    mem_hdl = dup(mem_hdl);
    }
    ret = dmabuf_dev->map(dmabuf_dev, mem_hdl, size, &dmabuf);
    if (ret) {
        MBLK_ERR("dma-buf map failed\n");
        return -1;
    }
    mblk = create_memblk_from_vpumem(dmabuf);
#else
    mblk = create_memblk_from_hdl(mem_hdl);
#endif

    if (mblk != NULL) {
        mblk->buff_size = size;
        pthread_mutex_lock(&p_mempool->list_mutex);
        list_add_tail(&mblk->lnk_status, &p_mempool->free_list);
        pthread_mutex_unlock(&p_mempool->list_mutex);
        tsem_up(&p_mempool->acq_sem);
    } else {
        return -1;
    }

    return mem_hdl;
}

static int reset_vpu_mem_pool(vpu_display_mem_pool *p)
{
    struct vpu_memory_block *mblk, *n;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;

    MBLK_DBG("In\n");

    assert(p_mempool);

    if (atomic_read(&p_mempool->used_cnt) > 0) {
        MBLK_WRN("reset vpu memory pool when %d memory handle still in used\n", atomic_read(&p_mempool->used_cnt));
    }

    pthread_mutex_lock(&p_mempool->list_mutex);
    atomic_set(&p_mempool->used_cnt, 0);
    tsem_up(&p_mempool->acq_sem);
    tsem_reset(&p_mempool->acq_sem);

#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    list_for_each_entry_safe(mblk, n, &p_mempool->free_list, lnk_status) {
        if (mblk) {
            dmabuf_dev->unmap(dmabuf_dev, mblk->dmabuf);
            MBLK_DBG("fd colse 0x%x",mblk->mem_hdl);
                close(mblk->mem_hdl);
            list_del_init(&mblk->lnk_status);
            free(mblk);
        }
    }
    list_for_each_entry_safe(mblk, n, &p_mempool->used_list, lnk_status) {
        if (mblk) {
            MBLK_INF("put to abort fd = %d \n",mblk->mem_hdl);
            list_del_init(&mblk->lnk_status);
            list_add_tail(&mblk->lnk_status, &p_mempool->abort_list);
            atomic_dec(&p_mempool->used_cnt);
            atomic_inc(&p_mempool->abort_cnt);
        }
    }
#endif

    INIT_LIST_HEAD(&p_mempool->free_list);
    INIT_LIST_HEAD(&p_mempool->used_list);

    p_mempool->buff_size = -1;

    pthread_mutex_unlock(&p_mempool->list_mutex);
    MBLK_DBG("reset vpu memory pool success\n");

    return 0;
}

#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
static void* get_free_memory_vpumem(vpu_display_mem_pool *p);
#endif

#if VPU_MEMORY_POOL_MANAGER_ENABLE
void* pool_manager_thread(void *param);
static void* vpu_memory_status_observer(void *param);
#endif

vpu_display_mem_pool* open_vpu_memory_pool()
{
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)calloc(1, sizeof(vpu_display_mem_pool_impl));

    if (p_mempool == NULL) {
        return NULL;
    }

#if VPU_MEMORY_POOL_MANAGER_ENABLE
    if (atomic_read(&pool_manager.init) != VPU_MEM_POOL_INIT_MAGIC) {
        INIT_LIST_HEAD(&pool_manager.pend_pool_list);
        atomic_set(&pool_manager.pend_cnt, 0);
        //pthread_create(&pool_manager.manager_thread, NULL, pool_manager_thread, NULL);
        //pthread_create(&pool_manager.observer_thread, NULL, vpu_memory_status_observer, NULL);
        atomic_set(&pool_manager.init, VPU_MEM_POOL_INIT_MAGIC);
        atomic_set(&pool_manager.total_mem_size, 0);
    }
#endif

#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    if (dmabuf_dev == NULL) {
        if (0 > vpu_dmabuf_open(4096, &dmabuf_dev, "inneralloc")) {
            MBLK_ERR("Open dmabuf device failed\n");
            return NULL;
        }
    }
#endif

    atomic_set(&p_mempool->used_cnt, 0);
    atomic_set(&p_mempool->abort_cnt, 0);
    tsem_init(&p_mempool->acq_sem, 0);

    pthread_mutex_init(&p_mempool->list_mutex, NULL);

    INIT_LIST_HEAD(&p_mempool->free_list);
    INIT_LIST_HEAD(&p_mempool->used_list);
    INIT_LIST_HEAD(&p_mempool->abort_list);

    p_mempool->commit_hdl = commit_memory_handle;
#if !ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    p_mempool->get_free   = get_free_memory_handle;
#else
    p_mempool->get_free   = get_free_memory_vpumem;
#endif
    p_mempool->put_used   = put_used_memory_handle;
    p_mempool->inc_used   = inc_used_memory_handle_ref;
    p_mempool->reset      = reset_vpu_mem_pool;
    p_mempool->get_unused_num = get_free_memory_num;

    p_mempool->version    = 1;

    p_mempool->buff_size = -1;
    p_mempool->size = 0;
    atomic_set(&p_mempool->init, VPU_MEM_POOL_INIT_MAGIC);

    MBLK_INF("success\n");

    return (vpu_display_mem_pool*)p_mempool;
}

void close_vpu_memory_pool(vpu_display_mem_pool *p)
{
    struct vpu_memory_block *mblk, *n;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;
    int trycnt = 50;

    MBLK_DBG("vpu memory pool destructure\n");

    assert(p_mempool);

    pthread_mutex_lock(&p_mempool->list_mutex);
#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    // memory from external allocator;
    list_for_each_entry_safe(mblk, n, &p_mempool->free_list, lnk_status) {
        if (mblk) {
            list_del_init(&mblk->lnk_status);
            dmabuf_dev->unmap(dmabuf_dev, mblk->dmabuf);
                // a pool with internal allocator.
            MBLK_DBG("fd colse 0x%x",mblk->mem_hdl);
                close(mblk->mem_hdl);
			if (p_mempool->version == 2) {
#if VPU_MEMORY_POOL_MANAGER_ENABLE
                atomic_sub(p_mempool->size, &pool_manager.total_mem_size);
		MBLK_INF("vpu memory pool size (%d)\n", atomic_read(&pool_manager.total_mem_size));
#endif
            }
            free(mblk);
        }
    }
#endif

#if !VPU_MEMORY_POOL_MANAGER_ENABLE
    while (atomic_read(&p_mempool->used_cnt) > 0 && trycnt-- > 0) {
        MBLK_WRN("close vpu memory pool when %d memory handle still in used\n", atomic_read(&p_mempool->used_cnt));
        usleep(10000);
    }
#else
    if (atomic_read(&p_mempool->used_cnt) > 0) {
        MBLK_WRN("close vpu memory pool when %d memory handle still in used, delay reclaim\n", atomic_read(&p_mempool->used_cnt));
        INIT_LIST_HEAD(&p_mempool->mgr_link);
        list_add_tail(&p_mempool->mgr_link, &pool_manager.pend_pool_list);
        atomic_inc(&pool_manager.pend_cnt);
        pthread_mutex_unlock(&p_mempool->list_mutex);
        return;
    }
#endif
    if(atomic_read(&p_mempool->abort_cnt)){
        list_for_each_entry_safe(mblk, n, &p_mempool->abort_list, lnk_status) {
            if (mblk) {
                list_del_init(&mblk->lnk_status);
                dmabuf_dev->unmap(dmabuf_dev, mblk->dmabuf);
                    // a pool with internal allocator.
                MBLK_DBG("fd colse 0x%x",mblk->mem_hdl);
                    close(mblk->mem_hdl);
				if (p_mempool->version == 2) {
#if VPU_MEMORY_POOL_MANAGER_ENABLE
                    atomic_sub(p_mempool->size, &pool_manager.total_mem_size);
        	MBLK_INF("vpu memory pool size (%d)\n", atomic_read(&pool_manager.total_mem_size));
#endif
                }
                free(mblk);
            }
        }
    }
    pthread_mutex_unlock(&p_mempool->list_mutex);

    tsem_up(&p_mempool->acq_sem);
    tsem_deinit(&p_mempool->acq_sem);

    atomic_set(&p_mempool->init, 0);
    pthread_mutex_destroy(&p_mempool->list_mutex);

    free(p_mempool);

    MBLK_INF("\n");
}

#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR

#include <ion/ion.h>

static struct vpu_memory_block* create_memblk_from_vpumem(VPUMemLinear_t *dmabuf)
{
    struct vpu_memory_block *memblk =
        (struct vpu_memory_block*)calloc(1, sizeof(struct vpu_memory_block));

    if (memblk == NULL) {
        MBLK_ERR("malloc memory block failed\n");
        return NULL;
    }

    atomic_set(&memblk->ref, 0);
    memblk->mem_hdl = dmabuf_dev->get_fd(dmabuf);
    memblk->dmabuf = dmabuf;

    INIT_LIST_HEAD(&memblk->lnk_status);

    return memblk;
}

static void* get_free_memory_vpumem(vpu_display_mem_pool *p)
{
    struct vpu_memory_block *mblk, *n;
    int trycnt = 0;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;

    assert(p_mempool);

    MBLK_DBG("In\n");

    if (atomic_read(&p_mempool->init) != VPU_MEM_POOL_INIT_MAGIC) {
        MBLK_ERR("memory pool pre-init\n");
        return NULL;
    }
    if((p_mempool->buff_size > p_mempool->size) && (p_mempool->version == 2)){
         pthread_mutex_lock(&p_mempool->list_mutex);
#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
         // memory from external allocator;
         list_for_each_entry_safe(mblk, n, &p_mempool->free_list, lnk_status) {
         if (mblk) {
                 MBLK_DBG("memory block (fd = %d) free dirctly\n", mblk->mem_hdl);
                 list_del_init(&mblk->lnk_status);
                 dmabuf_dev->unmap(dmabuf_dev, mblk->dmabuf);
                     // a pool with internal allocator.
                     close(mblk->mem_hdl);
				if (p_mempool->version == 2) {
#if VPU_MEMORY_POOL_MANAGER_ENABLE
                     atomic_sub(p_mempool->size, &pool_manager.total_mem_size);
                     MBLK_INF("vpu memory pool size (%d)\n", atomic_read(&pool_manager.total_mem_size));
#endif
                 }
                 free(mblk);
                 p_mempool->acq_sem.semval--;
             }
         }
#endif
        p_mempool->size = p_mempool->buff_size;
        pthread_mutex_unlock(&p_mempool->list_mutex);
    }
#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    // only for the pool which has its own internal allocator.
    if (p_mempool->version > 1 && p_mempool->acq_sem.semval < 2) {
        MBLK_DBG("memory handle count (%d) less than 2, allocate more\n", p_mempool->acq_sem.semval);
        tsem_up(&p_mempool->alloc_sem);
    }
#endif

    if (ETIMEDOUT == tsem_timed_down(&p_mempool->acq_sem, 1000)) {
        MBLK_ERR("timeout when acquire memory handle\n");
        return NULL;
    }

    pthread_mutex_lock(&p_mempool->list_mutex);
    list_for_each_entry_safe(mblk, n, &p_mempool->free_list, lnk_status) {
        if (mblk) {
            MBLK_DBG("memory block (fd = %d) available\n", mblk->mem_hdl);

            list_del_init(&mblk->lnk_status);
            list_add_tail(&mblk->lnk_status, &p_mempool->used_list);

            atomic_inc(&p_mempool->used_cnt);

            atomic_inc(&mblk->ref);
            pthread_mutex_unlock(&p_mempool->list_mutex);
            mblk->dmabuf->size = mblk->buff_size;
            return (void*)mblk->dmabuf;
        }
    }

    pthread_mutex_unlock(&p_mempool->list_mutex);

    MBLK_WRN("close vpu memory pool when acquire memory handle\n");

    return NULL;
}

static int commit_memory_vpumem(vpu_display_mem_pool *p, VPUMemLinear_t *dmabuf)
{
    struct vpu_memory_block *mblk;
    vpu_display_mem_pool_impl *p_mempool = (vpu_display_mem_pool_impl*)p;

    assert(p_mempool);

    MBLK_DBG("In\n");

    mblk = create_memblk_from_vpumem(dmabuf);
    if (mblk != NULL) {
        pthread_mutex_lock(&p_mempool->list_mutex);
        list_add_tail(&mblk->lnk_status, &p_mempool->free_list);
        pthread_mutex_unlock(&p_mempool->list_mutex);
        tsem_up(&p_mempool->acq_sem);
    } else {
        return -1;
    }

    return 0;
}

void* vpu_mem_allocator(void *param)
{
    vpu_display_mem_pool_impl *pool = (vpu_display_mem_pool_impl*)param;
    int ret;

    while (pool->run_flag) {
        //VPUMemLinear_t *dmabuf;
        int share_fd;
        int total_size;
        int pool_size = pool->size;
        tsem_down(&pool->alloc_sem);

        if (!pool->run_flag) {
            break;
        }

        MBLK_DBG("In\n");

#if VPU_MEMORY_POOL_MANAGER_ENABLE
        total_size = atomic_read(&pool_manager.total_mem_size);
        /*MBLK_INF("vpu memory pool size (%d)\n", total_size);*/
        if (total_size > TOTAL_VPUMEM_SIZE) {
            MBLK_ERR("vpu memory pool size (%d), disable allocation\n", total_size);
            continue;
        }
#endif
	if (pool->wait_recalim_flag) {
		tsem_reset(&pool->reclaim_sem);
		ret = tsem_timed_down(&pool->reclaim_sem, 40);
		if (ret != ETIMEDOUT)
			continue;
	}

    if (0 > ion_alloc_fd(pool->ion_client, pool_size, 4096, vpu_mem_judge_used_heaps_type(), 0, &share_fd)) {
            MBLK_ERR("ion_alloc_fd failed\n");
	    pool->wait_recalim_flag = true;
            continue;
        }

#if VPU_MEMORY_POOL_MANAGER_ENABLE
        atomic_add(pool_size, &pool_manager.total_mem_size);
        MBLK_INF("vpu memory pool size (%d)\n", atomic_read(&pool_manager.total_mem_size));
#endif

        MBLK_INF("ion_alloc_fd success, memory fd %d\n", share_fd);

        if (0 > commit_memory_handle((vpu_display_mem_pool*)pool, share_fd,pool_size)) {
            MBLK_ERR("commit memory_handle failed, memory fd %d\n", share_fd);
            close(share_fd);
            continue;
        }
    }

    return NULL;
}

int create_vpu_memory_pool_allocator(vpu_display_mem_pool **ipool, int num, int size)
{
    int i;
    vpu_display_mem_pool_impl *pool;
    struct vpu_memory_block *mblk, *n;

    if (size <= 0) {
        MBLK_ERR("Invalidate parameter, size = %d\n", size);
        return -1;
    }

    pool = (vpu_display_mem_pool_impl *)open_vpu_memory_pool();
    tsem_init(&pool->alloc_sem, 0);
    pool->version = 2; // indicate memory from internal allocator
    pool->wait_recalim_flag = false;
    tsem_init(&pool->reclaim_sem, 0);

    pool->ion_client = ion_open();
    if (pool->ion_client < 0) {
        MBLK_ERR("Open ion device failed\n");
        return pool->ion_client;
    }

    pool->run_flag = true;
    pool->size = size;
    pool->buff_size = size;

    if (pthread_create(&pool->td, NULL, vpu_mem_allocator, pool)) {
        MBLK_ERR("create allocator thread failed\n");
        goto fail;
    }

    for (i=0; i<num; i++) {
        tsem_up(&pool->alloc_sem);
    }

    *ipool = (vpu_display_mem_pool*)pool;

    return 0;

fail:
    if (pool->td > 0) {
        pool->run_flag = false;
        tsem_up(&pool->alloc_sem);
        pthread_join(pool->td, NULL);
    }

    if (pool->ion_client) {
        ion_close(pool->ion_client);
    }

    close_vpu_memory_pool((vpu_display_mem_pool*)pool);

    return -1;
}

void release_vpu_memory_pool_allocator(vpu_display_mem_pool *ipool)
{
    vpu_display_mem_pool_impl *pool = (vpu_display_mem_pool_impl*)ipool;
    struct vpu_memory_block *mblk, *n;

    assert(pool);

    pool->run_flag = false;
    tsem_up(&pool->alloc_sem);
    pthread_join(pool->td, NULL);

    tsem_deinit(&pool->alloc_sem);

    ion_close(pool->ion_client);

    close_vpu_memory_pool((vpu_display_mem_pool*)pool);
}

#endif

#if VPU_MEMORY_POOL_MANAGER_ENABLE
void* pool_manager_thread(void *param)
{
    while (1) {
        vpu_display_mem_pool_impl *pool = NULL, *n;
        struct vpu_memory_block *mblk, *m;

        sleep(1);

        if (atomic_read(&pool_manager.pend_cnt)) {
            MBLK_WRN("pools count (%d) still pending in pool manager\n", atomic_read(&pool_manager.pend_cnt));
        }

        list_for_each_entry_safe(pool, n, &pool_manager.pend_pool_list, mgr_link) {
            if (pool) {
                if (atomic_read(&pool->used_cnt) > 0) {
                    MBLK_WRN("close vpu memory pool(%p) when %d memory handle still in used, delay reclaim\n", pool, atomic_read(&pool->used_cnt));
                    continue;
                }

                tsem_up(&pool->acq_sem);
                tsem_deinit(&pool->acq_sem);

                atomic_set(&pool->init, 0);
                pthread_mutex_destroy(&pool->list_mutex);

#if ENABLE_VPU_MEMORY_POOL_ALLOCATOR
                list_for_each_entry_safe(mblk, m, &pool->free_list, lnk_status) {
                    if (mblk) {
                        list_del_init(&mblk->lnk_status);
                        dmabuf_dev->unmap(dmabuf_dev, mblk->dmabuf);
                            // a pool with internal allocator.

                        MBLK_DBG("fd colse 0x%x",mblk->mem_hdl);
                            close(mblk->mem_hdl);
                        if (pool->version == 2) {
                            atomic_sub(pool->size, &pool_manager.total_mem_size);
                            MBLK_INF("vpu memory pool size (%d)\n", atomic_read(&pool_manager.total_mem_size));
                        }
                        free(mblk);
                    }
                }
#endif
                list_del_init(&pool->mgr_link);

                free(pool);

                atomic_dec(&pool_manager.pend_cnt);

                MBLK_INF("\n");
            }
        }
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path) \
                      + strlen ((ptr)->sun_path))

#define BACKLOG 5     /* maxium clients connected in */
#define BUF_SIZE 200
int fd_A[BACKLOG];
int conn_amount = 0;

static void showclient() {
    int i;
    MBLK_INF("client amount: %d\n", conn_amount);
    for (i = 0; i < conn_amount; i++) {
        MBLK_INF("[%d]:%d  ", i, fd_A[i]);
    }
}

extern int vpu_dmabuf_dump(struct vpu_dmabuf_dev *idev, const char *parent);
extern vpu_dmabuf_dev* VPUMemGetDev();
static void* vpu_memory_status_observer(void *param)
{
    int sock_fd, new_fd;
    struct sockaddr_un server_addr;    // server address information
    struct sockaddr_un client_addr;    // connector's address information
    socklen_t sun_size;
    int yes = 1;
    char buf[BUF_SIZE];
    int ret;
    int i;

    // create observer socket
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        MBLK_ERR("Create listening socket error!");
        return NULL;
    }

    // configure observer socket
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        MBLK_ERR("setsockopt error!");
        return NULL;
    }

    server_addr.sun_family = AF_UNIX;         // host byte order
    strcpy(server_addr.sun_path, "/data/vpumem_observer");
    unlink("/data/vpumem_observer");

    if (bind(sock_fd, (struct sockaddr *)&server_addr, SUN_LEN(&server_addr)) == -1) {
        MBLK_ERR("bind error!");
        return NULL;
    }

    if (listen(sock_fd, BACKLOG) == -1) {
        MBLK_ERR("listen error!");
        return NULL;
    }

    MBLK_INF("observer: /data/vpumem_observer\n");

    fd_set fdsr;
    int maxsock;
    struct timeval tv;
    conn_amount = 0;
    sun_size = sizeof(client_addr);
    maxsock = sock_fd;
    while (1) {
        FD_ZERO(&fdsr);
        FD_SET(sock_fd, &fdsr);
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        for (i=0; i<BACKLOG; i++) {
            if (fd_A[i] != 0) {
                MBLK_DBG("fd_A[%d] = %d\n", i, fd_A[i]);
                FD_SET(fd_A[i], &fdsr);
            }
        }

        ret = select(maxsock + 1, &fdsr, NULL, NULL, &tv);
        if (ret < 0) {
            MBLK_ERR("select error!");
            break;
        } else if (ret == 0) {
            MBLK_DBG("timeout\n");
            continue;
        }

        for (i=0; i<conn_amount; i++) {
            if (FD_ISSET(fd_A[i], &fdsr)) {
                ret = recv(fd_A[i], buf, sizeof(buf), 0);
                if (ret <= 0) {
                    MBLK_DBG("client[%d] close\n", i);
                    close(fd_A[i]);
                    FD_CLR(fd_A[i], &fdsr);
                    fd_A[i] = 0;
                    conn_amount--;
                } else {
                    if (ret < BUF_SIZE)
                        memset(&buf[ret], '\0', 1);
                    MBLK_DBG("client[%d] send:%s\n", i, buf);

		    vpu_dmabuf_dump(dmabuf_dev, "inneralloc");
		    vpu_dmabuf_dump(VPUMemGetDev(), "vpumem");
                }
            }
        }

	// check if a new connect in
        if (FD_ISSET(sock_fd, &fdsr)) {
            new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sun_size);
            if (new_fd <= 0) {
                MBLK_ERR("accept socket error!");
                continue;
            }

            if (conn_amount < BACKLOG) {
                for (i=0; i<conn_amount+1; i++) {
                    if (fd_A[i] == 0) {
                        fd_A[i] = new_fd;
                        MBLK_DBG("new connection client[%d]\n", i);
                        if (new_fd > maxsock)
                            maxsock = new_fd;
                        break;
                    }
                }

                if (i == conn_amount+1) {
                    MBLK_ERR("Error! conn_amount %d, fd_A[%d]=%d\n", conn_amount, conn_amount, fd_A[conn_amount]);
                    return NULL;
                }

                conn_amount++;
            } else {
                MBLK_INF("max connections arrive, exit\n");
                send(new_fd, "bye", 4, 0);
                close(new_fd);
                break;
            }
        }
        //showclient();
    }

    // close all connection
    for (i = 0; i < BACKLOG; i++) {
        if (fd_A[i] != 0) {
            close(fd_A[i]);
        }
    }

    MBLK_INF("observer quit\n");
    return NULL;
}

#endif

#ifdef VPU_MEMORY_BLOCK_TEST

#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

static int64_t GetTime() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return((int64_t)now.tv_sec) * 1000000 + ((int64_t)now.tv_usec);
}

void* mem_consumer_thread(void *param)
{
    int i = 0;
    vpu_display_mem_pool *pool = (vpu_display_mem_pool*)param;

    unsigned int randomnum;
    int us;

    srand((unsigned int)(time(NULL)));
    randomnum = rand();

    us = randomnum * 10 / RAND_MAX + 1;

    while (i++ < 10) {
        VPUMemLinear_t *dmabuf;
        int hdl;

        dmabuf = (VPUMemLinear_t*)pool->get_free(pool);
        hdl = dmabuf_dev->get_fd(dmabuf);

        MBLK_DBG("thread [%d] get handle %d\n", us, hdl);

        usleep(us * 10);

        pool->inc_used(pool, hdl);

        usleep(us * 10);

        MBLK_DBG("thread [%d] put handle %d\n", us, hdl);

        pool->put_used(pool, hdl);
        pool->put_used(pool, hdl);
    }

    return NULL;
}

int main(int argc, char **argv)
{
    int i;
    int hdl[20];
    pthread_t tr[5];
    vpu_display_mem_pool *pool;
    int64_t intime, outtime;

#if !ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    if ((pool = open_vpu_memory_pool()) == NULL) {
        return -1;
    }

    for (i=0; i<20; i++) {
        hdl[i] = i;
        commit_memory_handle(pool, hdl[i]);
    }
#else
    intime = GetTime();
    create_vpu_memory_pool_allocator(&pool, 20, 3840*2160*3/2);
    outtime = GetTime();
    MBLK_DBG("create vpu memory pool consume %lld\n", outtime - intime);
#endif

    for (i=0; i<5; i++) {
        pthread_create(&tr[i], NULL, mem_consumer_thread, pool);
    }

    for (i=0; i<5; i++) {
        pthread_join(tr[i], NULL);
    }

#if !ENABLE_VPU_MEMORY_POOL_ALLOCATOR
    close_vpu_memory_pool(pool);
#else
    release_vpu_memory_pool_allocator(pool);
#endif

    return 0;
}
#endif

