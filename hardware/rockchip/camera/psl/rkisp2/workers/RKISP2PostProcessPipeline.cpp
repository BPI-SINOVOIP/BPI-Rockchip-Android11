/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#define LOG_TAG "RKISP2PostProcessPipeline"

#include "RKISP2PostProcessPipeline.h"

#include <sys/syscall.h>

#include "PerformanceTraces.h"
#include "CameraMetadataHelper.h"
#include "CameraStream.h"
#include "ImageScalerCore.h"
#include "RgaCropScale.h"
#include "LogHelper.h"
#include "FormatUtils.h"
#include "RKISP2FecUnit.h"
//#include "TuningServer.h"

#if defined(ANDROID_VERSION_ABOVE_12_X)
#include <hardware/hardware_rockchip.h>
#endif

#define ALIGN(value, x)	 ((value + (x-1)) & (~(x-1)))

// disable mirror handling by default
/* #define MIRROR_HANDLING_FOR_FRONT_CAMERA */

namespace android {
namespace camera2 {
namespace rkisp2 {

status_t
IPostProcessSource::attachListener(RKISP2IPostProcessListener* listener) {
    LOGD("@%s: %p", __FUNCTION__, listener);

    std::lock_guard<std::mutex> l(mListenersLock);

    mListeners.push_back(listener);

    return OK;
}

status_t
IPostProcessSource::notifyListeners(const std::shared_ptr<PostProcBuffer>& buf,
                                    const std::shared_ptr<RKISP2ProcUnitSettings>& settings,
                                    int err) {
    LOGD("@%s", __FUNCTION__);

    status_t status = OK;
    std::lock_guard<std::mutex> l(mListenersLock);
    for (auto &listener : mListeners) {
        status |= listener->notifyNewFrame(buf, settings, err);
    }

    return status;
}

status_t
PostProcBufferPools::createBufferPools(RKISP2PostProcessPipeline* pl, const FrameInfo& outfmt, int numBufs) {
    LOGD("@%s buffer num %d", __FUNCTION__, numBufs);

    mBufferPoolSize = numBufs;
    mPipeline = pl;

    mPostProcItemsPool.init(mBufferPoolSize, PostProcBuffer::reset);
    for (unsigned int i = 0; i < mBufferPoolSize; i++) {
        std::shared_ptr<PostProcBuffer> postprocbuf = nullptr;
        mPostProcItemsPool.acquireItem(postprocbuf);
        if (postprocbuf.get() == nullptr) {
            LOGE("Failed to get a post process buffer!");
            return UNKNOWN_ERROR;
        }
        postprocbuf->index = i;
        postprocbuf->fmt = outfmt;
    }

    return OK;
}

status_t
PostProcBufferPools::acquireItem(std::shared_ptr<PostProcBuffer> &procbuf) {

    LOGD("@%s : mPostProcItemsPool preallocate buffer %zu", __FUNCTION__, mPostProcItemsPool.availableItems());
    mPostProcItemsPool.acquireItem(procbuf);
    CheckError((procbuf.get() == nullptr), UNKNOWN_ERROR, "@%s, failed to acquire PostProcBuffer",
               __FUNCTION__);

    procbuf->cambuf = MemoryUtils::acquireOneBuffer(mPipeline->getCameraId(), procbuf->fmt.width, procbuf->fmt.height);

    CheckError((procbuf->cambuf.get() == nullptr), UNKNOWN_ERROR, "@%s, failed to acquire cambuf",
               __FUNCTION__);

    return OK;
}

std::shared_ptr<PostProcBuffer>
PostProcBufferPools::acquireItem() {
    std::shared_ptr<PostProcBuffer> procbuf;

    mPostProcItemsPool.acquireItem(procbuf);
    CheckError((procbuf.get() == nullptr), nullptr, "@%s, acquire PostProcBuffer failed",
               __FUNCTION__);

    procbuf->cambuf = MemoryUtils::acquireOneBuffer(mPipeline->getCameraId(), procbuf->fmt.width, procbuf->fmt.height);

    CheckError((procbuf->cambuf.get() == nullptr), nullptr, "@%s, acquire cambuf failed",
               __FUNCTION__);

    return procbuf;
}

RKISP2PostProcessUnit::RKISP2PostProcessUnit(const char* name, int type, uint32_t buftype, RKISP2PostProcessPipeline* pl) :
    mInternalBufPool(new PostProcBufferPools()),
    mName(name),
    mBufType(buftype),
    mEnable(true),
    mSyncProcess(false),
    mThreadRunning(false),
    mProcThread(new MessageThread(this, name)),
    mProcessUnitType(type),
    mPipeline(pl),
    mCurPostProcBufIn(nullptr),
    mCurProcSettings(nullptr),
    mCurPostProcBufOut(nullptr) {
    LOGD("%s: @%s ", mName, __FUNCTION__);
}

RKISP2PostProcessUnit::~RKISP2PostProcessUnit() {
    LOGD("%s: @%s ", mName, __FUNCTION__);

    if (mProcThread != nullptr) {
        mProcThread.reset();
        mProcThread = nullptr;
    }
#ifdef RK_EPTZ
    if (mEptzThread){
        ALOGI("rk-debug: delete mEptzThread ************");
        mEptzThread->runnable = false;
        mEptzThread.clear();
        mEptzThread = NULL;
    }
#endif
    mPipeline = NULL;
    mInBufferPool.clear();
    mOutBufferPool.clear();
    mCurPostProcBufIn.reset();
    mCurProcSettings.reset();
    mCurPostProcBufOut.reset();
}

status_t
RKISP2PostProcessUnit::prepare(const FrameInfo& outfmt, int bufNum) {
    LOGD("%s: @%s ", mName, __FUNCTION__);
    status_t status = OK;

    if (mBufType == kPostProcBufTypeInt) {
         status = mInternalBufPool->createBufferPools(mPipeline, outfmt, bufNum);
         if (status) {
            LOGE("%s: init buffer pool failed %d", __FUNCTION__, status);
            return status;
         }
    }

    return OK;
}

status_t
RKISP2PostProcessUnit::start() {
    LOGD("%s: @%s ", mName, __FUNCTION__);

    std::lock_guard<std::mutex> l(mApiLock);

    if (mThreadRunning) {
        LOGW("%s: post thread already running!", __FUNCTION__);
        return OK;
    }
    mThreadRunning = true;

    return mProcThread->run();
}

status_t
RKISP2PostProcessUnit::stop() {
    LOGD("%s: @%s ", mName, __FUNCTION__);

    std::unique_lock<std::mutex> l(mApiLock, std::defer_lock);
    l.lock();

    if (!mThreadRunning) {
        LOGW("%s: post thread already stopped!", __FUNCTION__);
        return OK;
    }
    mThreadRunning = false;
    mCondition.notify_all();
    l.unlock();

    return mProcThread->requestExitAndWait();
}

status_t
RKISP2PostProcessUnit::flush() {
    LOGD("%s: @%s ", mName, __FUNCTION__);

    std::lock_guard<std::mutex> l(mApiLock);

    mInBufferPool.clear();
    for (auto iter : mOutBufferPool)
        notifyListeners(iter, std::shared_ptr<RKISP2ProcUnitSettings>(), -1);
    mOutBufferPool.clear();
    mCurPostProcBufIn.reset();
    mCurProcSettings.reset();
    mCurPostProcBufOut.reset();

    return OK;
}

status_t
RKISP2PostProcessUnit::drain() {
    PERFORMANCE_ATRACE_CALL();
    LOGD("%s: @%s ", mName, __FUNCTION__);
    // the processing frame can't be stopped so just
    // wait current frame process done
    long timeout = 500 * 1000000;    //500ms timeout
    nsecs_t startTime = systemTime();
    nsecs_t interval = 0;

    while (!mInBufferPool.empty() ||
           mCurPostProcBufIn.get() != NULL ||
           mCurPostProcBufOut.get() != NULL) {
        usleep(5000); // wait 5ms
        interval = systemTime() - startTime;
        if(interval >= timeout) {
            LOGE("@%s :%s drain timeout, time spend:%" PRId64 "us > 500ms", __FUNCTION__, mName, interval / 1000);
            return UNKNOWN_ERROR;
        }
    }
    LOGI("@%s : It tooks %" PRId64 "us to drain %s", __FUNCTION__, interval / 1000, mName);

    return OK;
}

status_t
RKISP2PostProcessUnit::addOutputBuffer(std::shared_ptr<PostProcBuffer> buf) {
    LOGD("%s: @%s ", mName, __FUNCTION__);

    std::lock_guard<std::mutex> l(mApiLock);
    if (mBufType != kPostProcBufTypeExt) {
        LOGE("%s: %s can't accept external buffer!"
             "buffer type is %d", __FUNCTION__, mName, mBufType);
        return UNKNOWN_ERROR;
    }
    mOutBufferPool.push_back(buf);

    return OK;
}

status_t
RKISP2PostProcessUnit::setEnable(bool enable) {
    LOGD("%s: @%s ", mName, __FUNCTION__);

    std::lock_guard<std::mutex> l(mApiLock);

    mEnable = enable;

    return OK;
}

status_t
RKISP2PostProcessUnit::setProcessSync(bool sync) {
    LOGD("%s: @%s ", mName, __FUNCTION__);
    std::lock_guard<std::mutex> l(mApiLock);

    mSyncProcess = sync;

    return OK;
}

/* called by ThreadLoop */
void
RKISP2PostProcessUnit::prepareProcess() {
    // get a frame to be processed from input buffer queue
    std::unique_lock<std::mutex> l(mApiLock);
    if (mThreadRunning && mInBufferPool.empty())
        mCondition.wait(l);
    if (!mThreadRunning)
        return;
    LOGD("%s: @%s, mInBufferPool size:%d, mOutBufferPool size:%d",
        mName, __FUNCTION__, mInBufferPool.size(), mOutBufferPool.size());
    mCurPostProcBufIn = mInBufferPool[0].first;
    mCurProcSettings = mInBufferPool[0].second;
    mInBufferPool.erase(mInBufferPool.begin());
    // get an output buffer from output buffer queue or internal
    // buffer queue
    if (mCurPostProcBufOut.get() != nullptr) {
        LOGE("%s: %s busy !", __FUNCTION__, mName);
        return;
    }
    switch (mBufType) {
    case kPostProcBufTypeInt :
        mInternalBufPool->acquireItem(mCurPostProcBufOut);
        break;
    case kPostProcBufTypeExt :
        if (!mOutBufferPool.empty()) {
            mCurPostProcBufOut = mOutBufferPool[0];
            uint32_t inBufReqId = mCurProcSettings->request->getId();
            uint32_t outBufReqId = mCurPostProcBufOut->request->getId();
            // check the input buffer is needed for this unit
            if(inBufReqId == outBufReqId) {
                mOutBufferPool.erase(mOutBufferPool.begin());
            } else if(inBufReqId > outBufReqId) {
                LOGE("@%s: %s, new request %d is comming, reqeust %d won't be processed",
                     __FUNCTION__, mName, inBufReqId, outBufReqId);
                mCurPostProcBufOut.reset();
                return;
            } else {
                LOGW("@%s: %s, drop the input buffer for reqId mismatch, in(%d)/out(%d)",
                     __FUNCTION__, mName, inBufReqId, outBufReqId);
                mCurPostProcBufOut.reset();
                return;
            }
            if(mCurPostProcBufOut->cambuf->waitOnAcquireFence() != NO_ERROR) {
                // if wait on fence failed, just relay the buffer to xxframework
                LOGW("Wait on fence for buffer %p timed out", mCurPostProcBufOut->cambuf.get());
                relayToNextProcUnit(NO_ERROR);
            }
        }
        break;
    case kPostProcBufTypePre :
        mCurPostProcBufOut = mCurPostProcBufIn;
        break;
    default:
        LOGE("%s: error buffer type %d.", __FUNCTION__, mBufType);
    }

    if (mCurPostProcBufOut.get() == nullptr) {
        // relay to next proc unit
        LOGW("%s: no output buf for unit %s", __FUNCTION__, mName);
        relayToNextProcUnit(STATUS_FORWRAD_TO_NEXT_UNIT);
    }
}

/* called by ThreadLoop */
status_t
RKISP2PostProcessUnit::relayToNextProcUnit(int err) {
    LOGD("%s: @%s ", mName, __FUNCTION__);
    status_t status = OK;

    if (err == STATUS_NEED_NEXT_INPUT_FRAME) {
        mCurPostProcBufIn.reset();
        mCurProcSettings.reset();
        return err;
    }

    if (err == STATUS_FORWRAD_TO_NEXT_UNIT &&
        mBufType != kPostProcBufTypeExt)
        status = notifyListeners(mCurPostProcBufIn,
                                 mCurProcSettings, err);
    else if (mCurPostProcBufOut.get() != nullptr)
        status = notifyListeners(mCurPostProcBufOut,
                                 mCurProcSettings, err);
    else
        LOGW("%s: %s drop the input frame !", __FUNCTION__, mName);
    mCurPostProcBufOut.reset();
    mCurPostProcBufIn.reset();
    mCurProcSettings.reset();

    return status;
}

status_t
RKISP2PostProcessUnit::doProcess() {
    LOGD("%s: @%s ", mName, __FUNCTION__);

    status_t status = OK;

    std::unique_lock<std::mutex> l(mApiLock, std::defer_lock);
    l.lock();
    do {
        l.unlock();
        prepareProcess();
        if (mCurPostProcBufIn.get() && mCurPostProcBufOut.get()) {
            status = processFrame(mCurPostProcBufIn,
                                  mCurPostProcBufOut,
                                  mCurProcSettings);
#ifdef RK_EPTZ
            processEptzFrame(mCurPostProcBufOut);
#endif
            status = relayToNextProcUnit(status);
        }
        l.lock();
    } while(mThreadRunning && status == STATUS_NEED_NEXT_INPUT_FRAME);
    l.unlock();

    return OK;
}

void
RKISP2PostProcessUnit::messageThreadLoop(void) {

    std::unique_lock<std::mutex> l(mApiLock, std::defer_lock);
    l.lock();
#ifdef RK_FEC
    if (mFecUnit) {
        mFecUnit->distortionInit(2560,1440);
    }
#endif
    while (mThreadRunning) {
        l.unlock();
        doProcess();
        l.lock();
    }
    l.unlock();
}

status_t RKISP2PostProcessUnit::notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                                         const std::shared_ptr<RKISP2ProcUnitSettings>& settings,
                                         int err) {
    LOGD("%s: @%s, mInBufferPool size:%d", mName, __FUNCTION__, mInBufferPool.size() + 1);

    std::unique_lock<std::mutex> l(mApiLock, std::defer_lock);

    l.lock();
    /* TODO: handle err first ? */
    if (mThreadRunning == false) {
        LOGW("%s: proc unit %s has been stopped!", __FUNCTION__, mName);
        goto unlock_ret;
    }
    if (!mEnable) {
        l.unlock();
        return notifyListeners(buf, settings, err);
    }
    if (mSyncProcess) {
        l.unlock();
        return doProcess();
    } else {
        mInBufferPool.push_back(std::make_pair(buf, settings));
        mCondition.notify_all();
        goto unlock_ret;
    }

unlock_ret:
    l.unlock();
    return OK;
}

/* Consider the performance, this should not hold the ApiLock */
status_t
RKISP2PostProcessUnit::processFrame(const std::shared_ptr<PostProcBuffer>& in,
                              const std::shared_ptr<PostProcBuffer>& out,
                              const std::shared_ptr<RKISP2ProcUnitSettings>& settings) {
    PERFORMANCE_ATRACE_CALL();

    LOGD("%s: @%s, reqId: %d",
         mName, __FUNCTION__, settings->request->getId());
    status_t status = OK;
    bool mirror = false;
#ifdef MIRROR_HANDLING_FOR_FRONT_CAMERA
    if(PlatformData::facing(mPipeline->getCameraId()) == CAMERA_FACING_FRONT) {
        bool isPreview = false;
        camera3_stream_t* stream = mPipeline->getStreamByType(mProcessUnitType);

        if(stream) {
            isPreview = CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_COMPOSER);
            isPreview |= CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_TEXTURE);
            isPreview |= CHECK_FLAG(stream->usage, GRALLOC_USAGE_HW_RENDER);
        }
        mirror = isPreview;
        LOGD("@%s : mirror(%d) handling for front camera", __FUNCTION__, mirror);
    }
#endif

    if (mProcessUnitType == kPostProcessTypeDummy) {
        LOGD("@%s %d: dummy unit , do nothing", __FUNCTION__, __LINE__);
        return OK;
    }
    /*
     * use RGA to do memcpy.
     * TODO: using arm to do memcpy has cache issue, the buffer from framework
     * may enable cache but not fluch cache when buffer is unlocked.
     */
    if (mProcessUnitType == kPostProcessTypeCopy ||
        mProcessUnitType == kPostProcessTypeUVC ||
        mProcessUnitType == kPostProcessTypeScaleAndRotation) {
        int cropw, croph, croptop, cropleft;
        float inratio = (float)in->cambuf->width() / in->cambuf->height();
        float outratio = (float)out->cambuf->width() / out->cambuf->height();

        if (inratio < outratio) {
            // crop height
            cropw = in->cambuf->width();
            croph = in->cambuf->width() / outratio;
        } else {
            // crop width
            cropw = in->cambuf->height() * outratio;
            croph = in->cambuf->height();
        }
        // should align to 2
        cropw &= ~0x3;
        croph &= ~0x3;
        cropleft = (in->cambuf->width() - cropw) / 2;
        croptop = (in->cambuf->height() - croph) / 2;
        cropleft &= ~0x1;
        croptop &= ~0x1;

        LOGD("%s: crop region(%d,%d,%d,%d) from (%d,%d) to %dx%d, infmt %d,%d, outfmt %d,%d",
             __FUNCTION__, cropw, croph, cropleft, croptop,
             in->cambuf->width(), in->cambuf->height(),
             out->cambuf->width(), out->cambuf->height(),
             in->cambuf->format(),
             in->cambuf->v4l2Fmt(),
             out->cambuf->format(),
             out->cambuf->v4l2Fmt());

        RgaCropScale::Params rgain, rgaout;

        rgain.fd = in->cambuf->dmaBufFd();
        if (in->cambuf->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
            in->cambuf->v4l2Fmt() == V4L2_PIX_FMT_NV12)
            rgain.fmt = HAL_PIXEL_FORMAT_YCrCb_NV12;
        else
            rgain.fmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        rgain.vir_addr = (char*)in->cambuf->data();
        rgain.mirror = mirror;
        rgain.width = cropw;
        rgain.height = croph;
        rgain.offset_x = cropleft;
        rgain.offset_y = croptop;
        rgain.width_stride = in->cambuf->width();
        rgain.height_stride = in->cambuf->height();

        rgaout.fd = out->cambuf->dmaBufFd();
        // HAL_PIXEL_FORMAT_YCbCr_420_888 buffer layout is the same as NV12
        // in gralloc module implementation
        if (out->cambuf->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
            out->cambuf->format() == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
            out->cambuf->v4l2Fmt() == V4L2_PIX_FMT_NV12)
            rgaout.fmt = HAL_PIXEL_FORMAT_YCrCb_NV12;
        else
            rgaout.fmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        rgaout.vir_addr = (char*)out->cambuf->data();
        rgaout.mirror = false;
        rgaout.width = out->cambuf->width();
        rgaout.height = out->cambuf->height();
        rgaout.offset_x = 0;
        rgaout.offset_y = 0;
        rgaout.width_stride = out->cambuf->width();
        rgaout.height_stride = out->cambuf->height();

        if (RgaCropScale::CropScaleNV12Or21(&rgain, &rgaout)) {
            LOGE("%s:  crop&scale by RGA failed...", __FUNCTION__);
            PERFORMANCE_ATRACE_NAME("SWCropScale");
            ImageScalerCore::cropComposeUpscaleNV12_bl(
                             in->cambuf->data(), in->cambuf->height(), in->cambuf->width(),
                             cropleft, croptop, cropw, croph,
                             out->cambuf->data(), out->cambuf->height(), out->cambuf->width(),
                             0, 0, out->cambuf->width(), out->cambuf->height());
        }
    }

    return status;
}

#ifdef RK_EPTZ
status_t
RKISP2PostProcessUnit::processEptzFrame(const std::shared_ptr<PostProcBuffer>& mCurPostProcBufOut) {
    LOGD("%s, @%s ", mName, __FUNCTION__);
    if(!strcmp("JpegEnc",mName) || mBufType == 0){
        ALOGI("rk-debug %s, name %s mBufType %d return", __FUNCTION__, mName, mBufType);
        return OK;
    }
    RgaCropScale::Params rgain, rgaout;
    rgain.fd = mCurPostProcBufOut->cambuf->dmaBufFd();
    if (mCurPostProcBufOut->cambuf->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
        mCurPostProcBufOut->cambuf->v4l2Fmt() == V4L2_PIX_FMT_NV12)
        rgain.fmt = HAL_PIXEL_FORMAT_YCrCb_NV12;
    else
        rgain.fmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    rgain.vir_addr = (char*)mCurPostProcBufOut->cambuf->data();
    rgain.mirror = false;
    rgain.width = mCurPostProcBufOut->cambuf->width();
    rgain.height = mCurPostProcBufOut->cambuf->height();
    rgain.offset_x = 0;
    rgain.offset_y = 0;
    rgain.width_stride = mCurPostProcBufOut->cambuf->width();
    rgain.height_stride = mCurPostProcBufOut->cambuf->height();

    rgaout.fd = mCurPostProcBufOut->cambuf->dmaBufFd();
    if (mCurPostProcBufOut->cambuf->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
        mCurPostProcBufOut->cambuf->v4l2Fmt() == V4L2_PIX_FMT_NV12)
        rgaout.fmt = HAL_PIXEL_FORMAT_YCrCb_NV12;
    else
        rgaout.fmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    rgaout.vir_addr = (char*)mCurPostProcBufOut->cambuf->data();
    rgaout.mirror = false;
    rgaout.width = mCurPostProcBufOut->cambuf->width();
    rgaout.height = mCurPostProcBufOut->cambuf->height();
    rgaout.offset_x = 0;
    rgaout.offset_y = 0;
    rgaout.width_stride = mCurPostProcBufOut->cambuf->width();
    rgaout.height_stride = mCurPostProcBufOut->cambuf->height();

    //#ifdef RK_OCCLUSION
    char occlusion_property_value[PROPERTY_VALUE_MAX] = {0};
    property_get("vendor.camera.occlusion.enable", occlusion_property_value, "0");
    //#endif
    char eptz_property_value[PROPERTY_VALUE_MAX] = {0};
    property_get("vendor.camera.eptz.mode", eptz_property_value, "0");
    if(nullptr != mEptzThread){
        if(mEptzThread->runnable && mEptzThread->isInit){
            mEptzThread->converData(rgain);
            mEptzThread->calculateRect(&rgain);
        }
    }else{
        if (atoi(eptz_property_value) != 0 || atoi(occlusion_property_value) != 0){
            ALOGI("rk-debug mEptzThread create , name %s", mName);
            mEptzThread = new EptzThread();
            mEptzThread->setPreviewCfg(mCurPostProcBufOut->cambuf->width(), mCurPostProcBufOut->cambuf->height());
            mEptzThread->setMode(atoi(eptz_property_value));
            mEptzThread->setOcclusionMode(atoi(occlusion_property_value));
            mEptzThread->run("CamEPTZThread", PRIORITY_DISPLAY);
        }
    }
    if (mEptzThread && mEptzThread->getMode() != atoi(eptz_property_value)){
        if(atoi(eptz_property_value) == 0){
            ALOGI("rk-debug: delete mEptzThread ************");
            mEptzThread->runnable = false;
            mEptzThread.clear();
            mEptzThread = NULL;
        }else{
            mEptzThread->setMode(atoi(eptz_property_value));
        }
    }
    RgaCropScale::CropScaleNV12Or21(&rgain, &rgaout);
    return OK;
}
#endif

bool RKISP2PostProcessUnit::checkFmt(CameraBuffer* in, CameraBuffer* out) {
    return true;
}

RKISP2PostProcessPipeline::RKISP2PostProcessPipeline(RKISP2IPostProcessListener* listener,
                                         int camid) :
    mPostProcFrameListener(listener),
    mCameraId(camid),
    mThreadRunning(false),
    mMessageQueue("PPThread", static_cast<int>(MESSAGE_ID_MAX)),
    mMessageThread(nullptr),
    mMayNeedSyncStreamsOutput(false),
    mOutputBuffersHandler(new OutputBuffersHandler(this)) {

    mMessageThread = std::unique_ptr<MessageThread>(new MessageThread(this, "PPThread"));
    mMessageThread->run();
}

RKISP2PostProcessPipeline::~RKISP2PostProcessPipeline() {
    requestExitAndWait();
    if (mMessageThread != nullptr) {
        mMessageThread.reset();
        mMessageThread = nullptr;
    }
    mPostProcUnits.clear();
    mStreamToProcUnitMap.clear();
}

status_t
RKISP2PostProcessPipeline::addOutputBuffer(const std::vector<std::shared_ptr<PostProcBuffer>>& out) {
    status_t status = OK;
    for (auto iter : out) {
        if (iter->cambuf.get() == nullptr)
            continue;
        camera3_stream_t* stream = iter->cambuf->getOwner()->getStream();
        if (stream == nullptr)
            continue;
        status |= mStreamToProcUnitMap[stream]->addOutputBuffer(iter);
    }

    return status;
}

bool RKISP2PostProcessPipeline::IsRawStream(camera3_stream_t* stream) {
    if (stream == NULL) {
        LOGE("@%s : stream is NULL", __FUNCTION__);
        return false;
    }
    if(stream->format == HAL_PIXEL_FORMAT_RAW16 ||
       stream->format == HAL_PIXEL_FORMAT_RAW10 ||
       stream->format == HAL_PIXEL_FORMAT_RAW12 ||
       stream->format == HAL_PIXEL_FORMAT_RAW_OPAQUE)
        return true;
    return false;

}

status_t
RKISP2PostProcessPipeline::prepare(const FrameInfo& in,
                             const std::vector<camera3_stream_t*>& streams,
                             bool& needpostprocess,
                             int   pipelineDepth) {
    Message msg;
    msg.id = MESSAGE_ID_PREPARE;
    msg.prepareMsg.in = in;
    msg.prepareMsg.streams = streams;
    msg.prepareMsg.needpostprocess = false;
    /* TODO  should get needpostprocess from link result*/
    needpostprocess = true;
    msg.prepareMsg.pipelineDepth = pipelineDepth;
    status_t status = mMessageQueue.send(&msg);
    return status;
}

/*
 * TODO:
 * notice that the total process time of each other branch pipeline
 * should be less than the main pipeline(which output the
 * camera3_stream_buffer), or will cause no buffer issue in
 * OutputFrameWorker::prepareRun
 *
 */
status_t
RKISP2PostProcessPipeline::prepare_internal(const FrameInfo& in,
                             const std::vector<camera3_stream_t*>& streams,
                             bool& needpostprocess,
                             int   pipelineDepth) {
    LOGD("@%s enter", __FUNCTION__);
    status_t status = OK;
    int common_process_type = 0;
    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    // analyze which process unit do we need
    mStreamToTypeMap.clear();
    std::vector<std::map<camera3_stream_t*, int>>& streams_post_proc = mStreamToTypeMap;

    mMayNeedSyncStreamsOutput = streams.size() > 1;
    /* TODO: from metadata */
    common_process_type = 0;

    mUvc.width = in.width;
    mUvc.height = in.height;

    for (auto stream : streams) {
        int stream_process_type = 0;
        if(IsRawStream(stream)) {
            LOGD("@%s %d: add Raw unit for rawStream", __FUNCTION__, __LINE__);
            streams_post_proc.push_back(std::map<camera3_stream_t*, int> {{stream, kPostProcessTypeRaw}});
            continue;
        }

        // do nothing to app streams data by using dummy unit if
        // in fomat is raw. this may happen in the case:
        // CAMERA_DUMP_RAW + no rawPath
        if (graphconfig::utils::isRawFormat(in.format)) {
            LOGD("@%s %d: add dummpy unit for appStreams when raw input", __FUNCTION__, __LINE__);
            streams_post_proc.push_back(std::map<camera3_stream_t*, int> {{stream, kPostProcessTypeDummy}});
            continue;
        }

        if (stream->format == HAL_PIXEL_FORMAT_BLOB)
           stream_process_type |= kPostProcessTypeJpegEncoder;
        if (stream->width * stream->height != in.width * in.height)
           stream_process_type |= kPostProcessTypeScaleAndRotation;
        if (getRotationDegrees(stream))
           common_process_type |= kPostProcessTypeCropRotationScale;

        // TuningServer *pserver = TuningServer::GetInstance();
        // if (pserver && pserver->isTuningMode()){
        //     if (stream->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED){
        //         streams_post_proc.push_back(std::map<camera3_stream_t*, int> {{&mUvc, kPostProcessTypeUVC}});
        //     }
        // }
        camera_metadata_ro_entry entry = MetadataHelper::getMetadataEntry(meta,
                                    ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
        float max_digital_zoom = 1.0f;
        MetadataHelper::getValueByType(entry, 0, &max_digital_zoom);
#ifdef RK_FEC
        if (max_digital_zoom > 1.0)
           common_process_type |= kPostProcessTypeFec;
#else
        if (max_digital_zoom > 1.0)
           common_process_type |= kPostProcessTypeDigitalZoom;
#endif
#ifdef MIRROR_HANDLING_FOR_FRONT_CAMERA
        //for front camera mirror handling, front camera preview do twice mirror
        if(PlatformData::facing(mCameraId) == CAMERA_FACING_FRONT) {
            if(stream_process_type == 0)
                stream_process_type |= kPostProcessTypeCopy;
        }
#endif
        streams_post_proc.push_back(std::map<camera3_stream_t*, int> {{stream, stream_process_type}});
    }

    // add extra memcpy unit for streams if necessary
    int common_types_exclude_buffer_needed = common_process_type &
                                       ~NO_NEED_INTERNAL_BUFFER_PROCESS_TYPES;
    if (streams_post_proc.size() > 1 ||
       (streams_post_proc.size() == 1 && common_types_exclude_buffer_needed == 0)) {
       for (auto &stream_type_map : streams_post_proc) {
        int stream_process_type = stream_type_map.begin()->second;
        if (stream_process_type == 0) {
            stream_process_type |= kPostProcessTypeCopy;
            stream_type_map[stream_type_map.begin()->first] = stream_process_type;
            stream_type_map.begin()->second = stream_process_type;
        }

        LOGI("%s: stream %p process type 0x%x", __FUNCTION__,
            (stream_type_map.begin())->first, stream_process_type);
       }
    } else {
        LOGW("%s: no need buffer copy for stream!", __FUNCTION__);
    }

    LOGI("%s: common process type 0x%x", __FUNCTION__, common_process_type);
    // get the last proc unit for streams
    int stream_proc_types = 0;
    for (auto stream_type_map : streams_post_proc)
        stream_proc_types |= stream_type_map.begin()->second;

    LOGI("%s: streams process type 0x%x", __FUNCTION__, stream_proc_types);
    /* judge the steam's last process unit is the same as the common process */
    int last_level_proc_common = 0;
    if (stream_proc_types == 0) {
        // the last common proc unit is also the stream's last proc unit
        for (uint32_t i = 1; i < MAX_COMMON_PROC_UNIT_SHIFT; i++) {
            uint32_t test_type = 1 << i;
            if (common_process_type & test_type)
                last_level_proc_common = test_type;
        }
        LOGI("%s: the last common process unit is the same as stream's 0x%x.",
             __FUNCTION__, last_level_proc_common);
    }
    /* if there exist buffer needed common processes or
     * main stream(always the first stream) is buffer needed,
     * then |needpostprocess| is true
     */
    if (common_types_exclude_buffer_needed ||
        (streams_post_proc[0].begin()->second &
         ~NO_NEED_INTERNAL_BUFFER_PROCESS_TYPES))
        needpostprocess = true;
    else
        needpostprocess = false;

    // link common proc units
    std::shared_ptr<RKISP2PostProcessUnit> procunit_from;
    std::shared_ptr<RKISP2PostProcessUnit> procunit_to;
    std::shared_ptr<RKISP2PostProcessUnit> procunit_main_last;

    for (uint32_t i = 1; i < MAX_COMMON_PROC_UNIT_SHIFT; i++) {
        uint32_t test_type = 1 << i;
        bool last_proc_unit = (last_level_proc_common == test_type);
        uint32_t buf_type = last_proc_unit ?
                            RKISP2PostProcessUnit::kPostProcBufTypeExt :
                            RKISP2PostProcessUnit::kPostProcBufTypeInt;
        const char* process_unit_name = NULL;
        if (common_process_type & test_type) {
            switch (test_type) {
            case kPostProcessTypeFec :
                process_unit_name = "fecunit";
                procunit_from = std::make_shared<RKISP2PostProcessUnitFec>
                    (process_unit_name, test_type, mCameraId, buf_type, this);                
                break;
            case kPostProcessTypeDigitalZoom :
                process_unit_name = "digitalzoom";
                procunit_from = std::make_shared<RKISP2PostProcessUnitDigitalZoom>
                    (process_unit_name, test_type, mCameraId, buf_type, this);
                break;
            case kPostprocessTypeUvnr :
                process_unit_name = "uvnr";
                procunit_from = std::make_shared<RKISP2PostProcessUnit>
                    (process_unit_name, test_type, buf_type, this);
                break;
            case kPostProcessTypeCropRotationScale :
                process_unit_name = "CropRotationScale";
                procunit_from = std::make_shared<RKISP2PostProcessUnit>
                    (process_unit_name, test_type, buf_type, this);
                break;
            case kPostProcessTypeSwLsc :
                process_unit_name = "SoftwareLsc";
                procunit_from = std::make_shared<RKISP2PostProcessUnitSwLsc>
                    (process_unit_name, test_type, buf_type, this);
                break;
            case kPostProcessTypeFaceDetection :
                process_unit_name = "faceDetection";
                buf_type = RKISP2PostProcessUnit::kPostProcBufTypePre;
                procunit_from = std::make_shared<RKISP2PostProcessUnit>
                    (process_unit_name, test_type, buf_type, this);
                break;
            default:
                LOGW("%s: have no common process.", __FUNCTION__);
            }

            if (process_unit_name) {
                if (test_type == kPostProcessTypeFaceDetection) {
                    procunit_to = nullptr;
                } else {
                    procunit_to = procunit_main_last;
                    procunit_main_last = procunit_from;
                }
                LOGI("%s: add unit %s to %s, is the last proc unit %d",
                     __FUNCTION__, process_unit_name,
                     procunit_to.get() ? procunit_to.get()->mName : "first level",
                     last_proc_unit);
                if (last_proc_unit) {
                    linkPostProcUnit(procunit_from, procunit_to,
                        procunit_to.get() ? kLastLevel : kFirstLevel);
                    // link streams callback to last correspond procunit
                    procunit_from->attachListener(mOutputBuffersHandler.get());
                    /* should exist only one stream */
                    mStreamToProcUnitMap[streams[0]] = procunit_from.get();
                } else {
                    linkPostProcUnit(procunit_from, procunit_to,
                        procunit_to.get() ? kMiddleLevel : kFirstLevel);
                }
                /* TODO: should consider in and out format */
                procunit_from->prepare(in, pipelineDepth);
            }
       }
    }

    /* link the stream process units */
    for (auto proc_map : streams_post_proc) {
        std::shared_ptr<RKISP2PostProcessUnit> procunit_stream_last = procunit_main_last;
        // get the stream last process unit
        uint32_t last_level_proc_stream = 0;
        for (uint32_t i = MAX_COMMON_PROC_UNIT_SHIFT + 1;
             i < MAX_STREAM_PROC_UNIT_SHIFT; i++) {
            uint32_t test_type = 1 << i;
            uint32_t stream_proc_type = proc_map.begin()->second;
            if (stream_proc_type & test_type)
                last_level_proc_stream = test_type;
        }

        LOGI("%s: stream %p last process unit 0x%x", __FUNCTION__,
             proc_map.begin()->first, last_level_proc_stream);

        for (uint32_t i = MAX_COMMON_PROC_UNIT_SHIFT + 1;
             i < MAX_STREAM_PROC_UNIT_SHIFT; i++) {
            uint32_t test_type = 1 << i;
            bool last_proc_unit = (last_level_proc_stream == test_type);
            uint32_t buf_type = last_proc_unit ?
                                RKISP2PostProcessUnit::kPostProcBufTypeExt :
                                RKISP2PostProcessUnit::kPostProcBufTypeInt;
            const char* process_unit_name = NULL;
            uint32_t stream_proc_type = proc_map.begin()->second;
            if (stream_proc_type & test_type) {
                switch (test_type) {
                case kPostProcessTypeScaleAndRotation :
                    process_unit_name = "ScaleRotation";
                    procunit_from = std::make_shared<RKISP2PostProcessUnit>
                                    (process_unit_name, test_type, buf_type, this);
                    break;
                case kPostProcessTypeJpegEncoder :
                    process_unit_name = "JpegEnc";
                    procunit_from = std::make_shared<RKISP2PostProcessUnitJpegEnc>
                                    (process_unit_name, test_type, buf_type, this);
                    break;
                case kPostProcessTypeCopy :
                    process_unit_name = "MemCopy";
                    procunit_from = std::make_shared<RKISP2PostProcessUnit>
                                    (process_unit_name, test_type, buf_type, this);
                    break;
                case kPostProcessTypeUVC :
                    process_unit_name = "UVC";
                    procunit_from = std::make_shared<RKISP2PostProcessUnitUVC>
                                    (process_unit_name, test_type, RKISP2PostProcessUnit::kPostProcBufTypeInt, this);
                    break;
                case kPostProcessTypeRaw :
                    process_unit_name = "Raw";
                    // if need process the raw buffer in the future, the
                    // buffer type should be changed
                    procunit_from = std::make_shared<RKISP2PostProcessUnitRaw>
                                    (process_unit_name, test_type, RKISP2PostProcessUnit::kPostProcBufTypePre);
                    break;
                case kPostProcessTypeDummy :
                    process_unit_name = "Dummy";
                    procunit_from = std::make_shared<RKISP2PostProcessUnit>
                                    (process_unit_name, test_type, buf_type);
                    break;
                default:
                    LOGE("%s: unknown stream process unit type 0x%x",
                         __FUNCTION__, test_type);
                }
            }

            if (process_unit_name) {
                procunit_to = procunit_stream_last;
                procunit_stream_last = procunit_from;
                LOGI("%s: add unit %s to %s, is the last proc unit %d",
                     __FUNCTION__, process_unit_name,
                     procunit_to.get() ? procunit_to.get()->mName : "first level",
                     last_proc_unit);
                if (last_proc_unit) {
                    linkPostProcUnit(procunit_from, procunit_to,
                        procunit_to.get() ? kLastLevel : kFirstLevel);
                    // link streams callback to last correspond procunit
                    procunit_from->attachListener(mOutputBuffersHandler.get());
                    mStreamToProcUnitMap[proc_map.begin()->first] = procunit_from.get();
                } else {
                    linkPostProcUnit(procunit_from, procunit_to,
                        procunit_to.get() ? kMiddleLevel : kFirstLevel);
                }
                /* TODO: should consider in and out format */
                if (strstr(process_unit_name, "ScaleRotation")) {
                    FrameInfo outfmt = in;
                    outfmt.width = proc_map.begin()->first->width;
                    outfmt.height = proc_map.begin()->first->height;
                    procunit_from->prepare(outfmt, pipelineDepth);
                } else {
                    procunit_from->prepare(in, pipelineDepth);
                }
            }
        }
    }

    for (int i = 0; i < RKISP2PostProcessPipeline::kMaxLevel; i++) {
        for (auto iter : mPostProcUnitArray[i])
            LOGI("level %d, unit %s", i, iter->mName);
    }

    LOGD("@%s exit", __FUNCTION__);

    return status;
}

status_t
RKISP2PostProcessPipeline::start() {
    LOGD("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_START;
    status_t status = mMessageQueue.send(&msg);
    return status;
}

status_t
RKISP2PostProcessPipeline::clear() {
    LOGD("@%s", __FUNCTION__);
    status_t status = OK;

    mPostProcUnits.clear();
    mStreamToProcUnitMap.clear();
    for (int i = 0; i < RKISP2PostProcessPipeline::kMaxLevel; i++) {
        mPostProcUnitArray[i].clear();
    }

    return status;
}

status_t
RKISP2PostProcessPipeline::stop() {
    LOGD("@%s", __FUNCTION__);
    Message msg;
    msg.id = MESSAGE_ID_STOP;
    status_t status = mMessageQueue.send(&msg);
    return status;
}

void
RKISP2PostProcessPipeline::flush() {
    LOGD("@%s", __FUNCTION__);

    /* TODO  now only complete dummy flush, just wait all request done,
     * and flush here do nothing, in future vertion, add it*/
    /* flush from first level unit to last level */
    /* for (int i = 0; i < kMaxLevel; i++) */
    /*     for (auto iter : mPostProcUnitArray[i]) */
    /*        iter->flush(); */
}

status_t
RKISP2PostProcessPipeline::processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                  const std::vector<std::shared_ptr<PostProcBuffer>>& out,
                                  const std::shared_ptr<RKISP2ProcUnitSettings>& settings) {
    status_t status = OK;

    Message msg;
    msg.id = MESSAGE_ID_PROCESSFRAME;

    msg.processMsg.in = in;
    msg.processMsg.out = out;
    msg.processMsg.settings = settings;
    status = mMessageQueue.send(&msg);

    return status;
}

camera3_stream_t*
RKISP2PostProcessPipeline::getStreamByType(int stream_type) {
    for (auto proc_map : mStreamToTypeMap) {
        uint32_t stream_proc_type = proc_map.begin()->second;
        if (stream_proc_type & stream_type)
            return proc_map.begin()->first;
    }
    return NULL;
}

int
RKISP2PostProcessPipeline::getRotationDegrees(camera3_stream_t* stream) const {
    if (stream->stream_type != CAMERA3_STREAM_OUTPUT) {
        LOGI("%s, no need rotation for stream type %d", __FUNCTION__,
             stream->stream_type);
        return 0;
    }

#ifdef CHROME_BOARD
    if (stream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_90)
        return 90;
    else if (stream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_270)
        return 270;
#endif

    return 0;
}

status_t
RKISP2PostProcessPipeline::linkPostProcUnit(const std::shared_ptr<RKISP2PostProcessUnit>& from,
                                      const std::shared_ptr<RKISP2PostProcessUnit>& to,
                                      enum ProcessUnitLevel level) {
    LOGD("@%s", __FUNCTION__);

    if (from.get() == nullptr)
        return UNKNOWN_ERROR;

    if (to.get() != nullptr)
        to->attachListener(from.get());
    else if (level != kFirstLevel)
        return UNKNOWN_ERROR;

    mPostProcUnits.push_back(from);
    mPostProcUnitArray[level].push_back(from.get());

    return OK;
}

status_t
RKISP2PostProcessPipeline::enablePostProcUnit(RKISP2PostProcessUnit* procunit, bool enable) {
    LOGD("@%s", __FUNCTION__);
    status_t status = OK;

    if (procunit == nullptr)
        return OK;

    for (auto iter : mPostProcUnits) {
        if (iter.get() == procunit) {
            status = procunit->setEnable(enable);
            break;
        }
    }

    return status;
}

status_t
RKISP2PostProcessPipeline::setPostProcUnitAsync(RKISP2PostProcessUnit* procunit, bool async) {
    LOGD("@%s", __FUNCTION__);
    status_t status = OK;

    if (procunit == nullptr)
        return OK;

    for (auto iter : mPostProcUnits) {
        if (iter.get() == procunit) {
            status = procunit->setProcessSync(async);
            break;
        }
    }

    return status;
}

void
RKISP2PostProcessPipeline::messageThreadLoop()
{
    LOGD("@%s - Start", __FUNCTION__);

    mThreadRunning = true;
    while (mThreadRunning) {
        status_t status = NO_ERROR;

        PERFORMANCE_ATRACE_BEGIN("PP-PollMsg");
        Message msg;
        mMessageQueue.receive(&msg);
        PERFORMANCE_ATRACE_END();

        PERFORMANCE_ATRACE_NAME_SNPRINTF("PP-%s", ENUM2STR(PPMsg_stringEnum, msg.id));
        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        LOGD("@%s, receive message id:%d", __FUNCTION__, msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_ID_START:
            status = handleStart(msg);
            break;
        case MESSAGE_ID_STOP:
            status = handleStop(msg);
            break;
        case MESSAGE_ID_PREPARE:
            status = handlePrepare(msg);
            break;
        case MESSAGE_ID_PROCESSFRAME:
            status = handleProcessFrame(msg);
            break;
        case MESSAGE_ID_FLUSH:
            status = handleFlush(msg);
            break;
        default:
            LOGE("ERROR Unknown message %d", msg.id);
            status = BAD_VALUE;
            break;
        }
        if (status != NO_ERROR)
            LOGE("error %d in handling message: %d", status,
                    static_cast<int>(msg.id));
        LOGD("@%s, finish message id:%d", __FUNCTION__, msg.id);
        mMessageQueue.reply(msg.id, status);
        PERFORMANCE_ATRACE_END();
    }

    LOGD("%s: Exit", __FUNCTION__);
}

status_t
RKISP2PostProcessPipeline::requestExitAndWait()
{
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    status |= mMessageThread->requestExitAndWait();
    return status;
}

status_t
RKISP2PostProcessPipeline::handleMessageExit()
{
    mThreadRunning = false;
    return NO_ERROR;
}

status_t
RKISP2PostProcessPipeline::handleStart(Message &msg)
{
    LOGD("@%s : enter", __FUNCTION__);
    status_t status = OK;
    for (int i = 0; i < kMaxLevel; i++)
        for (auto iter : mPostProcUnitArray[i])
           status |= iter->start();

    return status;
}

status_t
RKISP2PostProcessPipeline::handleStop(Message &msg)
{
    LOGD("@%s : enter", __FUNCTION__);
    status_t status = OK;

    for (int i = 0; i < kMaxLevel; i++)
        for (auto iter : mPostProcUnitArray[i])
           iter->drain();

    for (int i = 0; i < kMaxLevel; i++)
        for (auto iter : mPostProcUnitArray[i])
           status |= iter->stop();

    clear();

    return status;
}

status_t
RKISP2PostProcessPipeline::handlePrepare(Message &msg)
{
    LOGD("@%s : enter", __FUNCTION__);
    prepare_internal(msg.prepareMsg.in, msg.prepareMsg.streams, msg.prepareMsg.needpostprocess, msg.prepareMsg.pipelineDepth);
    return NO_ERROR;
}

status_t
RKISP2PostProcessPipeline::handleProcessFrame(Message &msg)
{
    LOGD("@%s : enter", __FUNCTION__);
    status_t status = OK;
    // add |out| to correspond unit
    status = addOutputBuffer(msg.processMsg.out);
    if (status != OK)
        return status;
//    LOGE("rk-debug: ======== pid=%d", syscall(SYS_gettid));
    mOutputBuffersHandler->addSyncBuffersIfNeed(msg.processMsg.in, msg.processMsg.out);
    // send |in| to each first level process unit
    for (auto iter : mPostProcUnitArray[kFirstLevel])
        status |= iter->notifyNewFrame(msg.processMsg.in, msg.processMsg.settings, 0);
    return status;
}

status_t
RKISP2PostProcessPipeline::handleFlush(Message &msg)
{
    LOGD("@%s : enter", __FUNCTION__);
    return NO_ERROR;
}

RKISP2PostProcessPipeline::
OutputBuffersHandler::OutputBuffersHandler(RKISP2PostProcessPipeline* pipeline)
    : mPipeline(pipeline) {
}

status_t RKISP2PostProcessPipeline::OutputBuffersHandler::
notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
               const std::shared_ptr<RKISP2ProcUnitSettings>& settings,
               int err) {

    status_t status = OK;
    std::map<CameraBuffer*, std::shared_ptr<syncItem>>::iterator it;

    if (!mPipeline->mMayNeedSyncStreamsOutput)
        return mPipeline->mPostProcFrameListener->notifyNewFrame(buf, settings, err);

    {
        std::lock_guard<std::mutex> l(mLock);
        it = mCamBufToSyncItemMap.find(buf->cambuf.get());
    }
    if (it != mCamBufToSyncItemMap.end()) {
        if (--it->second->sync_nums == 0) {
            LOGI("@%s return sync buffer", __FUNCTION__);
            for (auto sync_buf : it->second->sync_buffers)
                status |= mPipeline->mPostProcFrameListener->notifyNewFrame(sync_buf, settings, err);
        }
        std::lock_guard<std::mutex> l(mLock);
        mCamBufToSyncItemMap.erase(it);
    } else {
        status = mPipeline->mPostProcFrameListener->notifyNewFrame(buf, settings, err);
    }

    return status;
}

void RKISP2PostProcessPipeline::OutputBuffersHandler::
addSyncBuffersIfNeed(const std::shared_ptr<PostProcBuffer>& in,
                     const std::vector<std::shared_ptr<PostProcBuffer>>& out) {
    if (mPipeline->mMayNeedSyncStreamsOutput &&
        out.size() > 1 &&
        in->cambuf->getBufferHandle() != nullptr) {
        bool need_sync = false;

        for (auto iter : out) {
            if (iter->cambuf.get() == in->cambuf.get())
                need_sync = true;
        }
        if (need_sync) {
            LOGD("@%s add sync buffer", __FUNCTION__);
            struct syncItem* sync_item = new syncItem();
            sync_item->sync_buffers = out;
            sync_item->sync_nums = out.size();
            std::lock_guard<std::mutex> l(mLock);
            std::shared_ptr<syncItem> shared_sync_item(sync_item);
            for (auto iter : out)
                mCamBufToSyncItemMap[iter->cambuf.get()] = shared_sync_item;
        }
    }
}

RKISP2PostProcessUnitUVC::RKISP2PostProcessUnitUVC(const char* name, int type,
                                               uint32_t buftype, RKISP2PostProcessPipeline* pl)
    :RKISP2PostProcessUnit(name, type, buftype, pl) {

    // TuningServer *pserver = TuningServer::GetInstance();
    // if(pserver) {
    //     pUvc_vpu_ops = pserver->get_vpu_ops();
    //     pUvc_proc_ops = pserver->get_proc_ops();
    // }
}

RKISP2PostProcessUnitUVC::~RKISP2PostProcessUnitUVC() {
    if (pUvc_vpu_ops && pUvc_vpu_ops->encode_deinit)
        pUvc_vpu_ops->encode_deinit();
}

status_t
RKISP2PostProcessUnitUVC::prepare(const FrameInfo& outfmt, int bufNum) {

    LOGD("@%s %d: instance:%p, name: %s", __FUNCTION__, __LINE__, this, mName);

    mOutFmtInfo = outfmt;
    mBufNum = bufNum;
    uvcFrameW = outfmt.width;
    uvcFrameH = outfmt.height;
    if (pUvc_vpu_ops && pUvc_vpu_ops->encode_init && pUvc_vpu_ops->encode_init(uvcFrameW, uvcFrameH, 5)) {
        LOGE("%s(%d): encode_init failed!",__FUNCTION__,__LINE__);
        return BAD_VALUE;
    }

    return RKISP2PostProcessUnit::prepare(outfmt, bufNum);
}

status_t
RKISP2PostProcessUnitUVC::processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                     const std::shared_ptr<PostProcBuffer>& out,
                                     const std::shared_ptr<RKISP2ProcUnitSettings>& settings) {
    unsigned int fcc;
    int width,height;
    unsigned char *enc_out_data;
    unsigned int enc_out_len;
    int ret = 0;
//    unsigned char *srcbuf, *dstbuf;

    LOGD("@%s %d: instance:%p, name: %s", __FUNCTION__, __LINE__, this, mName);

//    RKISP2PostProcessUnit::processFrame(in, out, settings);
//    in->cambuf->dumpImage(CAMERA_DUMP_VIDEO, "UVC_IN");
//    out->cambuf->dumpImage(CAMERA_DUMP_VIDEO, "UVC_OUT");

    if ((pUvc_proc_ops==NULL) || !pUvc_proc_ops->get_state || !pUvc_proc_ops->get_state() || !pUvc_proc_ops->transfer_data_enable()) {
        goto out;
    }else {
        fcc = pUvc_proc_ops->get_fcc();
        pUvc_proc_ops->get_res(&width, &height);

        if((width != uvcFrameW) || (height != uvcFrameH)){
            uvcFrameW = width;
            uvcFrameH = height;
            if (V4L2_PIX_FMT_MJPEG == fcc) {
                 pUvc_vpu_ops->encode_deinit();
                 if (pUvc_vpu_ops->encode_init && pUvc_vpu_ops->encode_init(uvcFrameW, uvcFrameH, 5)) {
                     LOGE("%s(%d): encode_init failed!",__FUNCTION__,__LINE__);
                 }

            }
        }

        std::shared_ptr<PostProcBuffer> tempBuf = std::make_shared<PostProcBuffer> ();
        if(V4L2_PIX_FMT_MJPEG == fcc){
            tempBuf->cambuf = MemoryUtils::acquireOneBufferWithNoCache(mPipeline->getCameraId(), uvcFrameW, uvcFrameH);
        }else if(V4L2_PIX_FMT_YUYV == fcc){
            tempBuf->cambuf = MemoryUtils::acquireOneBuffer(mPipeline->getCameraId(), uvcFrameW, uvcFrameH);
        }
        tempBuf->request = in->request;


        RKISP2PostProcessUnit::processFrame(in, tempBuf, settings);
        switch (fcc) {
            case V4L2_PIX_FMT_YUYV:
            {
                pUvc_proc_ops->transfer_data(NULL, 0, (void *)tempBuf->cambuf->data(), uvcFrameW * uvcFrameH * 2, fcc);
                break;
            }
            case V4L2_PIX_FMT_MJPEG:
            {
                std::shared_ptr<PostProcBuffer> tempBuf1 = std::make_shared<PostProcBuffer> ();
                tempBuf1->cambuf = MemoryUtils::acquireOneBufferWithNoCache(mPipeline->getCameraId(), uvcFrameW, uvcFrameH);
                pUvc_vpu_ops->encode_set_buf(tempBuf1->cambuf->dmaBufFd(),(void *)tempBuf1->cambuf->data(),
                                           tempBuf1->cambuf->dmaBufFd(),uvcFrameW * uvcFrameH);
                ret = pUvc_vpu_ops->encode_process((void *)tempBuf->cambuf->data(), tempBuf->cambuf->dmaBufFd(), uvcFrameW * uvcFrameH * 3 / 2);
                if (!ret) {
                    pUvc_vpu_ops->encode_get_buf(&enc_out_data, &enc_out_len);
                    pUvc_proc_ops->transfer_data(NULL, 0, enc_out_data, enc_out_len, fcc);
                }
               break;
            }
        }
    }
out:
    mCurPostProcBufOut.reset();
    mCurPostProcBufIn.reset();
    mCurProcSettings.reset();
    return OK;
}

RKISP2PostProcessUnitJpegEnc::RKISP2PostProcessUnitJpegEnc(const char* name, int type,
                                               uint32_t buftype, RKISP2PostProcessPipeline* pl)
    : RKISP2PostProcessUnit(name, type, buftype, pl) {
}

RKISP2PostProcessUnitJpegEnc::~RKISP2PostProcessUnitJpegEnc() {
}

status_t
RKISP2PostProcessUnitJpegEnc::notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                                         const std::shared_ptr<RKISP2ProcUnitSettings>& settings,
                                         int err) {
    std::unique_lock<std::mutex> l(mApiLock, std::defer_lock);
    l.lock();
    // fix VideoSnapshot exception:
    // Compared with normal capture, in videoSnapshot case, app would
    // not wait for the jpeg result and keep senting requests to hal.
    // Additionally, encoding jpeg may take a long time and block the
    // jpeg unit thread, therefor casue the growing of mInBufferPool
    // size and then cause the failure of acquiring RKISP2ProcUnitSettings.
    // finally lead to a fault error.
    if (mOutBufferPool.empty()) {
        l.unlock();
        return OK;
    }
    l.unlock();

    return RKISP2PostProcessUnit::notifyNewFrame(buf, settings, err);
}

status_t
RKISP2PostProcessUnitJpegEnc::processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                     const std::shared_ptr<PostProcBuffer>& out,
                                     const std::shared_ptr<RKISP2ProcUnitSettings>& settings) {
    PERFORMANCE_ATRACE_CALL();
    status_t status = OK;
    // to avoid the destruct of the in args
    std::shared_ptr<PostProcBuffer> inbuf = in;
    std::shared_ptr<PostProcBuffer> outBuf = out;
    std::shared_ptr<RKISP2ProcUnitSettings> procsettings = settings;

    LOGD("%s: @%s, reqId: %d",
         mName, __FUNCTION__, procsettings->request->getId());

    inbuf->cambuf->dumpImage(CAMERA_DUMP_JPEG, "before_jpeg_converion_nv12");
    // JPEG encoding
    status = mJpegTask->handleMessageSettings(*procsettings.get());
    CheckError((status != OK), status, "@%s, set settings failed! [%d]!",
               __FUNCTION__, status);

    status = convertJpeg(inbuf->cambuf, outBuf->cambuf, outBuf->request);
    //caputre buffer already done with holding release fence, now signal
    //the release fence. In normal case, capture done should be called in
    //OutputFrameWorker::notifyNewFrame, but in order to speed up capture
    //time in switch capture case, the pipeline flush and stop had been done
    //in advance, so it can't notify to outputFrameWork here, just do
    //cambuf->captureDone here.
    outBuf->cambuf->captureDone(outBuf->cambuf, true);

    mCurPostProcBufOut.reset();
    CheckError((status != OK), status, "@%s, JPEG conversion failed! [%d]!",
               __FUNCTION__, status);

    return status;
}

status_t
RKISP2PostProcessUnitJpegEnc::prepare(const FrameInfo& outfmt, int bufNum) {

    if (!mJpegTask.get()) {
        LOGI("Create RKISP2JpegEncodeTask");
        mJpegTask.reset(new RKISP2JpegEncodeTask(mPipeline->getCameraId()));

        if (mJpegTask->init() != NO_ERROR) {
            LOGE("Failed to init RKISP2JpegEncodeTask Task");
            mJpegTask.reset();
            return UNKNOWN_ERROR;
        }
    }

    return RKISP2PostProcessUnit::prepare(outfmt, bufNum);
}

status_t
RKISP2PostProcessUnitJpegEnc::convertJpeg(std::shared_ptr<CameraBuffer> buffer,
                                    std::shared_ptr<CameraBuffer> jpegBuffer,
                                    Camera3Request *request) {
    status_t status = NO_ERROR;

    RKISP2ITaskEventListener::PUTaskEvent msg;
    msg.buffer = jpegBuffer;
    msg.jpegInputbuffer = buffer;
    msg.request = request;

    PERFORMANCE_ATRACE_CALL();
    LOGI("jpeg inbuf wxh %dx%d stride %d, fmt 0x%x,0x%x size 0x%x",
         buffer->width(), buffer->height(), buffer->stride(),
         buffer->format(), buffer->v4l2Fmt(),
         buffer->size());

    if (mJpegTask.get()) {
        status = mJpegTask->handleMessageNewJpegInput(msg);
    }

    return status;

}

RKISP2PostProcessUnitRaw::RKISP2PostProcessUnitRaw(const char* name, int type,
                                               uint32_t buftype)
    : RKISP2PostProcessUnit(name, type, buftype) {
}

RKISP2PostProcessUnitRaw::~RKISP2PostProcessUnitRaw() {
}

status_t
RKISP2PostProcessUnitRaw::processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                     const std::shared_ptr<PostProcBuffer>& out,
                                     const std::shared_ptr<RKISP2ProcUnitSettings>& settings) {
    PERFORMANCE_ATRACE_CALL();
    status_t status = OK;

    LOGD("@%s: instance:%p, name: %s", __FUNCTION__, this, mName);
    /* in->cambuf->dumpImage(CAMERA_DUMP_RAW, "RawUnit"); */
    //std::string fileName;
    //std::string dumpPrefix = "";
    //char dumpSuffix[100] = {};
    //char szDateTime[20] = "";
    //struct timeval tval;
    // TuningServer *pserver = TuningServer::GetInstance();

    // if (pserver && (pserver->mStartCapture) && (pserver->mCapRawNum > 0)){
    //     if(pserver->mSkipFrame > 0){
    //         pserver->mSkipFrame--;
    //         return status;
    //     }
    //     gettimeofday(&tval, NULL);
    //     int64_t timestamp =  (((int64_t)tval.tv_sec) * 1000000L) + tval.tv_usec;
    //     strftime( szDateTime, sizeof(szDateTime), "%Y%m%d_%H%M%S", localtime(&tval.tv_sec) );
    //     snprintf(dumpSuffix, sizeof(dumpSuffix), "t%0.3f_g%0.3f_%dx%d_raw16_%s_%d", pserver->mCurTime/1000.0, pserver->mCurGain,
    //              out->cambuf->width(), out->cambuf->height(), szDateTime, pserver->mCapRawNum);
    //     fileName = std::string("/data/isptune/") + dumpPrefix + std::string(dumpSuffix) + std::string(".pgm");

    //     LOGD("%s filename is %s", __FUNCTION__, fileName.data());

    //     FILE *fp = fopen (fileName.data(), "w+");
    //     if (fp == nullptr) {
    //         LOGE("open file failed,%s",strerror(errno));
    //         return status;
    //     }
    //     //raw format,bayer pattern
    //     fprintf( fp,
    //             "P5\n%d %d\n#####<DCT Raw>\n#<Type>%u</Type>\n#<Layout>%u</Layout>\n#<TimeStampUs>%lli</TimeStampUs>\n#####</DCT Raw>\n%d\n",
    //             out->cambuf->width(), out->cambuf->height(), 0x11/*raw16*/, 0x12, timestamp, 65535);

    //     if ((fwrite(out->cambuf->data(), out->cambuf->width() * out->cambuf->height() * 2, 1, fp)) != 1)
    //         LOGW("Error or short count writing %d bytes to %s", out->cambuf->width() * out->cambuf->height() * 2, fileName.data());
    //     fclose (fp);
    //     pserver->mCapRawNum--;
    //     if(pserver->mCapRawNum == 0){
    //         pserver->stopCatureRaw();
    //         if(pserver->mMsgType == CMD_TYPE_SYNC)
    //             pserver->mUvc_proc_ops->uvc_signal();
    //     }
    // }
    return status;
}

RKISP2PostProcessUnitSwLsc::RKISP2PostProcessUnitSwLsc(const char* name, int type,
                                           uint32_t buftype, RKISP2PostProcessPipeline* pl)
    : RKISP2PostProcessUnit(name, type, buftype, pl) {
        memset(&mLscPara, 0, sizeof(mLscPara));
}

RKISP2PostProcessUnitSwLsc::~RKISP2PostProcessUnitSwLsc() {
    if (mLscPara.u32_coef_pic_gr)
        free(mLscPara.u32_coef_pic_gr);
}

status_t
RKISP2PostProcessUnitSwLsc::processFrame(const std::shared_ptr<PostProcBuffer>& in,
                                   const std::shared_ptr<PostProcBuffer>& out,
                                   const std::shared_ptr<RKISP2ProcUnitSettings>& settings) {
    PERFORMANCE_ATRACE_CALL();
    LOGD("%s: @%s, reqId: %d",
         mName, __FUNCTION__, settings->request->getId());
    ScopedPerfTrace lscper(3, "lscper", 30 * 1000);

    status_t status = OK;

    status = lsc((uint8_t *)in->cambuf->data(),
                 in->cambuf->width(),
                 in->cambuf->height(),
                 0,  // bayer pattern, ignored for Y_lsc
                 &mLscPara,
                 (uint8_t *)out->cambuf->data(),
                 16);
    if (status != OK) {
        LOGE("%s: failed", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    uint32_t y_size = in->cambuf->width() * in->cambuf->height();
    // copy uv
    memcpy((uint8_t *)out->cambuf->data() + y_size,
           (uint8_t *)in->cambuf->data() + y_size,
           y_size / 2);

    return OK;
}

status_t
RKISP2PostProcessUnitSwLsc::prepare(const FrameInfo& outfmt, int bufNum) {
    if (mLscPara.u32_coef_pic_gr)
        free(mLscPara.u32_coef_pic_gr);

    mLscPara.width = outfmt.width;
    mLscPara.height = outfmt.height;
    LOGI("%s: widthxheigt %dx%d", __FUNCTION__, mLscPara.width, mLscPara.height);
    lsc_config(&mLscPara);

    return RKISP2PostProcessUnit::prepare(outfmt, bufNum);
}

// parameter defined in rtl
// defined in isp.inc.v
//#define c_dw_do            10        // width of isp data out Y an C used at output of gamma_out
#define c_cfg_lsc          7         // lens shading configuration address width
#define c_lsc_base_adr     0x2200

// defined in ram_sizes.inc.v
#define c_lsc_ram_ad_bw    9                  // bit width for the RAM address
#define c_lsc_ram_d_bw     26                 // double correction factor, must be even numbers

#define c_lsc_size_bw      10                 // bit width for xsize and ysize values
#define c_lsc_grad_bw      12                 // bit width of the factor for x and y gradients calculation
#define c_lsc_size_bw_2x   (2*c_lsc_size_bw)
#define c_lsc_grad_bw_2x   (2*c_lsc_grad_bw)

#define c_lsc_sample_bw    (c_lsc_ram_d_bw/2) // bit width of the correction factor values stored in RAM
#define c_lsc_sample_bw_2x c_lsc_ram_d_bw

#define c_lsc_corr_bw       15     // bit width of the correction factor values used internal.
#define c_lsc_corr_frac_bw  12     // bit width of the fractional part of correction factor values used internal

#define c_lsc_grad_exp      15     // fixed exponent for the x and y gradients
#define c_lsc_corr_extend   10     // extended fractal part of dx,dy of internal correction factor
                                       // constraint : c_lsc_corr_extend <= c_lsc_grad_exp
#define c_extend_round      (1 << (c_lsc_corr_extend - 1))
#define c_frac_round        (1 << (c_lsc_corr_frac_bw-1))

// bit width difference of correction factor values between used internal and stored in RAM
#define c_corr_diff  (c_lsc_corr_bw - c_lsc_sample_bw)

#define c_dx_shift   (c_lsc_grad_exp - c_lsc_corr_extend)
#define c_dx_round   (1 << (c_dx_shift - 1))
#define c_dy_shift   (c_lsc_grad_exp - c_lsc_corr_extend - c_corr_diff)
#define c_dy_round   (1 << (c_dy_shift - 1))

#define c_dx_bw      (c_lsc_corr_bw + c_lsc_grad_bw - c_dx_shift)
#define c_dy_bw      (c_lsc_sample_bw + c_lsc_grad_bw - c_dy_shift)

/*****************************************************************************/
/**
 * @Purpose    bilinear interpolation unit
 *
 * @param   u16_coef_blk     input raw data
 * @param   pu32_coef_pic    output coef after bilinear interplation
 * @param   u32_z_max        the total number of lsc coef table
 * @param   u32_y_max        height of image
 * @param   u32_x_max        width of image
 * @param   plsc_a    other parameters
 *
 * @return                      Returns the result of the function call.
 * @retval  RET_SUCCESS
 *
 *****************************************************************************/
void
RKISP2PostProcessUnitSwLsc::calcu_coef(lsc_para_t const * plsc_a,
                                 uint16_t u16_coef_blk[2][17][18],
                                 uint32_t * pu32_coef_pic,
                                 uint32_t u32_z_max,
                                 uint32_t u32_y_max,
                                 uint32_t u32_x_max) {
    int32_t i             ;
    uint32_t u32_tmp      ;
    uint32_t u32_tmp2     ;
    uint8_t  u8_x_blk     ; // u32_x coordinate of block number of curent picture
    uint8_t  u8_y_blk     ; // u32_y coordinate of block number of curent picture
    uint16_t u16_x_base   ; // left up coordinate of curent block
    uint16_t u16_y_base   ; // left up coordinate of curent block
    uint16_t u16_x_offset ; // u32_x coordinate offset of curent block
    uint16_t u16_y_offset ; // u32_x coordinate offset of curent block

    uint16_t u16_sizey_cur ;
    uint16_t u16_grady_cur ;
    uint16_t u16_sizex_cur ;
    uint16_t u16_gradx_cur ;
    uint16_t u16_coef_lu   ; // left up
    uint16_t u16_coef_ld   ; // left down
    uint16_t u16_coef_ru   ; // right up
    uint16_t u16_coef_rd   ; // right down
    uint32_t u32_coef_l    ;
    uint32_t u32_coef_r    ;
    uint32_t u32_coef      ;

    for (i = 0; i < 2; i++) {
        for (u16_y_base = 0, u8_y_blk = 0; u8_y_blk < 16; u8_y_blk++) {
            u16_sizey_cur = (u8_y_blk<8)? plsc_a->sizey[u8_y_blk] : plsc_a->sizey[15-u8_y_blk];
            u16_grady_cur = (u8_y_blk<8)? plsc_a->grady[u8_y_blk] : plsc_a->grady[15-u8_y_blk];
            for (u16_x_base = 0, u8_x_blk = 0; u8_x_blk < 16; u8_x_blk++) {
                u16_sizex_cur = (u8_x_blk<8)? plsc_a->sizex[u8_x_blk] : plsc_a->sizex[15-u8_x_blk];
                u16_gradx_cur = (u8_x_blk<8)? plsc_a->gradx[u8_x_blk] : plsc_a->gradx[15-u8_x_blk];
                u16_coef_lu   = u16_coef_blk[i][u8_y_blk][u8_x_blk]     ; // left up
                u16_coef_ld   = u16_coef_blk[i][u8_y_blk+1][u8_x_blk]   ; // left down
                u16_coef_ru   = u16_coef_blk[i][u8_y_blk][u8_x_blk+1]   ; // right up
                u16_coef_rd   = u16_coef_blk[i][u8_y_blk+1][u8_x_blk+1] ; // right down
                for (u16_y_offset = 0; u16_y_offset < u16_sizey_cur; u16_y_offset++) {
                    u32_tmp    = abs(u16_coef_lu - u16_coef_ld);
                    u32_tmp    = u32_tmp * u16_grady_cur;
                    u32_tmp    = (u32_tmp + c_dy_round) >> c_dy_shift;
                    u32_tmp    = u32_tmp * u16_y_offset;
                    u32_tmp    = (u32_tmp + c_extend_round) >> c_lsc_corr_extend;
                    u32_tmp    = (u32_tmp << (32-c_lsc_corr_bw)) >> (32-c_lsc_corr_bw);
                    u32_coef_l = u16_coef_lu << c_corr_diff;
                    u32_coef_l = (u16_coef_lu > u16_coef_ld)? (u32_coef_l - u32_tmp) : (u32_coef_l + u32_tmp);

                    u32_tmp    = abs(u16_coef_ru - u16_coef_rd);
                    u32_tmp    = u32_tmp * u16_grady_cur;
                    u32_tmp    = (u32_tmp + c_dy_round) >> c_dy_shift;
                    u32_tmp    = u32_tmp * u16_y_offset;
                    u32_tmp    = (u32_tmp + c_extend_round) >> c_lsc_corr_extend;
                    u32_tmp    = (u32_tmp << (32-c_lsc_corr_bw)) >> (32-c_lsc_corr_bw);
                    u32_coef_r = u16_coef_ru << c_corr_diff;
                    u32_coef_r = (u16_coef_ru > u16_coef_rd)? (u32_coef_r - u32_tmp) : (u32_coef_r + u32_tmp);

                    u32_coef   = u32_coef_l << c_lsc_corr_extend;
                    /* TODO */
                    //u32_tmp = abs(u32_coef_r - u32_coef_l);
                    u32_tmp = abs(int32_t(u32_coef_r - u32_coef_l));
                    u32_tmp = u32_tmp * u16_gradx_cur;
                    u32_tmp = (u32_tmp + c_dx_round) >> c_dx_shift;
                    for (u16_x_offset = 0; u16_x_offset < u16_sizex_cur; u16_x_offset++) {
                        u32_tmp2 = (u32_coef + c_extend_round) >> c_lsc_corr_extend;
                        u32_tmp2 = (u32_tmp2 > ((2<<c_lsc_corr_bw)-1))? ((2<<c_lsc_corr_bw)-1) : u32_tmp2;
                        *(pu32_coef_pic + i*u32_y_max*u32_x_max + (u16_y_base+u16_y_offset)*u32_x_max
                                + (u16_x_base+u16_x_offset)) = (uint16_t)u32_tmp2;
                        u32_coef = (u32_coef_l > u32_coef_r)? (u32_coef - u32_tmp) : (u32_coef + u32_tmp);
                    }   // for u16_x_offset
                }   // for u16_y_offset
                u16_x_base += u16_sizex_cur;
            }   // for u8_x_blk
            u16_y_base += u16_sizey_cur;
        }   // for u8_y_blk
    }   // for i < 2
}


int RKISP2PostProcessUnitSwLsc::lsc_config(void *para_v)
{
    lsc_para_t *para= (lsc_para_t*)para_v;
    int32_t i ,x,y,z;
    int32_t width_align16 = (para->width + 0xf) & ~0xf;
    int32_t height_align16 = (para->height + 0xf) & ~0xf;
    /* this is for 1080p */
    uint16_t sizex[8] = {120 ,120 ,120 ,120 ,120, 120, 120, 120};
    uint16_t sizey[8] = {67 ,68 ,67 ,68, 67, 68, 67, 68};

    /* generic split for any resolution */
    for (i = 0; i++; i < 8) {
        sizex[i] = para->width / 2 / 8;
        sizey[i] = para->height / 2 / 8;
    }

    sizex[7] += (para->width % 16) / 2;
    sizey[7] += (para->height % 16) / 2;

    uint16_t xmlcoef_r[17][17]  = {
        {2955,2298,1926,1685,1514,1396,1316,1266,1258,1258,1282,1336,1433,1558,1758,2072,2542},
        {2727,2134,1827,1599,1435,1327,1251,1209,1192,1195,1222,1276,1359,1486,1668,1932,2359},
        {2513,2016,1728,1526,1372,1266,1203,1160,1142,1149,1175,1218,1294,1418,1586,1849,2215},
        {2371,1929,1662,1461,1317,1219,1163,1126,1112,1116,1137,1183,1257,1371,1533,1764,2094},
        {2271,1862,1601,1411,1282,1188,1132,1095,1081,1080,1108,1151,1222,1322,1479,1713,2028},
        {2176,1817,1556,1380,1252,1160,1105,1073,1059,1057,1083,1124,1193,1290,1441,1654,1960},
        {2155,1769,1535,1353,1226,1138,1083,1055,1037,1045,1070,1110,1176,1266,1418,1634,1913},
        {2107,1758,1509,1330,1209,1128,1082,1040,1030,1033,1060,1098,1163,1254,1401,1612,1902},
        {2091,1758,1512,1333,1208,1133,1076,1045,1024,1031,1052,1096,1164,1252,1395,1603,1888},
        {2107,1753,1509,1329,1211,1130,1073,1045,1027,1033,1060,1101,1162,1259,1401,1616,1886},
        {2111,1769,1524,1338,1219,1137,1076,1055,1037,1045,1066,1107,1173,1262,1409,1610,1921},
        {2148,1795,1547,1364,1232,1150,1097,1065,1055,1061,1078,1121,1186,1284,1426,1638,1913},
        {2226,1829,1574,1392,1254,1175,1119,1087,1076,1081,1105,1146,1207,1313,1458,1670,1969},
        {2287,1891,1630,1430,1294,1205,1150,1118,1104,1106,1137,1177,1241,1349,1506,1726,2046},
        {2410,1971,1687,1492,1351,1250,1192,1161,1146,1149,1170,1217,1282,1403,1556,1805,2131},
        {2591,2059,1771,1562,1408,1307,1238,1199,1186,1189,1208,1262,1340,1455,1632,1878,2245},
        {2761,2193,1875,1640,1465,1372,1295,1259,1235,1244,1266,1323,1405,1526,1719,2004,2401}
    };
    uint16_t xmlcoef_gr[17][17] = {
        {1377,1306,1244,1189,1157,1134,1112,1111,1101,1110,1120,1134,1149,1177,1233,1279,1373},
        {1358,1268,1202,1158,1132,1107,1100,1087,1081,1085,1092,1109,1115,1158,1185,1248,1306},
        {1301,1234,1184,1136,1110,1090,1077,1065,1068,1068,1075,1085,1109,1127,1170,1212,1294},
        {1273,1204,1156,1120,1094,1076,1061,1059,1056,1054,1061,1074,1087,1118,1146,1185,1254},
        {1251,1192,1149,1109,1088,1068,1054,1048,1048,1050,1054,1065,1084,1105,1133,1177,1218},
        {1235,1182,1130,1100,1073,1056,1053,1039,1039,1042,1049,1059,1078,1091,1123,1160,1216},
        {1228,1169,1121,1093,1074,1050,1038,1035,1027,1036,1039,1054,1064,1088,1116,1157,1209},
        {1211,1156,1117,1091,1063,1046,1035,1028,1028,1027,1038,1048,1063,1087,1109,1148,1196},
        {1210,1161,1114,1081,1065,1048,1035,1024,1024,1029,1035,1048,1064,1080,1112,1141,1193},
        {1221,1160,1121,1090,1067,1051,1039,1031,1027,1030,1039,1049,1064,1090,1116,1153,1196},
        {1235,1166,1127,1095,1071,1054,1042,1036,1033,1036,1043,1056,1073,1098,1121,1158,1211},
        {1239,1179,1132,1102,1073,1063,1049,1043,1042,1040,1052,1066,1084,1104,1135,1173,1239},
        {1244,1190,1145,1115,1083,1066,1057,1046,1045,1051,1055,1071,1086,1118,1142,1191,1234},
        {1277,1213,1158,1120,1101,1075,1066,1062,1058,1058,1064,1083,1108,1124,1165,1202,1265},
        {1322,1228,1192,1141,1119,1096,1081,1072,1074,1071,1083,1098,1124,1153,1180,1240,1288},
        {1337,1276,1200,1171,1133,1113,1102,1091,1093,1092,1100,1118,1140,1170,1208,1269,1347},
        {1387,1298,1251,1198,1161,1135,1121,1111,1113,1110,1124,1141,1168,1198,1242,1301,1377}
    };
    uint16_t xmlcoef_gb[17][17] = {
        {3351,2558,2124,1838,1631,1505,1411,1346,1320,1326,1352,1415,1527,1678,1900,2246,2813},
        {3057,2381,1989,1723,1539,1415,1333,1277,1254,1260,1281,1344,1436,1584,1785,2099,2576},
        {2807,2216,1865,1634,1455,1341,1262,1210,1193,1191,1224,1276,1359,1499,1697,1986,2408},
        {2636,2112,1785,1558,1391,1281,1218,1168,1149,1150,1172,1224,1308,1438,1628,1903,2298},
        {2499,2020,1715,1501,1345,1235,1169,1126,1110,1113,1139,1187,1264,1393,1572,1828,2195},
        {2403,1954,1665,1449,1305,1199,1136,1099,1075,1081,1105,1155,1236,1351,1520,1774,2123},
        {2349,1914,1627,1420,1271,1176,1108,1074,1055,1059,1086,1137,1209,1319,1497,1736,2094},
        {2315,1888,1601,1397,1255,1159,1095,1051,1035,1044,1069,1119,1197,1307,1472,1717,2067},
        {2279,1875,1582,1389,1247,1150,1083,1044,1029,1034,1061,1112,1186,1295,1461,1699,2038},
        {2273,1869,1584,1382,1240,1145,1083,1042,1024,1032,1057,1111,1184,1296,1457,1701,2050},
        {2310,1879,1598,1388,1243,1147,1085,1048,1033,1039,1067,1117,1191,1302,1467,1720,2061},
        {2325,1900,1615,1408,1253,1162,1100,1061,1045,1053,1079,1132,1206,1325,1492,1732,2080},
        {2399,1946,1647,1432,1279,1184,1119,1087,1068,1076,1100,1153,1226,1345,1520,1770,2119},
        {2479,1997,1695,1476,1317,1216,1153,1114,1095,1104,1130,1180,1262,1385,1561,1828,2214},
        {2622,2091,1762,1536,1371,1259,1191,1154,1135,1140,1171,1221,1301,1436,1622,1911,2313},
        {2776,2191,1840,1602,1432,1317,1239,1200,1177,1182,1209,1271,1361,1503,1698,1994,2434},
        {2974,2321,1936,1681,1501,1374,1293,1246,1230,1232,1260,1317,1425,1575,1784,2096,2590}
    };
    uint16_t xmlcoef_b[17][17]  = {
        {2740,2166,1837,1621,1485,1387,1328,1289,1292,1302,1337,1387,1483,1628,1815,2102,2610},
        {2531,2013,1734,1537,1402,1316,1261,1230,1227,1242,1264,1316,1404,1536,1714,1987,2388},
        {2318,1898,1639,1472,1343,1257,1206,1179,1174,1182,1210,1252,1333,1457,1626,1888,2227},
        {2211,1828,1581,1413,1283,1213,1171,1139,1131,1142,1163,1211,1277,1389,1561,1797,2129},
        {2108,1761,1531,1364,1244,1174,1131,1107,1097,1106,1131,1169,1236,1340,1501,1732,2035},
        {2035,1708,1485,1325,1217,1142,1101,1078,1077,1079,1100,1137,1209,1302,1453,1677,1981},
        {2003,1679,1459,1302,1194,1120,1077,1056,1051,1057,1080,1120,1183,1279,1422,1642,1930},
        {1973,1668,1446,1279,1176,1104,1066,1039,1033,1043,1067,1103,1165,1265,1401,1617,1910},
        {1960,1657,1429,1273,1167,1100,1057,1031,1025,1036,1064,1098,1160,1253,1396,1602,1883},
        {1973,1651,1431,1273,1163,1101,1053,1033,1024,1028,1054,1098,1156,1251,1394,1605,1898},
        {1973,1657,1436,1272,1168,1101,1060,1030,1030,1038,1064,1097,1167,1263,1398,1614,1913},
        {2008,1672,1449,1290,1172,1103,1066,1044,1036,1046,1072,1109,1175,1278,1424,1628,1945},
        {2041,1695,1470,1311,1186,1120,1082,1057,1055,1061,1088,1126,1196,1302,1452,1674,1976},
        {2096,1744,1511,1332,1219,1146,1111,1083,1074,1089,1115,1161,1227,1336,1495,1722,2049},
        {2204,1799,1558,1387,1266,1177,1139,1120,1111,1120,1145,1194,1266,1385,1552,1806,2153},
        {2318,1881,1621,1446,1314,1225,1175,1155,1150,1157,1191,1242,1319,1438,1626,1891,2258},
        {2455,1989,1695,1515,1378,1278,1226,1197,1190,1200,1226,1284,1369,1518,1712,1979,2404}
    };

    para->lsc_en    = 1;
    para->table_sel = 1;
    for ( i = 0; i < 8; i++)
    {
        para->sizex[i] = sizex[i];
        para->sizey[i] = sizey[i];
        para->gradx[i]        = (double)32768/(double)para->sizex[i] + 0.5;
        if (para->gradx[i] > 4095)
            para->gradx[i]    = 4095;
        para->grady[i]        = (double)32768/(double)para->sizey[i] + 0.5;
        if (para->grady[i] > 4095)
            para->grady[i]    = 4095;
    }

    for ( z = 0; z < 2; z++) //2 table for lens shade correction with the same coef
    {
        for ( x = 0; x < 17; x++)
        {
            for ( y = 0; y < 18; y++)
            {
                if (y==17)
                {
                    para->u16_coef_r[z][x][y]  = 0;
                    para->u16_coef_gr[z][x][y] = 0;
                    para->u16_coef_gb[z][x][y] = 0;
                    para->u16_coef_b[z][x][y]  = 0;
                }
                else
                {
                    para->u16_coef_r[z][x][y]  = xmlcoef_r[x][y];
                    para->u16_coef_gr[z][x][y] = xmlcoef_gr[x][y];
                    para->u16_coef_gb[z][x][y] = xmlcoef_gb[x][y];
                    para->u16_coef_b[z][x][y]  = xmlcoef_b[x][y];
                }
            }
        }
    }

    para->u32_coef_pic_gr = (uint32_t*)malloc(2* width_align16 * height_align16 * sizeof(uint32_t));

    if (para->u32_coef_pic_gr == NULL)
        return -1;

    return 0;

}

/*****************************************************************************/
/**
 * @Purpose   lens shading correction unit
 *
 * @param   indata        input raw data
 * @param   outdata      output raw data
 * @param   width        width of image
 * @param   height        height of image
 * @param   bayer_pat    bayer pattern of image
 * @param   lsc_para    other parameters
 *
 * @return                      Returns the result of the function call.
 * @retval  RET_SUCCESS
 *
 *****************************************************************************/
int
RKISP2PostProcessUnitSwLsc::lsc(uint8_t *indata, uint16_t input_h_size, uint16_t input_v_size,
                          uint8_t bayer_pat, lsc_para_t *lsc_para,
                          uint8_t *outdata, uint8_t  c_dw_si)
{
    //input_h_size
    //int32_t     index = 0;
    uint16_t     (*u16_coef_gr)[17][18] = lsc_para->u16_coef_gr;

    uint32_t *u32_coef_pic_gr;

    u32_coef_pic_gr = lsc_para->u32_coef_pic_gr;
    calcu_coef(lsc_para, u16_coef_gr, u32_coef_pic_gr, 2, input_v_size, input_h_size);

    /* uint16_t c_dw_si_shift = (1 << c_dw_si) - 1; */
    //lens shading correction
#if 0
    uint16_t     u16_data;
    for (index = 0 ; index < input_v_size * input_h_size; index++) {
            u16_data = (uint16_t)(indata[index]) << 8;
            u16_data = (u16_data * u32_coef_pic_gr[index] + c_frac_round) >> c_lsc_corr_frac_bw;
            /* u16_data = (u16_data > c_dw_si_shift)? c_dw_si_shift : u16_data; */
            outdata[index] = u16_data >> 8;
    }
#else
    /* uint32_t   total = input_v_size * input_h_size; */
    /* const uint32_t   c_frac_round_tmp = c_frac_round; */
    /* asm volatile ( */
    /* "0:                                                                   \n\t" */
    /* "vld1.u8 d0, [%[indata]]!       @uint8x8                              \n\t" */
    /* "vshll.u8 q2, d0, #8              @uint16x8 << 8                      \n\t" */

    /* "vmovl.u16 q3, d4               @uint16x8 --> uint32x4                \n\t" */
    /* "vmovl.u16 q4, d5               @uint16x8 --> uint32x4                \n\t" */

    /* "vld1.u32 {q0, q1}, [%[u32_coef_pic_gr]]!                             \n\t" */
    /* "vdup.32 q6, %[c_frac_round_tmp]    @uint32x4 c_frac_round            \n\t" */
    /* "vdup.32 q7, %[c_frac_round_tmp]  @uint32x4 c_frac_round              \n\t" */

    /* "vmla.u32 q6, q3, q0                                                  \n\t " */
    /* "vmla.u32 q7, q4, q1                                                  \n\t" */

    /* "vqshrn.u32 d0, q6, #12                                               \n\t" */
    /* "vqshrn.u32 d1, q7, #12                                               \n\t" */

    /* "vqshrn.u16 d2, q0, #8                                                \n\t" */

    /* "vst1.u8 d2, [%[outdata]]!                                            \n\t" */

    /* "add %[index], %[index], #8                                           \n\t" */
    /* "cmp %[index], %[total]                                               \n\t" */
    /* "blt 0b                                                               \n\t" */
    /* : [outdata] "+r" (outdata), [index] "+r" (index), [total] "+r" (total), [u32_coef_pic_gr] "+r" (u32_coef_pic_gr) */
    /* : [indata] "r" (indata), [c_frac_round_tmp] "r" (c_frac_round_tmp) */
    /* : "cc", "memory", "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7" */
    /* ); */
#endif

    return 0;
}

RKISP2PostProcessUnitDigitalZoom::RKISP2PostProcessUnitDigitalZoom(
    const char* name, int type, int camid, uint32_t buftype, RKISP2PostProcessPipeline* pl)
    : RKISP2PostProcessUnit(name, type, buftype, pl) {
    mApa = PlatformData::getActivePixelArray(camid);
}

RKISP2PostProcessUnitDigitalZoom::~RKISP2PostProcessUnitDigitalZoom() {
}

bool RKISP2PostProcessUnitDigitalZoom::checkFmt(CameraBuffer* in, CameraBuffer* out) {
    if (!in || !out)
        return false;

    // only support NV12 or NV21 now
    bool in_fmt_supported = in->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
                            in->format() == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
                            in->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED ||
                            in->format() == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                            in->v4l2Fmt() == V4L2_PIX_FMT_NV12 ||
                            in->v4l2Fmt() == V4L2_PIX_FMT_NV21;
    bool out_fmt_supported = out->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
                             out->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED ||
                             out->format() == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
                             out->format() == HAL_PIXEL_FORMAT_YCrCb_420_SP;
                             out->v4l2Fmt() == V4L2_PIX_FMT_NV12 ||
                             out->v4l2Fmt() == V4L2_PIX_FMT_NV21;
    return (in_fmt_supported && out_fmt_supported);
}

status_t
RKISP2PostProcessUnitDigitalZoom::processFrame(const std::shared_ptr<PostProcBuffer>& in,
                              const std::shared_ptr<PostProcBuffer>& out,
                              const std::shared_ptr<RKISP2ProcUnitSettings>& settings) {
    PERFORMANCE_ATRACE_CALL();
    CameraWindow& crop = settings->cropRegion;
    int jpegBufCount = settings->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB);

    bool mirror_handing = false;
#ifdef MIRROR_HANDLING_FOR_FRONT_CAMERA
    //for front camera mirror handling
    mirror_handing = PlatformData::facing(mPipeline->getCameraId()) == CAMERA_FACING_FRONT;
#endif
    LOGD("@%s : mirror handleing %d pid=%d", __FUNCTION__, mirror_handing, syscall(SYS_gettid));

    // check if zoom is required
    if (mBufType != kPostProcBufTypeExt &&
        crop.width() ==  mApa.width() && crop.height() == mApa.height()) {
        // HwJpeg encode require buffer width and height align to 16 or large enough.
        // digital zoom out buffer is internal gralloc buffer with size 2xWxH, so it
        // can always meet the Hwjpeg input condition. we use it as a workaround
        // for capture case
        if(jpegBufCount != 0) {
            LOGD("@%s : Use digital zoom out gralloc buffer as hwjpeg input buffer", __FUNCTION__);
        } else if(mirror_handing) {
            LOGD("@%s : use digitalZoom do mirror for front camera", __FUNCTION__);
        } else {
            return STATUS_FORWRAD_TO_NEXT_UNIT;
        }
    }

    if (!checkFmt(in->cambuf.get(), out->cambuf.get())) {
        LOGE("%s: unsupported format, only support NV12 or NV21 now !", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    // map crop window to in-buffer crop window
    int mapleft, maptop, mapwidth, mapheight;
    float wratio = (float)crop.width() / mApa.width();
    float hratio = (float)crop.height() / mApa.height();
    float hoffratio = (float)crop.left() / mApa.width();
    float voffratio = (float)crop.top() / mApa.height();

    mapleft = in->cambuf->width() * hoffratio;
    maptop = in->cambuf->height() * voffratio;
    mapwidth = in->cambuf->width() * wratio;
    mapheight = in->cambuf->height() * hratio;
    // should align to 2
    mapleft &= ~0x1;
    maptop &= ~0x1;
    mapwidth &= ~0x3;
    mapheight &= ~0x3;

    // do digital zoom
    LOGD("%s: crop region(%d,%d,%d,%d) from (%d,%d), infmt %d,%d, outfmt %d,%d",
         __FUNCTION__, mapleft, maptop, mapwidth, mapheight,
         in->cambuf->width(), in->cambuf->height(),
         in->cambuf->format(),
         in->cambuf->v4l2Fmt(),
         out->cambuf->format(),
         out->cambuf->v4l2Fmt());
    // try RGA firstly
    RgaCropScale::Params rgain, rgaout;
    rgain.fd = in->cambuf->dmaBufFd();
    if (in->cambuf->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
        in->cambuf->format() == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
        in->cambuf->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED ||
        in->cambuf->v4l2Fmt() == V4L2_PIX_FMT_NV12)
        rgain.fmt = HAL_PIXEL_FORMAT_YCrCb_NV12;
    else
        rgain.fmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    rgain.vir_addr = (char*)in->cambuf->data();
    rgain.mirror = mirror_handing;
    rgain.width = mapwidth;
    rgain.height = mapheight;
    rgain.offset_x = mapleft;
    rgain.offset_y = maptop;
    rgain.width_stride = in->cambuf->width();
    rgain.height_stride = in->cambuf->height();
    rgaout.fd = out->cambuf->dmaBufFd();
    if (out->cambuf->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
        out->cambuf->format() == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
        out->cambuf->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED ||
        out->cambuf->v4l2Fmt() == V4L2_PIX_FMT_NV12)
        rgaout.fmt = HAL_PIXEL_FORMAT_YCrCb_NV12;
    else
        rgaout.fmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    rgaout.vir_addr = (char*)out->cambuf->data();
    rgaout.mirror = false;
    rgaout.width = out->cambuf->width();
    rgaout.height = out->cambuf->height();
    rgaout.offset_x = 0;
    rgaout.offset_y = 0;
    rgaout.width_stride = out->cambuf->width();
    rgaout.height_stride = out->cambuf->height();

    if (RgaCropScale::CropScaleNV12Or21(&rgain, &rgaout)) {
        LOGW("%s: digital zoom by RGA failed, use arm instead...", __FUNCTION__);
        PERFORMANCE_ATRACE_NAME("SWCropScale");
        ImageScalerCore::cropComposeUpscaleNV12_bl(
                         in->cambuf->data(), in->cambuf->height(), in->cambuf->width(),
                         mapleft, maptop, mapwidth, mapheight,
                         out->cambuf->data(), out->cambuf->height(), out->cambuf->width(),
                         0, 0, out->cambuf->width(), out->cambuf->height());
    }

    return OK;
}

RKISP2PostProcessUnitFec::RKISP2PostProcessUnitFec(
    const char* name, int type, int camid, uint32_t buftype, RKISP2PostProcessPipeline* pl)
    : RKISP2PostProcessUnit(name, type, buftype, pl) {
    mApa = PlatformData::getActivePixelArray(camid);
    mFecUnit = std::make_shared<RKISP2FecUnit>();
}

RKISP2PostProcessUnitFec::~RKISP2PostProcessUnitFec() {
    LOGE(" distortionDeinit *************");
    if (mFecUnit)
        mFecUnit->distortionDeinit();
    mFecUnit.reset();
}

bool RKISP2PostProcessUnitFec::checkFmt(CameraBuffer* in, CameraBuffer* out) {
    if (!in || !out)
        return false;

    // only support NV12 or NV21 now
    bool in_fmt_supported = in->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
                            in->format() == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
                            in->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED ||
                            in->format() == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                            in->v4l2Fmt() == V4L2_PIX_FMT_NV12 ||
                            in->v4l2Fmt() == V4L2_PIX_FMT_NV21;
    bool out_fmt_supported = out->format() == HAL_PIXEL_FORMAT_YCrCb_NV12 ||
                             out->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED ||
                             out->format() == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
                             out->format() == HAL_PIXEL_FORMAT_YCrCb_420_SP;
                             out->v4l2Fmt() == V4L2_PIX_FMT_NV12 ||
                             out->v4l2Fmt() == V4L2_PIX_FMT_NV21;
    return (in_fmt_supported && out_fmt_supported);
}

status_t
RKISP2PostProcessUnitFec::processFrame(const std::shared_ptr<PostProcBuffer>& in,
                              const std::shared_ptr<PostProcBuffer>& out,
                              const std::shared_ptr<RKISP2ProcUnitSettings>& settings) {
    PERFORMANCE_ATRACE_CALL();
    CameraWindow& crop = settings->cropRegion;
    int jpegBufCount = settings->request->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB);

    bool mirror_handing = false;
#ifdef MIRROR_HANDLING_FOR_FRONT_CAMERA
    //for front camera mirror handling
    mirror_handing = PlatformData::facing(mPipeline->getCameraId()) == CAMERA_FACING_FRONT;
#endif
    LOGD("@%s : mirror handleing %d pid=%d", __FUNCTION__, mirror_handing, syscall(SYS_gettid));

    // check if zoom is required
    if (mBufType != kPostProcBufTypeExt &&
        crop.width() ==  mApa.width() && crop.height() == mApa.height()) {
        // HwJpeg encode require buffer width and height align to 16 or large enough.
        // digital zoom out buffer is internal gralloc buffer with size 2xWxH, so it
        // can always meet the Hwjpeg input condition. we use it as a workaround
        // for capture case
        if(jpegBufCount != 0) {
            LOGD("@%s : Use digital zoom out gralloc buffer as hwjpeg input buffer", __FUNCTION__);
        } else if(mirror_handing) {
            LOGD("@%s : use digitalZoom do mirror for front camera", __FUNCTION__);
        } else {
            return STATUS_FORWRAD_TO_NEXT_UNIT;
        }
    }

    if (!checkFmt(in->cambuf.get(), out->cambuf.get())) {
        LOGE("%s: unsupported format, only support NV12 or NV21 now !", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    // map crop window to in-buffer crop window
    int mapleft, maptop, mapwidth, mapheight;
    float wratio = (float)crop.width() / mApa.width();
    float hratio = (float)crop.height() / mApa.height();
    float hoffratio = (float)crop.left() / mApa.width();
    float voffratio = (float)crop.top() / mApa.height();

    mapleft = in->cambuf->width() * hoffratio;
    maptop = in->cambuf->height() * voffratio;
    mapwidth = in->cambuf->width() * wratio;
    mapheight = in->cambuf->height() * hratio;
    // should align to 2
    mapleft &= ~0x1;
    maptop &= ~0x1;
    mapwidth &= ~0x3;
    mapheight &= ~0x3;

    if (mFecUnit) {
        int fencefd = -1;
        mFecUnit->distortionInit(3840, 2160);
        mFecUnit->doFecProcess(mapwidth, mapheight, in->cambuf->dmaBufFd(),
            out->cambuf->width(), out->cambuf->height(), out->cambuf->dmaBufFd(), fencefd);
    }

    return OK;
}

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */
