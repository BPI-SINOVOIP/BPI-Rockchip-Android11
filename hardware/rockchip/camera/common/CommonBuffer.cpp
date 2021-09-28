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

#define LOG_TAG "CommonBuffer"
#include <sys/mman.h>
#include <sstream>
#include <iomanip> // std::setw, setfill
#include "LogHelper.h"
#include "CommonBuffer.h"
#include "UtilityMacros.h"
#include "Camera3V4l2Format.h"

NAMESPACE_DECLARATION {
/**
 * CommonBuffer
 *
 * Default constructor
 * This constructor is used when we pre-allocate the CommonBuffer object
 * The initialization will be done as a second stage with the method
 * init()
 */
CommonBuffer::CommonBuffer() :
    mWidth(0),
    mHeight(0),
    mStride(0),
    mType(BMT_HEAP),
    mSize(0),
    mDataPtr(nullptr),
    mHandle(nullptr),
    mFd(-1),
    mOffset(0),
    mV4L2Fmt(0),
    mInit(false),
    mIsOwner(false)
{
    LOGI("%s default constructor for buf %p", __FUNCTION__, this);
}

/**
 * constructor for using user ptr
 * buffer is initialized in contructor, no need to call init()
 *
 * \param fmt [IN]: v4l2 format
 * \param data [IN]: user data pointer
 * \param w/h/s [IN]: width/height/stride
 */
CommonBuffer::CommonBuffer(const BufferProps &props, void *data) :
    mSize(0),
    mDataPtr(nullptr),
    mHandle(nullptr),
    mFd(-1),
    mOffset(0),
    mInit(false),
    mIsOwner(false)
{
    LOGI("%s constructor with usrptr %p", __FUNCTION__, data);
    init(props, data);
}

/**
 * initialization
 * Used for object constructed with default constructor
 *
 * \param fmt [IN]: v4l2 format
 * \param data [IN]: user data pointer
 * \param w/h/s [IN]: width/height/stride
 */
status_t CommonBuffer::init(const BufferProps &props, void* data)
{
    mWidth   = props.width;
    mHeight  = props.height;
    mStride  = props.stride;
    mV4L2Fmt = props.format;
    mType    = props.type;
    if (mType == BMT_HEAP) {
        mDataPtr = data;
        LOGI("%s with %dx%d s:%d fmt:%x heap data: %p", __FUNCTION__,
                mWidth, mHeight, mStride, mV4L2Fmt, mDataPtr);
    } else if (mType == BMT_GFX) {
        mHandle = data;
        LOGI("%s with %dx%d s:%d fmt:%x gfx handle: %p", __FUNCTION__,
                mWidth, mHeight, mStride, mV4L2Fmt, mHandle);
    } else if (mType == BMT_MMAP) {
        mDataPtr = data;
        mFd      = props.fd;
        mOffset  = props.offset;
        LOGI("%s with %dx%d s:%d fmt:%x fd:%d offset:%d addr: %p", __FUNCTION__,
                mWidth, mHeight, mStride, mV4L2Fmt, mFd, mOffset, mDataPtr);
    }

    if (props.size > 0) {
        mSize = props.size;
        LOGI("%s size override:%d", __FUNCTION__, mSize);
    } else {
        mSize = frameSize(mV4L2Fmt, mStride, mHeight);
    }
    mInit = true;
    return NO_ERROR;
}

CommonBuffer::~CommonBuffer()
{
    LOGI("%s destroying buf %p", __FUNCTION__, this);
    if (mIsOwner)
        freeMemory();
}

/**
 * free memory for self-ownded buffer
 */
status_t CommonBuffer::freeMemory()
{
    switch(mType) {
    case BMT_HEAP:
        if (mDataPtr) {
            LOGI("%s release memory %p", __FUNCTION__, mDataPtr);
            free(mDataPtr);
            mDataPtr = nullptr;
        }
        break;
    case BMT_MMAP:
        if (mDataPtr != nullptr) {
            LOGI("%s munmap memory %p", __FUNCTION__, mDataPtr);
            munmap(mDataPtr, mSize);
            mDataPtr = nullptr;
        }
        break;
    case BMT_GFX:
    default:
        LOGE("Not supported yet for type:%d", mType);
        break;
    }

    return NO_ERROR;
}

/**
 * allocate memory for an initialized buffer
 *
 * The buffer should be initialized with an empty user pointer
 * The allocated memory is released in CommonBuffer destructor
 */
status_t CommonBuffer::allocMemory()
{
    if (!mInit) {
        LOGE("alloc error: buffer is not initialized");
        return NO_INIT;
    }

    switch(mType) {
    case BMT_HEAP:
        if (mDataPtr && mIsOwner) {
            LOGI("%s reallocate with size:%u", __FUNCTION__, mSize);
            free(mDataPtr);
        } else if (mDataPtr && !mIsOwner) {
            LOGW("trying to allocate memory for an userptr buffer");
            return UNKNOWN_ERROR;
        }
        mDataPtr = malloc(mSize);
        if (!mDataPtr) {
            LOGE("fail to malloc for size:%d", mSize);
            return NO_MEMORY;
        }
        LOGI("%s size:%u addr:%p", __FUNCTION__, mSize, mDataPtr);
        break;

    case BMT_MMAP:
        if (mDataPtr) {
            LOGD("already mapped add:%p owner:%d", mDataPtr, mIsOwner);
            return NO_ERROR;
        }

        mDataPtr = mmap(nullptr, mSize, PROT_READ | PROT_WRITE, MAP_SHARED, mFd, mOffset);
        if (mDataPtr == MAP_FAILED) {
            LOGE("Failed to MAP buffer, fd:%d error: %s", mFd, strerror(errno));
            mDataPtr = nullptr;
            return UNKNOWN_ERROR;
        }
        LOGI("%s mmap size:%u addr:%p", __FUNCTION__, mSize, mDataPtr);
        break;

    case BMT_GFX:
    default:
        LOGW("Alloc memory not supported yet for %d", mType);
        return UNKNOWN_ERROR;
    }

    mIsOwner = true;
    return NO_ERROR;
}

} NAMESPACE_DECLARATION_END

