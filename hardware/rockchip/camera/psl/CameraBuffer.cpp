/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#define LOG_TAG "CameraBuffer"
#include "LogHelper.h"
#include <sys/mman.h>
#include "PlatformData.h"
#include "CameraBuffer.h"
#include "CameraStream.h"
#include "Camera3GFXFormat.h"
#include "CameraMetadataHelper.h"
#include <unistd.h>
#include <sync/sync.h>

#include <sys/types.h>
#include <dirent.h>
#include <algorithm>

namespace android {
namespace camera2 {
extern int32_t gDumpInterval;
extern int32_t gDumpCount;

////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
////////////////////////////////////////////////////////////////////

/**
 * CameraBuffer
 *
 * Default constructor
 * This constructor is used when we pre-allocate the CameraBuffer object
 * The initialization will be done as a second stage wit the method
 * init(), where we initialize the wrapper with the gralloc buffer provided by
 * the framework
 */
CameraBuffer::CameraBuffer() :  mWidth(0),
                                mHeight(0),
                                mSize(0),
                                mFormat(0),
                                mV4L2Fmt(0),
                                mStride(0),
                                mUsage(0),
                                mInit(false),
                                mLocked(false),
                                mRegistered(false),
                                mType(BUF_TYPE_HANDLE),
                                mGbmBufferManager(nullptr),
                                mHandle(nullptr),
                                mHandlePtr(nullptr),
                                mOwner(nullptr),
                                mDataPtr(nullptr),
                                mRequestID(0),
                                mpSyncFence(nullptr),
                                captureDoned(false),
                                mCameraId(0),
                                mDmaBufFd(-1)
{
    LOGI("%s default constructor for buf %p", __FUNCTION__, this);
    CLEAR(mUserBuffer);
    CLEAR(mTimestamp);
    mUserBuffer.release_fence = -1;
    mUserBuffer.acquire_fence = -1;
}

/**
 * CameraBuffer
 *
 * Constructor for buffers allocated using MemoryUtils::allocateHeapBuffer
 *
 * \param w [IN] width
 * \param h [IN] height
 * \param s [IN] stride
 * \param v4l2fmt [IN] V4l2 format
 * \param usrPtr [IN] Data pointer
 * \param cameraId [IN] id of camera being used
 * \param dataSizeOverride [IN] buffer size input. Default is 0 and frameSize()
                                is used in that case.
 */
CameraBuffer::CameraBuffer(int w,
                           int h,
                           int s,
                           int v4l2fmt,
                           void* usrPtr,
                           int cameraId,
                           int dataSizeOverride):
        mWidth(w),
        mHeight(h),
        mSize(0),
        mFormat(0),
        mV4L2Fmt(v4l2fmt),
        mStride(s),
        mUsage(0),
        mInit(false),
        mLocked(true),
        mRegistered(false),
        mType(BUF_TYPE_MALLOC),
        mGbmBufferManager(nullptr),
        mHandle(nullptr),
        mHandlePtr(nullptr),
        mOwner(nullptr),
        mDataPtr(nullptr),
        mRequestID(0),
        mpSyncFence(nullptr),
        captureDoned(false),
        mCameraId(cameraId),
        mDmaBufFd(-1)
{
    LOGI("%s create malloc camera buffer %p", __FUNCTION__, this);
    if (usrPtr != nullptr) {
        mDataPtr = usrPtr;
        mInit = true;
        mSize = dataSizeOverride ? dataSizeOverride : frameSize(mV4L2Fmt, mStride, mHeight);
        mFormat = v4L2Fmt2GFXFmt(v4l2fmt);
    } else {
        LOGE("Tried to initialize a buffer with nullptr ptr!!");
    }
    CLEAR(mUserBuffer);
    CLEAR(mTimestamp);
    mUserBuffer.release_fence = -1;
    mUserBuffer.acquire_fence = -1;
}

/**
 * CameraBuffer
 *
 * Constructor for buffers allocated using mmap
 *
 * \param w [IN] width
 * \param h [IN] height
 * \param s [IN] stride
 * \param fd [IN] File descriptor to map
 * \param dmaBufFd [IN] File descriptor for dmabuf
 * \param length [IN] amount of data to map
 * \param v4l2fmt [IN] Pixel format in V4L2 enum
 * \param offset [IN] offset from the begining of the file (mmap param)
 * \param prot [IN] memory protection (mmap param)
 * \param flags [IN] flags (mmap param)
 *
 * Success of the mmap can be queried by checking the size of the resulting
 * buffer
 */
CameraBuffer::CameraBuffer(int w, int h, int s, int fd, int dmaBufFd, int length,
                           int v4l2fmt, int offset, int prot, int flags):
        mWidth(w),
        mHeight(h),
        mSize(length),
        mFormat(0),
        mV4L2Fmt(v4l2fmt),
        mStride(s),
        mUsage(0),
        mInit(false),
        mLocked(false),
        mRegistered(false),
        mType(BUF_TYPE_MMAP),
        mGbmBufferManager(nullptr),
        mHandle(nullptr),
        mHandlePtr(nullptr),
        mOwner(nullptr),
        mDataPtr(nullptr),
        mRequestID(0),
        mpSyncFence(nullptr),
        captureDoned(false),
        mCameraId(-1),
        mDmaBufFd(dmaBufFd)
{
    LOGI("%s create mmap camera buffer %p", __FUNCTION__, this);
    mLocked = true;
    mInit = true;
    CLEAR(mUserBuffer);
    CLEAR(mTimestamp);
    CLEAR(mHandle);
    mUserBuffer.release_fence = -1;
    mUserBuffer.acquire_fence = -1;

    mDataPtr = mmap(nullptr, length, prot, flags, fd, offset);
    if (CC_UNLIKELY(mDataPtr == MAP_FAILED)) {
        LOGE("Failed to MMAP the buffer %s", strerror(errno));
        mDataPtr = nullptr;
        return;
    }
    LOGI("mmaped address for %p length %d", mDataPtr, mSize);
}

/**
 * init
 *
 * Constructor to wrap a camera3_stream_buffer
 *
 * \param aBuffer [IN] camera3_stream_buffer buffer
 */
status_t CameraBuffer::init(const camera3_stream_buffer *aBuffer, int cameraId)
{
    mType = BUF_TYPE_HANDLE;
    mGbmBufferManager = arc::CameraBufferManager::GetInstance();
    mHandle = *aBuffer->buffer;
    mHandlePtr = aBuffer->buffer;
    mWidth = aBuffer->stream->width;
    mHeight = aBuffer->stream->height;
    mFormat = aBuffer->stream->format;
    mV4L2Fmt = mGbmBufferManager->GetV4L2PixelFormat(mHandle);
    // Use actual width from platform native handle for stride
    mStride = mGbmBufferManager->GetPlaneStride(*aBuffer->buffer, 0);
    mSize = 0;
    mLocked = false;
    mOwner = static_cast<CameraStream*>(aBuffer->stream->priv);
    mUsage = mOwner->usage()|RK_GRALLOC_USAGE_SPECIFY_STRIDE;
    mInit = true;
    mDataPtr = nullptr;
    mUserBuffer = *aBuffer;
    captureDoned = false;

    char fenceName[32] = {};
    snprintf(fenceName, sizeof(fenceName),
        "%dx%d_%s_%d", mWidth, mHeight, v4l2Fmt2Str(mV4L2Fmt), cameraId);
    mpSyncFence = std::make_shared<SyncFence>(1, fenceName);
    CheckError(!mpSyncFence.get(), UNKNOWN_ERROR, "@%s, No memory for new SyncFence",
                   __FUNCTION__);

    // the fence fd passing to framework must make a dup because framework
    // will make a dup to this fd and close this fd as soon, we still need to
    // access the fd to signal the fence in this case. so must do a dup.
    mUserBuffer.release_fence = mpSyncFence->dup();
    /* mUserBuffer.release_fence = -1; */

    mCameraId = cameraId;
    LOGI("@%s, mHandle:%p, mHandlePtr:%p, mFormat:%d, mWidth:%d, mHeight:%d, mStride:%d, mSize:%d, V4l2Fmt:%s, reqId:%d",
        __FUNCTION__, mHandle, mHandlePtr, mFormat, mWidth, mHeight, mStride, mSize, v4l2Fmt2Str(mV4L2Fmt), mRequestID);

    if (mHandle == nullptr) {
        LOGE("@%s: invalid buffer handle", __FUNCTION__);
        mUserBuffer.status = CAMERA3_BUFFER_STATUS_ERROR;
        return BAD_VALUE;
    }

    int ret = registerBuffer();
    LOGI("@%s,after register mHandle:%p, mHandlePtr:%p",__FUNCTION__, mHandle, mHandlePtr);

    if (ret) {
        mUserBuffer.status = CAMERA3_BUFFER_STATUS_ERROR;
        return UNKNOWN_ERROR;
    }

    /* TODO: add some consistency checks here and return an error */
    return NO_ERROR;
}
void CameraBuffer::reConfig(int w, int h){
    mWidth = w;
    mHeight =h;
    mStride = w;
}

status_t CameraBuffer::init(const camera3_stream_t* stream,
                            buffer_handle_t handle)
{
    mType = BUF_TYPE_HANDLE;
    mGbmBufferManager = arc::CameraBufferManager::GetInstance();
    mHandle = handle;
    mWidth = stream->width;
    mHeight = stream->height;
    mFormat = stream->format;
    mV4L2Fmt = mGbmBufferManager->GetV4L2PixelFormat(mHandle);
    // Use actual width from platform native handle for stride
    mStride = mGbmBufferManager->GetPlaneStride(handle, 0);
    mSize = 0;
    mLocked = false;
    mOwner = nullptr;
    mUsage = stream->usage|RK_GRALLOC_USAGE_SPECIFY_STRIDE;
    mInit = true;
    mDataPtr = nullptr;
    CLEAR(mUserBuffer);
    mUserBuffer.acquire_fence = -1;
    mUserBuffer.release_fence = -1;

    // hal internal buffer, just lock it and unlock in destruct function
    // mSize filled here
    lock();
    LOGI("@%s, mHandle:%p, mFormat:%d, mWidth:%d, mHeight:%d, mStride:%d, mSize:%d, V4l2Fmt:%s",
        __FUNCTION__, mHandle, mFormat, mWidth, mHeight, mStride, mSize, v4l2Fmt2Str(mV4L2Fmt));

    return NO_ERROR;
}

status_t CameraBuffer::deinit()
{
    return deregisterBuffer();
}

CameraBuffer::~CameraBuffer()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (mInit) {
        switch(mType) {
        case BUF_TYPE_MALLOC:
            free(mDataPtr);
            mDataPtr = nullptr;
            break;
        case BUF_TYPE_MMAP:
            if (mDataPtr != nullptr)
                munmap(mDataPtr, mSize);
            mDataPtr = nullptr;
            mSize = 0;
            close(mDmaBufFd);
            break;
        case BUF_TYPE_HANDLE:
            // Allocated by the HAL
            if (!(mUserBuffer.stream)) {
                LOGI("release internal buffer");
                if(isLocked())
                    unlock();
                mGbmBufferManager->Free(mHandle);
            }
            break;
        default:
            break;
        }
    }
    LOGI("%s destroying buf %p", __FUNCTION__, this);
}

status_t CameraBuffer::waitOnAcquireFence()
{
    const int WAIT_TIME_OUT_MS = 300;
    const int BUFFER_READY = -1;

    if (mUserBuffer.acquire_fence != BUFFER_READY) {
        PERFORMANCE_ATRACE_NAME("waitOnAcquireFence");
        LOGI("%s: Fence in HAL is %d", __FUNCTION__, mUserBuffer.acquire_fence);
        int ret = sync_wait(mUserBuffer.acquire_fence, WAIT_TIME_OUT_MS);
        if (ret) {
            mUserBuffer.release_fence = mUserBuffer.acquire_fence;
            mUserBuffer.acquire_fence = -1;
            mUserBuffer.status = CAMERA3_BUFFER_STATUS_ERROR;
            LOGE("Buffer sync_wait %d fail!", mUserBuffer.release_fence);
            return TIMED_OUT;
        } else {
            close(mUserBuffer.acquire_fence);
        }
        mUserBuffer.acquire_fence = BUFFER_READY;
    }

    return NO_ERROR;
}

/**
 * getFence
 *
 * return the fecne to request result
 */
status_t CameraBuffer::getFence(camera3_stream_buffer* buf)
{
    if (!buf)
        return BAD_VALUE;

    buf->acquire_fence = mUserBuffer.acquire_fence;
    buf->release_fence = mUserBuffer.release_fence;

    return NO_ERROR;
}

status_t CameraBuffer::registerBuffer()
{
#ifdef RK_GRALLOC_4
    buffer_handle_t outbuffer;
    int ret = mGbmBufferManager->Register(mHandle, &outbuffer);
    if (ret == 0) {
        mHandle = outbuffer;
        mHandlePtr = &outbuffer;
    }
#else
    int ret = mGbmBufferManager->Register(mHandle);
#endif
    if (ret < 0) {
        LOGE("@%s: call Register fail, mHandle:%p, ret:%d", __FUNCTION__, mHandle, ret);
        return UNKNOWN_ERROR;
    }

    mRegistered = true;
    return NO_ERROR;
}

status_t CameraBuffer::deregisterBuffer()
{
    if (mRegistered) {
        int ret = mGbmBufferManager->Deregister(mHandle);
        if (ret) {
            LOGE("@%s: call Deregister fail, mHandle:%p, ret:%d", __FUNCTION__, mHandle, ret);
            return UNKNOWN_ERROR;
        }
        mRegistered = false;
    }

    return NO_ERROR;
}

/**
 * lock
 *
 * lock the gralloc buffer with specified flags
 *
 * \param aBuffer [IN] int flags
 */
status_t CameraBuffer::lock(int flags)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    mDataPtr = nullptr;
    mSize = 0;
    int ret = 0;
    uint32_t planeNum = mGbmBufferManager->GetNumPlanes(mHandle);
    LOGI("@%s, planeNum:%d, mHandle:%p, mFormat:%d", __FUNCTION__, planeNum, mHandle, mFormat);

    if (planeNum == 1) {
        void* data = nullptr;
        ret = (mFormat == HAL_PIXEL_FORMAT_BLOB)
                ? mGbmBufferManager->Lock(mHandle, flags, 0, 0, mStride > mWidth ? mWidth : mStride, 1, &data)
                : mGbmBufferManager->Lock(mHandle, flags, 0, 0, mWidth, mHeight, &data);
        if (ret) {
            LOGE("@%s: call Lock fail, mHandle:%p", __FUNCTION__, mHandle);
            return UNKNOWN_ERROR;
        }
        mDataPtr = data;
    } else if (planeNum > 1) {
        struct android_ycbcr ycbrData;
        ret = mGbmBufferManager->LockYCbCr(mHandle, flags, 0, 0, mWidth, mHeight, &ycbrData);
        if (ret) {
            LOGE("@%s: call LockYCbCr fail, mHandle:%p", __FUNCTION__, mHandle);
            return UNKNOWN_ERROR;
        }
        mDataPtr = ycbrData.y;
    } else {
        LOGE("ERROR @%s: planeNum is 0", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    if (ret) {
        LOGE("ERROR @%s: Failed to lock buffer! %d", __FUNCTION__, ret);
        return UNKNOWN_ERROR;
    }

    for (int i = 0; i < planeNum; i++) {
        mSize += mGbmBufferManager->GetPlaneSize(mHandle, i);
    }
    LOGI("@%s, mDataPtr:%p, mSize:%d", __FUNCTION__, mDataPtr, mSize);
    if (!mSize) {
        LOGE("ERROR @%s: Failed to GetPlaneSize, it's 0", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    mLocked = true;

    return NO_ERROR;
}

status_t CameraBuffer::lock()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status;
    int lockMode;

    if (!mInit) {
        LOGE("@%s: Error: Cannot lock now this buffer, not initialized", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (mType != BUF_TYPE_HANDLE) {
         mLocked = true;
         return NO_ERROR;
    }

    if (mLocked) {
        LOGE("@%s: Error: Cannot lock buffer from stream(%d), already locked",
             __FUNCTION__,mOwner->seqNo());
        return INVALID_OPERATION;
    }

    lockMode = mUsage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK |
        GRALLOC_USAGE_HW_CAMERA_MASK);
    if (!lockMode) {
        LOGW("@%s:trying to lock a buffer with no flags", __FUNCTION__);
        return INVALID_OPERATION;
    }

    status = lock(lockMode);
    if (status != NO_ERROR) {
        mUserBuffer.status = CAMERA3_BUFFER_STATUS_ERROR;
    }

    return status;
}

status_t CameraBuffer::unlock()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    if (mLocked && mType != BUF_TYPE_HANDLE) {
         mLocked = false;
         return NO_ERROR;
    }

    if (mLocked) {
        LOGI("@%s, mHandle:%p, mFormat:%d", __FUNCTION__, mHandle, mFormat);
        int ret = mGbmBufferManager->Unlock(mHandle);
        if (ret) {
            LOGE("@%s: call Unlock fail, mHandle:%p, ret:%d", __FUNCTION__, mHandle, ret);
            return UNKNOWN_ERROR;
        }

        mLocked = false;
        return NO_ERROR;
    }
    LOGW("@%s:trying to unlock a buffer that is not locked", __FUNCTION__);
    return INVALID_OPERATION;
}

void CameraBuffer::dump()
{
    if (mInit) {
        LOGI("Buffer dump: handle %p: locked :%d: dataPtr:%p",
            (void*)&mHandle, mLocked, mDataPtr);
    } else {
        LOGI("Buffer dump: Buffer not initialized");
    }
}

void CameraBuffer::dumpImage(const int type, const char *name)
{
    if (CC_UNLIKELY(LogHelper::isDumpTypeEnable(type))) {
        dumpImage(name);
    }
}

void CameraBuffer::dumpImage(const char *name)
{
    if (mLocked) {
        dumpImage(mDataPtr, mSize, mWidth, mHeight, name);
    } else {
        status_t status = lock();
        CheckError((status != OK), VOID_VALUE, "failed to lock dump buffer");
        dumpImage(mDataPtr, mSize, mWidth, mHeight, name);
        unlock();
    }
}

void CameraBuffer::dumpImage(const void *data, const int size, int width, int height,
                                 const char *name)
{
    static unsigned int count = 0;
    count++;

    if (gDumpInterval > 1) {
        if (count % gDumpInterval != 0) {
            return;
        }
    }

    // one example for the file name: /tmp/dump_1920x1080_00000346_PREVIEW_0
    std::string fileName;
    std::string dumpPrefix = "dump_";
    char dumpSuffix[100] = {};
    snprintf(dumpSuffix, sizeof(dumpSuffix),
        "%dx%d_%08u_%s_%d", width, height, count, name, mRequestID);
    fileName = std::string(gDumpPath) + dumpPrefix + std::string(dumpSuffix);

    LOGI("%s filename is %s", __FUNCTION__, fileName.data());

    FILE *fp = fopen (fileName.data(), "w+");
    if (fp == nullptr) {
        LOGE("open file failed");
        return;
    }

    if ((fwrite(data, size, 1, fp)) != 1)
        LOGW("Error or short count writing %d bytes to %s", size, fileName.data());
    fclose (fp);

    // always leave the latest gDumpCount "dump_xxx" files
    if (gDumpCount <= 0) {
        return;
    }
    // read the "dump_xxx" files name into vector
    std::vector<std::string> fileNames;
    DIR* dir = opendir(gDumpPath);
    CheckError(dir == nullptr, VOID_VALUE, "@%s, call opendir() fail", __FUNCTION__);
    struct dirent* dp = nullptr;
    while ((dp = readdir(dir)) != nullptr) {
        char* ret = strstr(dp->d_name, dumpPrefix.c_str());
        if (ret) {
            fileNames.push_back(dp->d_name);
        }
    }
    closedir(dir);

    // remove the old files when the file number is > gDumpCount
    if (fileNames.size() > gDumpCount) {
        std::sort(fileNames.begin(), fileNames.end());
        for (size_t i = 0; i < (fileNames.size() - gDumpCount); ++i) {
            std::string fullName = gDumpPath + fileNames[i];
            remove(fullName.c_str());
        }
    }
}

status_t CameraBuffer::captureDone(std::shared_ptr<CameraBuffer> buffer, bool signalFence) {
    if(!mOwner) {
        // stream is NULL in the case: rawPath + CAMERA_DUMP_RAW
        // because the buf is the input mmap buffer for we set the
        // RawUnit buffer type as kPostProcBufTypePre in postpipeline
        LOGW("@%s : The buffer %p belong to none stream", __FUNCTION__, this);
        return OK;
    }

    if(signalFence) {
        switch (mOwner->getStreamType()) {
        case STREAM_PREVIEW:
            LOGD("@%s : Preview buffer signaled for req %d", __FUNCTION__, mRequestID);
            dumpImage(CAMERA_DUMP_PREVIEW, "PREVIEW");
            break;
        case STREAM_CAPTURE:
            LOGD("@%s : Capture buffer signaled for req %d", __FUNCTION__, mRequestID);
            dumpImage(CAMERA_DUMP_JPEG, ".jpg");
            break;
        case STREAM_VIDEO:
            LOGD("@%s : Video buffer signaled for req %d", __FUNCTION__, mRequestID);
            dumpImage(CAMERA_DUMP_VIDEO, "VIDEO");
            break;
        case STREAM_ZSL:
            LOGD("@%s : Zsl buffer signaled for req %d", __FUNCTION__, mRequestID);
            dumpImage(CAMERA_DUMP_ZSL, "ZSL");
            break;
        default :
            LOGW("%s:%d: Not support the stream type, is a bug, fix me", __FUNCTION__, __LINE__);
            break;
        }
        unlock();
        deinit();
        fenceInc();
        fenceInfo();
    }

    if(captureDoned) {
        return OK;
    }

    mOwner->captureDone(buffer);
    captureDoned = true;

    return OK;
}

/**
 * Utility methods to allocate CameraBuffers from HEAP or Gfx memory
 */
namespace MemoryUtils {

/**
 * Allocates the memory needed to store the image described by the parameters
 * passed during construction
 */
std::shared_ptr<CameraBuffer>
allocateHeapBuffer(int w,
                   int h,
                   int s,
                   int v4l2Fmt,
                   int cameraId,
                   int dataSizeOverride)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    void *dataPtr;

    int dataSize = dataSizeOverride ? dataSizeOverride : frameSize(v4l2Fmt, s, h);
    LOGI("@%s, dataSize:%d", __FUNCTION__, dataSize);

    int ret = posix_memalign(&dataPtr, sysconf(_SC_PAGESIZE), dataSize);
    if (dataPtr == nullptr || ret != 0) {
        LOGE("Could not allocate heap camera buffer of size %d", dataSize);
        return nullptr;
    }

    return std::shared_ptr<CameraBuffer>(new CameraBuffer(w, h, s, v4l2Fmt, dataPtr, cameraId, dataSizeOverride));
}

/**
 * Allocates internal GBM buffer
 */
std::shared_ptr<CameraBuffer>
allocateHandleBuffer(int w,
                     int h,
                     int gfxFmt,
                     int usage)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    PERFORMANCE_ATRACE_NAME_SNPRINTF("Allocate One Buf %dx%d", w, h);
    arc::CameraBufferManager* bufManager = arc::CameraBufferManager::GetInstance();
    buffer_handle_t handle;
    uint32_t stride = 0;

    LOGI("%s, [wxh] = [%dx%d], format 0x%x, usage 0x%x",
          __FUNCTION__, w, h, gfxFmt, usage);
    int ret = bufManager->Allocate(w, h, gfxFmt, usage, arc::GRALLOC, &handle, &stride);
    LOGI("Allocate handle:%p", &handle);
    if (ret != 0) {
        LOGE("Allocate handle failed! %d", ret);
        return nullptr;
    }

    std::shared_ptr<CameraBuffer> buffer(new CameraBuffer());
    camera3_stream_t stream;
    CLEAR(stream);
    stream.width = w;
    stream.height = h;
    stream.format = gfxFmt;
    stream.usage = usage;
    ret = buffer->init(&stream, handle);
    if (ret != NO_ERROR) {
        // buffer handle will free in CameraBuffer destructure function
        return nullptr;
    }

    return buffer;
}

#define MAX_CAMERA_INSTANCES 2
// preAllocate buffer pool
SharedItemPool<CameraBuffer> *PreAllocateBufferPool[MAX_CAMERA_INSTANCES];

status_t
creatHandlerBufferPool(int cameraId,
                       int w,
                       int h,
                       int gfxFmt,
                       int usage,
                       int nums)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    PERFORMANCE_ATRACE_NAME_SNPRINTF("PreBufPool %dx%d %d", w, h, nums);
    arc::CameraBufferManager* bufManager = arc::CameraBufferManager::GetInstance();
    buffer_handle_t handle;
    status_t ret;
    uint32_t stride = 0;

    CheckError(cameraId >= MAX_CAMERA_INSTANCES, UNKNOWN_ERROR,
               "@%s,  invaild cameraId: %d", __FUNCTION__, cameraId);

    char BufPoolName[64];
    snprintf(BufPoolName, 64, "%s-%d", "PreAllocateBufferPool", cameraId);
    PreAllocateBufferPool[cameraId] = new SharedItemPool<CameraBuffer>(BufPoolName);
    PreAllocateBufferPool[cameraId]->init(nums);

    LOGI("%s, [wxh] = [%dx%d], format 0x%x, usage 0x%x, nums %d",
          __FUNCTION__, w, h, gfxFmt, usage, nums);
    for (int i = 0; i < nums; ++i) {
        ret = bufManager->Allocate(w, h, gfxFmt, usage, arc::GRALLOC, &handle, &stride);
        LOGI("Allocate handle:%p", handle);
        if (ret != 0) {
            LOGE("Allocate handle failed! %d", ret);
            return ret;
        }
        std::shared_ptr<CameraBuffer> buffer = nullptr;
        PreAllocateBufferPool[cameraId]->acquireItem(buffer);

        camera3_stream_t stream;
        CLEAR(stream);
        stream.width = w;
        stream.height = h;
        stream.format = gfxFmt;
        stream.usage = usage;
        ret = buffer->init(&stream, handle);
    }

    return ret;
}

void destroyHandleBufferPool(int cameraId) {
    LOGD("@%s : cameraId:%d", __FUNCTION__, cameraId);
    CheckError(cameraId >= MAX_CAMERA_INSTANCES, ,
               "@%s,  invaild cameraId: %d", __FUNCTION__, cameraId);
    if (PreAllocateBufferPool[cameraId])
        delete PreAllocateBufferPool[cameraId];
    PreAllocateBufferPool[cameraId] = NULL;
}

std::shared_ptr<CameraBuffer> acquireOneBuffer(int cameraId, int w, int h, bool allocate) {
    status_t ret;
    std::shared_ptr<CameraBuffer> buffer = nullptr;

    PreAllocateBufferPool[cameraId]->acquireItem(buffer);
    if(allocate && buffer.get() == nullptr) {
        buffer = MemoryUtils::allocateHandleBuffer(w, h, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                                                   GRALLOC_USAGE_SW_READ_OFTEN |
                                                   GRALLOC_USAGE_HW_CAMERA_WRITE|
                                                   RK_GRALLOC_USAGE_SPECIFY_STRIDE|
                                                   /* TODO: same as the temp solution in RKISP1CameraHw.cpp configStreams func
                                                    * add GRALLOC_USAGE_HW_VIDEO_ENCODER is a temp patch for gpu bug:
                                                    * gpu cant alloc a nv12 buffer when format is
                                                    * HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED. Need gpu provide a patch */
                                                   GRALLOC_USAGE_HW_VIDEO_ENCODER);

        CheckError((buffer.get() == nullptr), nullptr, "@%s : No memeory, failed allocate buffer",
                   __FUNCTION__);
        LOGW("@%s : shortage of internal buffer, allocate a new one ", __FUNCTION__);
        return buffer;
    }

    // reuse the Camerabuffer, just change the stream width and height
    if (buffer.get() != nullptr)
        buffer->reConfig(w, h);

    return buffer;
}

std::shared_ptr<CameraBuffer> acquireOneBufferWithNoCache(int cameraId, int w, int h, bool allocate) {
    status_t ret;
    std::shared_ptr<CameraBuffer> buffer = nullptr;

    //PreAllocateBufferPool[cameraId]->acquireItem(buffer);
    if(allocate && buffer.get() == nullptr) {
        buffer = MemoryUtils::allocateHandleBuffer(w, h, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                                                   GRALLOC_USAGE_HW_CAMERA_WRITE|GRALLOC_USAGE_HW_CAMERA_READ|
                                                   RK_GRALLOC_USAGE_SPECIFY_STRIDE|
                                                   /* TODO: same as the temp solution in RKISP1CameraHw.cpp configStreams func
                                                    * add GRALLOC_USAGE_HW_VIDEO_ENCODER is a temp patch for gpu bug:
                                                    * gpu cant alloc a nv12 buffer when format is
                                                    * HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED. Need gpu provide a patch */
                                                   GRALLOC_USAGE_HW_VIDEO_ENCODER);

        CheckError((buffer.get() == nullptr), nullptr, "@%s : No memeory, failed allocate buffer",
                   __FUNCTION__);
        LOGW("@%s : shortage of internal buffer, allocate a new one ", __FUNCTION__);
        return buffer;
    }

    // reuse the Camerabuffer, just change the stream width and height
    if (buffer.get() != nullptr)
        buffer->reConfig(w, h);

    return buffer;
}

} // namespace MemoryUtils

} /* namespace camera2 */
} /* namespace android */
