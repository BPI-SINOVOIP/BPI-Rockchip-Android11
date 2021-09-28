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

#ifndef ANDROID_HARDWARE_CAMERA_MEM_MANAGER
#define ANDROID_HARDWARE_CAMERA_MEM_MANAGER

#include <dlfcn.h>
#include "utils/LightRefBase.h"
#ifndef RK_GRALLOC_4
#include "ExternalCameraGralloc.h"
#else
#include "ExternalCameraGralloc4.h"
#endif

namespace android {

enum buffer_type_enum{
    PREVIEWBUFFER,
    RAWBUFFER,
    JPEGBUFFER,
    VIDEOENCBUFFER,
};

struct bufferinfo_s{
    unsigned int mNumBffers; //invaild if this value is 0
    size_t mPerBuffersize;
    size_t mBufferSizes;
    unsigned long mPhyBaseAddr;
    unsigned long mVirBaseAddr;
    unsigned long mShareFd;
    buffer_type_enum mBufType;
    uint32_t width;
    uint32_t height;
    short mStatus; // 0:idle, 1:activel
};

typedef enum buffer_addr_e {
    buffer_addr_phy,
    buffer_addr_vir,
    buffer_sharre_fd
}buffer_addr_t;

class MemManagerBase : public virtual VirtualLightRefBase {
public :
    MemManagerBase();
    virtual ~MemManagerBase();
    virtual int createPreviewBuffer(struct bufferinfo_s* previewbuf) = 0;
    virtual int destroyPreviewBuffer() = 0;
    virtual int flushCacheMem(buffer_type_enum buftype) = 0;
    void setBufferStatus(enum buffer_type_enum buf_type,
                                unsigned int buf_idx, int status);
    unsigned long getBufferAddr(enum buffer_type_enum buf_type,
            unsigned int buf_idx, buffer_addr_t addr_type);
    int getIdleBufferIndex(enum buffer_type_enum buf_type);
    int dump();
protected:
    struct bufferinfo_s* mPreviewBufferInfo;
    mutable Mutex mLock;
};

class GrallocDrmMemManager:public MemManagerBase {
    public :
        GrallocDrmMemManager(bool iommuEnabled);
        ~GrallocDrmMemManager();
        virtual int createPreviewBuffer(struct bufferinfo_s* previewbuf);
        virtual int destroyPreviewBuffer();
        virtual int flushCacheMem(buffer_type_enum buftype);
    private:
        int createGrallocDrmBuffer(struct bufferinfo_s* grallocbuf);
        void destroyGrallocDrmBuffer(buffer_type_enum buftype);
        cam_mem_info_t** mPreviewData;
        cam_mem_handle_t* mHandle;
        cam_mem_ops_t* mOps;
};

}/* namespace android */

#endif // ANDROID_HARDWARE_CAMERA_MEM_MANAGER
