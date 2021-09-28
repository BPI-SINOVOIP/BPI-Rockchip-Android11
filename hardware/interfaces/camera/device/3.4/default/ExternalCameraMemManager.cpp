/*
 * Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "CamBufMgr"
#define LOG_NDEBUG 0

#include <sys/stat.h>
#include <unistd.h>
#include <log/log.h>
#include "ExternalCameraMemManager.h"

namespace android {
MemManagerBase::MemManagerBase()
{
    mPreviewBufferInfo = NULL;
}
MemManagerBase::~MemManagerBase()
{
    mPreviewBufferInfo = NULL;
}

void MemManagerBase::setBufferStatus(enum buffer_type_enum buf_type,
                                unsigned int buf_idx, int status) {
    struct bufferinfo_s *buf_info;

    switch(buf_type)
    {
        case PREVIEWBUFFER:
            buf_info = mPreviewBufferInfo;
        break;
        default:
            LOGE("Buffer type(0x%x) is invaildate",buf_type);
            goto getVirAddr_end;
    }

    if (buf_idx >= buf_info->mNumBffers) {
        LOGE("Buffer index(0x%x) is invalidate, Total buffer is 0x%x",
            buf_idx,buf_info->mNumBffers);
        goto getVirAddr_end;
    }

    (buf_info+buf_idx)->mStatus = status;

getVirAddr_end:
    return;
}

unsigned long MemManagerBase::getBufferAddr(enum buffer_type_enum buf_type,
                                unsigned int buf_idx, buffer_addr_t addr_type)
{
    unsigned long addr = 0x00;
    struct bufferinfo_s *buf_info;

    switch(buf_type)
    {
        case PREVIEWBUFFER:
            buf_info = mPreviewBufferInfo;
        break;
        default:
            LOGE("Buffer type(0x%x) is invaildate",buf_type);
            goto getVirAddr_end;
    }

    if (buf_idx > buf_info->mNumBffers) {
        LOGE("Buffer index(0x%x) is invalidate, Total buffer is 0x%x",
            buf_idx,buf_info->mNumBffers);
        goto getVirAddr_end;
    }

    if (addr_type == buffer_addr_vir) {
        addr = (buf_info+buf_idx)->mVirBaseAddr;
    } else if (addr_type == buffer_addr_phy) {
        addr = (buf_info+buf_idx)->mPhyBaseAddr;
    } else if (addr_type == buffer_sharre_fd) {
        addr = (buf_info+buf_idx)->mShareFd;
    }

getVirAddr_end:
    return addr;
}

int MemManagerBase::getIdleBufferIndex(enum buffer_type_enum buf_type) {
    unsigned long addr = 0x00;
    struct bufferinfo_s *buf_info;
    int index = -1;

    switch(buf_type)
    {
        case PREVIEWBUFFER:
            buf_info = mPreviewBufferInfo;
        break;
        default:
            LOGE("Buffer type(0x%x) is invaildate",buf_type);
            goto getVirAddr_end;
    }

    for (int i = 0; i < buf_info->mNumBffers; i++)
        if ((buf_info+i)->mStatus == 0) {
            index = i;
            break;
        }

    if (index >= buf_info->mNumBffers) {
        LOGE("Buffer index(0x%x) is invalidate, Total buffer is 0x%x",
            index,buf_info->mNumBffers);
        return -1;
    }
    
getVirAddr_end:
    return index;

}

int MemManagerBase::dump()
{
    return 0;
}

GrallocDrmMemManager::GrallocDrmMemManager(bool iommuEnabled)
                    :MemManagerBase(),
                    mPreviewData(NULL),
                    mHandle(NULL),
                    mOps(NULL)
{
    mOps = get_cam_ops(CAM_MEM_TYPE_GRALLOC);

    if (mOps) {
        mHandle = mOps->init(iommuEnabled ? 1:0,
                        CAM_MEM_FLAG_HW_WRITE |
                        CAM_MEM_FLAG_HW_READ  |
                        CAM_MEM_FLAG_SW_WRITE |
                        CAM_MEM_FLAG_SW_READ,
                        0);
    }
}

GrallocDrmMemManager::~GrallocDrmMemManager()
{
    LOGD("destruct mem manager");
    if (mPreviewData) {
        destroyPreviewBuffer();
        free(mPreviewData);
        mPreviewData = NULL;
    }
    if(mHandle)
        mOps->deInit(mHandle);
}

int GrallocDrmMemManager::createGrallocDrmBuffer(struct bufferinfo_s* grallocbuf)
{
    int ret =0,i = 0;
    int numBufs;
    int frame_size;
    cam_mem_info_t** tmpalloc = NULL;
    struct bufferinfo_s* tmp_buf = NULL;

    if (!grallocbuf) {
        LOGE("gralloc_alloc malloc buffer failed");
        return -1;
    }

    numBufs = grallocbuf->mNumBffers;
    frame_size = grallocbuf->mPerBuffersize;
    grallocbuf->mBufferSizes = numBufs*PAGE_ALIGN(frame_size);
    switch(grallocbuf->mBufType)
    {
        case PREVIEWBUFFER:
            tmpalloc = mPreviewData ;
            if((tmp_buf  = (struct bufferinfo_s*)malloc(numBufs*sizeof(struct bufferinfo_s))) != NULL) {
                mPreviewBufferInfo = tmp_buf;
            } else {
                LOGE("gralloc_alloc malloc buffer failed");
            return -1;
            }
        break;
        default:
            LOGE("do not support this buffer type");
            return -1;
    }

    for(i = 0;i < numBufs; i++) {
#ifndef RK_GRALLOC_4
        *tmpalloc = mOps->alloc(mHandle,grallocbuf->mPerBuffersize);
#else
        *tmpalloc = mOps->alloc(mHandle,grallocbuf->mPerBuffersize,
                            grallocbuf->width, grallocbuf->height);
#endif
        if (*tmpalloc) {
            LOGD("alloc success");
        } else {
            LOGE("gralloc mOps->alloc failed");
            ret = -1;
            break;
        }
        grallocbuf->mPhyBaseAddr = (unsigned long)((*tmpalloc)->phy_addr);
        grallocbuf->mVirBaseAddr = (unsigned long)((*tmpalloc)->vir_addr);
        grallocbuf->mPerBuffersize = PAGE_ALIGN(frame_size);
        grallocbuf->mShareFd     = (unsigned int)((*tmpalloc)->fd);
        grallocbuf->mStatus      = 0;
        LOGD("grallocbuf->mVirBaseAddr=0x%lx, grallocbuf->mShareFd=0x%lx",
            grallocbuf->mVirBaseAddr, grallocbuf->mShareFd);
        *tmp_buf = *grallocbuf;
        tmp_buf++;
        tmpalloc++;
    }
    if(ret < 0) {
        LOGE(" failed !");
        while(--i >= 0) {
            --tmpalloc;
            --tmp_buf;
            mOps->free(mHandle,*tmpalloc);
        }
        switch(grallocbuf->mBufType) {
            case PREVIEWBUFFER:
            if(mPreviewBufferInfo) {
                free(mPreviewBufferInfo);
                mPreviewBufferInfo = NULL;
            }
            break;
            default:
            break;
        }
    }
    return ret;
}

void GrallocDrmMemManager::destroyGrallocDrmBuffer(buffer_type_enum buftype)
{
    cam_mem_info_t** tmpalloc = NULL;
    struct bufferinfo_s* tmp_buf = NULL;

    switch(buftype)
    {
        case PREVIEWBUFFER:
            tmpalloc = mPreviewData;
            tmp_buf = mPreviewBufferInfo;
        break;
        default:
            LOGE("buffer type is wrong !");
        break;
    }


    for(unsigned int i = 0;(tmp_buf && (i < tmp_buf->mNumBffers));i++) {
        if(*tmpalloc && (*tmpalloc)->vir_addr) {
            LOGD("free graphic buffer");
            mOps->free(mHandle,*tmpalloc);
        }
        tmpalloc++;
    }

    switch(buftype)
    {
        case PREVIEWBUFFER:
            free(mPreviewData);
            mPreviewData = NULL;
            free(mPreviewBufferInfo);
            mPreviewBufferInfo = NULL;
            LOGD("free mPreviewData");
        break;
        default:
            LOGE("buffer type is wrong !");
        break;
    }
}

int GrallocDrmMemManager::createPreviewBuffer(struct bufferinfo_s* previewbuf)
{
    int ret;
    Mutex::Autolock lock(mLock);

    if(previewbuf->mBufType != PREVIEWBUFFER)
        LOGE("the type is not PREVIEWBUFFER");

    if(!mPreviewData) {
        mPreviewData = (cam_mem_info_t**)malloc(sizeof(cam_mem_info_t*) * previewbuf->mNumBffers);
        if(!mPreviewData) {
            LOGE("malloc mPreviewData failed!");
            ret = -1;
            return ret;
        }
    } else if ((*mPreviewData)->vir_addr) {
        LOGD("FREE the preview buffer alloced before firstly");
        destroyPreviewBuffer();
    }

    memset(mPreviewData,0,sizeof(cam_mem_info_t*)* previewbuf->mNumBffers);

    ret = createGrallocDrmBuffer(previewbuf);
    if (ret == 0) {
        LOGD("Preview buffer information(phy:0x%lx vir:0x%lx size:0x%zx)",
            mPreviewBufferInfo->mPhyBaseAddr,
            mPreviewBufferInfo->mVirBaseAddr,
            mPreviewBufferInfo->mBufferSizes);
    } else {
        LOGE("Preview buffer alloc failed");
        if (mPreviewData){
            free(mPreviewData);
            mPreviewData = NULL;
        }
    }

    return ret;
}
int GrallocDrmMemManager::destroyPreviewBuffer()
{
    Mutex::Autolock lock(mLock);
    destroyGrallocDrmBuffer(PREVIEWBUFFER);

    return 0;
}

int GrallocDrmMemManager::flushCacheMem(buffer_type_enum buftype)
{
    Mutex::Autolock lock(mLock);
    cam_mem_info_t** tmpalloc = NULL;
    struct bufferinfo_s* tmp_buf = NULL;

    switch(buftype)
    {
        case PREVIEWBUFFER:
            tmpalloc = mPreviewData;
            tmp_buf = mPreviewBufferInfo;
        break;
        default:
            LOGE("buffer type is wrong !");
        break;
    }

    for(unsigned int i = 0;(tmp_buf && (i < tmp_buf->mNumBffers));i++) {
        if(*tmpalloc && (*tmpalloc)->vir_addr) {
#ifndef RK_GRALLOC_4
            int ret = mOps->flush_cache(mHandle, *tmpalloc);
#else
            int ret = mOps->flush_cache(mHandle, *tmpalloc, (*tmpalloc)->width, (*tmpalloc)->height);
#endif
            if(ret != 0)
                LOGD("flush cache failed !");
        }
        tmpalloc++;
    }

    return 0;
}

}/* namespace android */
