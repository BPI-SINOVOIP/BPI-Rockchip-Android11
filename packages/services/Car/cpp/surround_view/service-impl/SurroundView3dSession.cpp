/*
 * Copyright 2020 The Android Open Source Project
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
#define ATRACE_TAG ATRACE_TAG_CAMERA

#include "SurroundView3dSession.h"

#include <android-base/logging.h>
#include <android/hardware_buffer.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <hidlmemory/mapping.h>
#include <system/camera_metadata.h>
#include <utils/SystemClock.h>
#include <utils/Trace.h>

#include <array>
#include <thread>
#include <set>

#include <android/hardware/camera/device/3.2/ICameraDevice.h>

#include "CameraUtils.h"
#include "sv_3d_params.h"

using ::std::adopt_lock;
using ::std::array;
using ::std::lock;
using ::std::lock_guard;
using ::std::map;
using ::std::mutex;
using ::std::scoped_lock;
using ::std::set;
using ::std::string;
using ::std::thread;
using ::std::unique_lock;
using ::std::unique_ptr;
using ::std::vector;

using ::android::hardware::automotive::evs::V1_0::EvsResult;
using ::android::hardware::camera::device::V3_2::Stream;
using ::android::hardware::hidl_memory;
using ::android::hidl::memory::V1_0::IMemory;

using GraphicsPixelFormat = ::android::hardware::graphics::common::V1_0::PixelFormat;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

typedef struct {
    int32_t id;
    int32_t width;
    int32_t height;
    int32_t format;
    int32_t direction;
    int32_t framerate;
} RawStreamConfig;

static const size_t kStreamCfgSz = sizeof(RawStreamConfig) / sizeof(int32_t);
static const uint8_t kGrayColor = 128;
static const int kNumFrames = 4;
static const int kInputNumChannels = 4;
static const int kOutputNumChannels = 4;
static const float kUndistortionScales[4] = {1.0f, 1.0f, 1.0f, 1.0f};

SurroundView3dSession::FramesHandler::FramesHandler(
    sp<IEvsCamera> pCamera, sp<SurroundView3dSession> pSession)
    : mCamera(pCamera),
      mSession(pSession) {}

Return<void> SurroundView3dSession::FramesHandler::deliverFrame(
    const BufferDesc_1_0& bufDesc_1_0) {
    LOG(INFO) << "Ignores a frame delivered from v1.0 EVS service.";
    mCamera->doneWithFrame(bufDesc_1_0);

    return {};
}

Return<void> SurroundView3dSession::FramesHandler::deliverFrame_1_1(
    const hidl_vec<BufferDesc_1_1>& buffers) {
    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    LOG(INFO) << "Received " << buffers.size() << " frames from the camera";
    mSession->mSequenceId++;

    {
        scoped_lock<mutex> lock(mSession->mAccessLock);
        if (mSession->mProcessingEvsFrames) {
            LOG(WARNING) << "EVS frames are being processed. Skip frames:"
                         << mSession->mSequenceId;
            mCamera->doneWithFrame_1_1(buffers);
            return {};
        } else {
            // Sets the flag to true immediately so the new coming frames will
            // be skipped.
            mSession->mProcessingEvsFrames = true;
        }
    }

    if (buffers.size() != kNumFrames) {
        scoped_lock<mutex> lock(mSession->mAccessLock);
        LOG(ERROR) << "The number of incoming frames is " << buffers.size()
                   << ", which is different from the number " << kNumFrames
                   << ", specified in config file";
        mSession->mProcessingEvsFrames = false;
        mCamera->doneWithFrame_1_1(buffers);
        return {};
    }

    {
        scoped_lock<mutex> lock(mSession->mAccessLock);

        // The incoming frames may not follow the same order as listed cameras.
        // We should re-order them following the camera ids listed in camera
        // config.
        vector<int> indices;
        for (const auto& id
                : mSession->mIOModuleConfig->cameraConfig.evsCameraIds) {
            for (int i = 0; i < kNumFrames; i++) {
                if (buffers[i].deviceId == id) {
                    indices.emplace_back(i);
                    break;
                }
            }
        }

        // If the size of indices is smaller than the kNumFrames, it means that
        // there is frame(s) that comes from different camera(s) than we
        // expected.
        if (indices.size() != kNumFrames) {
            LOG(ERROR) << "The frames are not from the cameras we expected!";
            mSession->mProcessingEvsFrames = false;
            mCamera->doneWithFrame_1_1(buffers);
            return {};
        }

        for (int i = 0; i < kNumFrames; i++) {
            LOG(DEBUG) << "Copying buffer from camera ["
                       << buffers[indices[i]].deviceId
                       << "] to Surround View Service";
            mSession->copyFromBufferToPointers(buffers[indices[i]],
                                               mSession->mInputPointers[i]);
        }
    }

    mCamera->doneWithFrame_1_1(buffers);

    // Notify the session that a new set of frames is ready
    mSession->mFramesSignal.notify_all();

    ATRACE_END();

    return {};
}

Return<void> SurroundView3dSession::FramesHandler::notify(const EvsEventDesc& event) {
    switch(event.aType) {
        case EvsEventType::STREAM_STOPPED:
            // The Surround View STREAM_STOPPED event is generated when the
            // service finished processing the queued frames. So it does not
            // rely on the Evs STREAM_STOPPED event.
            LOG(INFO) << "Received a STREAM_STOPPED event from Evs.";
            break;

        case EvsEventType::PARAMETER_CHANGED:
            LOG(INFO) << "Camera parameter " << std::hex << event.payload[0]
                      << " is set to " << event.payload[1];
            break;

        // Below events are ignored in reference implementation.
        case EvsEventType::STREAM_STARTED:
        [[fallthrough]];
        case EvsEventType::FRAME_DROPPED:
        [[fallthrough]];
        case EvsEventType::TIMEOUT:
            LOG(INFO) << "Event " << std::hex << static_cast<unsigned>(event.aType)
                      << "is received but ignored.";
            break;
        default:
            LOG(ERROR) << "Unknown event id: " << static_cast<unsigned>(event.aType);
            break;
    }

    return {};
}

bool SurroundView3dSession::copyFromBufferToPointers(
    BufferDesc_1_1 buffer, SurroundViewInputBufferPointers pointers) {

    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    AHardwareBuffer_Desc* pDesc =
        reinterpret_cast<AHardwareBuffer_Desc *>(&buffer.buffer.description);

    ATRACE_BEGIN("Create Graphic Buffer");
    // create a GraphicBuffer from the existing handle
    sp<GraphicBuffer> inputBuffer = new GraphicBuffer(
        buffer.buffer.nativeHandle, GraphicBuffer::CLONE_HANDLE, pDesc->width,
        pDesc->height, pDesc->format, pDesc->layers,
        GRALLOC_USAGE_HW_TEXTURE, pDesc->stride);

    if (inputBuffer == nullptr) {
        LOG(ERROR) << "Failed to allocate GraphicBuffer to wrap image handle";
        // Returning "true" in this error condition because we already released the
        // previous image (if any) and so the texture may change in unpredictable
        // ways now!
        return false;
    } else {
        LOG(INFO) << "Managed to allocate GraphicBuffer with "
                  << " width: " << pDesc->width
                  << " height: " << pDesc->height
                  << " format: " << pDesc->format
                  << " stride: " << pDesc->stride;
    }
    ATRACE_END();

    ATRACE_BEGIN("Lock input buffer (gpu to cpu)");
    // Lock the input GraphicBuffer and map it to a pointer.  If we failed to
    // lock, return false.
    void* inputDataPtr;
    inputBuffer->lock(
        GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_NEVER,
        &inputDataPtr);
    if (!inputDataPtr) {
        LOG(ERROR) << "Failed to gain read access to GraphicBuffer";
        inputBuffer->unlock();
        return false;
    } else {
        LOG(INFO) << "Managed to get read access to GraphicBuffer";
    }
    ATRACE_END();

    ATRACE_BEGIN("Copy input data");
    // Both source and destination are with 4 channels
    memcpy(pointers.cpu_data_pointer, inputDataPtr,
           pDesc->height * pDesc->width * kInputNumChannels);
    LOG(INFO) << "Buffer copying finished";
    ATRACE_END();

    ATRACE_BEGIN("Unlock input buffer (cpu to gpu)");
    inputBuffer->unlock();
    ATRACE_END();

    // Paired with ATRACE_BEGIN in the beginning of the method.
    ATRACE_END();

    return true;
}

void SurroundView3dSession::processFrames() {
    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    ATRACE_BEGIN("SV core lib method: Start3dPipeline");
    if (mSurroundView->Start3dPipeline()) {
        LOG(INFO) << "Start3dPipeline succeeded";
    } else {
        LOG(ERROR) << "Start3dPipeline failed";
        return;
    }
    ATRACE_END();

    while (true) {
        {
            unique_lock<mutex> lock(mAccessLock);

            if (mStreamState != RUNNING) {
                break;
            }

            mFramesSignal.wait(lock, [this]() { return mProcessingEvsFrames; });
        }

        handleFrames(mSequenceId);

        {
            // Set the boolean to false to receive the next set of frames.
            scoped_lock<mutex> lock(mAccessLock);
            mProcessingEvsFrames = false;
        }
    }

    // Notify the SV client that no new results will be delivered.
    LOG(DEBUG) << "Notify SvEvent::STREAM_STOPPED";
    mStream->notify(SvEvent::STREAM_STOPPED);

    {
        scoped_lock<mutex> lock(mAccessLock);
        mStreamState = STOPPED;
        mStream = nullptr;
        LOG(DEBUG) << "Stream marked STOPPED.";
    }

    ATRACE_END();
}

SurroundView3dSession::SurroundView3dSession(sp<IEvsEnumerator> pEvs,
                                             VhalHandler* vhalHandler,
                                             AnimationModule* animationModule,
                                             IOModuleConfig* pConfig) :
      mEvs(pEvs),
      mStreamState(STOPPED),
      mVhalHandler(vhalHandler),
      mAnimationModule(animationModule),
      mIOModuleConfig(pConfig) {}

SurroundView3dSession::~SurroundView3dSession() {
    // In case the client did not call stopStream properly, we should stop the
    // stream explicitly. Otherwise the process thread will take forever to
    // join.
    stopStream();

    // Waiting for the process thread to finish the buffered frames.
    if (mProcessThread.joinable()) {
        mProcessThread.join();
    }

    mEvs->closeCamera(mCamera);
}

// Methods from ::android::hardware::automotive::sv::V1_0::ISurroundViewSession.
Return<SvResult> SurroundView3dSession::startStream(
    const sp<ISurroundViewStream>& stream) {
    LOG(DEBUG) << __FUNCTION__;
    scoped_lock<mutex> lock(mAccessLock);

    if (!mIsInitialized && !initialize()) {
        LOG(ERROR) << "There is an error while initializing the use case. "
                   << "Exiting";
        return SvResult::INTERNAL_ERROR;
    }

    if (mStreamState != STOPPED) {
        LOG(ERROR) << "Ignoring startVideoStream call when a stream is "
                   << "already running.";
        return SvResult::INTERNAL_ERROR;
    }

    if (mViews.empty()) {
        LOG(ERROR) << "No views have been set for current Surround View"
                   << "3d Session. Please call setViews before starting"
                   << "the stream.";
        return SvResult::VIEW_NOT_SET;
    }

    if (stream == nullptr) {
        LOG(ERROR) << "The input stream is invalid";
        return SvResult::INTERNAL_ERROR;
    }
    mStream = stream;

    mSequenceId = 0;
    startEvs();

    if (mVhalHandler != nullptr) {
        if (!mVhalHandler->startPropertiesUpdate()) {
            LOG(WARNING) << "VhalHandler cannot be started properly";
        }
    } else {
        LOG(WARNING) << "VhalHandler is null. Ignored";
    }

    // TODO(b/158131080): the STREAM_STARTED event is not implemented in EVS
    // reference implementation yet. Once implemented, this logic should be
    // moved to EVS notify callback.
    LOG(DEBUG) << "Notify SvEvent::STREAM_STARTED";
    mStream->notify(SvEvent::STREAM_STARTED);
    mProcessingEvsFrames = false;

    // Start the frame generation thread
    mStreamState = RUNNING;

    mProcessThread = thread([this]() {
        processFrames();
    });

    return SvResult::OK;
}

Return<void> SurroundView3dSession::stopStream() {
    LOG(DEBUG) << __FUNCTION__;
    unique_lock <mutex> lock(mAccessLock);

    if (mVhalHandler != nullptr) {
        mVhalHandler->stopPropertiesUpdate();
    } else {
        LOG(WARNING) << "VhalHandler is null. Ignored";
    }

    if (mStreamState == RUNNING) {
        // Tell the processFrames loop to stop processing frames
        mStreamState = STOPPING;

        // Stop the EVS stream asynchronizely
        mCamera->stopVideoStream();
    }

    return {};
}

Return<void> SurroundView3dSession::doneWithFrames(
    const SvFramesDesc& svFramesDesc){
    LOG(DEBUG) << __FUNCTION__;
    scoped_lock <mutex> lock(mAccessLock);

    mFramesRecord.inUse = false;

    (void)svFramesDesc;
    return {};
}

// Methods from ISurroundView3dSession follow.
Return<SvResult> SurroundView3dSession::setViews(
    const hidl_vec<View3d>& views) {
    LOG(DEBUG) << __FUNCTION__;
    scoped_lock <mutex> lock(mAccessLock);

    if (views.size() == 0) {
        LOG(ERROR) << "Empty view argument, at-least one view is required.";
        return SvResult::VIEW_NOT_SET;
    }

    mViews.resize(views.size());
    for (int i=0; i<views.size(); i++) {
        mViews[i] = views[i];
    }

    return SvResult::OK;
}

Return<SvResult> SurroundView3dSession::set3dConfig(const Sv3dConfig& sv3dConfig) {
    LOG(DEBUG) << __FUNCTION__;
    scoped_lock <mutex> lock(mAccessLock);

    if (sv3dConfig.width <=0 || sv3dConfig.width > 4096) {
        LOG(WARNING) << "The width of 3d config is out of the range (0, 4096]"
                     << "Ignored!";
        return SvResult::INVALID_ARG;
    }

    if (sv3dConfig.height <=0 || sv3dConfig.height > 4096) {
        LOG(WARNING) << "The height of 3d config is out of the range (0, 4096]"
                     << "Ignored!";
        return SvResult::INVALID_ARG;
    }

    mConfig.width = sv3dConfig.width;
    mConfig.height = sv3dConfig.height;
    mConfig.carDetails = sv3dConfig.carDetails;

    if (mStream != nullptr) {
        LOG(DEBUG) << "Notify SvEvent::CONFIG_UPDATED";
        mStream->notify(SvEvent::CONFIG_UPDATED);
    }

    return SvResult::OK;
}

Return<void> SurroundView3dSession::get3dConfig(get3dConfig_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;

    _hidl_cb(mConfig);
    return {};
}

bool VerifyAndGetOverlays(const OverlaysData& overlaysData, std::vector<Overlay>* svCoreOverlays) {
    // Clear the overlays.
    svCoreOverlays->clear();

    // Check size of shared memory matches overlaysMemoryDesc.
    const int kVertexSize = 16;
    const int kIdSize = 2;
    int memDescSize = 0;
    for (auto& overlayMemDesc : overlaysData.overlaysMemoryDesc) {
        memDescSize += kIdSize + kVertexSize * overlayMemDesc.verticesCount;
    }
    if (overlaysData.overlaysMemory.size() < memDescSize) {
        LOG(ERROR) << "Allocated shared memory size is less than overlaysMemoryDesc size.";
        return false;
    }

    // Map memory.
    sp<IMemory> pSharedMemory = mapMemory(overlaysData.overlaysMemory);
    if(pSharedMemory == nullptr) {
        LOG(ERROR) << "mapMemory failed.";
        return false;
    }

    // Get Data pointer.
    uint8_t* pData = static_cast<uint8_t*>(
        static_cast<void*>(pSharedMemory->getPointer()));
    if (pData == nullptr) {
        LOG(ERROR) << "Shared memory getPointer() failed.";
        return false;
    }

    int idOffset = 0;
    set<uint16_t> overlayIdSet;
    for (auto& overlayMemDesc : overlaysData.overlaysMemoryDesc) {

        if (overlayIdSet.find(overlayMemDesc.id) != overlayIdSet.end()) {
            LOG(ERROR) << "Duplicate id within memory descriptor.";
            svCoreOverlays->clear();
            return false;
        }
        overlayIdSet.insert(overlayMemDesc.id);

        if(overlayMemDesc.verticesCount < 3) {
            LOG(ERROR) << "Less than 3 vertices.";
            svCoreOverlays->clear();
            return false;
        }

        if (overlayMemDesc.overlayPrimitive == OverlayPrimitive::TRIANGLES &&
                overlayMemDesc.verticesCount % 3 != 0) {
            LOG(ERROR) << "Triangles primitive does not have vertices "
                       << "multiple of 3.";
            svCoreOverlays->clear();
            return false;
        }

        const uint16_t overlayId = *((uint16_t*)(pData + idOffset));

        if (overlayId != overlayMemDesc.id) {
            LOG(ERROR) << "Overlay id mismatch " << overlayId << ", " << overlayMemDesc.id;
            svCoreOverlays->clear();
            return false;
        }

        // Copy over shared memory data to sv core overlays.
        Overlay svCoreOverlay;
        svCoreOverlay.id = overlayMemDesc.id;
        svCoreOverlay.vertices.resize(overlayMemDesc.verticesCount);
        uint8_t* verticesDataPtr = pData + idOffset + kIdSize;
        memcpy(svCoreOverlay.vertices.data(), verticesDataPtr,
                kVertexSize * overlayMemDesc.verticesCount);
        svCoreOverlays->push_back(svCoreOverlay);

        idOffset += kIdSize + (kVertexSize * overlayMemDesc.verticesCount);
    }

    return true;
}

Return<SvResult>  SurroundView3dSession::updateOverlays(const OverlaysData& overlaysData) {
    LOG(DEBUG) << __FUNCTION__;

    scoped_lock <mutex> lock(mAccessLock);
    if(!VerifyAndGetOverlays(overlaysData, &mOverlays)) {
        LOG(ERROR) << "VerifyAndGetOverlays failed.";
        return SvResult::INVALID_ARG;
    }

    mOverlayIsUpdated = true;
    return SvResult::OK;
}

Return<void> SurroundView3dSession::projectCameraPointsTo3dSurface(
        const hidl_vec<Point2dInt>& cameraPoints, const hidl_string& cameraId,
        projectCameraPointsTo3dSurface_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;
    bool cameraIdFound = false;
    int cameraIndex = 0;
    std::vector<Point3dFloat> points3d;

    // Note: mEvsCameraIds must be in the order front, right, rear, left.
    for (auto& evsCameraId : mEvsCameraIds) {
        if (cameraId == evsCameraId) {
            cameraIdFound = true;
            LOG(DEBUG) << "Camera id found for projection: " << cameraId;
            break;
        }
        cameraIndex++;
    }

    if (!cameraIdFound) {
        LOG(ERROR) << "Camera id not found for projection: " << cameraId;
        _hidl_cb(points3d);
        return {};
    }

    for (const auto& cameraPoint : cameraPoints) {
        Point3dFloat point3d = {false, 0.0, 0.0, 0.0};

        // Verify if camera point is within the camera resolution bounds.
        const Size2dInteger cameraSize = mCameraParams[cameraIndex].size;
        point3d.isValid = (cameraPoint.x >= 0 && cameraPoint.x < cameraSize.width &&
                           cameraPoint.y >= 0 && cameraPoint.y < cameraSize.height);
        if (!point3d.isValid) {
            LOG(WARNING) << "Camera point (" << cameraPoint.x << ", " << cameraPoint.y
                         << ") is out of camera resolution bounds.";
            points3d.push_back(point3d);
            continue;
        }

        // Project points using mSurroundView function.
        const Coordinate2dInteger camCoord(cameraPoint.x, cameraPoint.y);
        Coordinate3dFloat projPoint3d(0.0, 0.0, 0.0);
        point3d.isValid =
                mSurroundView->GetProjectionPointFromRawCameraToSurroundView3d(camCoord,
                                                                               cameraIndex,
                                                                               &projPoint3d);
        // Convert projPoint3d in meters to point3d which is in milli-meters.
        point3d.x = projPoint3d.x * 1000.0;
        point3d.y = projPoint3d.y * 1000.0;
        point3d.z = projPoint3d.z * 1000.0;
        points3d.push_back(point3d);
    }
    _hidl_cb(points3d);
    return {};
}

bool SurroundView3dSession::handleFrames(int sequenceId) {
    LOG(INFO) << __FUNCTION__ << "Handling sequenceId " << sequenceId << ".";

    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    // TODO(b/157498592): Now only one sets of EVS input frames and one SV
    // output frame is supported. Implement buffer queue for both of them.
    {
        scoped_lock<mutex> lock(mAccessLock);

        if (mFramesRecord.inUse) {
            LOG(DEBUG) << "Notify SvEvent::FRAME_DROPPED";
            mStream->notify(SvEvent::FRAME_DROPPED);
            return true;
        }
    }

    // If the width/height was changed, re-allocate the data pointer.
    if (mOutputWidth != mConfig.width
        || mOutputHeight != mConfig.height) {
        LOG(DEBUG) << "Config changed. Re-allocate memory. "
                   << "Old width: "
                   << mOutputWidth
                   << ", old height: "
                   << mOutputHeight
                   << "; New width: "
                   << mConfig.width
                   << ", new height: "
                   << mConfig.height;
        delete[] static_cast<char*>(mOutputPointer.cpu_data_pointer);
        mOutputWidth = mConfig.width;
        mOutputHeight = mConfig.height;
        mOutputPointer.height = mOutputHeight;
        mOutputPointer.width = mOutputWidth;
        mOutputPointer.format = Format::RGBA;
        mOutputPointer.cpu_data_pointer =
                static_cast<void*>(new char[mOutputHeight * mOutputWidth * kOutputNumChannels]);

        if (!mOutputPointer.cpu_data_pointer) {
            LOG(ERROR) << "Memory allocation failed. Exiting.";
            return false;
        }

        Size2dInteger size = Size2dInteger(mOutputWidth, mOutputHeight);
        mSurroundView->Update3dOutputResolution(size);

        mSvTexture = new GraphicBuffer(mOutputWidth,
                                       mOutputHeight,
                                       HAL_PIXEL_FORMAT_RGBA_8888,
                                       1,
                                       GRALLOC_USAGE_HW_TEXTURE,
                                       "SvTexture");
        if (mSvTexture->initCheck() == OK) {
            LOG(INFO) << "Successfully allocated Graphic Buffer";
        } else {
            LOG(ERROR) << "Failed to allocate Graphic Buffer";
            return false;
        }
    }

    ATRACE_BEGIN("SV core lib method: Set3dOverlay");
    // Set 3d overlays.
    {
        scoped_lock<mutex> lock(mAccessLock);
        if (mOverlayIsUpdated) {
            if (!mSurroundView->Set3dOverlay(mOverlays)) {
                LOG(ERROR) << "Set 3d overlays failed.";
            }
            mOverlayIsUpdated = false;
        }
    }
    ATRACE_END();

    ATRACE_BEGIN("VhalHandler method: getPropertyValues");
    // Get the latest VHal property values
    if (mVhalHandler != nullptr) {
        if (!mVhalHandler->getPropertyValues(&mPropertyValues)) {
            LOG(ERROR) << "Failed to get property values";
        }
    } else {
        LOG(WARNING) << "VhalHandler is null. Ignored";
    }
    ATRACE_END();

    ATRACE_BEGIN("AnimationModule method: getUpdatedAnimationParams");
    vector<AnimationParam> params;
    if (mAnimationModule != nullptr) {
        params = mAnimationModule->getUpdatedAnimationParams(mPropertyValues);
    } else {
        LOG(WARNING) << "AnimationModule is null. Ignored";
    }
    ATRACE_END();

    ATRACE_BEGIN("SV core lib method: SetAnimations");
    if (!params.empty()) {
        mSurroundView->SetAnimations(params);
    } else {
        LOG(INFO) << "AnimationParams is empty. Ignored";
    }
    ATRACE_END();

    // Get the view.
    // TODO(161399517): Only single view is currently supported, add support for multiple views.
    const View3d view3d = mViews[0];
    const RotationQuat quat = view3d.pose.rotation;
    const Translation trans = view3d.pose.translation;
    const std::array<float, 4> viewQuaternion = {quat.x, quat.y, quat.z, quat.w};
    const std::array<float, 3> viewTranslation = {trans.x, trans.y, trans.z};

    ATRACE_BEGIN("SV core lib method: Get3dSurroundView");
    if (mSurroundView->Get3dSurroundView(
            mInputPointers, viewQuaternion, viewTranslation, &mOutputPointer)) {
        LOG(INFO) << "Get3dSurroundView succeeded";
    } else {
        LOG(ERROR) << "Get3dSurroundView failed. "
                   << "Using memset to initialize to gray.";
        memset(mOutputPointer.cpu_data_pointer, kGrayColor,
               mOutputHeight * mOutputWidth * kOutputNumChannels);
    }
    ATRACE_END();

    ATRACE_BEGIN("Lock output texture (gpu to cpu)");
    void* textureDataPtr = nullptr;
    mSvTexture->lock(GRALLOC_USAGE_SW_WRITE_OFTEN
                    | GRALLOC_USAGE_SW_READ_NEVER,
                    &textureDataPtr);
    ATRACE_END();

    if (!textureDataPtr) {
        LOG(ERROR) << "Failed to gain write access to GraphicBuffer!";
        return false;
    }

    ATRACE_BEGIN("Copy output result");
    // Note: there is a chance that the stride of the texture is not the
    // same as the width. For example, when the input frame is 1920 * 1080,
    // the width is 1080, but the stride is 2048. So we'd better copy the
    // data line by line, instead of single memcpy.
    uint8_t* writePtr = static_cast<uint8_t*>(textureDataPtr);
    uint8_t* readPtr = static_cast<uint8_t*>(mOutputPointer.cpu_data_pointer);
    const int readStride = mOutputWidth * kOutputNumChannels;
    const int writeStride = mSvTexture->getStride() * kOutputNumChannels;
    if (readStride == writeStride) {
        memcpy(writePtr, readPtr, readStride * mSvTexture->getHeight());
    } else {
        for (int i=0; i<mSvTexture->getHeight(); i++) {
            memcpy(writePtr, readPtr, readStride);
            writePtr = writePtr + writeStride;
            readPtr = readPtr + readStride;
        }
    }
    LOG(INFO) << "memcpy finished!";
    ATRACE_END();

    ATRACE_BEGIN("Unlock output texture (cpu to gpu)");
    mSvTexture->unlock();
    ATRACE_END();

    ANativeWindowBuffer* buffer = mSvTexture->getNativeBuffer();
    LOG(DEBUG) << "ANativeWindowBuffer->handle: " << buffer->handle;

    {
        scoped_lock<mutex> lock(mAccessLock);

        mFramesRecord.frames.svBuffers.resize(1);
        SvBuffer& svBuffer = mFramesRecord.frames.svBuffers[0];
        svBuffer.viewId = 0;
        svBuffer.hardwareBuffer.nativeHandle = buffer->handle;
        AHardwareBuffer_Desc* pDesc =
            reinterpret_cast<AHardwareBuffer_Desc *>(
                &svBuffer.hardwareBuffer.description);
        pDesc->width = mOutputWidth;
        pDesc->height = mOutputHeight;
        pDesc->layers = 1;
        pDesc->usage = GRALLOC_USAGE_HW_TEXTURE;
        pDesc->stride = mSvTexture->getStride();
        pDesc->format = HAL_PIXEL_FORMAT_RGBA_8888;
        mFramesRecord.frames.timestampNs = elapsedRealtimeNano();
        mFramesRecord.frames.sequenceId = sequenceId;

        mFramesRecord.inUse = true;
        mStream->receiveFrames(mFramesRecord.frames);
    }

    ATRACE_END();

    return true;
}

bool SurroundView3dSession::initialize() {
    lock_guard<mutex> lock(mAccessLock, adopt_lock);

    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    if (!setupEvs()) {
        LOG(ERROR) << "Failed to setup EVS components for 3d session";
        return false;
    }

    // TODO(b/150412555): ask core-lib team to add API description for "create"
    // method in the .h file.
    // The create method will never return a null pointer based the API
    // description.
    mSurroundView = unique_ptr<SurroundView>(Create());

    SurroundViewStaticDataParams params =
            SurroundViewStaticDataParams(
                    mCameraParams,
                    mIOModuleConfig->sv2dConfig.sv2dParams,
                    mIOModuleConfig->sv3dConfig.sv3dParams,
                    vector<float>(std::begin(kUndistortionScales),
                                  std::end(kUndistortionScales)),
                    mIOModuleConfig->sv2dConfig.carBoundingBox,
                    mIOModuleConfig->carModelConfig.carModel.texturesMap,
                    mIOModuleConfig->carModelConfig.carModel.partsMap);
    ATRACE_BEGIN("SV core lib method: SetStaticData");
    mSurroundView->SetStaticData(params);
    ATRACE_END();

    ATRACE_BEGIN("Allocate cpu buffers");
    mInputPointers.resize(kNumFrames);
    for (int i = 0; i < kNumFrames; i++) {
        mInputPointers[i].width = mCameraParams[i].size.width;
        mInputPointers[i].height = mCameraParams[i].size.height;
        mInputPointers[i].format = Format::RGBA;
        mInputPointers[i].cpu_data_pointer =
                static_cast<void*>(new uint8_t[mInputPointers[i].width * mInputPointers[i].height *
                                               kInputNumChannels]);
    }
    LOG(INFO) << "Allocated " << kNumFrames << " input pointers";

    mOutputWidth = mIOModuleConfig->sv3dConfig.sv3dParams.resolution.width;
    mOutputHeight = mIOModuleConfig->sv3dConfig.sv3dParams.resolution.height;

    mConfig.width = mOutputWidth;
    mConfig.height = mOutputHeight;
    mConfig.carDetails = SvQuality::HIGH;

    mOutputPointer.height = mOutputHeight;
    mOutputPointer.width = mOutputWidth;
    mOutputPointer.format = Format::RGBA;
    mOutputPointer.cpu_data_pointer =
            static_cast<void*>(new char[mOutputHeight * mOutputWidth * kOutputNumChannels]);

    if (!mOutputPointer.cpu_data_pointer) {
        LOG(ERROR) << "Memory allocation failed. Exiting.";
        return false;
    }
    ATRACE_END();

    ATRACE_BEGIN("Allocate output texture");
    mSvTexture = new GraphicBuffer(mOutputWidth,
                                   mOutputHeight,
                                   HAL_PIXEL_FORMAT_RGBA_8888,
                                   1,
                                   GRALLOC_USAGE_HW_TEXTURE,
                                   "SvTexture");

    if (mSvTexture->initCheck() == OK) {
        LOG(INFO) << "Successfully allocated Graphic Buffer";
    } else {
        LOG(ERROR) << "Failed to allocate Graphic Buffer";
        return false;
    }
    ATRACE_END();

    mIsInitialized = true;

    ATRACE_END();

    return true;
}

bool SurroundView3dSession::setupEvs() {
    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    // Reads the camera related information from the config object
    const string evsGroupId = mIOModuleConfig->cameraConfig.evsGroupId;

    // Setup for EVS
    LOG(INFO) << "Requesting camera list";
    mEvs->getCameraList_1_1(
            [this, evsGroupId] (hidl_vec<CameraDesc> cameraList) {
        LOG(INFO) << "Camera list callback received " << cameraList.size();
        for (auto&& cam : cameraList) {
            LOG(INFO) << "Found camera " << cam.v1.cameraId;
            if (cam.v1.cameraId == evsGroupId) {
                mCameraDesc = cam;
            }
        }
    });

    bool foundCfg = false;
    std::unique_ptr<Stream> targetCfg(new Stream());

    // This logic picks the configuration with the largest area that supports
    // RGBA8888 format
    int32_t maxArea = 0;
    camera_metadata_entry_t streamCfgs;
    if (!find_camera_metadata_entry(
             reinterpret_cast<camera_metadata_t *>(mCameraDesc.metadata.data()),
             ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
             &streamCfgs)) {
        // Stream configurations are found in metadata
        RawStreamConfig *ptr = reinterpret_cast<RawStreamConfig *>(
            streamCfgs.data.i32);
        for (unsigned idx = 0; idx < streamCfgs.count; idx += kStreamCfgSz) {
            if (ptr->direction ==
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT &&
                ptr->format == HAL_PIXEL_FORMAT_RGBA_8888) {

                if (ptr->width * ptr->height > maxArea) {
                    targetCfg->id = ptr->id;
                    targetCfg->width = ptr->width;
                    targetCfg->height = ptr->height;

                    // This client always wants below input data format
                    targetCfg->format =
                        static_cast<GraphicsPixelFormat>(
                            HAL_PIXEL_FORMAT_RGBA_8888);

                    maxArea = ptr->width * ptr->height;

                    foundCfg = true;
                }
            }
            ++ptr;
        }
    } else {
        LOG(WARNING) << "No stream configuration data is found; "
                     << "default parameters will be used.";
    }

    if (!foundCfg) {
        LOG(INFO) << "No config was found";
        targetCfg = nullptr;
        return false;
    }

    string camId = mCameraDesc.v1.cameraId.c_str();
    mCamera = mEvs->openCamera_1_1(camId.c_str(), *targetCfg);
    if (mCamera == nullptr) {
        LOG(ERROR) << "Failed to allocate EVS Camera interface for " << camId;
        return false;
    } else {
        LOG(INFO) << "Logical camera " << camId << " is opened successfully";
    }

    mEvsCameraIds = mIOModuleConfig->cameraConfig.evsCameraIds;
    if (mEvsCameraIds.size() < kNumFrames) {
        LOG(ERROR) << "Incorrect camera info is stored in the camera config";
        return false;
    }

    map<string, AndroidCameraParams> cameraIdToAndroidParameters;
    for (const auto& id : mEvsCameraIds) {
        AndroidCameraParams params;
        if (getAndroidCameraParams(mCamera, id, params)) {
            cameraIdToAndroidParameters.emplace(id, params);
            LOG(INFO) << "Camera parameters are fetched successfully for "
                      << "physical camera: " << id;
        } else {
            LOG(ERROR) << "Failed to get camera parameters for "
                       << "physical camera: " << id;
            return false;
        }
    }

    mCameraParams =
            convertToSurroundViewCameraParams(cameraIdToAndroidParameters);

    for (auto& camera : mCameraParams) {
        camera.size.width = targetCfg->width;
        camera.size.height = targetCfg->height;
        camera.circular_fov = 179;
    }

    // Add validity mask filenames.
    for (int i = 0; i < mCameraParams.size(); i++) {
        mCameraParams[i].validity_mask_filename = mIOModuleConfig->cameraConfig.maskFilenames[i];
    }
    ATRACE_END();
    return true;
}

bool SurroundView3dSession::startEvs() {
    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    mFramesHandler = new FramesHandler(mCamera, this);
    Return<EvsResult> result = mCamera->startVideoStream(mFramesHandler);
    if (result != EvsResult::OK) {
        LOG(ERROR) << "Failed to start video stream";
        return false;
    } else {
        LOG(INFO) << "Video stream was started successfully";
    }

    ATRACE_END();

    return true;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
