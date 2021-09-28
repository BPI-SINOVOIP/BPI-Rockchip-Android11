/*
 * Copyright (C) 2018 The Android Open Source Project
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
#define LOG_TAG "ExtFakeCamDevSsn@3.4"
//#define LOG_NDEBUG 0
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>

#include <inttypes.h>

#include "ExternalFakeCameraDeviceSession_3.4.h"

#include "android-base/macros.h"
#include <utils/Timers.h>
#include <utils/Trace.h>
#include <linux/videodev2.h>
#include <sync/sync.h>

#define HAVE_JPEG // required for libyuv.h to export MJPEG decode APIs
#include <libyuv.h>

#include <jpeglib.h>
#include "RgaCropScale.h"
#ifndef RK_GRALLOC_4
#include "ExternalCameraGralloc.h"
#else
#include "ExternalCameraGralloc4.h"
#endif#define NV12_HW_CONVERT
#define PLANES_NUM 1

#define RK_GRALLOC_USAGE_SPECIFY_STRIDE 1ULL << 30

//#define DUMP_YUV

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_4 {
namespace implementation {

namespace {
// Size of request/result metadata fast message queue. Change to 0 to always use hwbinder buffer.
static constexpr size_t kMetadataMsgQueueSize = 1 << 18 /* 256kB */;

const int kBadFramesAfterStreamOn = 4; // drop x frames after streamOn to get rid of some initial
                                       // bad frames. TODO: develop a better bad frame detection
                                       // method
constexpr int MAX_RETRY = 15; // Allow retry some ioctl failures a few times to account for some
                             // webcam showing temporarily ioctl failures.
constexpr int IOCTL_RETRY_SLEEP_US = 33000; // 33ms * MAX_RETRY = 0.5 seconds

// Constants for tryLock during dumpstate
static constexpr int kDumpLockRetries = 50;
static constexpr int kDumpLockSleep = 60000;

bool tryLock(Mutex& mutex)
{
    bool locked = false;
    for (int i = 0; i < kDumpLockRetries; ++i) {
        if (mutex.tryLock() == NO_ERROR) {
            locked = true;
            break;
        }
        usleep(kDumpLockSleep);
    }
    return locked;
}

bool tryLock(std::mutex& mutex)
{
    bool locked = false;
    for (int i = 0; i < kDumpLockRetries; ++i) {
        if (mutex.try_lock()) {
            locked = true;
            break;
        }
        usleep(kDumpLockSleep);
    }
    return locked;
}

} // Anonymous namespace

// Static instances
const int ExternalFakeCameraDeviceSession::kMaxProcessedStream;
const int ExternalFakeCameraDeviceSession::kMaxStallStream;
HandleImporter ExternalFakeCameraDeviceSession::sHandleImporter;

ExternalFakeCameraDeviceSession::ExternalFakeCameraDeviceSession(
        const sp<ICameraDeviceCallback>& callback,
        const ExternalCameraConfig& cfg,
        const std::vector<SupportedV4L2Format>& sortedFormats,
        const CroppingType& croppingType,
        const common::V1_0::helper::CameraMetadata& chars,
        const std::string& cameraId,
        unique_fd v4l2Fd) :
        mCallback(callback),
        mCfg(cfg),
        mCameraCharacteristics(chars),
        mSupportedFormats(sortedFormats),
        mCroppingType(croppingType),
        mCameraId(cameraId),
        mV4l2Fd(std::move(v4l2Fd)),
        mMaxThumbResolution(getMaxThumbResolution()),
        mMaxJpegResolution(getMaxJpegResolution()) {}

void ExternalFakeCameraDeviceSession::createPreviewBuffer() {
    struct bufferinfo_s mGrallocBuf;
    int tempWidth, tempHeight;

    memset(&mGrallocBuf,0,sizeof(struct bufferinfo_s));
    mGrallocBuf.mNumBffers = mCfg.numVideoBuffers;
    tempWidth = (mV4l2StreamingFmt.width + 15) & (~15);
    tempHeight = (mV4l2StreamingFmt.height + 15) & (~15);
    LOGD("alloc buffer W:H=%dx%d", tempWidth, tempHeight);
    mGrallocBuf.mPerBuffersize = PAGE_ALIGN(tempWidth * tempHeight * 2);
    mGrallocBuf.mBufType = PREVIEWBUFFER;
    mGrallocBuf.width = tempWidth;
    mGrallocBuf.height = tempHeight;
    mFormatConvertThread->mCamMemManager = new GrallocDrmMemManager(false);
    if(mFormatConvertThread->mCamMemManager->createPreviewBuffer(&mGrallocBuf)) {
        LOGE("alloc graphic buffer failed !");
    }
}

bool ExternalFakeCameraDeviceSession::initialize() {
    /*if (mV4l2Fd.get() < 0) {
        ALOGE("%s: invalid v4l2 device fd %d!", __FUNCTION__, mV4l2Fd.get());
        return true;
    }

    struct v4l2_capability capability;
    int ret = ioctl(mV4l2Fd.get(), VIDIOC_QUERYCAP, &capability);
    std::string make, model;
    if (ret < 0) {
        ALOGW("%s v4l2 QUERYCAP failed", __FUNCTION__);
        mExifMake = "Generic UVC webcam";
        mExifModel = "Generic UVC webcam";
    } else {
        // capability.card is UTF-8 encoded
        char card[32];
        int j = 0;
        for (int i = 0; i < 32; i++) {
            if (capability.card[i] < 128) {
                card[j++] = capability.card[i];
            }
            if (capability.card[i] == '\0') {
                break;
            }
        }
        if (j == 0 || card[j - 1] != '\0') {
            mExifMake = "Generic UVC webcam";
            mExifModel = "Generic UVC webcam";
        } else {
            mExifMake = card;
            mExifModel = card;
        }
    }*/

    mExifMake = "Generic UVC webcam";
    mExifModel = "Generic UVC webcam";

    initOutputThread();
    if (mOutputThread == nullptr) {
        ALOGE("%s: init OutputThread failed!", __FUNCTION__);
        return true;
    }
    mOutputThread->setExifMakeModel(mExifMake, mExifModel);
    mFormatConvertThread->createJpegDecoder();

    status_t status = initDefaultRequests();
    if (status != OK) {
        ALOGE("%s: init default requests failed!", __FUNCTION__);
        return true;
    }

    mRequestMetadataQueue = std::make_unique<RequestMetadataQueue>(
            kMetadataMsgQueueSize, false /* non blocking */);
    if (!mRequestMetadataQueue->isValid()) {
        ALOGE("%s: invalid request fmq", __FUNCTION__);
        return true;
    }
    mResultMetadataQueue = std::make_shared<ResultMetadataQueue>(
            kMetadataMsgQueueSize, false /* non blocking */);
    if (!mResultMetadataQueue->isValid()) {
        ALOGE("%s: invalid result fmq", __FUNCTION__);
        return true;
    }

    // TODO: check is PRIORITY_DISPLAY enough?
    mOutputThread->run("ExtCamOut", PRIORITY_DISPLAY);
    mFormatConvertThread->run("ExtFmtCvt", PRIORITY_DISPLAY);
    return false;
}

bool ExternalFakeCameraDeviceSession::isInitFailed() {
    Mutex::Autolock _l(mLock);
    if (!mInitialized) {
        mInitFail = initialize();
        mInitialized = true;
    }
    return mInitFail;
}

void ExternalFakeCameraDeviceSession::initOutputThread() {
    mOutputThread = new OutputThread(this, mCroppingType, mCameraCharacteristics);
    mFormatConvertThread = new FormatConvertThread(mOutputThread);
}

void ExternalFakeCameraDeviceSession::closeOutputThread() {
    closeOutputThreadImpl();
}

void ExternalFakeCameraDeviceSession::closeOutputThreadImpl() {
    if (mOutputThread) {
        mOutputThread->flush();
        mOutputThread->requestExit();
        mOutputThread->join();
        mOutputThread.clear();
    }
}

Status ExternalFakeCameraDeviceSession::initStatus() const {
    Mutex::Autolock _l(mLock);
    Status status = Status::OK;
    if (mInitFail || mClosed) {
        ALOGI("%s: sesssion initFailed %d closed %d", __FUNCTION__, mInitFail, mClosed);
        status = Status::INTERNAL_ERROR;
    }
    return status;
}

ExternalFakeCameraDeviceSession::~ExternalFakeCameraDeviceSession() {
    mFormatConvertThread->destroyJpegDecoder();
    if (!isClosed()) {
        ALOGE("ExternalFakeCameraDeviceSession deleted before close!");
        close(/*callerIsDtor*/true);
    }
}


void ExternalFakeCameraDeviceSession::dumpState(const native_handle_t* handle) {
    if (handle->numFds != 1 || handle->numInts != 0) {
        ALOGE("%s: handle must contain 1 FD and 0 integers! Got %d FDs and %d ints",
                __FUNCTION__, handle->numFds, handle->numInts);
        return;
    }
    int fd = handle->data[0];

    bool intfLocked = tryLock(mInterfaceLock);
    if (!intfLocked) {
        dprintf(fd, "!! ExternalFakeCameraDeviceSession interface may be deadlocked !!\n");
    }

    if (isClosed()) {
        dprintf(fd, "External camera %s is closed\n", mCameraId.c_str());
        return;
    }

    bool streaming = false;
    size_t v4L2BufferCount = 0;
    SupportedV4L2Format streamingFmt;
    {
        bool sessionLocked = tryLock(mLock);
        if (!sessionLocked) {
            dprintf(fd, "!! ExternalFakeCameraDeviceSession mLock may be deadlocked !!\n");
        }
        streaming = mV4l2Streaming;
        streamingFmt = mV4l2StreamingFmt;
        v4L2BufferCount = mV4L2BufferCount;

        if (sessionLocked) {
            mLock.unlock();
        }
    }

    std::unordered_set<uint32_t>  inflightFrames;
    {
        bool iffLocked = tryLock(mInflightFramesLock);
        if (!iffLocked) {
            dprintf(fd,
                    "!! ExternalFakeCameraDeviceSession mInflightFramesLock may be deadlocked !!\n");
        }
        inflightFrames = mInflightFrames;
        if (iffLocked) {
            mInflightFramesLock.unlock();
        }
    }

    dprintf(fd, "External camera %s V4L2 FD %d, cropping type %s, %s\n",
            mCameraId.c_str(), mV4l2Fd.get(),
            (mCroppingType == VERTICAL) ? "vertical" : "horizontal",
            streaming ? "streaming" : "not streaming");
    if (streaming) {
        // TODO: dump fps later
        dprintf(fd, "Current V4L2 format %c%c%c%c %dx%d @ %ffps\n",
                streamingFmt.fourcc & 0xFF,
                (streamingFmt.fourcc >> 8) & 0xFF,
                (streamingFmt.fourcc >> 16) & 0xFF,
                (streamingFmt.fourcc >> 24) & 0xFF,
                streamingFmt.width, streamingFmt.height,
                mV4l2StreamingFps);

        size_t numDequeuedV4l2Buffers = 0;
        {
            std::lock_guard<std::mutex> lk(mV4l2BufferLock);
            numDequeuedV4l2Buffers = mNumDequeuedV4l2Buffers;
        }
        dprintf(fd, "V4L2 buffer queue size %zu, dequeued %zu\n",
                v4L2BufferCount, numDequeuedV4l2Buffers);
    }

    dprintf(fd, "In-flight frames (not sorted):");
    for (const auto& frameNumber : inflightFrames) {
        dprintf(fd, "%d, ", frameNumber);
    }
    dprintf(fd, "\n");
    mOutputThread->dump(fd);
    dprintf(fd, "\n");

    if (intfLocked) {
        mInterfaceLock.unlock();
    }

    return;
}

Return<void> ExternalFakeCameraDeviceSession::constructDefaultRequestSettings(
        V3_2::RequestTemplate type,
        V3_2::ICameraDeviceSession::constructDefaultRequestSettings_cb _hidl_cb) {
    V3_2::CameraMetadata outMetadata;
    Status status = constructDefaultRequestSettingsRaw(
            static_cast<RequestTemplate>(type), &outMetadata);
    _hidl_cb(status, outMetadata);
    return Void();
}

Status ExternalFakeCameraDeviceSession::constructDefaultRequestSettingsRaw(RequestTemplate type,
        V3_2::CameraMetadata *outMetadata) {
    CameraMetadata emptyMd;
    Status status = initStatus();
    if (status != Status::OK) {
        return status;
    }

    switch (type) {
        case RequestTemplate::PREVIEW:
        case RequestTemplate::STILL_CAPTURE:
        case RequestTemplate::VIDEO_RECORD:
        case RequestTemplate::VIDEO_SNAPSHOT: {
            *outMetadata = mDefaultRequests[type];
            break;
        }
        case RequestTemplate::MANUAL:
        case RequestTemplate::ZERO_SHUTTER_LAG:
            // Don't support MANUAL, ZSL templates
            status = Status::ILLEGAL_ARGUMENT;
            break;
        default:
            ALOGE("%s: unknown request template type %d", __FUNCTION__, static_cast<int>(type));
            status = Status::ILLEGAL_ARGUMENT;
            break;
    }
    return status;
}

Return<void> ExternalFakeCameraDeviceSession::configureStreams(
        const V3_2::StreamConfiguration& streams,
        ICameraDeviceSession::configureStreams_cb _hidl_cb) {
    V3_2::HalStreamConfiguration outStreams;
    V3_3::HalStreamConfiguration outStreams_v33;
    Mutex::Autolock _il(mInterfaceLock);

    Status status = configureStreams(streams, &outStreams_v33);
    size_t size = outStreams_v33.streams.size();
    outStreams.streams.resize(size);
    for (size_t i = 0; i < size; i++) {
        outStreams.streams[i] = outStreams_v33.streams[i].v3_2;
    }
    _hidl_cb(status, outStreams);
    return Void();
}

Return<void> ExternalFakeCameraDeviceSession::configureStreams_3_3(
        const V3_2::StreamConfiguration& streams,
        ICameraDeviceSession::configureStreams_3_3_cb _hidl_cb) {
    V3_3::HalStreamConfiguration outStreams;
    Mutex::Autolock _il(mInterfaceLock);

    Status status = configureStreams(streams, &outStreams);
    _hidl_cb(status, outStreams);
    return Void();
}

Return<void> ExternalFakeCameraDeviceSession::configureStreams_3_4(
        const V3_4::StreamConfiguration& requestedConfiguration,
        ICameraDeviceSession::configureStreams_3_4_cb _hidl_cb)  {
    V3_2::StreamConfiguration config_v32;
    V3_3::HalStreamConfiguration outStreams_v33;
    V3_4::HalStreamConfiguration outStreams;
    Mutex::Autolock _il(mInterfaceLock);

    config_v32.operationMode = requestedConfiguration.operationMode;
    config_v32.streams.resize(requestedConfiguration.streams.size());
    uint32_t blobBufferSize = 0;
    int numStallStream = 0;
    for (size_t i = 0; i < config_v32.streams.size(); i++) {
        config_v32.streams[i] = requestedConfiguration.streams[i].v3_2;
        if (config_v32.streams[i].format == PixelFormat::BLOB) {
            blobBufferSize = requestedConfiguration.streams[i].bufferSize;
            numStallStream++;
        }
    }

    // Fail early if there are multiple BLOB streams
    if (numStallStream > kMaxStallStream) {
        ALOGE("%s: too many stall streams (expect <= %d, got %d)", __FUNCTION__,
                kMaxStallStream, numStallStream);
        _hidl_cb(Status::ILLEGAL_ARGUMENT, outStreams);
        return Void();
    }

    Status status = configureStreams(config_v32, &outStreams_v33, blobBufferSize);

    outStreams.streams.resize(outStreams_v33.streams.size());
    for (size_t i = 0; i < outStreams.streams.size(); i++) {
        outStreams.streams[i].v3_3 = outStreams_v33.streams[i];
    }
    _hidl_cb(status, outStreams);
    return Void();
}

Return<void> ExternalFakeCameraDeviceSession::getCaptureRequestMetadataQueue(
    ICameraDeviceSession::getCaptureRequestMetadataQueue_cb _hidl_cb) {
    Mutex::Autolock _il(mInterfaceLock);
    _hidl_cb(*mRequestMetadataQueue->getDesc());
    return Void();
}

Return<void> ExternalFakeCameraDeviceSession::getCaptureResultMetadataQueue(
    ICameraDeviceSession::getCaptureResultMetadataQueue_cb _hidl_cb) {
    Mutex::Autolock _il(mInterfaceLock);
    _hidl_cb(*mResultMetadataQueue->getDesc());
    return Void();
}

Return<void> ExternalFakeCameraDeviceSession::processCaptureRequest(
        const hidl_vec<CaptureRequest>& requests,
        const hidl_vec<BufferCache>& cachesToRemove,
        ICameraDeviceSession::processCaptureRequest_cb _hidl_cb) {
    Mutex::Autolock _il(mInterfaceLock);
    updateBufferCaches(cachesToRemove);

    uint32_t numRequestProcessed = 0;
    Status s = Status::OK;
    for (size_t i = 0; i < requests.size(); i++, numRequestProcessed++) {
        s = processOneCaptureRequest(requests[i]);
        if (s != Status::OK) {
            break;
        }
    }

    _hidl_cb(s, numRequestProcessed);
    return Void();
}

Return<void> ExternalFakeCameraDeviceSession::processCaptureRequest_3_4(
        const hidl_vec<V3_4::CaptureRequest>& requests,
        const hidl_vec<V3_2::BufferCache>& cachesToRemove,
        ICameraDeviceSession::processCaptureRequest_3_4_cb _hidl_cb) {
    Mutex::Autolock _il(mInterfaceLock);
    updateBufferCaches(cachesToRemove);

    uint32_t numRequestProcessed = 0;
    Status s = Status::OK;
    for (size_t i = 0; i < requests.size(); i++, numRequestProcessed++) {
        s = processOneCaptureRequest(requests[i].v3_2);
        if (s != Status::OK) {
            break;
        }
    }

    _hidl_cb(s, numRequestProcessed);
    return Void();
}

Return<Status> ExternalFakeCameraDeviceSession::flush() {
    ATRACE_CALL();
    Mutex::Autolock _il(mInterfaceLock);
    Status status = initStatus();
    if (status != Status::OK) {
        return status;
    }
    mOutputThread->flush();
    return Status::OK;
}

Return<void> ExternalFakeCameraDeviceSession::close(bool callerIsDtor) {
    Mutex::Autolock _il(mInterfaceLock);
    bool closed = isClosed();
    if (!closed) {
        if (callerIsDtor) {
            closeOutputThreadImpl();
        } else {
            closeOutputThread();
        }
        //mFormatConvertThread->flush();
        mFormatConvertThread->requestExit();
        mFormatConvertThread->join();

        Mutex::Autolock _l(mLock);
        // free all buffers
        {
            Mutex::Autolock _l(mCbsLock);
            for(auto pair : mStreamMap) {
                cleanupBuffersLocked(/*Stream ID*/pair.first);
            }
        }
        v4l2StreamOffLocked();
        ALOGV("%s: closing V4L2 camera FD %d", __FUNCTION__, mV4l2Fd.get());
        mV4l2Fd.reset();
        mClosed = true;
    }
    return Void();
}

Status ExternalFakeCameraDeviceSession::importRequestLocked(
    const CaptureRequest& request,
    hidl_vec<buffer_handle_t*>& allBufPtrs,
    hidl_vec<int>& allFences) {
    return importRequestLockedImpl(request, allBufPtrs, allFences);
}

Status ExternalFakeCameraDeviceSession::importBuffer(int32_t streamId,
        uint64_t bufId, buffer_handle_t buf,
        /*out*/buffer_handle_t** outBufPtr,
        bool allowEmptyBuf) {
    Mutex::Autolock _l(mCbsLock);
    return importBufferLocked(streamId, bufId, buf, outBufPtr, allowEmptyBuf);
}

Status ExternalFakeCameraDeviceSession::importBufferLocked(int32_t streamId,
        uint64_t bufId, buffer_handle_t buf,
        /*out*/buffer_handle_t** outBufPtr,
        bool allowEmptyBuf) {
    return importBufferImpl(
            mCirculatingBuffers, sHandleImporter, streamId,
            bufId, buf, outBufPtr, allowEmptyBuf);
}

Status ExternalFakeCameraDeviceSession::importRequestLockedImpl(
        const CaptureRequest& request,
        hidl_vec<buffer_handle_t*>& allBufPtrs,
        hidl_vec<int>& allFences,
        bool allowEmptyBuf) {
    size_t numOutputBufs = request.outputBuffers.size();
    size_t numBufs = numOutputBufs;
    // Validate all I/O buffers
    hidl_vec<buffer_handle_t> allBufs;
    hidl_vec<uint64_t> allBufIds;
    allBufs.resize(numBufs);
    allBufIds.resize(numBufs);
    allBufPtrs.resize(numBufs);
    allFences.resize(numBufs);
    std::vector<int32_t> streamIds(numBufs);

    for (size_t i = 0; i < numOutputBufs; i++) {
        allBufs[i] = request.outputBuffers[i].buffer.getNativeHandle();
        allBufIds[i] = request.outputBuffers[i].bufferId;
        allBufPtrs[i] = &allBufs[i];
        streamIds[i] = request.outputBuffers[i].streamId;
    }

    {
        Mutex::Autolock _l(mCbsLock);
        for (size_t i = 0; i < numBufs; i++) {
            Status st = importBufferLocked(
                    streamIds[i], allBufIds[i], allBufs[i], &allBufPtrs[i],
                    allowEmptyBuf);
            if (st != Status::OK) {
                // Detailed error logs printed in importBuffer
                return st;
            }
        }
    }

    // All buffers are imported. Now validate output buffer acquire fences
    for (size_t i = 0; i < numOutputBufs; i++) {
        if (!sHandleImporter.importFence(
                request.outputBuffers[i].acquireFence, allFences[i])) {
            ALOGE("%s: output buffer %zu acquire fence is invalid", __FUNCTION__, i);
            cleanupInflightFences(allFences, i);
            return Status::INTERNAL_ERROR;
        }
    }
    return Status::OK;
}

void ExternalFakeCameraDeviceSession::cleanupInflightFences(
        hidl_vec<int>& allFences, size_t numFences) {
    for (size_t j = 0; j < numFences; j++) {
        sHandleImporter.closeFence(allFences[j]);
    }
}

int ExternalFakeCameraDeviceSession::waitForV4L2BufferReturnLocked(std::unique_lock<std::mutex>& lk) {
    ATRACE_CALL();
    std::chrono::seconds timeout = std::chrono::seconds(kBufferWaitTimeoutSec);
    mLock.unlock();
    auto st = mV4L2BufferReturned.wait_for(lk, timeout);
    // Here we introduce a order where mV4l2BufferLock is acquired before mLock, while
    // the normal lock acquisition order is reversed. This is fine because in most of
    // cases we are protected by mInterfaceLock. The only thread that can cause deadlock
    // is the OutputThread, where we do need to make sure we don't acquire mLock then
    // mV4l2BufferLock
    mLock.lock();
    if (st == std::cv_status::timeout) {
        ALOGE("%s: wait for V4L2 buffer return timeout!", __FUNCTION__);
        return -1;
    }
    return 0;
}

Status ExternalFakeCameraDeviceSession::processOneCaptureRequest(const CaptureRequest& request)  {
    ATRACE_CALL();
    Status status = initStatus();
    if (status != Status::OK) {
        return status;
    }

    if (request.inputBuffer.streamId != -1) {
        ALOGE("%s: external camera does not support reprocessing!", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    Mutex::Autolock _l(mLock);
    if (!mV4l2Streaming) {
        ALOGE("%s: cannot process request in streamOff state!", __FUNCTION__);
        return Status::INTERNAL_ERROR;
    }

    const camera_metadata_t *rawSettings = nullptr;
    bool converted = true;
    CameraMetadata settingsFmq;  // settings from FMQ
    if (request.fmqSettingsSize > 0) {
        // non-blocking read; client must write metadata before calling
        // processOneCaptureRequest
        settingsFmq.resize(request.fmqSettingsSize);
        bool read = mRequestMetadataQueue->read(settingsFmq.data(), request.fmqSettingsSize);
        if (read) {
            converted = V3_2::implementation::convertFromHidl(settingsFmq, &rawSettings);
        } else {
            ALOGE("%s: capture request settings metadata couldn't be read from fmq!", __FUNCTION__);
            converted = false;
        }
    } else {
        converted = V3_2::implementation::convertFromHidl(request.settings, &rawSettings);
    }

    if (converted && rawSettings != nullptr) {
        mLatestReqSetting = rawSettings;
    }

    if (!converted) {
        ALOGE("%s: capture request settings metadata is corrupt!", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    if (mFirstRequest && rawSettings == nullptr) {
        ALOGE("%s: capture request settings must not be null for first request!",
                __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    hidl_vec<buffer_handle_t*> allBufPtrs;
    hidl_vec<int> allFences;
    size_t numOutputBufs = request.outputBuffers.size();

    if (numOutputBufs == 0) {
        ALOGE("%s: capture request must have at least one output buffer!", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    camera_metadata_entry fpsRange = mLatestReqSetting.find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
    if (fpsRange.count == 2) {
        double requestFpsMax = fpsRange.data.i32[1];
        double closestFps = 0.0;
        double fpsError = 1000.0;
        bool fpsSupported = false;
        for (const auto& fr : mV4l2StreamingFmt.frameRates) {
            double f = fr.getDouble();
            if (std::fabs(requestFpsMax - f) < 1.0) {
                fpsSupported = true;
                break;
            }
            if (std::fabs(requestFpsMax - f) < fpsError) {
                fpsError = std::fabs(requestFpsMax - f);
                closestFps = f;
            }
        }
        if (!fpsSupported) {
            /* This can happen in a few scenarios:
             * 1. The application is sending a FPS range not supported by the configured outputs.
             * 2. The application is sending a valid FPS range for all cofigured outputs, but
             *    the selected V4L2 size can only run at slower speed. This should be very rare
             *    though: for this to happen a sensor needs to support at least 3 different aspect
             *    ratio outputs, and when (at least) two outputs are both not the main aspect ratio
             *    of the webcam, a third size that's larger might be picked and runs into this
             *    issue.
             */
            ALOGW("%s: cannot reach fps %d! Will do %f instead",
                    __FUNCTION__, fpsRange.data.i32[1], closestFps);
            requestFpsMax = closestFps;
        }

        if (requestFpsMax != mV4l2StreamingFps) {
            {
                std::unique_lock<std::mutex> lk(mV4l2BufferLock);
                while (mNumDequeuedV4l2Buffers != 0) {
                    // Wait until pipeline is idle before reconfigure stream
                    int waitRet = waitForV4L2BufferReturnLocked(lk);
                    if (waitRet != 0) {
                        ALOGE("%s: wait for pipeline idle failed!", __FUNCTION__);
                        return Status::INTERNAL_ERROR;
                    }
                }
            }
            configureV4l2StreamLocked(mV4l2StreamingFmt, requestFpsMax);
        }
    }

    status = importRequestLocked(request, allBufPtrs, allFences);
    if (status != Status::OK) {
        return status;
    }

    nsecs_t shutterTs = 0;
    sp<YuvFrame> frameIn = dequeueV4l2FrameLocked(&shutterTs);
    if ( frameIn == nullptr) {
        ALOGE("%s: V4L2 deque frame failed!", __FUNCTION__);
        return Status::INTERNAL_ERROR;
    }

    std::shared_ptr<HalRequest> halReq = std::make_shared<HalRequest>();
    halReq->frameNumber = request.frameNumber;
    halReq->setting = mLatestReqSetting;
    halReq->yuvframeIn = frameIn;
    halReq->shutterTs = shutterTs;
    halReq->buffers.resize(numOutputBufs);
    for (size_t i = 0; i < numOutputBufs; i++) {
        HalStreamBuffer& halBuf = halReq->buffers[i];
        int streamId = halBuf.streamId = request.outputBuffers[i].streamId;
        halBuf.bufferId = request.outputBuffers[i].bufferId;
        const Stream& stream = mStreamMap[streamId];
        halBuf.width = stream.width;
        halBuf.height = stream.height;
        halBuf.format = stream.format;
        halBuf.usage = stream.usage;
        halBuf.bufPtr = allBufPtrs[i];
        halBuf.acquireFence = allFences[i];
        halBuf.fenceTimeout = false;
    }
    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        mInflightFrames.insert(halReq->frameNumber);
    }
    // Send request to OutputThread for the rest of processing
    //mOutputThread->submitRequest(halReq);
    mFormatConvertThread->submitRequest(halReq);;
    mFirstRequest = false;
    return Status::OK;
}

void ExternalFakeCameraDeviceSession::notifyShutter(uint32_t frameNumber, nsecs_t shutterTs) {
    NotifyMsg msg;
    msg.type = MsgType::SHUTTER;
    msg.msg.shutter.frameNumber = frameNumber;
    msg.msg.shutter.timestamp = shutterTs;
    mCallback->notify({msg});
}

void ExternalFakeCameraDeviceSession::notifyError(
        uint32_t frameNumber, int32_t streamId, ErrorCode ec) {
    NotifyMsg msg;
    msg.type = MsgType::ERROR;
    msg.msg.error.frameNumber = frameNumber;
    msg.msg.error.errorStreamId = streamId;
    msg.msg.error.errorCode = ec;
    mCallback->notify({msg});
}

//TODO: refactor with processCaptureResult
Status ExternalFakeCameraDeviceSession::processCaptureRequestError(
        const std::shared_ptr<HalRequest>& req,
        /*out*/std::vector<NotifyMsg>* outMsgs,
        /*out*/std::vector<CaptureResult>* outResults) {
    ATRACE_CALL();
    // Return V4L2 buffer to V4L2 buffer queue
    sp<V3_4::implementation::YuvFrame> v4l2Frame =
            static_cast<V3_4::implementation::YuvFrame*>(req->yuvframeIn.get());
    enqueueV4l2Frame(v4l2Frame);

    if (outMsgs == nullptr) {
        notifyShutter(req->frameNumber, req->shutterTs);
        notifyError(/*frameNum*/req->frameNumber, /*stream*/-1, ErrorCode::ERROR_REQUEST);
    } else {
        NotifyMsg shutter;
        shutter.type = MsgType::SHUTTER;
        shutter.msg.shutter.frameNumber = req->frameNumber;
        shutter.msg.shutter.timestamp = req->shutterTs;

        NotifyMsg error;
        error.type = MsgType::ERROR;
        error.msg.error.frameNumber = req->frameNumber;
        error.msg.error.errorStreamId = -1;
        error.msg.error.errorCode = ErrorCode::ERROR_REQUEST;
        outMsgs->push_back(shutter);
        outMsgs->push_back(error);
    }

    // Fill output buffers
    hidl_vec<CaptureResult> results;
    results.resize(1);
    CaptureResult& result = results[0];
    result.frameNumber = req->frameNumber;
    result.partialResult = 1;
    result.inputBuffer.streamId = -1;
    result.outputBuffers.resize(req->buffers.size());
    for (size_t i = 0; i < req->buffers.size(); i++) {
        result.outputBuffers[i].streamId = req->buffers[i].streamId;
        result.outputBuffers[i].bufferId = req->buffers[i].bufferId;
        result.outputBuffers[i].status = BufferStatus::ERROR;
        if (req->buffers[i].acquireFence >= 0) {
            native_handle_t* handle = native_handle_create(/*numFds*/1, /*numInts*/0);
            handle->data[0] = req->buffers[i].acquireFence;
            result.outputBuffers[i].releaseFence.setTo(handle, /*shouldOwn*/false);
        }
    }

    // update inflight records
    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        mInflightFrames.erase(req->frameNumber);
    }

    if (outResults == nullptr) {
        // Callback into framework
        invokeProcessCaptureResultCallback(results, /* tryWriteFmq */true);
        freeReleaseFences(results);
    } else {
        outResults->push_back(result);
    }
    return Status::OK;
}

Status ExternalFakeCameraDeviceSession::processCaptureResult(std::shared_ptr<HalRequest>& req) {
    ATRACE_CALL();
    // Return V4L2 buffer to V4L2 buffer queue
    sp<V3_4::implementation::YuvFrame> v4l2Frame =
            static_cast<V3_4::implementation::YuvFrame*>(req->yuvframeIn.get());
    enqueueV4l2Frame(v4l2Frame);

    // NotifyShutter
    notifyShutter(req->frameNumber, req->shutterTs);

    // Fill output buffers
    hidl_vec<CaptureResult> results;
    results.resize(1);
    CaptureResult& result = results[0];
    result.frameNumber = req->frameNumber;
    result.partialResult = 1;
    result.inputBuffer.streamId = -1;
    result.outputBuffers.resize(req->buffers.size());
    for (size_t i = 0; i < req->buffers.size(); i++) {
        result.outputBuffers[i].streamId = req->buffers[i].streamId;
        result.outputBuffers[i].bufferId = req->buffers[i].bufferId;
        if (req->buffers[i].fenceTimeout) {
            result.outputBuffers[i].status = BufferStatus::ERROR;
            if (req->buffers[i].acquireFence >= 0) {
                native_handle_t* handle = native_handle_create(/*numFds*/1, /*numInts*/0);
                handle->data[0] = req->buffers[i].acquireFence;
                result.outputBuffers[i].releaseFence.setTo(handle, /*shouldOwn*/false);
            }
            notifyError(req->frameNumber, req->buffers[i].streamId, ErrorCode::ERROR_BUFFER);
        } else {
            result.outputBuffers[i].status = BufferStatus::OK;
            // TODO: refactor
            if (req->buffers[i].acquireFence >= 0) {
                native_handle_t* handle = native_handle_create(/*numFds*/1, /*numInts*/0);
                handle->data[0] = req->buffers[i].acquireFence;
                result.outputBuffers[i].releaseFence.setTo(handle, /*shouldOwn*/false);
            }
        }
    }

    // Fill capture result metadata
    fillCaptureResult(req->setting, req->shutterTs);
    const camera_metadata_t *rawResult = req->setting.getAndLock();
    V3_2::implementation::convertToHidl(rawResult, &result.result);
    req->setting.unlock(rawResult);

    // update inflight records
    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        mInflightFrames.erase(req->frameNumber);
    }

    // Callback into framework
    invokeProcessCaptureResultCallback(results, /* tryWriteFmq */true);
    freeReleaseFences(results);
    return Status::OK;
}

void ExternalFakeCameraDeviceSession::invokeProcessCaptureResultCallback(
        hidl_vec<CaptureResult> &results, bool tryWriteFmq) {
    if (mProcessCaptureResultLock.tryLock() != OK) {
        const nsecs_t NS_TO_SECOND = 1000000000;
        ALOGV("%s: previous call is not finished! waiting 1s...", __FUNCTION__);
        if (mProcessCaptureResultLock.timedLock(/* 1s */NS_TO_SECOND) != OK) {
            ALOGE("%s: cannot acquire lock in 1s, cannot proceed",
                    __FUNCTION__);
            return;
        }
    }
    if (tryWriteFmq && mResultMetadataQueue->availableToWrite() > 0) {
        for (CaptureResult &result : results) {
            if (result.result.size() > 0) {
                if (mResultMetadataQueue->write(result.result.data(), result.result.size())) {
                    result.fmqResultSize = result.result.size();
                    result.result.resize(0);
                } else {
                    ALOGW("%s: couldn't utilize fmq, fall back to hwbinder", __FUNCTION__);
                    result.fmqResultSize = 0;
                }
            } else {
                result.fmqResultSize = 0;
            }
        }
    }
    auto status = mCallback->processCaptureResult(results);
    if (!status.isOk()) {
        ALOGE("%s: processCaptureResult ERROR : %s", __FUNCTION__,
              status.description().c_str());
    }

    mProcessCaptureResultLock.unlock();
}

extern "C" void debugShowFakeCameraFPS() {
    static int mFrameCount = 0;
    static int mLastFrameCount = 0;
    static nsecs_t mLastFpsTime = 0;
    static float mFps = 0;
    mFrameCount++;
    if (!(mFrameCount & 0x1F)) {
        nsecs_t now = systemTime();
        nsecs_t diff = now - mLastFpsTime;
        mFps = ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
        LOGD("Camera %d Frames, %2.3f FPS", mFrameCount, mFps);
    }
}
ExternalFakeCameraDeviceSession::FormatConvertThread::FormatConvertThread(
        sp<OutputThread>& mOutputThread) {
    //memset(&mHWJpegDecoder, 0, sizeof(MpiJpegDecoder));
    //memset(&mHWDecoderFrameOut, 0, sizeof(MpiJpegDecoder::OutputFrame_t));
    mFmtOutputThread  = mOutputThread;
}

ExternalFakeCameraDeviceSession::FormatConvertThread::~FormatConvertThread() {}

void ExternalFakeCameraDeviceSession::FormatConvertThread::createJpegDecoder() {
    int ret = mHWJpegDecoder.prepareDecoder();
    if (!ret) {
        ALOGE("failed to prepare JPEG decoder");
        mHWJpegDecoder.flushBuffer();
    }
    memset(&mHWDecoderFrameOut, 0, sizeof(MpiJpegDecoder::OutputFrame_t));
}

void ExternalFakeCameraDeviceSession::FormatConvertThread::destroyJpegDecoder() {
    //mHWJpegDecoder.deinitOutputFrame(&mHWDecoderFrameOut);
    mHWJpegDecoder.flushBuffer();
}

int ExternalFakeCameraDeviceSession::FormatConvertThread::jpegDecoder(
        unsigned int mShareFd, uint8_t* inData, size_t inDataSize) {
    int ret = 0;
    unsigned int output_len = 0;
    unsigned int input_len = inDataSize;
    char *srcbuf = (char*)inData;

    mHWJpegDecoder.deinitOutputFrame(&mHWDecoderFrameOut);
    if (input_len <= 0) {
        LOGE("frame size is invalid !");
        return -1;
    }
    mHWDecoderFrameOut.outputPhyAddr = mShareFd;
    if ((srcbuf[0] == 0xff) && (srcbuf[1] == 0xd8) && (srcbuf[2] == 0xff)) {
        // decoder to NV12
        ret = mHWJpegDecoder.decodePacket((char*)inData, inDataSize, &mHWDecoderFrameOut);
        if (!ret) {
            ALOGE("mjpeg decodePacket failed!");
            mHWJpegDecoder.flushBuffer();
        }
    } else {
        LOGE("mjpeg data error!!");
        return -1;
    }

    return ret;
}

void ExternalFakeCameraDeviceSession::FormatConvertThread:: yuyvToNv12(
            int v4l2_fmt_dst, char *srcbuf, char *dstbuf,
            int src_w, int src_h,int dst_w, int dst_h) {
    int *dstint_y, *dstint_uv, *srcint, y_size, i;

    y_size = src_w * src_h;
    if (v4l2_fmt_dst == V4L2_PIX_FMT_NV12) {
        if ((src_w == dst_w) && (src_h == dst_h)) {
            dstint_y = (int*)dstbuf;
            srcint = (int*)srcbuf;
            dstint_uv =  (int*)(dstbuf + y_size);
#if defined(__arm64__) || defined(__aarch64__)
            for (i = 0; i < src_h; i++) {
                for (int j = 0; j < (src_w >> 2); j++) {
                    if (i % 2 == 0) {
                        *dstint_uv++ = (*(srcint+1) & 0xff000000) |
                                    ((*(srcint+1) & 0x0000ff00) << 8) |
                                    ((*srcint & 0xff000000) >> 16) |
                                    ((*srcint & 0x0000ff00) >> 8);
                    }
                    *dstint_y++ = ((*(srcint+1) & 0x00ff0000) << 8) |
                                    ((*(srcint+1) & 0x000000ff) << 16) |
                                    ((*srcint & 0x00ff0000) >> 8) |
                                    (*srcint & 0x000000ff);
                    srcint += 2;
                }
            }
#else
            for (i = 0; i < src_h; i++) {
                int n = src_w;
                char tmp = i % 2; // get uv only when in even row
                asm volatile (
                    "   pld [%[src], %[src_stride], lsl #2]                 \n\t"
                    "   cmp %[n], #16                                       \n\t"
                    "   blt 5f                                              \n\t"
                    "0: @ 16 pixel swap                                     \n\t"
                    "   vld2.8  {q0,q1} , [%[src]]!  @ q0 = y q1 = uv       \n\t"
                    "   vst1.16 {q0},[%[dst_y]]!     @ now q0  -> dst       \n\t"
                    "   cmp %[tmp], #1                                      \n\t"
                    "   bge 1f                                              \n\t"
                    "   vst1.16 {q1},[%[dst_uv]]!    @ now q1  -> dst       \n\t"
                    "1: @ don't need get uv in odd row                      \n\t"
                    "   sub %[n], %[n], #16                                 \n\t"
                    "   cmp %[n], #16                                       \n\t"
                    "   bge 0b                                              \n\t"
                    "5: @ end                                               \n\t"
                    : [dst_y] "+r" (dstint_y), [dst_uv] "+r" (dstint_uv),
                      [src] "+r" (srcint), [n] "+r" (n),[tmp] "+r" (tmp)
                    : [src_stride] "r" (src_w)
                    : "cc", "memory", "q0", "q1", "q2"
                );
            }
#endif
        }
    } else {
        LOGE("don't support this format !");
    }
}

bool ExternalFakeCameraDeviceSession::FormatConvertThread::threadLoop() {
    std::shared_ptr<HalRequest> req;
    uint8_t* inData;
    size_t inDataSize;
    unsigned long mVirAddr;
    unsigned long mShareFd;

    waitForNextRequest(&req);
    if (req == nullptr) {
        // No new request, wait again
        return true;
    }
    if (req->yuvframeIn->mFourcc != V4L2_PIX_FMT_MJPEG &&
            req->yuvframeIn->mFourcc != V4L2_PIX_FMT_Z16 &&
            req->yuvframeIn->mFourcc != V4L2_PIX_FMT_YUYV &&
            req->yuvframeIn->mFourcc != V4L2_PIX_FMT_NV12) {

         LOGD("do not support V4L2 format %c%c%c%c",
                req->yuvframeIn->mFourcc & 0xFF,
                (req->yuvframeIn->mFourcc >> 8) & 0xFF,
                (req->yuvframeIn->mFourcc >> 16) & 0xFF,
                (req->yuvframeIn->mFourcc >> 24) & 0xFF);
         return true;
    }
    debugShowFakeCameraFPS();
    if (req->yuvframeIn->getData(&inData, &inDataSize) != 0) {
         LOGE("%s(%d)getData failed!\n", __FUNCTION__, __LINE__);
    }

    mShareFd = mCamMemManager->getBufferAddr(
            PREVIEWBUFFER, req->yuvframeIn->mBufferIndex, buffer_sharre_fd);
    mVirAddr = mCamMemManager->getBufferAddr(
            PREVIEWBUFFER, req->yuvframeIn->mBufferIndex, buffer_addr_vir);

    ALOGV("%s(%d)mShareFd(%d) mVirAddr(%p)!\n", __FUNCTION__, __LINE__, mShareFd, mVirAddr);

    int tmpW = req->yuvframeIn->mWidth;
    int tmpH = req->yuvframeIn->mHeight;
    if (req->yuvframeIn->mFourcc == V4L2_PIX_FMT_MJPEG) {
#ifdef RK_HW_JPEG_DECODER
        int ret = jpegDecoder(mShareFd, inData, inDataSize);
        if(!ret) {
            LOGE("mjpeg decode failed");
            mFmtOutputThread->submitRequest(req);
            return true;
        }
#ifdef DUMP_YUV
        {
            int frameCount = req->frameNumber;
            if(frameCount > 5 && frameCount<10){
                FILE* fp =NULL;
                char filename[128];
                filename[0] = 0x00;
                sprintf(filename, "/data/camera/camera_dump_hwjpeg_%dx%d_%d.yuv",
                        tmpW, tmpH, frameCount);
                fp = fopen(filename, "wb+");
                if (fp != NULL) {
                    fwrite((char*)mVirAddr,1,tmpW*tmpH*1.5,fp);
                    fclose(fp);
                    ALOGI("Write success YUV data to %s",filename);
                } else {
                    ALOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
                }
            }
        }
#endif

#endif
        req->mShareFd = mShareFd;
        req->mVirAddr = mVirAddr;

    } else if (req->yuvframeIn->mFourcc == V4L2_PIX_FMT_YUYV) {
        //yuyvToNv12(V4L2_PIX_FMT_NV12, (char*)inData,
        //        (char*)mVirAddr, tmpW, tmpH, tmpW, tmpH);
        //mShareFd = mVirAddr; // YUYV:rga use vir addr
        //req->mShareFd = reinterpret_cast<unsigned long>(inData);
    }

    req->inData = inData;
    req->inDataSize = inDataSize;
    mFmtOutputThread->submitRequest(req);

    return true;
}

Status ExternalFakeCameraDeviceSession::FormatConvertThread::submitRequest(
        const std::shared_ptr<HalRequest>& req) {
    std::unique_lock<std::mutex> lk(mRequestListLock);
    mRequestList.push_back(req);
    lk.unlock();
    mRequestCond.notify_one();
    return Status::OK;
}

void ExternalFakeCameraDeviceSession::FormatConvertThread::waitForNextRequest(
        std::shared_ptr<HalRequest>* out) {
    ATRACE_CALL();
    if (out == nullptr) {
        ALOGE("%s: out is null", __FUNCTION__);
        return;
    }
    std::unique_lock<std::mutex> lk(mRequestListLock);
    int waitTimes = 0;
    while (mRequestList.empty()) {
        if (exitPending()) {
            return;
        }
        std::chrono::milliseconds timeout = std::chrono::milliseconds(kReqWaitTimeoutMs);
        auto st = mRequestCond.wait_for(lk, timeout);
        if (st == std::cv_status::timeout) {
            waitTimes++;
            if (waitTimes == kReqWaitTimesMax) {
                // no new request, return
                return;
            }
        }
    }
    *out = mRequestList.front();
    mRequestList.pop_front();
}
ExternalFakeCameraDeviceSession::OutputThread::OutputThread(
        wp<OutputThreadInterface> parent, CroppingType ct,
        const common::V1_0::helper::CameraMetadata& chars) :
        mParent(parent), mCroppingType(ct), mCameraCharacteristics(chars) {}

ExternalFakeCameraDeviceSession::OutputThread::~OutputThread() {}

void ExternalFakeCameraDeviceSession::OutputThread::setExifMakeModel(
        const std::string& make, const std::string& model) {
    mExifMake = make;
    mExifModel = model;
}

int ExternalFakeCameraDeviceSession::OutputThread::cropAndScaleLocked(
        sp<AllocatedFrame>& in, const Size& outSz, YCbCrLayout* out) {
    Size inSz = {in->mWidth, in->mHeight};

    int ret;
    if (inSz == outSz) {
        ret = in->getLayout(out);
        if (ret != 0) {
            ALOGE("%s: failed to get input image layout", __FUNCTION__);
            return ret;
        }
        return ret;
    }

    // Cropping to output aspect ratio
    IMapper::Rect inputCrop;
    ret = getCropRect(mCroppingType, inSz, outSz, &inputCrop);
    if (ret != 0) {
        ALOGE("%s: failed to compute crop rect for output size %dx%d",
                __FUNCTION__, outSz.width, outSz.height);
        return ret;
    }

    YCbCrLayout croppedLayout;
    ret = in->getCroppedLayout(inputCrop, &croppedLayout);
    if (ret != 0) {
        ALOGE("%s: failed to crop input image %dx%d to output size %dx%d",
                __FUNCTION__, inSz.width, inSz.height, outSz.width, outSz.height);
        return ret;
    }

    if ((mCroppingType == VERTICAL && inSz.width == outSz.width) ||
            (mCroppingType == HORIZONTAL && inSz.height == outSz.height)) {
        // No scale is needed
        *out = croppedLayout;
        return 0;
    }

    auto it = mScaledYu12Frames.find(outSz);
    sp<AllocatedFrame> scaledYu12Buf;
    if (it != mScaledYu12Frames.end()) {
        scaledYu12Buf = it->second;
    } else {
        it = mIntermediateBuffers.find(outSz);
        if (it == mIntermediateBuffers.end()) {
            ALOGE("%s: failed to find intermediate buffer size %dx%d",
                    __FUNCTION__, outSz.width, outSz.height);
            return -1;
        }
        scaledYu12Buf = it->second;
    }
    // Scale
    YCbCrLayout outLayout;
    ret = scaledYu12Buf->getLayout(&outLayout);
    if (ret != 0) {
        ALOGE("%s: failed to get output buffer layout", __FUNCTION__);
        return ret;
    }

    ret = libyuv::I420Scale(
            static_cast<uint8_t*>(croppedLayout.y),
            croppedLayout.yStride,
            static_cast<uint8_t*>(croppedLayout.cb),
            croppedLayout.cStride,
            static_cast<uint8_t*>(croppedLayout.cr),
            croppedLayout.cStride,
            inputCrop.width,
            inputCrop.height,
            static_cast<uint8_t*>(outLayout.y),
            outLayout.yStride,
            static_cast<uint8_t*>(outLayout.cb),
            outLayout.cStride,
            static_cast<uint8_t*>(outLayout.cr),
            outLayout.cStride,
            outSz.width,
            outSz.height,
            // TODO: b/72261744 see if we can use better filter without losing too much perf
            libyuv::FilterMode::kFilterNone);

    if (ret != 0) {
        ALOGE("%s: failed to scale buffer from %dx%d to %dx%d. Ret %d",
                __FUNCTION__, inputCrop.width, inputCrop.height,
                outSz.width, outSz.height, ret);
        return ret;
    }

    *out = outLayout;
    mScaledYu12Frames.insert({outSz, scaledYu12Buf});
    return 0;
}


int ExternalFakeCameraDeviceSession::OutputThread::cropAndScaleThumbLocked(
        sp<AllocatedFrame>& in, const Size &outSz, YCbCrLayout* out) {
    Size inSz  {in->mWidth, in->mHeight};

    if ((outSz.width * outSz.height) >
        (mYu12ThumbFrame->mWidth * mYu12ThumbFrame->mHeight)) {
        ALOGE("%s: Requested thumbnail size too big (%d,%d) > (%d,%d)",
              __FUNCTION__, outSz.width, outSz.height,
              mYu12ThumbFrame->mWidth, mYu12ThumbFrame->mHeight);
        return -1;
    }

    int ret;

    /* This will crop-and-zoom the input YUV frame to the thumbnail size
     * Based on the following logic:
     *  1) Square pixels come in, square pixels come out, therefore single
     *  scale factor is computed to either make input bigger or smaller
     *  depending on if we are upscaling or downscaling
     *  2) That single scale factor would either make height too tall or width
     *  too wide so we need to crop the input either horizontally or vertically
     *  but not both
     */

    /* Convert the input and output dimensions into floats for ease of math */
    float fWin = static_cast<float>(inSz.width);
    float fHin = static_cast<float>(inSz.height);
    float fWout = static_cast<float>(outSz.width);
    float fHout = static_cast<float>(outSz.height);

    /* Compute the one scale factor from (1) above, it will be the smaller of
     * the two possibilities. */
    float scaleFactor = std::min( fHin / fHout, fWin / fWout );

    /* Since we are crop-and-zooming (as opposed to letter/pillar boxing) we can
     * simply multiply the output by our scaleFactor to get the cropped input
     * size. Note that at least one of {fWcrop, fHcrop} is going to wind up
     * being {fWin, fHin} respectively because fHout or fWout cancels out the
     * scaleFactor calculation above.
     *
     * Specifically:
     *  if ( fHin / fHout ) < ( fWin / fWout ) we crop the sides off
     * input, in which case
     *    scaleFactor = fHin / fHout
     *    fWcrop = fHin / fHout * fWout
     *    fHcrop = fHin
     *
     * Note that fWcrop <= fWin ( because ( fHin / fHout ) * fWout < fWin, which
     * is just the inequality above with both sides multiplied by fWout
     *
     * on the other hand if ( fWin / fWout ) < ( fHin / fHout) we crop the top
     * and the bottom off of input, and
     *    scaleFactor = fWin / fWout
     *    fWcrop = fWin
     *    fHCrop = fWin / fWout * fHout
     */
    float fWcrop = scaleFactor * fWout;
    float fHcrop = scaleFactor * fHout;

    /* Convert to integer and truncate to an even number */
    Size cropSz = { 2*static_cast<uint32_t>(fWcrop/2.0f),
                    2*static_cast<uint32_t>(fHcrop/2.0f) };

    /* Convert to a centered rectange with even top/left */
    IMapper::Rect inputCrop {
        2*static_cast<int32_t>((inSz.width - cropSz.width)/4),
        2*static_cast<int32_t>((inSz.height - cropSz.height)/4),
        static_cast<int32_t>(cropSz.width),
        static_cast<int32_t>(cropSz.height) };

    if ((inputCrop.top < 0) ||
        (inputCrop.top >= static_cast<int32_t>(inSz.height)) ||
        (inputCrop.left < 0) ||
        (inputCrop.left >= static_cast<int32_t>(inSz.width)) ||
        (inputCrop.width <= 0) ||
        (inputCrop.width + inputCrop.left > static_cast<int32_t>(inSz.width)) ||
        (inputCrop.height <= 0) ||
        (inputCrop.height + inputCrop.top > static_cast<int32_t>(inSz.height)))
    {
        ALOGE("%s: came up with really wrong crop rectangle",__FUNCTION__);
        ALOGE("%s: input layout %dx%d to for output size %dx%d",
             __FUNCTION__, inSz.width, inSz.height, outSz.width, outSz.height);
        ALOGE("%s: computed input crop +%d,+%d %dx%d",
             __FUNCTION__, inputCrop.left, inputCrop.top,
             inputCrop.width, inputCrop.height);
        return -1;
    }

    YCbCrLayout inputLayout;
    ret = in->getCroppedLayout(inputCrop, &inputLayout);
    if (ret != 0) {
        ALOGE("%s: failed to crop input layout %dx%d to for output size %dx%d",
             __FUNCTION__, inSz.width, inSz.height, outSz.width, outSz.height);
        ALOGE("%s: computed input crop +%d,+%d %dx%d",
             __FUNCTION__, inputCrop.left, inputCrop.top,
             inputCrop.width, inputCrop.height);
        return ret;
    }
    ALOGV("%s: crop input layout %dx%d to for output size %dx%d",
          __FUNCTION__, inSz.width, inSz.height, outSz.width, outSz.height);
    ALOGV("%s: computed input crop +%d,+%d %dx%d",
          __FUNCTION__, inputCrop.left, inputCrop.top,
          inputCrop.width, inputCrop.height);


    // Scale
    YCbCrLayout outFullLayout;

    ret = mYu12ThumbFrame->getLayout(&outFullLayout);
    if (ret != 0) {
        ALOGE("%s: failed to get output buffer layout", __FUNCTION__);
        return ret;
    }


    ret = libyuv::I420Scale(
            static_cast<uint8_t*>(inputLayout.y),
            inputLayout.yStride,
            static_cast<uint8_t*>(inputLayout.cb),
            inputLayout.cStride,
            static_cast<uint8_t*>(inputLayout.cr),
            inputLayout.cStride,
            inputCrop.width,
            inputCrop.height,
            static_cast<uint8_t*>(outFullLayout.y),
            outFullLayout.yStride,
            static_cast<uint8_t*>(outFullLayout.cb),
            outFullLayout.cStride,
            static_cast<uint8_t*>(outFullLayout.cr),
            outFullLayout.cStride,
            outSz.width,
            outSz.height,
            libyuv::FilterMode::kFilterNone);

    if (ret != 0) {
        ALOGE("%s: failed to scale buffer from %dx%d to %dx%d. Ret %d",
                __FUNCTION__, inputCrop.width, inputCrop.height,
                outSz.width, outSz.height, ret);
        return ret;
    }

    *out = outFullLayout;
    return 0;
}

/*
 * TODO: There needs to be a mechanism to discover allocated buffer size
 * in the HAL.
 *
 * This is very fragile because it is duplicated computation from:
 * frameworks/av/services/camera/libcameraservice/device3/Camera3Device.cpp
 *
 */

/* This assumes mSupportedFormats have all been declared as supporting
 * HAL_PIXEL_FORMAT_BLOB to the framework */
Size ExternalFakeCameraDeviceSession::getMaxJpegResolution() const {
    Size ret { 0, 0 };
    for(auto & fmt : mSupportedFormats) {
        if(fmt.width * fmt.height > ret.width * ret.height) {
            ret = Size { fmt.width, fmt.height };
        }
    }
    return ret;
}

Size ExternalFakeCameraDeviceSession::getMaxThumbResolution() const {
    return getMaxThumbnailResolution(mCameraCharacteristics);
}

ssize_t ExternalFakeCameraDeviceSession::getJpegBufferSize(
        uint32_t width, uint32_t height) const {
    // Constant from camera3.h
    const ssize_t kMinJpegBufferSize = 256 * 1024 + sizeof(CameraBlob);
    // Get max jpeg size (area-wise).
    if (mMaxJpegResolution.width == 0) {
        ALOGE("%s: Do not have a single supported JPEG stream",
                __FUNCTION__);
        return BAD_VALUE;
    }

    // Get max jpeg buffer size
    ssize_t maxJpegBufferSize = 0;
    camera_metadata_ro_entry jpegBufMaxSize =
            mCameraCharacteristics.find(ANDROID_JPEG_MAX_SIZE);
    if (jpegBufMaxSize.count == 0) {
        ALOGE("%s: Can't find maximum JPEG size in static metadata!",
              __FUNCTION__);
        return BAD_VALUE;
    }
    maxJpegBufferSize = jpegBufMaxSize.data.i32[0];

    if (maxJpegBufferSize <= kMinJpegBufferSize) {
        ALOGE("%s: ANDROID_JPEG_MAX_SIZE (%zd) <= kMinJpegBufferSize (%zd)",
              __FUNCTION__, maxJpegBufferSize, kMinJpegBufferSize);
        return BAD_VALUE;
    }

    // Calculate final jpeg buffer size for the given resolution.
    float scaleFactor = ((float) (width * height)) /
            (mMaxJpegResolution.width * mMaxJpegResolution.height);
    ssize_t jpegBufferSize = scaleFactor * (maxJpegBufferSize - kMinJpegBufferSize) +
            kMinJpegBufferSize;
    if (jpegBufferSize > maxJpegBufferSize) {
        jpegBufferSize = maxJpegBufferSize;
    }

    return jpegBufferSize;
}

int ExternalFakeCameraDeviceSession::OutputThread::createJpegLocked(
        HalStreamBuffer &halBuf,
        const common::V1_0::helper::CameraMetadata& setting)
{
    ATRACE_CALL();
    int ret;
    auto lfail = [&](auto... args) {
        ALOGE(args...);

        return 1;
    };
    auto parent = mParent.promote();
    if (parent == nullptr) {
       ALOGE("%s: session has been disconnected!", __FUNCTION__);
       return 1;
    }

    ALOGV("%s: HAL buffer sid: %d bid: %" PRIu64 " w: %u h: %u",
          __FUNCTION__, halBuf.streamId, static_cast<uint64_t>(halBuf.bufferId),
          halBuf.width, halBuf.height);
    ALOGV("%s: HAL buffer fmt: %x usage: %" PRIx64 " ptr: %p",
          __FUNCTION__, halBuf.format, static_cast<uint64_t>(halBuf.usage),
          halBuf.bufPtr);
    ALOGV("%s: YV12 buffer %d x %d",
          __FUNCTION__,
          mYu12Frame->mWidth, mYu12Frame->mHeight);

    int jpegQuality, thumbQuality;
    Size thumbSize;
    bool outputThumbnail = true;

    if (setting.exists(ANDROID_JPEG_QUALITY)) {
        camera_metadata_ro_entry entry =
            setting.find(ANDROID_JPEG_QUALITY);
        jpegQuality = entry.data.u8[0];
    } else {
        return lfail("%s: ANDROID_JPEG_QUALITY not set",__FUNCTION__);
    }

    if (setting.exists(ANDROID_JPEG_THUMBNAIL_QUALITY)) {
        camera_metadata_ro_entry entry =
            setting.find(ANDROID_JPEG_THUMBNAIL_QUALITY);
        thumbQuality = entry.data.u8[0];
    } else {
        return lfail(
            "%s: ANDROID_JPEG_THUMBNAIL_QUALITY not set",
            __FUNCTION__);
    }

    if (setting.exists(ANDROID_JPEG_THUMBNAIL_SIZE)) {
        camera_metadata_ro_entry entry =
            setting.find(ANDROID_JPEG_THUMBNAIL_SIZE);
        thumbSize = Size { static_cast<uint32_t>(entry.data.i32[0]),
                           static_cast<uint32_t>(entry.data.i32[1])
        };
        if (thumbSize.width == 0 && thumbSize.height == 0) {
            outputThumbnail = false;
        }
    } else {
        return lfail(
            "%s: ANDROID_JPEG_THUMBNAIL_SIZE not set", __FUNCTION__);
    }

    /* Cropped and scaled YU12 buffer for main and thumbnail */
    YCbCrLayout yu12Main;
    Size jpegSize { halBuf.width, halBuf.height };

    /* Compute temporary buffer sizes accounting for the following:
     * thumbnail can't exceed APP1 size of 64K
     * main image needs to hold APP1, headers, and at most a poorly
     * compressed image */
    const ssize_t maxThumbCodeSize = 64 * 1024;
    const ssize_t maxJpegCodeSize = mBlobBufferSize == 0 ?
            parent->getJpegBufferSize(jpegSize.width, jpegSize.height) :
            mBlobBufferSize;

    /* Check that getJpegBufferSize did not return an error */
    if (maxJpegCodeSize < 0) {
        return lfail(
            "%s: getJpegBufferSize returned %zd",__FUNCTION__,maxJpegCodeSize);
    }


    /* Hold actual thumbnail and main image code sizes */
    size_t thumbCodeSize = 0, jpegCodeSize = 0;
    /* Temporary thumbnail code buffer */
    std::vector<uint8_t> thumbCode(outputThumbnail ? maxThumbCodeSize : 0);

    YCbCrLayout yu12Thumb;
    if (outputThumbnail) {
        ret = cropAndScaleThumbLocked(mYu12Frame, thumbSize, &yu12Thumb);

        if (ret != 0) {
            return lfail(
                "%s: crop and scale thumbnail failed!", __FUNCTION__);
        }
    }

    /* Scale and crop main jpeg */
    ret = cropAndScaleLocked(mYu12Frame, jpegSize, &yu12Main);

    if (ret != 0) {
        return lfail("%s: crop and scale main failed!", __FUNCTION__);
    }

    /* Encode the thumbnail image */
    if (outputThumbnail) {
        ret = encodeJpegYU12(thumbSize, yu12Thumb,
                thumbQuality, 0, 0,
                &thumbCode[0], maxThumbCodeSize, thumbCodeSize);

        if (ret != 0) {
            return lfail("%s: thumbnail encodeJpegYU12 failed with %d",__FUNCTION__, ret);
        }
    }

    /* Combine camera characteristics with request settings to form EXIF
     * metadata */
    common::V1_0::helper::CameraMetadata meta(mCameraCharacteristics);
    meta.append(setting);

    /* Generate EXIF object */
    std::unique_ptr<ExifUtils> utils(ExifUtils::create());
    /* Make sure it's initialized */
    utils->initialize();

    utils->setFromMetadata(meta, jpegSize.width, jpegSize.height);
    utils->setMake(mExifMake);
    utils->setModel(mExifModel);

    ret = utils->generateApp1(outputThumbnail ? &thumbCode[0] : 0, thumbCodeSize);

    if (!ret) {
        return lfail("%s: generating APP1 failed", __FUNCTION__);
    }

    /* Get internal buffer */
    size_t exifDataSize = utils->getApp1Length();
    const uint8_t* exifData = utils->getApp1Buffer();

    /* Lock the HAL jpeg code buffer */
    void *bufPtr = sHandleImporter.lock(
            *(halBuf.bufPtr), halBuf.usage, maxJpegCodeSize);

    if (!bufPtr) {
        return lfail("%s: could not lock %zu bytes", __FUNCTION__, maxJpegCodeSize);
    }

    /* Encode the main jpeg image */
    ret = encodeJpegYU12(jpegSize, yu12Main,
            jpegQuality, exifData, exifDataSize,
            bufPtr, maxJpegCodeSize, jpegCodeSize);

    /* TODO: Not sure this belongs here, maybe better to pass jpegCodeSize out
     * and do this when returning buffer to parent */
    CameraBlob blob { CameraBlobId::JPEG, static_cast<uint32_t>(jpegCodeSize) };
    void *blobDst =
        reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(bufPtr) +
                           maxJpegCodeSize -
                           sizeof(CameraBlob));
    memcpy(blobDst, &blob, sizeof(CameraBlob));

    /* Unlock the HAL jpeg code buffer */
    int relFence = sHandleImporter.unlock(*(halBuf.bufPtr));
    if (relFence >= 0) {
        halBuf.acquireFence = relFence;
    }

    /* Check if our JPEG actually succeeded */
    if (ret != 0) {
        return lfail(
            "%s: encodeJpegYU12 failed with %d",__FUNCTION__, ret);
    }

    ALOGV("%s: encoded JPEG (ret:%d) with Q:%d max size: %zu",
          __FUNCTION__, ret, jpegQuality, maxJpegCodeSize);

    return 0;
}

bool ExternalFakeCameraDeviceSession::OutputThread::threadLoop() {
    std::shared_ptr<HalRequest> req;
    auto parent = mParent.promote();
    if (parent == nullptr) {
       ALOGE("%s: session has been disconnected!", __FUNCTION__);
       return false;
    }

    // TODO: maybe we need to setup a sensor thread to dq/enq v4l frames
    //       regularly to prevent v4l buffer queue filled with stale buffers
    //       when app doesn't program a preveiw request
    waitForNextRequest(&req);
    if (req == nullptr) {
        // No new request, wait again
        return true;
    }

    auto onDeviceError = [&](auto... args) {
        ALOGE(args...);
        parent->notifyError(
                req->frameNumber, /*stream*/-1, ErrorCode::ERROR_DEVICE);
        signalRequestDone();
        return false;
    };

    if (req->yuvframeIn->mFourcc != V4L2_PIX_FMT_MJPEG &&
          req->yuvframeIn->mFourcc != V4L2_PIX_FMT_Z16 &&
          req->yuvframeIn->mFourcc != V4L2_PIX_FMT_YUYV &&
          req->yuvframeIn->mFourcc != V4L2_PIX_FMT_NV12) {

        return onDeviceError("%s: do not support V4L2 format %c%c%c%c", __FUNCTION__,
                req->yuvframeIn->mFourcc & 0xFF,
                (req->yuvframeIn->mFourcc >> 8) & 0xFF,
                (req->yuvframeIn->mFourcc >> 16) & 0xFF,
                (req->yuvframeIn->mFourcc >> 24) & 0xFF);
    }

    int res = requestBufferStart(req->buffers);
    if (res != 0) {
        ALOGE("%s: send BufferRequest failed! res %d", __FUNCTION__, res);
        return onDeviceError("%s: failed to send buffer request!", __FUNCTION__);
    }

    std::unique_lock<std::mutex> lk(mBufferLock);
    // Convert input V4L2 frame to YU12 of the same size
    // TODO: see if we can save some computation by converting to YV12 here
    /* remove to FormatConvertThread
    uint8_t* inData;
    size_t inDataSize;
    if (req->frameIn->getData(&inData, &inDataSize) != 0) {
        lk.unlock();
        return onDeviceError("%s: V4L2 buffer map failed", __FUNCTION__);
    }
	*/
    // TODO: in some special case maybe we can decode jpg directly to gralloc output?
    int is16Align = true;
    bool isBlobOrYv12 = false;
    int tempFrameWidth  = mYu12Frame->mWidth;
    int tempFrameHeight = mYu12Frame->mHeight;
    for (auto& halBuf : req->buffers) {
        if(halBuf.format == PixelFormat::BLOB || halBuf.format == PixelFormat::YV12) {
            isBlobOrYv12 = true;
        }
    }

    if (req->yuvframeIn->mFourcc == V4L2_PIX_FMT_MJPEG) {
        if((tempFrameWidth & 0x0f) || (tempFrameHeight & 0x0f)) {
            is16Align = false;
            tempFrameWidth  = ((tempFrameWidth + 15) & (~15));
            tempFrameHeight = ((tempFrameHeight + 15) & (~15));
        }
    }

    if (isBlobOrYv12 && req->yuvframeIn->mFourcc == V4L2_PIX_FMT_MJPEG) {
        /*LOGD("format is BLOB or YV12,use software jpeg decoder, framenumber(%d)", req->frameNumber);
        ATRACE_BEGIN("MJPGtoI420");
        int res = libyuv::MJPGToI420(
            req->inData, req->inDataSize, static_cast<uint8_t*>(mYu12FrameLayout.y), mYu12FrameLayout.yStride,
            static_cast<uint8_t*>(mYu12FrameLayout.cb), mYu12FrameLayout.cStride,
            static_cast<uint8_t*>(mYu12FrameLayout.cr), mYu12FrameLayout.cStride,
            mYu12Frame->mWidth, mYu12Frame->mHeight, mYu12Frame->mWidth, mYu12Frame->mHeight);
        ATRACE_END();*/
        YCbCrLayout input;
        input.y = (uint8_t*)req->mVirAddr;
        input.yStride = tempFrameWidth; //mYu12Frame->mWidth;
        input.cb = (uint8_t*)(req->mVirAddr) + tempFrameWidth * tempFrameHeight;
        input.cStride = tempFrameWidth; //mYu12Frame->mWidth;
        LOGD("format is BLOB or YV12, use software NV12ToI420");

        int ret = libyuv::NV12ToI420(
                static_cast<uint8_t*>(input.y),
                input.yStride,
                static_cast<uint8_t*>(input.cb),
                input.cStride,
                static_cast<uint8_t*>(mYu12FrameLayout.y),
                mYu12FrameLayout.yStride,
                static_cast<uint8_t*>(mYu12FrameLayout.cb),
                mYu12FrameLayout.cStride,
                static_cast<uint8_t*>(mYu12FrameLayout.cr),
                mYu12FrameLayout.cStride,
                mYu12Frame->mWidth, mYu12Frame->mHeight);

        if (res != 0) {
            // For some webcam, the first few V4L2 frames might be malformed...
            ALOGE("%s: Convert V4L2 frame to YU12 failed! res %d", __FUNCTION__, res);
            lk.unlock();
            Status st = parent->processCaptureRequestError(req);
            if (st != Status::OK) {
                return onDeviceError("%s: failed to process capture request error!", __FUNCTION__);
            }
            signalRequestDone();
            return true;
        }
    }

#ifndef NV12_HW_CONVERT
        if (req->yuvframeIn->mFourcc == V4L2_PIX_FMT_NV12) {
        ALOGV("%s NV12toI420", __FUNCTION__);
        ATRACE_BEGIN("NV12toI420");
        ALOGD("format is BLOB or YV12, use software NV12ToI420");
        YCbCrLayout input;
        input.y = (uint8_t*)req->inData;
        input.yStride = mYu12Frame->mWidth;
        input.cb = (uint8_t*)(req->inData) + mYu12Frame->mWidth * mYu12Frame->mHeight;
        input.cStride = mYu12Frame->mWidth;

        int res = libyuv::NV12ToI420(
                static_cast<uint8_t*>(input.y),
                input.yStride,
                static_cast<uint8_t*>(input.cb),
                input.cStride,
                static_cast<uint8_t*>(mYu12FrameLayout.y),
                mYu12FrameLayout.yStride,
                static_cast<uint8_t*>(mYu12FrameLayout.cb),
                mYu12FrameLayout.cStride,
                static_cast<uint8_t*>(mYu12FrameLayout.cr),
                mYu12FrameLayout.cStride,
                mYu12Frame->mWidth, mYu12Frame->mHeight);
       ATRACE_END();

       if (res != 0) {
            // For some webcam, the first few V4L2 frames might be malformed...
            ALOGE("%s: Convert V4L2 frame to YU12 failed! res %d", __FUNCTION__, res);
            lk.unlock();
            Status st = parent->processCaptureRequestError(req);
            if (st != Status::OK) {
                return onDeviceError("%s: failed to process capture request error!", __FUNCTION__);
            }
            signalRequestDone();
            return true;
       }
   }
#else
    if (isBlobOrYv12 && req->yuvframeIn->mFourcc == V4L2_PIX_FMT_NV12) {
        ALOGV("%s NV12toI420", __FUNCTION__);
        ATRACE_BEGIN("NV12toI420");
        ALOGD("format is BLOB or YV12, use software NV12ToI420");
        YCbCrLayout input;
        input.y = (uint8_t*)req->inData;
        input.yStride = mYu12Frame->mWidth;
        input.cb = (uint8_t*)(req->inData) + mYu12Frame->mWidth * mYu12Frame->mHeight;
        input.cStride = mYu12Frame->mWidth;

        int res = libyuv::NV12ToI420(
                static_cast<uint8_t*>(input.y),
                input.yStride,
                static_cast<uint8_t*>(input.cb),
                input.cStride,
                static_cast<uint8_t*>(mYu12FrameLayout.y),
                mYu12FrameLayout.yStride,
                static_cast<uint8_t*>(mYu12FrameLayout.cb),
                mYu12FrameLayout.cStride,
                static_cast<uint8_t*>(mYu12FrameLayout.cr),
                mYu12FrameLayout.cStride,
                mYu12Frame->mWidth, mYu12Frame->mHeight);
       ATRACE_END();

       if (res != 0) {
            // For some webcam, the first few V4L2 frames might be malformed...
            ALOGE("%s: Convert V4L2 frame to YU12 failed! res %d", __FUNCTION__, res);
            lk.unlock();
            Status st = parent->processCaptureRequestError(req);
            if (st != Status::OK) {
                return onDeviceError("%s: failed to process capture request error!", __FUNCTION__);
            }
            signalRequestDone();
            return true;
       }
    }
#endif

    if (isBlobOrYv12 && req->yuvframeIn->mFourcc == V4L2_PIX_FMT_YUYV) {
        YCbCrLayout input;
        input.y = (uint8_t*)req->inData;
        input.yStride = mYu12Frame->mWidth;
        input.cb = (uint8_t*)(req->inData) + mYu12Frame->mWidth * mYu12Frame->mHeight;
        input.cStride = mYu12Frame->mWidth;
        LOGD("format is BLOB or YV12, use software YUYVtoI420");

        ALOGV("%s libyuvToI420", __FUNCTION__);
        ATRACE_BEGIN("YUYVtoI420");
        int ret = libyuv::YUY2ToI420(
            req->inData, (mYu12Frame->mWidth)*2, static_cast<uint8_t*>(mYu12FrameLayout.y), mYu12FrameLayout.yStride,
            static_cast<uint8_t*>(mYu12FrameLayout.cb), mYu12FrameLayout.cStride,
            static_cast<uint8_t*>(mYu12FrameLayout.cr), mYu12FrameLayout.cStride,
            mYu12Frame->mWidth, mYu12Frame->mHeight);
        ATRACE_END();
        if (ret != 0) {
            // For some webcam, the first few V4L2 frames might be malformed...
            ALOGE("%s: Convert V4L2 frame to YU12 failed! res %d", __FUNCTION__, ret);
            lk.unlock();
            Status st = parent->processCaptureRequestError(req);
            if (st != Status::OK) {
                return onDeviceError("%s: failed to process capture request error!", __FUNCTION__);
            }
            signalRequestDone();
            return true;
        }
    }

    ATRACE_BEGIN("Wait for BufferRequest done");
    res = waitForBufferRequestDone(&req->buffers);
    ATRACE_END();

    if (res != 0) {
        ALOGE("%s: wait for BufferRequest done failed! res %d", __FUNCTION__, res);
        lk.unlock();
        return onDeviceError("%s: failed to process buffer request error!", __FUNCTION__);
    }
    
    ALOGV("%s processing new request", __FUNCTION__);
    const int kSyncWaitTimeoutMs = 500;
    for (auto& halBuf : req->buffers) {
        if (*(halBuf.bufPtr) == nullptr) {
            ALOGW("%s: buffer for stream %d missing", __FUNCTION__, halBuf.streamId);
            halBuf.fenceTimeout = true;
        } else if (halBuf.acquireFence >= 0) {
            int ret = sync_wait(halBuf.acquireFence, kSyncWaitTimeoutMs);
            if (ret) {
                halBuf.fenceTimeout = true;
            } else {
                ::close(halBuf.acquireFence);
                halBuf.acquireFence = -1;
            }
        }

        if (halBuf.fenceTimeout) {
            continue;
        }

        // Gralloc lockYCbCr the buffer
        switch (halBuf.format) {
            case PixelFormat::BLOB: {
                int ret = createJpegLocked(halBuf, req->setting);

                if(ret != 0) {
                    lk.unlock();
                    return onDeviceError("%s: createJpegLocked failed with %d",
                          __FUNCTION__, ret);
                }
            } break;
            case PixelFormat::Y16: {
                void* outLayout = sHandleImporter.lock(*(halBuf.bufPtr), halBuf.usage, req->inDataSize);

                std::memcpy(outLayout, req->inData, req->inDataSize);

                int relFence = sHandleImporter.unlock(*(halBuf.bufPtr));
                if (relFence >= 0) {
                    halBuf.acquireFence = relFence;
                }
            } break;
            case PixelFormat::YV12: {
                IMapper::Rect outRect {0, 0,
                        static_cast<int32_t>(halBuf.width),
                        static_cast<int32_t>(halBuf.height)};
                YCbCrLayout outLayout = sHandleImporter.lockYCbCr(
                        *(halBuf.bufPtr), halBuf.usage, outRect);
                ALOGV("%s: outLayout y %p cb %p cr %p y_str %d c_str %d c_step %d",
                        __FUNCTION__, outLayout.y, outLayout.cb, outLayout.cr,
                        outLayout.yStride, outLayout.cStride, outLayout.chromaStep);

                // Convert to output buffer size/format
                uint32_t outputFourcc = getFourCcFromLayout(outLayout);
                ALOGV("%s: converting to format %c%c%c%c", __FUNCTION__,
                        outputFourcc & 0xFF,
                        (outputFourcc >> 8) & 0xFF,
                        (outputFourcc >> 16) & 0xFF,
                        (outputFourcc >> 24) & 0xFF);

                YCbCrLayout cropAndScaled;
                ATRACE_BEGIN("cropAndScaleLocked");
                int ret = cropAndScaleLocked(
                        mYu12Frame,
                        Size { halBuf.width, halBuf.height },
                        &cropAndScaled);
                ATRACE_END();
                if (ret != 0) {
                    lk.unlock();
                    return onDeviceError("%s: crop and scale failed!", __FUNCTION__);
                }

                Size sz {halBuf.width, halBuf.height};
                ATRACE_BEGIN("formatConvert");
                ret = formatConvert(cropAndScaled, outLayout, sz, outputFourcc);
                ATRACE_END();
                if (ret != 0) {
                    lk.unlock();
                    return onDeviceError("%s: format coversion failed!", __FUNCTION__);
                }
                int relFence = sHandleImporter.unlock(*(halBuf.bufPtr));
                if (relFence >= 0) {
                    halBuf.acquireFence = relFence;
                }
            } break;
            case PixelFormat::YCBCR_420_888:
            case PixelFormat::IMPLEMENTATION_DEFINED:
            case PixelFormat::YCRCB_420_SP: {
                if (req->yuvframeIn->mFourcc == V4L2_PIX_FMT_YUYV){
                    ALOGV("%s libyuvToI420", __FUNCTION__);
                    ATRACE_BEGIN("YUYVtoI420");
                    int ret = libyuv::YUY2ToI420(
                        req->inData, (mYu12Frame->mWidth)*2, static_cast<uint8_t*>(mYu12FrameLayout.y), mYu12FrameLayout.yStride,
                        static_cast<uint8_t*>(mYu12FrameLayout.cb), mYu12FrameLayout.cStride,
                        static_cast<uint8_t*>(mYu12FrameLayout.cr), mYu12FrameLayout.cStride,
                        mYu12Frame->mWidth, mYu12Frame->mHeight);
                    ATRACE_END();
                    IMapper::Rect outRect {0, 0,
                            static_cast<int32_t>(halBuf.width),
                            static_cast<int32_t>(halBuf.height)};
                    YCbCrLayout outLayout = sHandleImporter.lockYCbCr(
                            *(halBuf.bufPtr), halBuf.usage, outRect);
                    ALOGV("%s: outLayout y %p cb %p cr %p y_str %d c_str %d c_step %d",
                            __FUNCTION__, outLayout.y, outLayout.cb, outLayout.cr,
                            outLayout.yStride, outLayout.cStride, outLayout.chromaStep);

                    // Convert to output buffer size/format
                    uint32_t outputFourcc = getFourCcFromLayout(outLayout);
                    ALOGV("%s: converting to format %c%c%c%c", __FUNCTION__,
                            outputFourcc & 0xFF,
                            (outputFourcc >> 8) & 0xFF,
                            (outputFourcc >> 16) & 0xFF,
                            (outputFourcc >> 24) & 0xFF);

                    YCbCrLayout cropAndScaled;
                    ATRACE_BEGIN("cropAndScaleLocked");
                    ret = cropAndScaleLocked(
                            mYu12Frame,
                            Size { halBuf.width, halBuf.height },
                            &cropAndScaled);
                    ATRACE_END();
                    if (ret != 0) {
                        lk.unlock();
                        return onDeviceError("%s: crop and scale failed!", __FUNCTION__);
                    }
                    Size sz {halBuf.width, halBuf.height};
                    ATRACE_BEGIN("formatConvert");
                    ret = formatConvert(cropAndScaled, outLayout, sz, outputFourcc);
                    ATRACE_END();
                    if (ret != 0) {
                        lk.unlock();
                        return onDeviceError("%s: format coversion failed!", __FUNCTION__);
                    }
                    int relFence = sHandleImporter.unlock(*(halBuf.bufPtr));
                    if (relFence >= 0) {
                        halBuf.acquireFence = relFence;
                    }
                } else if (req->yuvframeIn->mFourcc == V4L2_PIX_FMT_NV12){

                    int handle_fd = -1, ret;
#ifndef RK_GRALLOC_4
                    gralloc_module_t const* mGrallocModule;
                    const hw_module_t *allocMod = NULL;
                    const native_handle_t* tmp_hand = (const native_handle_t*)*(halBuf.bufPtr);
                    ret= hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &allocMod);
                    mGrallocModule = reinterpret_cast<gralloc_module_t const *>(allocMod);
                    mGrallocModule->perform(
                            mGrallocModule,
                            GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD,
                            tmp_hand,
                            &handle_fd);
#else
                    const native_handle_t* tmp_hand = (const native_handle_t*)(*(halBuf.bufPtr));
                    ret = ExCamGralloc4::get_share_fd(tmp_hand, &handle_fd);
#endif
                    if (handle_fd == -1) {
                        LOGE("convert tmp_hand to dst_fd error");
                        return -EINVAL;
                    }
                    ALOGV("%s(%d): halBuf handle_fd(%d)", __FUNCTION__, __LINE__, handle_fd);
                    ALOGV("%s(%d) halbuf_wxh(%dx%d) frameNumber(%d)", __FUNCTION__, __LINE__,
                        halBuf.width, halBuf.height, req->frameNumber);
                    unsigned long vir_addr =  reinterpret_cast<unsigned long>(req->inData);
                    camera2::RgaCropScale::rga_nv12_scale_crop(
                        tempFrameWidth, tempFrameHeight, vir_addr, handle_fd,
                        halBuf.width, halBuf.height, 100, false, true,
                        (halBuf.format == PixelFormat::YCRCB_420_SP), is16Align,
                        true);
                }else {

                    if (req->mShareFd <= 0) {
                        lk.unlock();
                        Status st = parent->processCaptureRequestError(req);
                        if (st != Status::OK) {
                            return onDeviceError("%s: failed to process capture request error!", __FUNCTION__);
                        }
                        signalRequestDone();
                        return true;
                    }
#ifndef RK_HW_JPEG_DECODER
                     int res = libyuv::MJPGToI420(
                         req->inData, req->inDataSize, static_cast<uint8_t*>(mYu12FrameLayout.y), mYu12FrameLayout.yStride,
                         static_cast<uint8_t*>(mYu12FrameLayout.cb), mYu12FrameLayout.cStride,
                         static_cast<uint8_t*>(mYu12FrameLayout.cr), mYu12FrameLayout.cStride,
                         mYu12Frame->mWidth, mYu12Frame->mHeight, mYu12Frame->mWidth, mYu12Frame->mHeight);
                     ALOGV("%s MJPGToI420 end, I420ToNV12 start", __FUNCTION__);
                     ATRACE_BEGIN("I420ToNV12");
                     YCbCrLayout output;
                     output.y = (uint8_t*)req->mVirAddr;
                     output.yStride = mYu12Frame->mWidth;
                     output.cb = (uint8_t*)(req->mVirAddr) + tempFrameWidth * tempFrameHeight;
                     output.cStride = mYu12Frame->mWidth;

                     res = libyuv::I420ToNV12(
                             static_cast<uint8_t*>(mYu12FrameLayout.y),
                             mYu12FrameLayout.yStride,
                             static_cast<uint8_t*>(mYu12FrameLayout.cb),
                             mYu12FrameLayout.cStride,
                             static_cast<uint8_t*>(mYu12FrameLayout.cr),
                             mYu12FrameLayout.cStride,
                             static_cast<uint8_t*>(output.y),
                             output.yStride,
                             static_cast<uint8_t*>(output.cb),
                             output.cStride,
                             mYu12Frame->mWidth, mYu12Frame->mHeight);
                    ATRACE_END();
#ifdef DUMP_YUV
                    {
                        static int frameCount = req->frameNumber;
                        if(++frameCount > 5 && frameCount<10){
                            FILE* fp =NULL;
                            char filename[128];
                            filename[0] = 0x00;
                            sprintf(filename, "/data/camera/camera_dump_%dx%d_%d.yuv",
                                    tempFrameWidth, tempFrameHeight, frameCount);
                            fp = fopen(filename, "wb+");
                            if (fp != NULL) {
                                fwrite((char*)req->mVirAddr, 1, tempFrameWidth*tempFrameHeight*1.5, fp);
                                fclose(fp);
                                ALOGI("Write success YUV data to %s",filename);
                            } else {
                                ALOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
                            }
                        }
                    }
#endif
#endif

                    int handle_fd = -1, ret;
#ifndef RK_GRALLOC_4
                    gralloc_module_t const* mGrallocModule;
                    const hw_module_t *allocMod = NULL;
                    const native_handle_t* tmp_hand = (const native_handle_t*)*(halBuf.bufPtr);
                    ret= hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &allocMod);
                    mGrallocModule = reinterpret_cast<gralloc_module_t const *>(allocMod);
                    mGrallocModule->perform(
                            mGrallocModule,
                            GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD,
                            tmp_hand,
                            &handle_fd);
#else
                    const native_handle_t* tmp_hand = (const native_handle_t*)(*(halBuf.bufPtr));
                    ret = ExCamGralloc4::get_share_fd(tmp_hand, &handle_fd);
#endif
                    if (handle_fd == -1) {
                        LOGE("convert tmp_hand to dst_fd error");
                        return -EINVAL;
                    }
                    ALOGV("%s(%d): halBuf handle_fd(%d)", __FUNCTION__, __LINE__, handle_fd);
                    ALOGV("%s(%d) halbuf_wxh(%dx%d) frameNumber(%d)", __FUNCTION__, __LINE__,
                        halBuf.width, halBuf.height, req->frameNumber);

                    camera2::RgaCropScale::rga_nv12_scale_crop(
                        tempFrameWidth, tempFrameHeight, req->mShareFd, handle_fd,
                        halBuf.width, halBuf.height, 100, false, true,
                        (halBuf.format == PixelFormat::YCRCB_420_SP), is16Align,
                        req->yuvframeIn->mFourcc == V4L2_PIX_FMT_YUYV);
#ifdef DUMP_YUV
                    {
                        void* mVirAddr = NULL;
                        ret = ExCamGralloc4::lock(
                                    tmp_hand,
                                    halBuf.usage,
                                    0,
                                    0,
                                    halBuf.width,
                                    halBuf.height,
                                    (void**)&mVirAddr);
                        if (ret) {
                            LOGE("lock buffer error : %s", strerror(errno));
                        }
                        ExCamGralloc4::unlock(tmp_hand);
                        int frameCount = req->frameNumber;
                        if( frameCount > 4 && frameCount<10){
                            FILE* fp =NULL;
                            char filename[128];
                            filename[0] = 0x00;
                            sprintf(filename, "/data/camera/camera_dump_%dx%d_%d.yuv",
                                    tempFrameWidth, tempFrameHeight, frameCount);
                            fp = fopen(filename, "wb+");
                            if (fp != NULL) {
                                fwrite((char*)req->mVirAddr, 1, tempFrameWidth*tempFrameHeight*1.5, fp);
                                fclose(fp);
                                ALOGI("Write success YUV data to %s",filename);
                            } else {
                                ALOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
                            }
                            sprintf(filename, "/data/camera/camera_dump_halbuf_%dx%d_%d.yuv",
                                    halBuf.width, halBuf.height, frameCount);
                            fp = fopen(filename, "wb+");
                            if (fp != NULL) {
                                fwrite((char*)mVirAddr, 1, tempFrameWidth*tempFrameHeight*1.5, fp);
                                fclose(fp);
                                ALOGI("Write success YUV data to %s",filename);
                            } else {
                                ALOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
                            }
                        }
                    }
#endif

                }
            } break;
            default:
                lk.unlock();
                return onDeviceError("%s: unknown output format %x", __FUNCTION__, halBuf.format);
        }
    } // for each buffer
    mScaledYu12Frames.clear();

    // Don't hold the lock while calling back to parent
    lk.unlock();
    Status st = parent->processCaptureResult(req);
    if (st != Status::OK) {
        return onDeviceError("%s: failed to process capture result!", __FUNCTION__);
    }
    signalRequestDone();
    return true;
}

Status ExternalFakeCameraDeviceSession::OutputThread::allocateIntermediateBuffers(
        const Size& v4lSize, const Size& thumbSize,
        const hidl_vec<Stream>& streams,
        uint32_t blobBufferSize) {
    std::lock_guard<std::mutex> lk(mBufferLock);
    if (mScaledYu12Frames.size() != 0) {
        ALOGE("%s: intermediate buffer pool has %zu inflight buffers! (expect 0)",
                __FUNCTION__, mScaledYu12Frames.size());
        return Status::INTERNAL_ERROR;
    }

    // Allocating intermediate YU12 frame
    if (mYu12Frame == nullptr || mYu12Frame->mWidth != v4lSize.width ||
            mYu12Frame->mHeight != v4lSize.height) {
        mYu12Frame.clear();
        mYu12Frame = new AllocatedFrame(v4lSize.width, v4lSize.height);
        int ret = mYu12Frame->allocate(&mYu12FrameLayout);
        if (ret != 0) {
            ALOGE("%s: allocating YU12 frame failed!", __FUNCTION__);
            return Status::INTERNAL_ERROR;
        }
    }

    // Allocating intermediate YU12 thumbnail frame
    if (mYu12ThumbFrame == nullptr ||
        mYu12ThumbFrame->mWidth != thumbSize.width ||
        mYu12ThumbFrame->mHeight != thumbSize.height) {
        mYu12ThumbFrame.clear();
        mYu12ThumbFrame = new AllocatedFrame(thumbSize.width, thumbSize.height);
        int ret = mYu12ThumbFrame->allocate(&mYu12ThumbFrameLayout);
        if (ret != 0) {
            ALOGE("%s: allocating YU12 thumb frame failed!", __FUNCTION__);
            return Status::INTERNAL_ERROR;
        }
    }

    // Allocating scaled buffers
    for (const auto& stream : streams) {
        Size sz = {stream.width, stream.height};
        if (sz == v4lSize) {
            continue; // Don't need an intermediate buffer same size as v4lBuffer
        }
        if (mIntermediateBuffers.count(sz) == 0) {
            // Create new intermediate buffer
            sp<AllocatedFrame> buf = new AllocatedFrame(stream.width, stream.height);
            int ret = buf->allocate();
            if (ret != 0) {
                ALOGE("%s: allocating intermediate YU12 frame %dx%d failed!",
                            __FUNCTION__, stream.width, stream.height);
                return Status::INTERNAL_ERROR;
            }
            mIntermediateBuffers[sz] = buf;
        }
    }

    // Remove unconfigured buffers
    auto it = mIntermediateBuffers.begin();
    while (it != mIntermediateBuffers.end()) {
        bool configured = false;
        auto sz = it->first;
        for (const auto& stream : streams) {
            if (stream.width == sz.width && stream.height == sz.height) {
                configured = true;
                break;
            }
        }
        if (configured) {
            it++;
        } else {
            it = mIntermediateBuffers.erase(it);
        }
    }

    mBlobBufferSize = blobBufferSize;
    return Status::OK;
}

void ExternalFakeCameraDeviceSession::OutputThread::clearIntermediateBuffers() {
    std::lock_guard<std::mutex> lk(mBufferLock);
    mYu12Frame.clear();
    mYu12ThumbFrame.clear();
    mIntermediateBuffers.clear();
    mBlobBufferSize = 0;
}

Status ExternalFakeCameraDeviceSession::OutputThread::submitRequest(
        const std::shared_ptr<HalRequest>& req) {
    std::unique_lock<std::mutex> lk(mRequestListLock);
    mRequestList.push_back(req);
    lk.unlock();
    mRequestCond.notify_one();
    return Status::OK;
}

void ExternalFakeCameraDeviceSession::OutputThread::flush() {
    ATRACE_CALL();
    auto parent = mParent.promote();
    if (parent == nullptr) {
       ALOGE("%s: session has been disconnected!", __FUNCTION__);
       return;
    }

    std::unique_lock<std::mutex> lk(mRequestListLock);
    std::list<std::shared_ptr<HalRequest>> reqs = std::move(mRequestList);
    mRequestList.clear();
    if (mProcessingRequest) {
        std::chrono::seconds timeout = std::chrono::seconds(kFlushWaitTimeoutSec);
        auto st = mRequestDoneCond.wait_for(lk, timeout);
        if (st == std::cv_status::timeout) {
            ALOGE("%s: wait for inflight request finish timeout!", __FUNCTION__);
        }
    }

    ALOGV("%s: flusing inflight requests", __FUNCTION__);
    lk.unlock();
    for (const auto& req : reqs) {
        parent->processCaptureRequestError(req);
    }
}

std::list<std::shared_ptr<HalRequest>>
ExternalFakeCameraDeviceSession::OutputThread::switchToOffline() {
    ATRACE_CALL();
    std::list<std::shared_ptr<HalRequest>> emptyList;
    auto parent = mParent.promote();
    if (parent == nullptr) {
       ALOGE("%s: session has been disconnected!", __FUNCTION__);
       return emptyList;
    }

    std::unique_lock<std::mutex> lk(mRequestListLock);
    std::list<std::shared_ptr<HalRequest>> reqs = std::move(mRequestList);
    mRequestList.clear();
    if (mProcessingRequest) {
        std::chrono::seconds timeout = std::chrono::seconds(kFlushWaitTimeoutSec);
        auto st = mRequestDoneCond.wait_for(lk, timeout);
        if (st == std::cv_status::timeout) {
            ALOGE("%s: wait for inflight request finish timeout!", __FUNCTION__);
        }
    }
    lk.unlock();
    clearIntermediateBuffers();
    ALOGV("%s: returning %zu request for offline processing", __FUNCTION__, reqs.size());
    return reqs;
}

void ExternalFakeCameraDeviceSession::OutputThread::waitForNextRequest(
        std::shared_ptr<HalRequest>* out) {
    ATRACE_CALL();
    if (out == nullptr) {
        ALOGE("%s: out is null", __FUNCTION__);
        return;
    }

    std::unique_lock<std::mutex> lk(mRequestListLock);
    int waitTimes = 0;
    while (mRequestList.empty()) {
        if (exitPending()) {
            return;
        }
        std::chrono::milliseconds timeout = std::chrono::milliseconds(kReqWaitTimeoutMs);
        auto st = mRequestCond.wait_for(lk, timeout);
        if (st == std::cv_status::timeout) {
            waitTimes++;
            if (waitTimes == kReqWaitTimesMax) {
                // no new request, return
                return;
            }
        }
    }
    *out = mRequestList.front();
    mRequestList.pop_front();
    mProcessingRequest = true;
    mProcessingFrameNumer = (*out)->frameNumber;
}

void ExternalFakeCameraDeviceSession::OutputThread::signalRequestDone() {
    std::unique_lock<std::mutex> lk(mRequestListLock);
    mProcessingRequest = false;
    mProcessingFrameNumer = 0;
    lk.unlock();
    mRequestDoneCond.notify_one();
}

void ExternalFakeCameraDeviceSession::OutputThread::dump(int fd) {
    std::lock_guard<std::mutex> lk(mRequestListLock);
    if (mProcessingRequest) {
        dprintf(fd, "OutputThread processing frame %d\n", mProcessingFrameNumer);
    } else {
        dprintf(fd, "OutputThread not processing any frames\n");
    }
    dprintf(fd, "OutputThread request list contains frame: ");
    for (const auto& req : mRequestList) {
        dprintf(fd, "%d, ", req->frameNumber);
    }
    dprintf(fd, "\n");
}

void ExternalFakeCameraDeviceSession::cleanupBuffersLocked(int id) {
    for (auto& pair : mCirculatingBuffers.at(id)) {
        sHandleImporter.freeBuffer(pair.second);
    }
    mCirculatingBuffers[id].clear();
    mCirculatingBuffers.erase(id);
}

void ExternalFakeCameraDeviceSession::updateBufferCaches(const hidl_vec<BufferCache>& cachesToRemove) {
    Mutex::Autolock _l(mCbsLock);
    for (auto& cache : cachesToRemove) {
        auto cbsIt = mCirculatingBuffers.find(cache.streamId);
        if (cbsIt == mCirculatingBuffers.end()) {
            // The stream could have been removed
            continue;
        }
        CirculatingBuffers& cbs = cbsIt->second;
        auto it = cbs.find(cache.bufferId);
        if (it != cbs.end()) {
            sHandleImporter.freeBuffer(it->second);
            cbs.erase(it);
        } else {
            ALOGE("%s: stream %d buffer %" PRIu64 " is not cached",
                    __FUNCTION__, cache.streamId, cache.bufferId);
        }
    }
}

bool ExternalFakeCameraDeviceSession::isSupported(const Stream& stream,
        const std::vector<SupportedV4L2Format>& supportedFormats,
        const ExternalCameraConfig& devCfg) {
    int32_t ds = static_cast<int32_t>(stream.dataSpace);
    PixelFormat fmt = stream.format;
    uint32_t width = stream.width;
    uint32_t height = stream.height;
    // TODO: check usage flags

    if (stream.streamType != StreamType::OUTPUT) {
        ALOGE("%s: does not support non-output stream type", __FUNCTION__);
        return false;
    }

    if (stream.rotation != StreamRotation::ROTATION_0) {
        ALOGE("%s: does not support stream rotation", __FUNCTION__);
        return false;
    }

    switch (fmt) {
        case PixelFormat::BLOB:
            if (ds != static_cast<int32_t>(Dataspace::V0_JFIF)) {
                ALOGI("%s: BLOB format does not support dataSpace %x", __FUNCTION__, ds);
                return false;
            }
            break;
        case PixelFormat::IMPLEMENTATION_DEFINED:
        case PixelFormat::YCBCR_420_888:
        case PixelFormat::YV12:
        case PixelFormat::YCRCB_420_SP:
            // TODO: check what dataspace we can support here.
            // intentional no-ops.
            break;
        case PixelFormat::Y16:
            if (!devCfg.depthEnabled) {
                ALOGI("%s: Depth is not Enabled", __FUNCTION__);
                return false;
            }
            if (!(ds & Dataspace::DEPTH)) {
                ALOGI("%s: Y16 supports only dataSpace DEPTH", __FUNCTION__);
                return false;
            }
            break;
        default:
            ALOGI("%s: does not support format %x", __FUNCTION__, fmt);
            return false;
    }

    // Assume we can convert any V4L2 format to any of supported output format for now, i.e,
    // ignoring v4l2Fmt.fourcc for now. Might need more subtle check if we support more v4l format
    // in the futrue.
    for (const auto& v4l2Fmt : supportedFormats) {
        if (width == v4l2Fmt.width && height == v4l2Fmt.height) {
            return true;
        }
    }
    ALOGI("%s: resolution %dx%d is not supported", __FUNCTION__, width, height);
    return false;
}

int ExternalFakeCameraDeviceSession::v4l2StreamOffLocked() {
    if (!mV4l2Streaming) {
        return OK;
    }

    {
        std::lock_guard<std::mutex> lk(mV4l2BufferLock);
        if (mNumDequeuedV4l2Buffers != 0)  {
            ALOGE("%s: there are %zu inflight V4L buffers",
                __FUNCTION__, mNumDequeuedV4l2Buffers);
            return -1;
        }
    }
    mV4L2BufferCount = 0;

    // VIDIOC_STREAMOFF
    /*v4l2_buf_type capture_type;
    if (mCapability.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else
        capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_STREAMOFF, &capture_type)) < 0) {
        ALOGE("%s: STREAMOFF failed: %s", __FUNCTION__, strerror(errno));
        return -errno;
    }

    // VIDIOC_REQBUFS: clear buffers
    v4l2_requestbuffers req_buffers{};
    if (mCapability.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        req_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else
        req_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_buffers.memory = V4L2_MEMORY_MMAP;
    req_buffers.count = 0;
    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_REQBUFS, &req_buffers)) < 0) {
        ALOGE("%s: REQBUFS failed: %s", __FUNCTION__, strerror(errno));
        return -errno;
    }*/

    mV4l2Streaming = false;
    return OK;
}

int ExternalFakeCameraDeviceSession::setV4l2FpsLocked(double fps) {
    // VIDIOC_G_PARM/VIDIOC_S_PARM: set fps
    /*v4l2_streamparm streamparm;
    if (mCapability.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else
        streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // The following line checks that the driver knows about framerate get/set.
    int ret = TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_G_PARM, &streamparm));
    if (ret != 0) {
        if (errno == -EINVAL) {
            ALOGW("%s: device does not support VIDIOC_G_PARM", __FUNCTION__);
        }
        return -errno;
    }
    // Now check if the device is able to accept a capture framerate set.
    if (!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        ALOGW("%s: device does not support V4L2_CAP_TIMEPERFRAME", __FUNCTION__);
        return -EINVAL;
    }

    // fps is float, approximate by a fraction.
    const int kFrameRatePrecision = 10000;
    streamparm.parm.capture.timeperframe.numerator = kFrameRatePrecision;
    streamparm.parm.capture.timeperframe.denominator =
        (fps * kFrameRatePrecision);

    if (TEMP_FAILURE_RETRY(ioctl(mV4l2Fd.get(), VIDIOC_S_PARM, &streamparm)) < 0) {
        ALOGE("%s: failed to set framerate to %f: %s", __FUNCTION__, fps, strerror(errno));
        return -1;
    }

    double retFps = streamparm.parm.capture.timeperframe.denominator /
            static_cast<double>(streamparm.parm.capture.timeperframe.numerator);
    ALOGV("%s: retFps(%f)", __FUNCTION__, retFps);
    if (std::fabs(fps - retFps) > 1.0) {
        ALOGE("%s: expect fps %f, got %f instead", __FUNCTION__, fps, retFps);
        return -1;
    }*/
    mV4l2StreamingFps = fps;
    return 0;
}

int ExternalFakeCameraDeviceSession::configureV4l2StreamLocked(
        SupportedV4L2Format& v4l2Fmt, double requestFps) {
    ATRACE_CALL();
    ALOGD("V4L configuration format:%c%c%c%c, w %d, h %d",
        v4l2Fmt.fourcc & 0xFF,
        (v4l2Fmt.fourcc >> 8) & 0xFF,
        (v4l2Fmt.fourcc >> 16) & 0xFF,
        (v4l2Fmt.fourcc >> 24) & 0xFF,
        v4l2Fmt.width, v4l2Fmt.height);

    int ret = v4l2StreamOffLocked();
    if (ret != OK) {
        ALOGE("%s: stop v4l2 streaming failed: ret %d", __FUNCTION__, ret);
        return ret;
    }

    mMaxV4L2BufferSize = v4l2Fmt.width * v4l2Fmt.height * 1.5;

    const double kDefaultFps = 30.0;
    double fps = 1000.0;
    if (requestFps != 0.0) {
        fps = requestFps;
    } else {
        double maxFps = -1.0;
        // Try to pick the slowest fps that is at least 30
        for (const auto& fr : v4l2Fmt.frameRates) {
            double f = fr.getDouble();
            if (maxFps < f) {
                maxFps = f;
            }
            if (f >= kDefaultFps && f < fps) {
                fps = f;
            }
        }
        if (fps == 1000.0) {
            fps = maxFps;
        }
    }

    int fpsRet = setV4l2FpsLocked(fps);
    if (fpsRet != 0 && fpsRet != -EINVAL) {
        ALOGE("%s: set fps failed: %s", __FUNCTION__, strerror(fpsRet));
        return fpsRet;
    }

    uint32_t v4lBufferCount = (fps >= kDefaultFps) ?
            mCfg.numVideoBuffers : mCfg.numStillBuffers;

    ALOGD("v4lBufferCount:%d", v4lBufferCount);
    
    mV4L2BufferCount = v4lBufferCount;

    ALOGI("%s: start V4L2 streaming %dx%d@%ffps",
                __FUNCTION__, v4l2Fmt.width, v4l2Fmt.height, fps);
    mV4l2StreamingFmt = v4l2Fmt;
    mV4l2Streaming = true;
    return OK;
}

sp<YuvFrame> ExternalFakeCameraDeviceSession::dequeueV4l2FrameLocked(/*out*/nsecs_t* shutterTs) {
    ATRACE_CALL();
    sp<YuvFrame> ret = nullptr;

    if (shutterTs == nullptr) {
        ALOGE("%s: shutterTs must not be null!", __FUNCTION__);
        return ret;
    }

    {
        std::unique_lock<std::mutex> lk(mV4l2BufferLock);
        if (mNumDequeuedV4l2Buffers == mV4L2BufferCount) {
            int waitRet = waitForV4L2BufferReturnLocked(lk);
            if (waitRet != 0) {
                return ret;
            }
        }
    }

    ATRACE_BEGIN("VIDIOC_DQBUF");
    int index = mFormatConvertThread->mCamMemManager->getIdleBufferIndex(PREVIEWBUFFER);
    ATRACE_END();

    if (index < 0) {
        ALOGE("%s: Invalid buffer id: %d", __FUNCTION__, index);
        return ret;
    }

    unsigned long viraddr = mFormatConvertThread->mCamMemManager->getBufferAddr(
            PREVIEWBUFFER, index, buffer_addr_vir);

    size_t size = 0;
    if (mV4l2StreamingFmt.fourcc == V4L2_PIX_FMT_MJPEG) {
        FILE* fp =NULL;
        char filename[128];
        filename[0] = 0x00;
        sprintf(filename, "/data/camera/camera_%dx%d.jpg",
            mV4l2StreamingFmt.width, mV4l2StreamingFmt.height);
        fp = fopen(filename, "r+");
        if (fp != NULL) {
            size = fread((char*)viraddr,1,mV4l2StreamingFmt.width*mV4l2StreamingFmt.height*1.5,fp);
            fclose(fp);
            ALOGD("read success jpeg data to %s size:%d",filename, size);
        } else {
            ALOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
        }
    } else if (mV4l2StreamingFmt.fourcc == V4L2_PIX_FMT_NV12) {
        FILE* fp =NULL;
        char filename[128];
        filename[0] = 0x00;
        sprintf(filename, "/data/camera/camera_%dx%d.yuv",
            mV4l2StreamingFmt.width, mV4l2StreamingFmt.height);
        fp = fopen(filename, "r+");
        if (fp != NULL) {
            size = fread((char*)viraddr,1,mV4l2StreamingFmt.width*mV4l2StreamingFmt.height*1.5,fp);
            fclose(fp);
            ALOGV("read success NV12 data to %s size:%d",filename, size);
        } else {
            ALOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
        }
    }
    
    mFormatConvertThread->mCamMemManager->setBufferStatus(
            PREVIEWBUFFER, index, 1);

    *shutterTs = systemTime(SYSTEM_TIME_MONOTONIC);

    {
        std::lock_guard<std::mutex> lk(mV4l2BufferLock);
        mNumDequeuedV4l2Buffers++;
    }

    return new YuvFrame(
            mV4l2StreamingFmt.width, mV4l2StreamingFmt.height, mV4l2StreamingFmt.fourcc,
            index, (uint8_t*) viraddr, size);

}

void ExternalFakeCameraDeviceSession::enqueueV4l2Frame(const sp<YuvFrame>& frame) {
    ATRACE_CALL();
    mFormatConvertThread->mCamMemManager->setBufferStatus(
            PREVIEWBUFFER, frame->mBufferIndex, 0);

    ATRACE_END();

    {
        std::lock_guard<std::mutex> lk(mV4l2BufferLock);
        mNumDequeuedV4l2Buffers--;
    }
    mV4L2BufferReturned.notify_one();
}

Status ExternalFakeCameraDeviceSession::isStreamCombinationSupported(
        const V3_2::StreamConfiguration& config,
        const std::vector<SupportedV4L2Format>& supportedFormats,
        const ExternalCameraConfig& devCfg) {
    if (config.operationMode != StreamConfigurationMode::NORMAL_MODE) {
        ALOGE("%s: unsupported operation mode: %d", __FUNCTION__, config.operationMode);
        return Status::ILLEGAL_ARGUMENT;
    }

    if (config.streams.size() == 0) {
        ALOGE("%s: cannot configure zero stream", __FUNCTION__);
        return Status::ILLEGAL_ARGUMENT;
    }

    int numProcessedStream = 0;
    int numStallStream = 0;
    for (const auto& stream : config.streams) {
        // Check if the format/width/height combo is supported
        if (!isSupported(stream, supportedFormats, devCfg)) {
            return Status::ILLEGAL_ARGUMENT;
        }
        if (stream.format == PixelFormat::BLOB) {
            numStallStream++;
        } else {
            numProcessedStream++;
        }
    }

    if (numProcessedStream > kMaxProcessedStream) {
        ALOGE("%s: too many processed streams (expect <= %d, got %d)", __FUNCTION__,
                kMaxProcessedStream, numProcessedStream);
        return Status::ILLEGAL_ARGUMENT;
    }

    if (numStallStream > kMaxStallStream) {
        ALOGE("%s: too many stall streams (expect <= %d, got %d)", __FUNCTION__,
                kMaxStallStream, numStallStream);
        return Status::ILLEGAL_ARGUMENT;
    }

    return Status::OK;
}

Status ExternalFakeCameraDeviceSession::configureStreams(
        const V3_2::StreamConfiguration& config,
        V3_3::HalStreamConfiguration* out,
        uint32_t blobBufferSize) {
    ATRACE_CALL();

    Status status = isStreamCombinationSupported(config, mSupportedFormats, mCfg);
    if (status != Status::OK) {
        return status;
    }

    status = initStatus();
    if (status != Status::OK) {
        return status;
    }


    {
        std::lock_guard<std::mutex> lk(mInflightFramesLock);
        if (!mInflightFrames.empty()) {
            ALOGE("%s: trying to configureStreams while there are still %zu inflight frames!",
                    __FUNCTION__, mInflightFrames.size());
            return Status::INTERNAL_ERROR;
        }
    }

    Mutex::Autolock _l(mLock);
    {
        Mutex::Autolock _l(mCbsLock);
        // Add new streams
        for (const auto& stream : config.streams) {
            if (mStreamMap.count(stream.id) == 0) {
                mStreamMap[stream.id] = stream;
                mCirculatingBuffers.emplace(stream.id, CirculatingBuffers{});
            }
        }

        // Cleanup removed streams
        for(auto it = mStreamMap.begin(); it != mStreamMap.end();) {
            int id = it->first;
            bool found = false;
            for (const auto& stream : config.streams) {
                if (id == stream.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Unmap all buffers of deleted stream
                cleanupBuffersLocked(id);
                it = mStreamMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Now select a V4L2 format to produce all output streams
    float desiredAr = (mCroppingType == VERTICAL) ? kMaxAspectRatio : kMinAspectRatio;
    uint32_t maxDim = 0;
    for (const auto& stream : config.streams) {
        float aspectRatio = ASPECT_RATIO(stream);
        ALOGI("%s: request stream %dx%d, format: 0x%x", __FUNCTION__, stream.width, stream.height, stream.format);
        if ((mCroppingType == VERTICAL && aspectRatio < desiredAr) ||
                (mCroppingType == HORIZONTAL && aspectRatio > desiredAr)) {
            desiredAr = aspectRatio;
        }

        // The dimension that's not cropped
        uint32_t dim = (mCroppingType == VERTICAL) ? stream.width : stream.height;
        if (dim > maxDim) {
            maxDim = dim;
        }
    }
    // Find the smallest format that matches the desired aspect ratio and is wide/high enough
    SupportedV4L2Format v4l2Fmt {.width = 0, .height = 0};
    SupportedV4L2Format v4l2Fmt_tmp {.width = 0, .height = 0};
    for (const auto& fmt : mSupportedFormats) {
        uint32_t dim = (mCroppingType == VERTICAL) ? fmt.width : fmt.height;
        if (dim >= maxDim) {
            float aspectRatio = ASPECT_RATIO(fmt);
            if (isAspectRatioClose(aspectRatio, desiredAr)) {
                v4l2Fmt_tmp = fmt;
                // since mSupportedFormats is sorted by width then height, the first matching fmt
                // will be the smallest one with matching aspect ratio
                if ((fmt.fourcc == V4L2_PIX_FMT_MJPEG) ||
                    (fmt.fourcc == V4L2_PIX_FMT_NV12)) {
                    v4l2Fmt_tmp = fmt;
                    break;
                }
            }
        }
    }
    v4l2Fmt = v4l2Fmt_tmp;
    if (v4l2Fmt.width == 0) {
        // Cannot find exact good aspect ratio candidate, try to find a close one
        for (const auto& fmt : mSupportedFormats) {
            uint32_t dim = (mCroppingType == VERTICAL) ? fmt.width : fmt.height;
            if (dim >= maxDim) {
                float aspectRatio = ASPECT_RATIO(fmt);
                if ((mCroppingType == VERTICAL && aspectRatio < desiredAr) ||
                        (mCroppingType == HORIZONTAL && aspectRatio > desiredAr)) {
                    v4l2Fmt = fmt;
                    break;
                }
            }
        }
    }

    if (v4l2Fmt.width == 0) {
        ALOGE("%s: unable to find a resolution matching (%s at least %d, aspect ratio %f)"
                , __FUNCTION__, (mCroppingType == VERTICAL) ? "width" : "height",
                maxDim, desiredAr);
        return Status::ILLEGAL_ARGUMENT;
    }

    if (configureV4l2StreamLocked(v4l2Fmt) != 0) {
        ALOGE("V4L configuration failed!, format:%c%c%c%c, w %d, h %d",
            v4l2Fmt.fourcc & 0xFF,
            (v4l2Fmt.fourcc >> 8) & 0xFF,
            (v4l2Fmt.fourcc >> 16) & 0xFF,
            (v4l2Fmt.fourcc >> 24) & 0xFF,
            v4l2Fmt.width, v4l2Fmt.height);
        return Status::INTERNAL_ERROR;
    }

    createPreviewBuffer();

    Size v4lSize = {v4l2Fmt.width, v4l2Fmt.height};
    Size thumbSize { 0, 0 };
    camera_metadata_ro_entry entry =
        mCameraCharacteristics.find(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES);
    for(uint32_t i = 0; i < entry.count; i += 2) {
        Size sz { static_cast<uint32_t>(entry.data.i32[i]),
                  static_cast<uint32_t>(entry.data.i32[i+1]) };
        if(sz.width * sz.height > thumbSize.width * thumbSize.height) {
            thumbSize = sz;
        }
    }

    if (thumbSize.width * thumbSize.height == 0) {
        ALOGE("%s: non-zero thumbnail size not available", __FUNCTION__);
        return Status::INTERNAL_ERROR;
    }

    mBlobBufferSize = blobBufferSize;
    status = mOutputThread->allocateIntermediateBuffers(v4lSize,
                mMaxThumbResolution, config.streams, blobBufferSize);
    if (status != Status::OK) {
        ALOGE("%s: allocating intermediate buffers failed!", __FUNCTION__);
        return status;
    }

    out->streams.resize(config.streams.size());
    for (size_t i = 0; i < config.streams.size(); i++) {
        out->streams[i].overrideDataSpace = config.streams[i].dataSpace;
        out->streams[i].v3_2.id = config.streams[i].id;
        // TODO: double check should we add those CAMERA flags
        mStreamMap[config.streams[i].id].usage =
                out->streams[i].v3_2.producerUsage = config.streams[i].usage |
                BufferUsage::CPU_WRITE_OFTEN |
                BufferUsage::CAMERA_OUTPUT | RK_GRALLOC_USAGE_SPECIFY_STRIDE;
        out->streams[i].v3_2.consumerUsage = 0;
        out->streams[i].v3_2.maxBuffers  = mV4L2BufferCount;

        switch (config.streams[i].format) {
            case PixelFormat::BLOB:
            case PixelFormat::YCBCR_420_888:
            case PixelFormat::YV12: // Used by SurfaceTexture
            case PixelFormat::Y16:
             case PixelFormat::YCRCB_420_SP:
                // No override
                out->streams[i].v3_2.overrideFormat = config.streams[i].format;
                break;
            case PixelFormat::IMPLEMENTATION_DEFINED:
			
                // Override based on VIDEO or not
                /*
                out->streams[i].v3_2.overrideFormat =
                        (config.streams[i].usage & BufferUsage::VIDEO_ENCODER) ?
                        PixelFormat::YCBCR_420_888 : PixelFormat::YV12;
                */
                out->streams[i].v3_2.overrideFormat = config.streams[i].format;
                // Save overridden formt in mStreamMap
                mStreamMap[config.streams[i].id].format = out->streams[i].v3_2.overrideFormat;
                break;
            default:
                ALOGE("%s: unsupported format 0x%x", __FUNCTION__, config.streams[i].format);
                return Status::ILLEGAL_ARGUMENT;
        }
    }

    mFirstRequest = true;
    return Status::OK;
}

bool ExternalFakeCameraDeviceSession::isClosed() {
    Mutex::Autolock _l(mLock);
    return mClosed;
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define UPDATE(md, tag, data, size)               \
do {                                              \
    if ((md).update((tag), (data), (size))) {     \
        ALOGE("Update " #tag " failed!");         \
        return BAD_VALUE;                         \
    }                                             \
} while (0)

status_t ExternalFakeCameraDeviceSession::initDefaultRequests() {
    ::android::hardware::camera::common::V1_0::helper::CameraMetadata md;

    const uint8_t aberrationMode = ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF;
    UPDATE(md, ANDROID_COLOR_CORRECTION_ABERRATION_MODE, &aberrationMode, 1);

    const int32_t exposureCompensation = 0;
    UPDATE(md, ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &exposureCompensation, 1);

    const uint8_t videoStabilizationMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    UPDATE(md, ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &videoStabilizationMode, 1);

    const uint8_t awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
    UPDATE(md, ANDROID_CONTROL_AWB_MODE, &awbMode, 1);

    const uint8_t aeMode = ANDROID_CONTROL_AE_MODE_ON;
    UPDATE(md, ANDROID_CONTROL_AE_MODE, &aeMode, 1);

    const uint8_t aePrecaptureTrigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
    UPDATE(md, ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, &aePrecaptureTrigger, 1);

    const uint8_t afMode = ANDROID_CONTROL_AF_MODE_AUTO;
    UPDATE(md, ANDROID_CONTROL_AF_MODE, &afMode, 1);

    const uint8_t afTrigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    UPDATE(md, ANDROID_CONTROL_AF_TRIGGER, &afTrigger, 1);

    const uint8_t sceneMode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
    UPDATE(md, ANDROID_CONTROL_SCENE_MODE, &sceneMode, 1);

    const uint8_t effectMode = ANDROID_CONTROL_EFFECT_MODE_OFF;
    UPDATE(md, ANDROID_CONTROL_EFFECT_MODE, &effectMode, 1);

    const uint8_t flashMode = ANDROID_FLASH_MODE_OFF;
    UPDATE(md, ANDROID_FLASH_MODE, &flashMode, 1);

    const int32_t thumbnailSize[] = {240, 180};
    UPDATE(md, ANDROID_JPEG_THUMBNAIL_SIZE, thumbnailSize, 2);

    const uint8_t jpegQuality = 90;
    UPDATE(md, ANDROID_JPEG_QUALITY, &jpegQuality, 1);
    UPDATE(md, ANDROID_JPEG_THUMBNAIL_QUALITY, &jpegQuality, 1);

    const int32_t jpegOrientation = 0;
    UPDATE(md, ANDROID_JPEG_ORIENTATION, &jpegOrientation, 1);

    const uint8_t oisMode = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
    UPDATE(md, ANDROID_LENS_OPTICAL_STABILIZATION_MODE, &oisMode, 1);

    const uint8_t nrMode = ANDROID_NOISE_REDUCTION_MODE_OFF;
    UPDATE(md, ANDROID_NOISE_REDUCTION_MODE, &nrMode, 1);

    const int32_t testPatternModes = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
    UPDATE(md, ANDROID_SENSOR_TEST_PATTERN_MODE, &testPatternModes, 1);

    const uint8_t fdMode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    UPDATE(md, ANDROID_STATISTICS_FACE_DETECT_MODE, &fdMode, 1);

    const uint8_t hotpixelMode = ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF;
    UPDATE(md, ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE, &hotpixelMode, 1);

    bool support30Fps = false;
    int32_t maxFps = std::numeric_limits<int32_t>::min();
    for (const auto& supportedFormat : mSupportedFormats) {
        for (const auto& fr : supportedFormat.frameRates) {
            int32_t framerateInt = static_cast<int32_t>(fr.getDouble());
            if (maxFps < framerateInt) {
                maxFps = framerateInt;
            }
            if (framerateInt == 30) {
                support30Fps = true;
                break;
            }
        }
        if (support30Fps) {
            break;
        }
    }
    int32_t defaultFramerate = support30Fps ? 30 : maxFps;
    int32_t defaultFpsRange[] = {defaultFramerate / 2, defaultFramerate};
    UPDATE(md, ANDROID_CONTROL_AE_TARGET_FPS_RANGE, defaultFpsRange, ARRAY_SIZE(defaultFpsRange));

    uint8_t antibandingMode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    UPDATE(md, ANDROID_CONTROL_AE_ANTIBANDING_MODE, &antibandingMode, 1);

    const uint8_t controlMode = ANDROID_CONTROL_MODE_AUTO;
    UPDATE(md, ANDROID_CONTROL_MODE, &controlMode, 1);

    auto requestTemplates = hidl_enum_range<RequestTemplate>();
    for (RequestTemplate type : requestTemplates) {
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata mdCopy = md;
        uint8_t intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
        switch (type) {
            case RequestTemplate::PREVIEW:
                intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
                break;
            case RequestTemplate::STILL_CAPTURE:
                intent = ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
                break;
            case RequestTemplate::VIDEO_RECORD:
                intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
                break;
            case RequestTemplate::VIDEO_SNAPSHOT:
                intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
                break;
            default:
                ALOGV("%s: unsupported RequestTemplate type %d", __FUNCTION__, type);
                continue;
        }
        UPDATE(mdCopy, ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);

        camera_metadata_t* rawMd = mdCopy.release();
        CameraMetadata hidlMd;
        hidlMd.setToExternal(
                (uint8_t*) rawMd, get_camera_metadata_size(rawMd));
        mDefaultRequests[type] = hidlMd;
        free_camera_metadata(rawMd);
    }

    return OK;
}

status_t ExternalFakeCameraDeviceSession::fillCaptureResult(
        common::V1_0::helper::CameraMetadata &md, nsecs_t timestamp) {
    bool afTrigger = false;
    {
        std::lock_guard<std::mutex> lk(mAfTriggerLock);
        afTrigger = mAfTrigger;
        if (md.exists(ANDROID_CONTROL_AF_TRIGGER)) {
            camera_metadata_entry entry = md.find(ANDROID_CONTROL_AF_TRIGGER);
            if (entry.data.u8[0] == ANDROID_CONTROL_AF_TRIGGER_START) {
                mAfTrigger = afTrigger = true;
            } else if (entry.data.u8[0] == ANDROID_CONTROL_AF_TRIGGER_CANCEL) {
                mAfTrigger = afTrigger = false;
            }
        }
    }

    // For USB camera, the USB camera handles everything and we don't have control
    // over AF. We only simply fake the AF metadata based on the request
    // received here.
    uint8_t afState;
    if (afTrigger) {
        afState = ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED;
    } else {
        afState = ANDROID_CONTROL_AF_STATE_INACTIVE;
    }
    UPDATE(md, ANDROID_CONTROL_AF_STATE, &afState, 1);

    camera_metadata_ro_entry activeArraySize =
            mCameraCharacteristics.find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);

    return fillCaptureResultCommon(md, timestamp, activeArraySize);
}

#undef ARRAY_SIZE
#undef UPDATE

}  // namespace implementation
}  // namespace V3_4
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
