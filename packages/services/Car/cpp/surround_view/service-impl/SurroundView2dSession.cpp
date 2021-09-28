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

#include "SurroundView2dSession.h"

#include "CameraUtils.h"

#include <android-base/logging.h>
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <android/hardware_buffer.h>
#include <system/camera_metadata.h>
#include <utils/SystemClock.h>
#include <utils/Trace.h>
#include <vndk/hardware_buffer.h>

#include <thread>

using ::std::adopt_lock;
using ::std::lock;
using ::std::lock_guard;
using ::std::map;
using ::std::mutex;
using ::std::scoped_lock;
using ::std::string;
using ::std::thread;
using ::std::unique_lock;
using ::std::unique_ptr;
using ::std::vector;

using ::android::hardware::automotive::evs::V1_0::EvsResult;
using ::android::hardware::camera::device::V3_2::Stream;

using GraphicsPixelFormat = ::android::hardware::graphics::common::V1_0::PixelFormat;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// TODO(b/158479099): There are a lot of redundant code between 2d and 3d.
// Decrease the degree of redundancy.
typedef struct {
    int32_t id;
    int32_t width;
    int32_t height;
    int32_t format;
    int32_t direction;
    int32_t framerate;
} RawStreamConfig;

static const size_t kStreamCfgSz = sizeof(RawStreamConfig) / sizeof(int32_t);
static const int kInputNumChannels = 4;
static const int kOutputNumChannels = 3;
static const int kNumFrames = 4;
static const int kSv2dViewId = 0;
static const float kUndistortionScales[4] = {1.0f, 1.0f, 1.0f, 1.0f};

SurroundView2dSession::FramesHandler::FramesHandler(
    sp<IEvsCamera> pCamera, sp<SurroundView2dSession> pSession)
    : mCamera(pCamera),
      mSession(pSession) {}

Return<void> SurroundView2dSession::FramesHandler::deliverFrame(
    const BufferDesc_1_0& bufDesc_1_0) {
    LOG(INFO) << "Ignores a frame delivered from v1.0 EVS service.";
    mCamera->doneWithFrame(bufDesc_1_0);

    return {};
}

Return<void> SurroundView2dSession::FramesHandler::deliverFrame_1_1(
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

        if (indices.size() != kNumFrames) {
            LOG(ERROR) << "The frames are not from the cameras we expected!";
            mSession->mProcessingEvsFrames = false;
            mCamera->doneWithFrame_1_1(buffers);
            return {};
        }

        if (mSession->mGpuAccelerationEnabled) {
            for (int i = 0; i < kNumFrames; i++) {
                LOG(DEBUG) << "Importing graphic buffer from camera ["
                           << buffers[indices[i]].deviceId << "]";
                const AHardwareBuffer_Desc* pDesc = reinterpret_cast<const AHardwareBuffer_Desc*>(
                        &buffers[indices[i]].buffer.description);

                AHardwareBuffer* hardwareBuffer;
                status_t status = AHardwareBuffer_createFromHandle(
                        pDesc, buffers[indices[i]].buffer.nativeHandle,
                        AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_CLONE, &hardwareBuffer);

                if (status != NO_ERROR) {
                    LOG(ERROR) << "Can't create AHardwareBuffer from handle. Error: " << status;
                    return {};
                }

                mSession->mInputPointers[i].gpu_data_pointer = static_cast<void*>(hardwareBuffer);

                // Keep a reference to the EVS graphic buffers, so we can
                // release them after Surround View stitching is done.
                mSession->mEvsGraphicBuffers = buffers;
            }
        } else {
            for (int i = 0; i < kNumFrames; i++) {
                LOG(DEBUG) << "Copying buffer from camera [" << buffers[indices[i]].deviceId
                           << "] to Surround View Service";
                mSession->copyFromBufferToPointers(buffers[indices[i]],
                                                   mSession->mInputPointers[i]);
            }

            // On the CPU version, we do not need to hold the Graphic Buffers
            // any more since they are copied already.
            mCamera->doneWithFrame_1_1(buffers);
        }
    }

    // Notify the session that a new set of frames is ready
    mSession->mFramesSignal.notify_all();

    ATRACE_END();

    return {};
}

Return<void> SurroundView2dSession::FramesHandler::notify(const EvsEventDesc& event) {
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

bool SurroundView2dSession::copyFromBufferToPointers(
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
    LOG(DEBUG) << "Buffer copying finished";
    ATRACE_END();

    ATRACE_BEGIN("Unlock input buffer (cpu to gpu)");
    inputBuffer->unlock();
    ATRACE_END();

    // Paired with ATRACE_BEGIN in the beginning of the method.
    ATRACE_END();

    return true;
}

void SurroundView2dSession::processFrames() {
    ATRACE_BEGIN(__PRETTY_FUNCTION__);

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

SurroundView2dSession::SurroundView2dSession(sp<IEvsEnumerator> pEvs,
                                             IOModuleConfig* pConfig)
    : mEvs(pEvs),
      mIOModuleConfig(pConfig),
      mStreamState(STOPPED) {}

SurroundView2dSession::~SurroundView2dSession() {
    // In case the client did not call stopStream properly, we should stop the
    // stream explicitly. Otherwise the process thread will take forever to
    // join.
    stopStream();

    // Waiting for the process thread to finish the buffered frames.
    if (mProcessThread.joinable()) {
        mProcessThread.join();
    }

    mEvs->closeCamera(mCamera);

    // TODO(b/175176576): properly release the mInputPointers and mOutputPointer
}

// Methods from ::android::hardware::automotive::sv::V1_0::ISurroundViewSession
Return<SvResult> SurroundView2dSession::startStream(
    const sp<ISurroundViewStream>& stream) {
    LOG(DEBUG) << __FUNCTION__;
    scoped_lock<mutex> lock(mAccessLock);

    if (!mIsInitialized && !initialize()) {
        LOG(ERROR) << "There is an error while initializing the use case. "
                   << "Exiting";
        return SvResult::INTERNAL_ERROR;
    }

    if (mStreamState != STOPPED) {
        LOG(ERROR) << "Ignoring startVideoStream call"
                   << "when a stream is already running.";
        return SvResult::INTERNAL_ERROR;
    }

    if (stream == nullptr) {
        LOG(ERROR) << "The input stream is invalid";
        return SvResult::INTERNAL_ERROR;
    }
    mStream = stream;

    mSequenceId = 0;
    startEvs();

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

Return<void> SurroundView2dSession::stopStream() {
    LOG(DEBUG) << __FUNCTION__;
    unique_lock<mutex> lock(mAccessLock);

    if (mStreamState == RUNNING) {
        // Tell the processFrames loop to stop processing frames
        mStreamState = STOPPING;

        // Stop the EVS stream asynchronizely
        mCamera->stopVideoStream();
        mFramesHandler = nullptr;
    }

    return {};
}

Return<void> SurroundView2dSession::doneWithFrames(
    const SvFramesDesc& svFramesDesc){
    LOG(DEBUG) << __FUNCTION__;
    scoped_lock <mutex> lock(mAccessLock);

    mFramesRecord.inUse = false;

    (void)svFramesDesc;
    return {};
}

// Methods from ISurroundView2dSession follow.
Return<void> SurroundView2dSession::get2dMappingInfo(
    get2dMappingInfo_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;

    _hidl_cb(mInfo);
    return {};
}

Return<SvResult> SurroundView2dSession::set2dConfig(
    const Sv2dConfig& sv2dConfig) {
    LOG(DEBUG) << __FUNCTION__;
    scoped_lock <mutex> lock(mAccessLock);

    if (sv2dConfig.width <=0 || sv2dConfig.width > 4096) {
        LOG(WARNING) << "The width of 2d config is out of the range (0, 4096]"
                     << "Ignored!";
        return SvResult::INVALID_ARG;
    }

    mConfig.width = sv2dConfig.width;
    mConfig.blending = sv2dConfig.blending;
    mHeight = mConfig.width * mInfo.height / mInfo.width;

    if (mStream != nullptr) {
        LOG(DEBUG) << "Notify SvEvent::CONFIG_UPDATED";
        mStream->notify(SvEvent::CONFIG_UPDATED);
    }

    return SvResult::OK;
}

Return<void> SurroundView2dSession::get2dConfig(get2dConfig_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;

    _hidl_cb(mConfig);
    return {};
}

Return<void> SurroundView2dSession::projectCameraPoints(const hidl_vec<Point2dInt>& points2dCamera,
                                                        const hidl_string& cameraId,
                                                        projectCameraPoints_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;
    std::vector<Point2dFloat> outPoints;
    bool cameraIdFound = false;
    int cameraIndex = 0;
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
        _hidl_cb(outPoints);
        return {};
    }

    int width = mConfig.width;
    int height = mHeight;
    for (const auto& cameraPoint : points2dCamera) {
        Point2dFloat outPoint = {false, 0.0, 0.0};
        // Check of the camear point is within the camera resolution bounds.
        if (cameraPoint.x < 0 || cameraPoint.x > width - 1 || cameraPoint.y < 0 ||
            cameraPoint.y > height - 1) {
            LOG(WARNING) << "Camera point (" << cameraPoint.x << ", " << cameraPoint.y
                         << ") is out of camera resolution bounds.";
            outPoint.isValid = false;
            outPoints.push_back(outPoint);
            continue;
        }

        // Project points using mSurroundView function.
        const Coordinate2dInteger camPoint(cameraPoint.x, cameraPoint.y);
        Coordinate2dFloat projPoint2d(0.0, 0.0);

        outPoint.isValid =
                mSurroundView->GetProjectionPointFromRawCameraToSurroundView2d(camPoint,
                                                                               cameraIndex,
                                                                               &projPoint2d);
        outPoint.x = projPoint2d.x;
        outPoint.y = projPoint2d.y;
        outPoints.push_back(outPoint);
    }

    _hidl_cb(outPoints);
    return {};
}

// TODO(b/175176765): implement a GPU version of this method separately.
bool SurroundView2dSession::handleFrames(int sequenceId) {
    LOG(INFO) << __FUNCTION__ << "Handling sequenceId " << sequenceId << ".";

    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    // TODO(b/157498592): Now only one sets of EVS input frames and one SV
    // output frame is supported. Implement buffer queue for both of them.
    {
        scoped_lock<mutex> lock(mAccessLock);

        if (mFramesRecord.inUse) {
            LOG(DEBUG) << "Notify SvEvent::FRAME_DROPPED";
            mStream->notify(SvEvent::FRAME_DROPPED);

            // For GPU solution only (the frames were released already for CPU solution).
            if (mGpuAccelerationEnabled) {
                mCamera->doneWithFrame_1_1(mEvsGraphicBuffers);
            }
            return true;
        }
    }

    // TODO(b/175177030): modifying the width/length on the fly is not supported by the GPU approach
    // yet.
    if (!mGpuAccelerationEnabled) {
        if (mOutputWidth != mConfig.width || mOutputHeight != mHeight) {
            LOG(DEBUG) << "Config changed. Re-allocate memory."
                       << " Old width: " << mOutputWidth << " Old height: " << mOutputHeight
                       << " New width: " << mConfig.width << " New height: " << mHeight;
            delete[] static_cast<char*>(mOutputPointer.cpu_data_pointer);
            mOutputWidth = mConfig.width;
            mOutputHeight = mHeight;
            mOutputPointer.height = mOutputHeight;
            mOutputPointer.width = mOutputWidth;
            mOutputPointer.format = Format::RGB;
            mOutputPointer.cpu_data_pointer =
                    static_cast<void*>(new char[mOutputHeight * mOutputWidth * kOutputNumChannels]);

            if (!mOutputPointer.cpu_data_pointer) {
                LOG(ERROR) << "Memory allocation failed. Exiting.";
                return false;
            }

            Size2dInteger size = Size2dInteger(mOutputWidth, mOutputHeight);
            mSurroundView->Update2dOutputResolution(size);

            mSvTexture = new GraphicBuffer(mOutputWidth, mOutputHeight, HAL_PIXEL_FORMAT_RGB_888, 1,
                                           GRALLOC_USAGE_HW_TEXTURE, "SvTexture");
            if (mSvTexture->initCheck() == OK) {
                LOG(INFO) << "Successfully allocated Graphic Buffer";
            } else {
                LOG(ERROR) << "Failed to allocate Graphic Buffer";
                return false;
            }
        }
        LOG(INFO) << "Output Pointer data format: " << mOutputPointer.format;
    }

    ATRACE_BEGIN("SV core lib method: Get2dSurroundView");
    const string gpuEnabledText = mGpuAccelerationEnabled ? " with GPU acceleration flag enabled"
                                                          : " with GPU acceleration flag disabled";
    if (mSurroundView->Get2dSurroundView(mInputPointers, &mOutputPointer)) {
        LOG(INFO) << "Get2dSurroundView succeeded" << gpuEnabledText;
    } else {
        LOG(ERROR) << "Get2dSurroundView failed" << gpuEnabledText;
    }
    ATRACE_END();

    // For GPU solution only (the frames were released already for CPU solution).
    if (mGpuAccelerationEnabled) {
        ATRACE_BEGIN("Release the evs frames");
        mCamera->doneWithFrame_1_1(mEvsGraphicBuffers);
        ATRACE_END();
    }

    ANativeWindowBuffer* buffer;
    if (mGpuAccelerationEnabled) {
        buffer = mOutputHolder->getNativeBuffer();
    } else {
        ATRACE_BEGIN("Lock output texture (gpu to cpu)");
        void* textureDataPtr = nullptr;
        mSvTexture->lock(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_NEVER,
                         &textureDataPtr);
        ATRACE_END();

        if (!textureDataPtr) {
            LOG(ERROR) << "Failed to gain write access to GraphicBuffer!";
            return false;
        }

        ATRACE_BEGIN("Copy output result");
        // Note: there is a chance that the stride of the texture is not the same
        // as the width. For example, when the input frame is 1920 * 1080, the
        // width is 1080, but the stride is 2048. So we'd better copy the data line
        // by line, instead of single memcpy.
        uint8_t* writePtr = static_cast<uint8_t*>(textureDataPtr);
        uint8_t* readPtr = static_cast<uint8_t*>(mOutputPointer.cpu_data_pointer);
        const int readStride = mOutputWidth * kOutputNumChannels;
        const int writeStride = mSvTexture->getStride() * kOutputNumChannels;
        if (readStride == writeStride) {
            memcpy(writePtr, readPtr, readStride * mSvTexture->getHeight());
        } else {
            for (int i = 0; i < mSvTexture->getHeight(); i++) {
                memcpy(writePtr, readPtr, readStride);
                writePtr = writePtr + writeStride;
                readPtr = readPtr + readStride;
            }
        }
        LOG(DEBUG) << "memcpy finished";
        ATRACE_END();

        ATRACE_BEGIN("Unlock output texture (cpu to gpu)");
        mSvTexture->unlock();
        ATRACE_END();

        buffer = mSvTexture->getNativeBuffer();
        LOG(DEBUG) << "ANativeWindowBuffer->handle: " << buffer->handle;
    }

    {
        scoped_lock<mutex> lock(mAccessLock);

        mFramesRecord.frames.svBuffers.resize(1);
        SvBuffer& svBuffer = mFramesRecord.frames.svBuffers[0];
        svBuffer.viewId = kSv2dViewId;
        svBuffer.hardwareBuffer.nativeHandle = buffer->handle;
        AHardwareBuffer_Desc* pDesc =
            reinterpret_cast<AHardwareBuffer_Desc*>(
                &svBuffer.hardwareBuffer.description);
        if (mGpuAccelerationEnabled) {
            pDesc->width = mOutputPointer.width;
            pDesc->height = mOutputPointer.height;
            pDesc->stride = mOutputHolder->getStride();
            pDesc->format = HAL_PIXEL_FORMAT_RGBA_8888;
        } else {
            pDesc->width = mOutputWidth;
            pDesc->height = mOutputHeight;
            pDesc->stride = mSvTexture->getStride();
            pDesc->format = HAL_PIXEL_FORMAT_RGB_888;
        }
        pDesc->layers = 1;
        pDesc->usage = GRALLOC_USAGE_HW_TEXTURE;
        mFramesRecord.frames.timestampNs = elapsedRealtimeNano();
        mFramesRecord.frames.sequenceId = sequenceId;

        mFramesRecord.inUse = true;
        mStream->receiveFrames(mFramesRecord.frames);
    }

    ATRACE_END();

    return true;
}

// TODO(b/175176765): consider to HW-specific initialization procedures into
// separate methods.
bool SurroundView2dSession::initialize() {
    lock_guard<mutex> lock(mAccessLock, adopt_lock);

    ATRACE_BEGIN(__PRETTY_FUNCTION__);

    if (!setupEvs()) {
        LOG(ERROR) << "Failed to setup EVS components for 2d session";
        return false;
    }

    // TODO(b/150412555): ask core-lib team to add API description for "create"
    // method in the .h file.
    // The create method will never return a null pointer based the API
    // description.
    mSurroundView = unique_ptr<SurroundView>(Create());

    SurroundViewStaticDataParams params =
            SurroundViewStaticDataParams(mCameraParams,
                                         mIOModuleConfig->sv2dConfig.sv2dParams,
                                         mIOModuleConfig->sv3dConfig.sv3dParams,
                                         vector<float>(std::begin(kUndistortionScales),
                                                       std::end(kUndistortionScales)),
                                         mIOModuleConfig->sv2dConfig.carBoundingBox,
                                         mIOModuleConfig->carModelConfig.carModel.texturesMap,
                                         mIOModuleConfig->carModelConfig.carModel.partsMap);
    mGpuAccelerationEnabled = mIOModuleConfig->sv2dConfig.sv2dParams.gpu_acceleration_enabled;

    ATRACE_BEGIN("SV core lib method: SetStaticData");
    mSurroundView->SetStaticData(params);
    ATRACE_END();

    ATRACE_BEGIN("SV core lib method: Start2dPipeline");
    const string gpuEnabledText = mGpuAccelerationEnabled ? "with GPU acceleration flag enabled"
                                                          : "with GPU acceleration flag disabled";
    if (mSurroundView->Start2dPipeline()) {
        LOG(INFO) << "Start2dPipeline succeeded " << gpuEnabledText;
    } else {
        LOG(ERROR) << "Start2dPipeline failed " << gpuEnabledText;
        return false;
    }
    ATRACE_END();

    ATRACE_BEGIN("Allocate cpu buffers");
    mInputPointers.resize(kNumFrames);
    for (int i = 0; i < kNumFrames; i++) {
        mInputPointers[i].width = mCameraParams[i].size.width;
        mInputPointers[i].height = mCameraParams[i].size.height;

        // Only allocate CPU memory for CPU solution
        // For GPU solutions, the Graphic Buffers from EVS will be converted and
        // stored in gpu_data_pointer
        if (!mGpuAccelerationEnabled) {
            mInputPointers[i].format = Format::RGBA;
            mInputPointers[i].cpu_data_pointer =
                    static_cast<void*>(new char[mInputPointers[i].width * mInputPointers[i].height *
                                                kInputNumChannels]);
        }
    }
    LOG(INFO) << "Allocated " << kNumFrames << " input pointers";

    mOutputWidth = mIOModuleConfig->sv2dConfig.sv2dParams.resolution.width;
    mOutputHeight = mIOModuleConfig->sv2dConfig.sv2dParams.resolution.height;

    mConfig.width = mOutputWidth;
    mConfig.blending = SvQuality::HIGH;
    mHeight = mOutputHeight;

    mOutputPointer.height = mOutputHeight;
    mOutputPointer.width = mOutputWidth;

    // Only allocate CPU memory for CPU solution
    if (!mGpuAccelerationEnabled) {
        mOutputPointer.format = Format::RGB;
        mOutputPointer.cpu_data_pointer =
                static_cast<void*>(new char[mOutputHeight * mOutputWidth * kOutputNumChannels]);

        if (!mOutputPointer.cpu_data_pointer) {
            LOG(ERROR) << "Memory allocation failed. Exiting.";
            return false;
        }
    }
    ATRACE_END();

    ATRACE_BEGIN("Allocate output texture");
    if (mGpuAccelerationEnabled) {
        mOutputHolder = new GraphicBuffer(mOutputWidth, mOutputHeight, HAL_PIXEL_FORMAT_RGBA_8888,
                                          1, GRALLOC_USAGE_HW_TEXTURE, "SvOutputHolder");
        if (mOutputHolder->initCheck() == OK) {
            LOG(INFO) << "Successfully allocated Graphic Buffer for SvOutputHolder";
        } else {
            LOG(ERROR) << "Failed to allocate Graphic Buffer for SvOutputHolder";
            return false;
        }
        mOutputPointer.gpu_data_pointer = static_cast<void*>(mOutputHolder->toAHardwareBuffer());
    } else {
        mSvTexture = new GraphicBuffer(mOutputWidth, mOutputHeight, HAL_PIXEL_FORMAT_RGB_888, 1,
                                       GRALLOC_USAGE_HW_TEXTURE, "SvTexture");
        if (mSvTexture->initCheck() == OK) {
            LOG(INFO) << "Successfully allocated Graphic Buffer";
        } else {
            LOG(ERROR) << "Failed to allocate Graphic Buffer";
            return false;
        }
    }

    // Note: sv2dParams is in meters while mInfo must be in milli-meters.
    mInfo.width = mIOModuleConfig->sv2dConfig.sv2dParams.physical_size.width * 1000.0;
    mInfo.height = mIOModuleConfig->sv2dConfig.sv2dParams.physical_size.height * 1000.0;
    mInfo.center.isValid = true;
    mInfo.center.x = mIOModuleConfig->sv2dConfig.sv2dParams.physical_center.x * 1000.0;
    mInfo.center.y = mIOModuleConfig->sv2dConfig.sv2dParams.physical_center.y * 1000.0;

    mIsInitialized = true;

    ATRACE_END();

    return true;
}

bool SurroundView2dSession::setupEvs() {
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

bool SurroundView2dSession::startEvs() {
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
