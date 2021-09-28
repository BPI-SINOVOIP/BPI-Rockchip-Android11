/*
 * Copyright (C) 2016-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef _CAMERA3_HAL_COMMON_BUFFER_H_
#define _CAMERA3_HAL_COMMON_BUFFER_H_

#include <stdio.h>
#include <string.h>
#include <utils/Errors.h>

/**
 * \class CommonBuffer
 *
 * Buffer abstraction for all platform independent camera HAL buffer types
 *
 * CommonBuffer provides common interface for all HAL buffer types
 * Inlcudes:
 *  1. HALBuffer (PSL buffer, going to inherit from this class and extend)
 *  2. The buffer type that is needed for ImgEncoder and ImgProcessor
 *  3. Statistic buffer (taken as height 1, width/stride = data size)
 *  4. ...Possible others
 *
 * By using this, buffer can be shared directly between HAL modules without
 * conversion, such as: PSL buffer can be directly passed to ImgEncoder or
 * ImgProcessor.
 *
 * The CommonBuffer can be just a shell or a memory self-owned buffer
 * Free memory is needed when the data memory of this buffer is allocated
 * by itself (allocMemory is called)
 */

NAMESPACE_DECLARATION {
enum BufferMemoryType {
    BMT_HEAP, /* normal heap memory buffer */
    BMT_GFX,  /* graphic memory buffer */
    BMT_MMAP, /* memory is mapped from kernel */
};

struct BufferProps {
    BufferProps():
        width(0),
        height(0),
        stride(0),
        format(0),
        size(0),
        fd(-1),
        offset(0),
        type(BMT_HEAP){}
    int width;
    int height;
    int stride;
    int format; /* V4l2 Format */
    int size;   /* override the size if user provide valid value(>0) */
    int fd;     /* for MMAP buffer only, device fd */
    int offset; /* for MMAP buffer only, offset to device memory start */
    BufferMemoryType type;
};

class CommonBuffer {
public:
    CommonBuffer();
    explicit CommonBuffer(const BufferProps &props, void* data = nullptr);
    virtual ~CommonBuffer();

    virtual status_t init(const BufferProps &props, void* data = nullptr);
    virtual status_t allocMemory();
    virtual status_t freeMemory();

    virtual void* data() const { return mDataPtr; }
    virtual void* gfxHandle() const { return mHandle; }
    virtual int width() const {return mWidth; }
    virtual int height() const {return mHeight; }
    virtual int stride() const {return mStride; }
    virtual int type() const { return mType; }
    virtual unsigned int size() const {return mSize; }
    virtual int v4l2Fmt() const {return mV4L2Fmt; }

protected:
    int             mWidth;
    int             mHeight;
    int             mStride;
    BufferMemoryType mType;
    unsigned int    mSize;    /*!< size in bytes */
    void*           mDataPtr; /*!< the vaddr */
    void*           mHandle;  /*!< the graphic handle, valid for a GFX type buffer */
    int             mFd;      /*!< the device fd, valid for a MMAP type buffer */
    int             mOffset;  /*!< offset to device memory start, valid for MMAP type only */
    int             mV4L2Fmt; /*!< V4L2 fourcc format code */
    bool            mInit;    /*!< Boolean to check the integrity */
    bool            mIsOwner; /*!< Boolean to check if self-owned buffer memory */
};
} NAMESPACE_DECLARATION_END

#endif // _CAMERA3_HAL_COMMON_BUFFER_H_
