/*
 * Copyright (C) 2013-2018 The Android Open Source Project
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

#define LOG_TAG "Camera3-Device"
#define ATRACE_TAG ATRACE_TAG_CAMERA
//#define LOG_NDEBUG 0
//#define LOG_NNDEBUG 0  // Per-frame verbose logging

#ifdef LOG_NNDEBUG
#define ALOGVV(...) ALOGV(__VA_ARGS__)
#else
#define ALOGVV(...) ((void)0)
#endif

// Convenience macro for transient errors
#define CLOGE(fmt, ...) ALOGE("Camera %s: %s: " fmt, mId.string(), __FUNCTION__, \
            ##__VA_ARGS__)

#define CLOGW(fmt, ...) ALOGW("Camera %s: %s: " fmt, mId.string(), __FUNCTION__, \
            ##__VA_ARGS__)

// Convenience macros for transitioning to the error state
#define SET_ERR(fmt, ...) setErrorState(   \
    "%s: " fmt, __FUNCTION__,              \
    ##__VA_ARGS__)
#define SET_ERR_L(fmt, ...) setErrorStateLocked( \
    "%s: " fmt, __FUNCTION__,                    \
    ##__VA_ARGS__)

#include <inttypes.h>

#include <utility>

#include <utils/Log.h>
#include <utils/Trace.h>
#include <utils/Timers.h>
#include <cutils/properties.h>

#include <android/hardware/camera2/ICameraDeviceUser.h>

#include "utils/CameraTraces.h"
#include "mediautils/SchedulingPolicyService.h"
#include "device3/Camera3Device.h"
#include "device3/Camera3OutputStream.h"
#include "device3/Camera3InputStream.h"
#include "device3/Camera3DummyStream.h"
#include "device3/Camera3SharedOutputStream.h"
#include "CameraService.h"
#include "utils/CameraThreadState.h"
#include "utils/TraceHFR.h"

#include <algorithm>
#include <tuple>

using namespace android::camera3;
using namespace android::hardware::camera;
using namespace android::hardware::camera::device::V3_2;

namespace android {

Camera3Device::Camera3Device(const String8 &id):
        mId(id),
        mOperatingMode(NO_MODE),
        mIsConstrainedHighSpeedConfiguration(false),
        mStatus(STATUS_UNINITIALIZED),
        mStatusWaiters(0),
        mUsePartialResult(false),
        mNumPartialResults(1),
        mTimestampOffset(0),
        mNextResultFrameNumber(0),
        mNextReprocessResultFrameNumber(0),
        mNextZslStillResultFrameNumber(0),
        mNextShutterFrameNumber(0),
        mNextReprocessShutterFrameNumber(0),
        mNextZslStillShutterFrameNumber(0),
        mListener(NULL),
        mVendorTagId(CAMERA_METADATA_INVALID_VENDOR_ID),
        mLastTemplateId(-1),
        mNeedFixupMonochromeTags(false)
{
    ATRACE_CALL();
    ALOGV("%s: Created device for camera %s", __FUNCTION__, mId.string());
}

Camera3Device::~Camera3Device()
{
    ATRACE_CALL();
    ALOGV("%s: Tearing down for camera id %s", __FUNCTION__, mId.string());
    disconnectImpl();
}

const String8& Camera3Device::getId() const {
    return mId;
}

status_t Camera3Device::initialize(sp<CameraProviderManager> manager, const String8& monitorTags) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    ALOGV("%s: Initializing HIDL device for camera %s", __FUNCTION__, mId.string());
    if (mStatus != STATUS_UNINITIALIZED) {
        CLOGE("Already initialized!");
        return INVALID_OPERATION;
    }
    if (manager == nullptr) return INVALID_OPERATION;

    sp<ICameraDeviceSession> session;
    ATRACE_BEGIN("CameraHal::openSession");
    status_t res = manager->openSession(mId.string(), this,
            /*out*/ &session);
    ATRACE_END();
    if (res != OK) {
        SET_ERR_L("Could not open camera session: %s (%d)", strerror(-res), res);
        return res;
    }

    res = manager->getCameraCharacteristics(mId.string(), &mDeviceInfo);
    if (res != OK) {
        SET_ERR_L("Could not retrieve camera characteristics: %s (%d)", strerror(-res), res);
        session->close();
        return res;
    }
    mSupportNativeZoomRatio = manager->supportNativeZoomRatio(mId.string());

    std::vector<std::string> physicalCameraIds;
    bool isLogical = manager->isLogicalCamera(mId.string(), &physicalCameraIds);
    if (isLogical) {
        for (auto& physicalId : physicalCameraIds) {
            res = manager->getCameraCharacteristics(
                    physicalId, &mPhysicalDeviceInfoMap[physicalId]);
            if (res != OK) {
                SET_ERR_L("Could not retrieve camera %s characteristics: %s (%d)",
                        physicalId.c_str(), strerror(-res), res);
                session->close();
                return res;
            }

            bool usePrecorrectArray =
                    DistortionMapper::isDistortionSupported(mPhysicalDeviceInfoMap[physicalId]);
            if (usePrecorrectArray) {
                res = mDistortionMappers[physicalId].setupStaticInfo(
                        mPhysicalDeviceInfoMap[physicalId]);
                if (res != OK) {
                    SET_ERR_L("Unable to read camera %s's calibration fields for distortion "
                            "correction", physicalId.c_str());
                    session->close();
                    return res;
                }
            }

            mZoomRatioMappers[physicalId] = ZoomRatioMapper(
                    &mPhysicalDeviceInfoMap[physicalId],
                    mSupportNativeZoomRatio, usePrecorrectArray);
        }
    }

    std::shared_ptr<RequestMetadataQueue> queue;
    auto requestQueueRet = session->getCaptureRequestMetadataQueue(
        [&queue](const auto& descriptor) {
            queue = std::make_shared<RequestMetadataQueue>(descriptor);
            if (!queue->isValid() || queue->availableToWrite() <= 0) {
                ALOGE("HAL returns empty request metadata fmq, not use it");
                queue = nullptr;
                // don't use the queue onwards.
            }
        });
    if (!requestQueueRet.isOk()) {
        ALOGE("Transaction error when getting request metadata fmq: %s, not use it",
                requestQueueRet.description().c_str());
        return DEAD_OBJECT;
    }

    std::unique_ptr<ResultMetadataQueue>& resQueue = mResultMetadataQueue;
    auto resultQueueRet = session->getCaptureResultMetadataQueue(
        [&resQueue](const auto& descriptor) {
            resQueue = std::make_unique<ResultMetadataQueue>(descriptor);
            if (!resQueue->isValid() || resQueue->availableToWrite() <= 0) {
                ALOGE("HAL returns empty result metadata fmq, not use it");
                resQueue = nullptr;
                // Don't use the resQueue onwards.
            }
        });
    if (!resultQueueRet.isOk()) {
        ALOGE("Transaction error when getting result metadata queue from camera session: %s",
                resultQueueRet.description().c_str());
        return DEAD_OBJECT;
    }
    IF_ALOGV() {
        session->interfaceChain([](
            ::android::hardware::hidl_vec<::android::hardware::hidl_string> interfaceChain) {
                ALOGV("Session interface chain:");
                for (const auto& iface : interfaceChain) {
                    ALOGV("  %s", iface.c_str());
                }
            });
    }

    camera_metadata_entry bufMgrMode =
            mDeviceInfo.find(ANDROID_INFO_SUPPORTED_BUFFER_MANAGEMENT_VERSION);
    if (bufMgrMode.count > 0) {
         mUseHalBufManager = (bufMgrMode.data.u8[0] ==
            ANDROID_INFO_SUPPORTED_BUFFER_MANAGEMENT_VERSION_HIDL_DEVICE_3_5);
    }

    camera_metadata_entry_t capabilities = mDeviceInfo.find(ANDROID_REQUEST_AVAILABLE_CAPABILITIES);
    for (size_t i = 0; i < capabilities.count; i++) {
        uint8_t capability = capabilities.data.u8[i];
        if (capability == ANDROID_REQUEST_AVAILABLE_CAPABILITIES_OFFLINE_PROCESSING) {
            mSupportOfflineProcessing = true;
        }
    }

    mInterface = new HalInterface(session, queue, mUseHalBufManager, mSupportOfflineProcessing);
    std::string providerType;
    mVendorTagId = manager->getProviderTagIdLocked(mId.string());
    mTagMonitor.initialize(mVendorTagId);
    if (!monitorTags.isEmpty()) {
        mTagMonitor.parseTagsToMonitor(String8(monitorTags));
    }

    // Metadata tags needs fixup for monochrome camera device version less
    // than 3.5.
    hardware::hidl_version maxVersion{0,0};
    res = manager->getHighestSupportedVersion(mId.string(), &maxVersion);
    if (res != OK) {
        ALOGE("%s: Error in getting camera device version id: %s (%d)",
                __FUNCTION__, strerror(-res), res);
        return res;
    }
    int deviceVersion = HARDWARE_DEVICE_API_VERSION(
            maxVersion.get_major(), maxVersion.get_minor());

    bool isMonochrome = false;
    for (size_t i = 0; i < capabilities.count; i++) {
        uint8_t capability = capabilities.data.u8[i];
        if (capability == ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MONOCHROME) {
            isMonochrome = true;
        }
    }
    mNeedFixupMonochromeTags = (isMonochrome && deviceVersion < CAMERA_DEVICE_API_VERSION_3_5);

    return initializeCommonLocked();
}

status_t Camera3Device::initializeCommonLocked() {

    /** Start up status tracker thread */
    mStatusTracker = new StatusTracker(this);
    status_t res = mStatusTracker->run(String8::format("C3Dev-%s-Status", mId.string()).string());
    if (res != OK) {
        SET_ERR_L("Unable to start status tracking thread: %s (%d)",
                strerror(-res), res);
        mInterface->close();
        mStatusTracker.clear();
        return res;
    }

    /** Register in-flight map to the status tracker */
    mInFlightStatusId = mStatusTracker->addComponent();

    if (mUseHalBufManager) {
        res = mRequestBufferSM.initialize(mStatusTracker);
        if (res != OK) {
            SET_ERR_L("Unable to start request buffer state machine: %s (%d)",
                    strerror(-res), res);
            mInterface->close();
            mStatusTracker.clear();
            return res;
        }
    }

    /** Create buffer manager */
    mBufferManager = new Camera3BufferManager();

    Vector<int32_t> sessionParamKeys;
    camera_metadata_entry_t sessionKeysEntry = mDeviceInfo.find(
            ANDROID_REQUEST_AVAILABLE_SESSION_KEYS);
    if (sessionKeysEntry.count > 0) {
        sessionParamKeys.insertArrayAt(sessionKeysEntry.data.i32, 0, sessionKeysEntry.count);
    }

    /** Start up request queue thread */
    mRequestThread = new RequestThread(
            this, mStatusTracker, mInterface, sessionParamKeys, mUseHalBufManager);
    res = mRequestThread->run(String8::format("C3Dev-%s-ReqQueue", mId.string()).string());
    if (res != OK) {
        SET_ERR_L("Unable to start request queue thread: %s (%d)",
                strerror(-res), res);
        mInterface->close();
        mRequestThread.clear();
        return res;
    }

    mPreparerThread = new PreparerThread();

    internalUpdateStatusLocked(STATUS_UNCONFIGURED);
    mNextStreamId = 0;
    mDummyStreamId = NO_STREAM;
    mNeedConfig = true;
    mPauseStateNotify = false;

    // Measure the clock domain offset between camera and video/hw_composer
    camera_metadata_entry timestampSource =
            mDeviceInfo.find(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE);
    if (timestampSource.count > 0 && timestampSource.data.u8[0] ==
            ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME) {
        mTimestampOffset = getMonoToBoottimeOffset();
    }

    // Will the HAL be sending in early partial result metadata?
    camera_metadata_entry partialResultsCount =
            mDeviceInfo.find(ANDROID_REQUEST_PARTIAL_RESULT_COUNT);
    if (partialResultsCount.count > 0) {
        mNumPartialResults = partialResultsCount.data.i32[0];
        mUsePartialResult = (mNumPartialResults > 1);
    }

    camera_metadata_entry configs =
            mDeviceInfo.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    for (uint32_t i = 0; i < configs.count; i += 4) {
        if (configs.data.i32[i] == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
                configs.data.i32[i + 3] ==
                ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT) {
            mSupportedOpaqueInputSizes.add(Size(configs.data.i32[i + 1],
                    configs.data.i32[i + 2]));
        }
    }

    bool usePrecorrectArray = DistortionMapper::isDistortionSupported(mDeviceInfo);
    if (usePrecorrectArray) {
        res = mDistortionMappers[mId.c_str()].setupStaticInfo(mDeviceInfo);
        if (res != OK) {
            SET_ERR_L("Unable to read necessary calibration fields for distortion correction");
            return res;
        }
    }

    mZoomRatioMappers[mId.c_str()] = ZoomRatioMapper(&mDeviceInfo,
            mSupportNativeZoomRatio, usePrecorrectArray);

    if (RotateAndCropMapper::isNeeded(&mDeviceInfo)) {
        mRotateAndCropMappers.emplace(mId.c_str(), &mDeviceInfo);
    }

    return OK;
}

status_t Camera3Device::disconnect() {
    return disconnectImpl();
}

status_t Camera3Device::disconnectImpl() {
    ATRACE_CALL();
    ALOGI("%s: E", __FUNCTION__);

    status_t res = OK;
    std::vector<wp<Camera3StreamInterface>> streams;
    {
        Mutex::Autolock il(mInterfaceLock);
        nsecs_t maxExpectedDuration = getExpectedInFlightDuration();
        {
            Mutex::Autolock l(mLock);
            if (mStatus == STATUS_UNINITIALIZED) return res;

            if (mStatus == STATUS_ACTIVE ||
                    (mStatus == STATUS_ERROR && mRequestThread != NULL)) {
                res = mRequestThread->clearRepeatingRequests();
                if (res != OK) {
                    SET_ERR_L("Can't stop streaming");
                    // Continue to close device even in case of error
                } else {
                    res = waitUntilStateThenRelock(/*active*/ false, maxExpectedDuration);
                    if (res != OK) {
                        SET_ERR_L("Timeout waiting for HAL to drain (% " PRIi64 " ns)",
                                maxExpectedDuration);
                        // Continue to close device even in case of error
                    }
                }
            }

            if (mStatus == STATUS_ERROR) {
                CLOGE("Shutting down in an error state");
            }

            if (mStatusTracker != NULL) {
                mStatusTracker->requestExit();
            }

            if (mRequestThread != NULL) {
                mRequestThread->requestExit();
            }

            streams.reserve(mOutputStreams.size() + (mInputStream != nullptr ? 1 : 0));
            for (size_t i = 0; i < mOutputStreams.size(); i++) {
                streams.push_back(mOutputStreams[i]);
            }
            if (mInputStream != nullptr) {
                streams.push_back(mInputStream);
            }
        }
    }
    // Joining done without holding mLock and mInterfaceLock, otherwise deadlocks may ensue
    // as the threads try to access parent state (b/143513518)
    if (mRequestThread != NULL && mStatus != STATUS_ERROR) {
        // HAL may be in a bad state, so waiting for request thread
        // (which may be stuck in the HAL processCaptureRequest call)
        // could be dangerous.
        // give up mInterfaceLock here and then lock it again. Could this lead
        // to other deadlocks
        mRequestThread->join();
    }
    {
        Mutex::Autolock il(mInterfaceLock);
        if (mStatusTracker != NULL) {
            mStatusTracker->join();
        }

        HalInterface* interface;
        {
            Mutex::Autolock l(mLock);
            mRequestThread.clear();
            Mutex::Autolock stLock(mTrackerLock);
            mStatusTracker.clear();
            interface = mInterface.get();
        }

        // Call close without internal mutex held, as the HAL close may need to
        // wait on assorted callbacks,etc, to complete before it can return.
        interface->close();

        flushInflightRequests();

        {
            Mutex::Autolock l(mLock);
            mInterface->clear();
            mOutputStreams.clear();
            mInputStream.clear();
            mDeletedStreams.clear();
            mBufferManager.clear();
            internalUpdateStatusLocked(STATUS_UNINITIALIZED);
        }

        for (auto& weakStream : streams) {
              sp<Camera3StreamInterface> stream = weakStream.promote();
            if (stream != nullptr) {
                ALOGE("%s: Stream %d leaked! strong reference (%d)!",
                        __FUNCTION__, stream->getId(), stream->getStrongCount() - 1);
            }
        }
    }
    ALOGI("%s: X", __FUNCTION__);
    return res;
}

// For dumping/debugging only -
// try to acquire a lock a few times, eventually give up to proceed with
// debug/dump operations
bool Camera3Device::tryLockSpinRightRound(Mutex& lock) {
    bool gotLock = false;
    for (size_t i = 0; i < kDumpLockAttempts; ++i) {
        if (lock.tryLock() == NO_ERROR) {
            gotLock = true;
            break;
        } else {
            usleep(kDumpSleepDuration);
        }
    }
    return gotLock;
}

Camera3Device::Size Camera3Device::getMaxJpegResolution() const {
    int32_t maxJpegWidth = 0, maxJpegHeight = 0;
    const int STREAM_CONFIGURATION_SIZE = 4;
    const int STREAM_FORMAT_OFFSET = 0;
    const int STREAM_WIDTH_OFFSET = 1;
    const int STREAM_HEIGHT_OFFSET = 2;
    const int STREAM_IS_INPUT_OFFSET = 3;
    camera_metadata_ro_entry_t availableStreamConfigs =
            mDeviceInfo.find(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    if (availableStreamConfigs.count == 0 ||
            availableStreamConfigs.count % STREAM_CONFIGURATION_SIZE != 0) {
        return Size(0, 0);
    }

    // Get max jpeg size (area-wise).
    for (size_t i=0; i < availableStreamConfigs.count; i+= STREAM_CONFIGURATION_SIZE) {
        int32_t format = availableStreamConfigs.data.i32[i + STREAM_FORMAT_OFFSET];
        int32_t width = availableStreamConfigs.data.i32[i + STREAM_WIDTH_OFFSET];
        int32_t height = availableStreamConfigs.data.i32[i + STREAM_HEIGHT_OFFSET];
        int32_t isInput = availableStreamConfigs.data.i32[i + STREAM_IS_INPUT_OFFSET];
        if (isInput == ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT
                && format == HAL_PIXEL_FORMAT_BLOB &&
                (width * height > maxJpegWidth * maxJpegHeight)) {
            maxJpegWidth = width;
            maxJpegHeight = height;
        }
    }

    return Size(maxJpegWidth, maxJpegHeight);
}

nsecs_t Camera3Device::getMonoToBoottimeOffset() {
    // try three times to get the clock offset, choose the one
    // with the minimum gap in measurements.
    const int tries = 3;
    nsecs_t bestGap, measured;
    for (int i = 0; i < tries; ++i) {
        const nsecs_t tmono = systemTime(SYSTEM_TIME_MONOTONIC);
        const nsecs_t tbase = systemTime(SYSTEM_TIME_BOOTTIME);
        const nsecs_t tmono2 = systemTime(SYSTEM_TIME_MONOTONIC);
        const nsecs_t gap = tmono2 - tmono;
        if (i == 0 || gap < bestGap) {
            bestGap = gap;
            measured = tbase - ((tmono + tmono2) >> 1);
        }
    }
    return measured;
}

hardware::graphics::common::V1_0::PixelFormat Camera3Device::mapToPixelFormat(
        int frameworkFormat) {
    return (hardware::graphics::common::V1_0::PixelFormat) frameworkFormat;
}

DataspaceFlags Camera3Device::mapToHidlDataspace(
        android_dataspace dataSpace) {
    return dataSpace;
}

BufferUsageFlags Camera3Device::mapToConsumerUsage(
        uint64_t usage) {
    return usage;
}

StreamRotation Camera3Device::mapToStreamRotation(camera3_stream_rotation_t rotation) {
    switch (rotation) {
        case CAMERA3_STREAM_ROTATION_0:
            return StreamRotation::ROTATION_0;
        case CAMERA3_STREAM_ROTATION_90:
            return StreamRotation::ROTATION_90;
        case CAMERA3_STREAM_ROTATION_180:
            return StreamRotation::ROTATION_180;
        case CAMERA3_STREAM_ROTATION_270:
            return StreamRotation::ROTATION_270;
    }
    ALOGE("%s: Unknown stream rotation %d", __FUNCTION__, rotation);
    return StreamRotation::ROTATION_0;
}

status_t Camera3Device::mapToStreamConfigurationMode(
        camera3_stream_configuration_mode_t operationMode, StreamConfigurationMode *mode) {
    if (mode == nullptr) return BAD_VALUE;
    if (operationMode < CAMERA3_VENDOR_STREAM_CONFIGURATION_MODE_START) {
        switch(operationMode) {
            case CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE:
                *mode = StreamConfigurationMode::NORMAL_MODE;
                break;
            case CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE:
                *mode = StreamConfigurationMode::CONSTRAINED_HIGH_SPEED_MODE;
                break;
            default:
                ALOGE("%s: Unknown stream configuration mode %d", __FUNCTION__, operationMode);
                return BAD_VALUE;
        }
    } else {
        *mode = static_cast<StreamConfigurationMode>(operationMode);
    }
    return OK;
}

int Camera3Device::mapToFrameworkFormat(
        hardware::graphics::common::V1_0::PixelFormat pixelFormat) {
    return static_cast<uint32_t>(pixelFormat);
}

android_dataspace Camera3Device::mapToFrameworkDataspace(
        DataspaceFlags dataSpace) {
    return static_cast<android_dataspace>(dataSpace);
}

uint64_t Camera3Device::mapConsumerToFrameworkUsage(
        BufferUsageFlags usage) {
    return usage;
}

uint64_t Camera3Device::mapProducerToFrameworkUsage(
        BufferUsageFlags usage) {
    return usage;
}

ssize_t Camera3Device::getJpegBufferSize(uint32_t width, uint32_t height) const {
    // Get max jpeg size (area-wise).
    Size maxJpegResolution = getMaxJpegResolution();
    if (maxJpegResolution.width == 0) {
        ALOGE("%s: Camera %s: Can't find valid available jpeg sizes in static metadata!",
                __FUNCTION__, mId.string());
        return BAD_VALUE;
    }

    // Get max jpeg buffer size
    ssize_t maxJpegBufferSize = 0;
    camera_metadata_ro_entry jpegBufMaxSize = mDeviceInfo.find(ANDROID_JPEG_MAX_SIZE);
    if (jpegBufMaxSize.count == 0) {
        ALOGE("%s: Camera %s: Can't find maximum JPEG size in static metadata!", __FUNCTION__,
                mId.string());
        return BAD_VALUE;
    }
    maxJpegBufferSize = jpegBufMaxSize.data.i32[0];
    assert(kMinJpegBufferSize < maxJpegBufferSize);

    // Calculate final jpeg buffer size for the given resolution.
    float scaleFactor = ((float) (width * height)) /
            (maxJpegResolution.width * maxJpegResolution.height);
    ssize_t jpegBufferSize = scaleFactor * (maxJpegBufferSize - kMinJpegBufferSize) +
            kMinJpegBufferSize;
    if (jpegBufferSize > maxJpegBufferSize) {
        jpegBufferSize = maxJpegBufferSize;
    }

    return jpegBufferSize;
}

ssize_t Camera3Device::getPointCloudBufferSize() const {
    const int FLOATS_PER_POINT=4;
    camera_metadata_ro_entry maxPointCount = mDeviceInfo.find(ANDROID_DEPTH_MAX_DEPTH_SAMPLES);
    if (maxPointCount.count == 0) {
        ALOGE("%s: Camera %s: Can't find maximum depth point cloud size in static metadata!",
                __FUNCTION__, mId.string());
        return BAD_VALUE;
    }
    ssize_t maxBytesForPointCloud = sizeof(android_depth_points) +
            maxPointCount.data.i32[0] * sizeof(float) * FLOATS_PER_POINT;
    return maxBytesForPointCloud;
}

ssize_t Camera3Device::getRawOpaqueBufferSize(int32_t width, int32_t height) const {
    const int PER_CONFIGURATION_SIZE = 3;
    const int WIDTH_OFFSET = 0;
    const int HEIGHT_OFFSET = 1;
    const int SIZE_OFFSET = 2;
    camera_metadata_ro_entry rawOpaqueSizes =
        mDeviceInfo.find(ANDROID_SENSOR_OPAQUE_RAW_SIZE);
    size_t count = rawOpaqueSizes.count;
    if (count == 0 || (count % PER_CONFIGURATION_SIZE)) {
        ALOGE("%s: Camera %s: bad opaque RAW size static metadata length(%zu)!",
                __FUNCTION__, mId.string(), count);
        return BAD_VALUE;
    }

    for (size_t i = 0; i < count; i += PER_CONFIGURATION_SIZE) {
        if (width == rawOpaqueSizes.data.i32[i + WIDTH_OFFSET] &&
                height == rawOpaqueSizes.data.i32[i + HEIGHT_OFFSET]) {
            return rawOpaqueSizes.data.i32[i + SIZE_OFFSET];
        }
    }

    ALOGE("%s: Camera %s: cannot find size for %dx%d opaque RAW image!",
            __FUNCTION__, mId.string(), width, height);
    return BAD_VALUE;
}

status_t Camera3Device::dump(int fd, const Vector<String16> &args) {
    ATRACE_CALL();
    (void)args;

    // Try to lock, but continue in case of failure (to avoid blocking in
    // deadlocks)
    bool gotInterfaceLock = tryLockSpinRightRound(mInterfaceLock);
    bool gotLock = tryLockSpinRightRound(mLock);

    ALOGW_IF(!gotInterfaceLock,
            "Camera %s: %s: Unable to lock interface lock, proceeding anyway",
            mId.string(), __FUNCTION__);
    ALOGW_IF(!gotLock,
            "Camera %s: %s: Unable to lock main lock, proceeding anyway",
            mId.string(), __FUNCTION__);

    bool dumpTemplates = false;

    String16 templatesOption("-t");
    int n = args.size();
    for (int i = 0; i < n; i++) {
        if (args[i] == templatesOption) {
            dumpTemplates = true;
        }
        if (args[i] == TagMonitor::kMonitorOption) {
            if (i + 1 < n) {
                String8 monitorTags = String8(args[i + 1]);
                if (monitorTags == "off") {
                    mTagMonitor.disableMonitoring();
                } else {
                    mTagMonitor.parseTagsToMonitor(monitorTags);
                }
            } else {
                mTagMonitor.disableMonitoring();
            }
        }
    }

    String8 lines;

    const char *status =
            mStatus == STATUS_ERROR         ? "ERROR" :
            mStatus == STATUS_UNINITIALIZED ? "UNINITIALIZED" :
            mStatus == STATUS_UNCONFIGURED  ? "UNCONFIGURED" :
            mStatus == STATUS_CONFIGURED    ? "CONFIGURED" :
            mStatus == STATUS_ACTIVE        ? "ACTIVE" :
            "Unknown";

    lines.appendFormat("    Device status: %s\n", status);
    if (mStatus == STATUS_ERROR) {
        lines.appendFormat("    Error cause: %s\n", mErrorCause.string());
    }
    lines.appendFormat("    Stream configuration:\n");
    const char *mode =
            mOperatingMode == static_cast<int>(StreamConfigurationMode::NORMAL_MODE) ? "NORMAL" :
            mOperatingMode == static_cast<int>(
                StreamConfigurationMode::CONSTRAINED_HIGH_SPEED_MODE) ? "CONSTRAINED_HIGH_SPEED" :
            "CUSTOM";
    lines.appendFormat("    Operation mode: %s (%d) \n", mode, mOperatingMode);

    if (mInputStream != NULL) {
        write(fd, lines.string(), lines.size());
        mInputStream->dump(fd, args);
    } else {
        lines.appendFormat("      No input stream.\n");
        write(fd, lines.string(), lines.size());
    }
    for (size_t i = 0; i < mOutputStreams.size(); i++) {
        mOutputStreams[i]->dump(fd,args);
    }

    if (mBufferManager != NULL) {
        lines = String8("    Camera3 Buffer Manager:\n");
        write(fd, lines.string(), lines.size());
        mBufferManager->dump(fd, args);
    }

    lines = String8("    In-flight requests:\n");
    if (mInFlightMap.size() == 0) {
        lines.append("      None\n");
    } else {
        for (size_t i = 0; i < mInFlightMap.size(); i++) {
            InFlightRequest r = mInFlightMap.valueAt(i);
            lines.appendFormat("      Frame %d |  Timestamp: %" PRId64 ", metadata"
                    " arrived: %s, buffers left: %d\n", mInFlightMap.keyAt(i),
                    r.shutterTimestamp, r.haveResultMetadata ? "true" : "false",
                    r.numBuffersLeft);
        }
    }
    write(fd, lines.string(), lines.size());

    if (mRequestThread != NULL) {
        mRequestThread->dumpCaptureRequestLatency(fd,
                "    ProcessCaptureRequest latency histogram:");
    }

    {
        lines = String8("    Last request sent:\n");
        write(fd, lines.string(), lines.size());

        CameraMetadata lastRequest = getLatestRequestLocked();
        lastRequest.dump(fd, /*verbosity*/2, /*indentation*/6);
    }

    if (dumpTemplates) {
        const char *templateNames[CAMERA3_TEMPLATE_COUNT] = {
            "TEMPLATE_PREVIEW",
            "TEMPLATE_STILL_CAPTURE",
            "TEMPLATE_VIDEO_RECORD",
            "TEMPLATE_VIDEO_SNAPSHOT",
            "TEMPLATE_ZERO_SHUTTER_LAG",
            "TEMPLATE_MANUAL",
        };

        for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; i++) {
            camera_metadata_t *templateRequest = nullptr;
            mInterface->constructDefaultRequestSettings(
                    (camera3_request_template_t) i, &templateRequest);
            lines = String8::format("    HAL Request %s:\n", templateNames[i-1]);
            if (templateRequest == nullptr) {
                lines.append("       Not supported\n");
                write(fd, lines.string(), lines.size());
            } else {
                write(fd, lines.string(), lines.size());
                dump_indented_camera_metadata(templateRequest,
                        fd, /*verbosity*/2, /*indentation*/8);
            }
            free_camera_metadata(templateRequest);
        }
    }

    mTagMonitor.dumpMonitoredMetadata(fd);

    if (mInterface->valid()) {
        lines = String8("     HAL device dump:\n");
        write(fd, lines.string(), lines.size());
        mInterface->dump(fd);
    }

    if (gotLock) mLock.unlock();
    if (gotInterfaceLock) mInterfaceLock.unlock();

    return OK;
}

const CameraMetadata& Camera3Device::infoPhysical(const String8& physicalId) const {
    ALOGVV("%s: E", __FUNCTION__);
    if (CC_UNLIKELY(mStatus == STATUS_UNINITIALIZED ||
                    mStatus == STATUS_ERROR)) {
        ALOGW("%s: Access to static info %s!", __FUNCTION__,
                mStatus == STATUS_ERROR ?
                "when in error state" : "before init");
    }
    if (physicalId.isEmpty()) {
        return mDeviceInfo;
    } else {
        std::string id(physicalId.c_str());
        if (mPhysicalDeviceInfoMap.find(id) != mPhysicalDeviceInfoMap.end()) {
            return mPhysicalDeviceInfoMap.at(id);
        } else {
            ALOGE("%s: Invalid physical camera id %s", __FUNCTION__, physicalId.c_str());
            return mDeviceInfo;
        }
    }
}

const CameraMetadata& Camera3Device::info() const {
    String8 emptyId;
    return infoPhysical(emptyId);
}

status_t Camera3Device::checkStatusOkToCaptureLocked() {
    switch (mStatus) {
        case STATUS_ERROR:
            CLOGE("Device has encountered a serious error");
            return INVALID_OPERATION;
        case STATUS_UNINITIALIZED:
            CLOGE("Device not initialized");
            return INVALID_OPERATION;
        case STATUS_UNCONFIGURED:
        case STATUS_CONFIGURED:
        case STATUS_ACTIVE:
            // OK
            break;
        default:
            SET_ERR_L("Unexpected status: %d", mStatus);
            return INVALID_OPERATION;
    }
    return OK;
}

status_t Camera3Device::convertMetadataListToRequestListLocked(
        const List<const PhysicalCameraSettingsList> &metadataList,
        const std::list<const SurfaceMap> &surfaceMaps,
        bool repeating,
        RequestList *requestList) {
    if (requestList == NULL) {
        CLOGE("requestList cannot be NULL.");
        return BAD_VALUE;
    }

    int32_t burstId = 0;
    List<const PhysicalCameraSettingsList>::const_iterator metadataIt = metadataList.begin();
    std::list<const SurfaceMap>::const_iterator surfaceMapIt = surfaceMaps.begin();
    for (; metadataIt != metadataList.end() && surfaceMapIt != surfaceMaps.end();
            ++metadataIt, ++surfaceMapIt) {
        sp<CaptureRequest> newRequest = setUpRequestLocked(*metadataIt, *surfaceMapIt);
        if (newRequest == 0) {
            CLOGE("Can't create capture request");
            return BAD_VALUE;
        }

        newRequest->mRepeating = repeating;

        // Setup burst Id and request Id
        newRequest->mResultExtras.burstId = burstId++;
        auto requestIdEntry = metadataIt->begin()->metadata.find(ANDROID_REQUEST_ID);
        if (requestIdEntry.count == 0) {
            CLOGE("RequestID does not exist in metadata");
            return BAD_VALUE;
        }
        newRequest->mResultExtras.requestId = requestIdEntry.data.i32[0];

        requestList->push_back(newRequest);

        ALOGV("%s: requestId = %" PRId32, __FUNCTION__, newRequest->mResultExtras.requestId);
    }
    if (metadataIt != metadataList.end() || surfaceMapIt != surfaceMaps.end()) {
        ALOGE("%s: metadataList and surfaceMaps are not the same size!", __FUNCTION__);
        return BAD_VALUE;
    }

    // Setup batch size if this is a high speed video recording request.
    if (mIsConstrainedHighSpeedConfiguration && requestList->size() > 0) {
        auto firstRequest = requestList->begin();
        for (auto& outputStream : (*firstRequest)->mOutputStreams) {
            if (outputStream->isVideoStream()) {
                (*firstRequest)->mBatchSize = requestList->size();
                break;
            }
        }
    }

    return OK;
}

status_t Camera3Device::capture(CameraMetadata &request, int64_t* lastFrameNumber) {
    ATRACE_CALL();

    List<const PhysicalCameraSettingsList> requestsList;
    std::list<const SurfaceMap> surfaceMaps;
    convertToRequestList(requestsList, surfaceMaps, request);

    return captureList(requestsList, surfaceMaps, lastFrameNumber);
}

void Camera3Device::convertToRequestList(List<const PhysicalCameraSettingsList>& requestsList,
        std::list<const SurfaceMap>& surfaceMaps,
        const CameraMetadata& request) {
    PhysicalCameraSettingsList requestList;
    requestList.push_back({std::string(getId().string()), request});
    requestsList.push_back(requestList);

    SurfaceMap surfaceMap;
    camera_metadata_ro_entry streams = request.find(ANDROID_REQUEST_OUTPUT_STREAMS);
    // With no surface list passed in, stream and surface will have 1-to-1
    // mapping. So the surface index is 0 for each stream in the surfaceMap.
    for (size_t i = 0; i < streams.count; i++) {
        surfaceMap[streams.data.i32[i]].push_back(0);
    }
    surfaceMaps.push_back(surfaceMap);
}

status_t Camera3Device::submitRequestsHelper(
        const List<const PhysicalCameraSettingsList> &requests,
        const std::list<const SurfaceMap> &surfaceMaps,
        bool repeating,
        /*out*/
        int64_t *lastFrameNumber) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    status_t res = checkStatusOkToCaptureLocked();
    if (res != OK) {
        // error logged by previous call
        return res;
    }

    RequestList requestList;

    res = convertMetadataListToRequestListLocked(requests, surfaceMaps,
            repeating, /*out*/&requestList);
    if (res != OK) {
        // error logged by previous call
        return res;
    }

    if (repeating) {
        res = mRequestThread->setRepeatingRequests(requestList, lastFrameNumber);
    } else {
        res = mRequestThread->queueRequestList(requestList, lastFrameNumber);
    }

    if (res == OK) {
        waitUntilStateThenRelock(/*active*/true, kActiveTimeout);
        if (res != OK) {
            SET_ERR_L("Can't transition to active in %f seconds!",
                    kActiveTimeout/1e9);
        }
        ALOGV("Camera %s: Capture request %" PRId32 " enqueued", mId.string(),
              (*(requestList.begin()))->mResultExtras.requestId);
    } else {
        CLOGE("Cannot queue request. Impossible.");
        return BAD_VALUE;
    }

    return res;
}

hardware::Return<void> Camera3Device::requestStreamBuffers(
        const hardware::hidl_vec<hardware::camera::device::V3_5::BufferRequest>& bufReqs,
        requestStreamBuffers_cb _hidl_cb) {
    RequestBufferStates states {
        mId, mRequestBufferInterfaceLock, mUseHalBufManager, mOutputStreams,
        *this, *mInterface, *this};
    camera3::requestStreamBuffers(states, bufReqs, _hidl_cb);
    return hardware::Void();
}

hardware::Return<void> Camera3Device::returnStreamBuffers(
        const hardware::hidl_vec<hardware::camera::device::V3_2::StreamBuffer>& buffers) {
    ReturnBufferStates states {
        mId, mUseHalBufManager, mOutputStreams, *mInterface};
    camera3::returnStreamBuffers(states, buffers);
    return hardware::Void();
}

hardware::Return<void> Camera3Device::processCaptureResult_3_4(
        const hardware::hidl_vec<
                hardware::camera::device::V3_4::CaptureResult>& results) {
    // Ideally we should grab mLock, but that can lead to deadlock, and
    // it's not super important to get up to date value of mStatus for this
    // warning print, hence skipping the lock here
    if (mStatus == STATUS_ERROR) {
        // Per API contract, HAL should act as closed after device error
        // But mStatus can be set to error by framework as well, so just log
        // a warning here.
        ALOGW("%s: received capture result in error state.", __FUNCTION__);
    }

    sp<NotificationListener> listener;
    {
        std::lock_guard<std::mutex> l(mOutputLock);
        listener = mListener.promote();
    }

    if (mProcessCaptureResultLock.tryLock() != OK) {
        // This should never happen; it indicates a wrong client implementation
        // that doesn't follow the contract. But, we can be tolerant here.
        ALOGE("%s: callback overlapped! waiting 1s...",
                __FUNCTION__);
        if (mProcessCaptureResultLock.timedLock(1000000000 /* 1s */) != OK) {
            ALOGE("%s: cannot acquire lock in 1s, dropping results",
                    __FUNCTION__);
            // really don't know what to do, so bail out.
            return hardware::Void();
        }
    }
    CaptureOutputStates states {
        mId,
        mInFlightLock, mLastCompletedRegularFrameNumber,
        mLastCompletedReprocessFrameNumber, mLastCompletedZslFrameNumber,
        mInFlightMap, mOutputLock,  mResultQueue, mResultSignal,
        mNextShutterFrameNumber,
        mNextReprocessShutterFrameNumber, mNextZslStillShutterFrameNumber,
        mNextResultFrameNumber,
        mNextReprocessResultFrameNumber, mNextZslStillResultFrameNumber,
        mUseHalBufManager, mUsePartialResult, mNeedFixupMonochromeTags,
        mNumPartialResults, mVendorTagId, mDeviceInfo, mPhysicalDeviceInfoMap,
        mResultMetadataQueue, mDistortionMappers, mZoomRatioMappers, mRotateAndCropMappers,
        mTagMonitor, mInputStream, mOutputStreams, listener, *this, *this, *mInterface
    };

    for (const auto& result : results) {
        processOneCaptureResultLocked(states, result.v3_2, result.physicalCameraMetadata);
    }
    mProcessCaptureResultLock.unlock();
    return hardware::Void();
}

// Only one processCaptureResult should be called at a time, so
// the locks won't block. The locks are present here simply to enforce this.
hardware::Return<void> Camera3Device::processCaptureResult(
        const hardware::hidl_vec<
                hardware::camera::device::V3_2::CaptureResult>& results) {
    hardware::hidl_vec<hardware::camera::device::V3_4::PhysicalCameraMetadata> noPhysMetadata;

    // Ideally we should grab mLock, but that can lead to deadlock, and
    // it's not super important to get up to date value of mStatus for this
    // warning print, hence skipping the lock here
    if (mStatus == STATUS_ERROR) {
        // Per API contract, HAL should act as closed after device error
        // But mStatus can be set to error by framework as well, so just log
        // a warning here.
        ALOGW("%s: received capture result in error state.", __FUNCTION__);
    }

    sp<NotificationListener> listener;
    {
        std::lock_guard<std::mutex> l(mOutputLock);
        listener = mListener.promote();
    }

    if (mProcessCaptureResultLock.tryLock() != OK) {
        // This should never happen; it indicates a wrong client implementation
        // that doesn't follow the contract. But, we can be tolerant here.
        ALOGE("%s: callback overlapped! waiting 1s...",
                __FUNCTION__);
        if (mProcessCaptureResultLock.timedLock(1000000000 /* 1s */) != OK) {
            ALOGE("%s: cannot acquire lock in 1s, dropping results",
                    __FUNCTION__);
            // really don't know what to do, so bail out.
            return hardware::Void();
        }
    }

    CaptureOutputStates states {
        mId,
        mInFlightLock, mLastCompletedRegularFrameNumber,
        mLastCompletedReprocessFrameNumber, mLastCompletedZslFrameNumber,
        mInFlightMap, mOutputLock,  mResultQueue, mResultSignal,
        mNextShutterFrameNumber,
        mNextReprocessShutterFrameNumber, mNextZslStillShutterFrameNumber,
        mNextResultFrameNumber,
        mNextReprocessResultFrameNumber, mNextZslStillResultFrameNumber,
        mUseHalBufManager, mUsePartialResult, mNeedFixupMonochromeTags,
        mNumPartialResults, mVendorTagId, mDeviceInfo, mPhysicalDeviceInfoMap,
        mResultMetadataQueue, mDistortionMappers, mZoomRatioMappers, mRotateAndCropMappers,
        mTagMonitor, mInputStream, mOutputStreams, listener, *this, *this, *mInterface
    };

    for (const auto& result : results) {
        processOneCaptureResultLocked(states, result, noPhysMetadata);
    }
    mProcessCaptureResultLock.unlock();
    return hardware::Void();
}

hardware::Return<void> Camera3Device::notify(
        const hardware::hidl_vec<hardware::camera::device::V3_2::NotifyMsg>& msgs) {
    // Ideally we should grab mLock, but that can lead to deadlock, and
    // it's not super important to get up to date value of mStatus for this
    // warning print, hence skipping the lock here
    if (mStatus == STATUS_ERROR) {
        // Per API contract, HAL should act as closed after device error
        // But mStatus can be set to error by framework as well, so just log
        // a warning here.
        ALOGW("%s: received notify message in error state.", __FUNCTION__);
    }

    sp<NotificationListener> listener;
    {
        std::lock_guard<std::mutex> l(mOutputLock);
        listener = mListener.promote();
    }

    CaptureOutputStates states {
        mId,
        mInFlightLock, mLastCompletedRegularFrameNumber,
        mLastCompletedReprocessFrameNumber, mLastCompletedZslFrameNumber,
        mInFlightMap, mOutputLock,  mResultQueue, mResultSignal,
        mNextShutterFrameNumber,
        mNextReprocessShutterFrameNumber, mNextZslStillShutterFrameNumber,
        mNextResultFrameNumber,
        mNextReprocessResultFrameNumber, mNextZslStillResultFrameNumber,
        mUseHalBufManager, mUsePartialResult, mNeedFixupMonochromeTags,
        mNumPartialResults, mVendorTagId, mDeviceInfo, mPhysicalDeviceInfoMap,
        mResultMetadataQueue, mDistortionMappers, mZoomRatioMappers, mRotateAndCropMappers,
        mTagMonitor, mInputStream, mOutputStreams, listener, *this, *this, *mInterface
    };
    for (const auto& msg : msgs) {
        camera3::notify(states, msg);
    }
    return hardware::Void();
}

status_t Camera3Device::captureList(const List<const PhysicalCameraSettingsList> &requestsList,
                                    const std::list<const SurfaceMap> &surfaceMaps,
                                    int64_t *lastFrameNumber) {
    ATRACE_CALL();

    return submitRequestsHelper(requestsList, surfaceMaps, /*repeating*/false, lastFrameNumber);
}

status_t Camera3Device::setStreamingRequest(const CameraMetadata &request,
                                            int64_t* /*lastFrameNumber*/) {
    ATRACE_CALL();

    List<const PhysicalCameraSettingsList> requestsList;
    std::list<const SurfaceMap> surfaceMaps;
    convertToRequestList(requestsList, surfaceMaps, request);

    return setStreamingRequestList(requestsList, /*surfaceMap*/surfaceMaps,
                                   /*lastFrameNumber*/NULL);
}

status_t Camera3Device::setStreamingRequestList(
        const List<const PhysicalCameraSettingsList> &requestsList,
        const std::list<const SurfaceMap> &surfaceMaps, int64_t *lastFrameNumber) {
    ATRACE_CALL();

    return submitRequestsHelper(requestsList, surfaceMaps, /*repeating*/true, lastFrameNumber);
}

sp<Camera3Device::CaptureRequest> Camera3Device::setUpRequestLocked(
        const PhysicalCameraSettingsList &request, const SurfaceMap &surfaceMap) {
    status_t res;

    if (mStatus == STATUS_UNCONFIGURED || mNeedConfig) {
        // This point should only be reached via API1 (API2 must explicitly call configureStreams)
        // so unilaterally select normal operating mode.
        res = filterParamsAndConfigureLocked(request.begin()->metadata,
                CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE);
        // Stream configuration failed. Client might try other configuraitons.
        if (res != OK) {
            CLOGE("Can't set up streams: %s (%d)", strerror(-res), res);
            return NULL;
        } else if (mStatus == STATUS_UNCONFIGURED) {
            // Stream configuration successfully configure to empty stream configuration.
            CLOGE("No streams configured");
            return NULL;
        }
    }

    sp<CaptureRequest> newRequest = createCaptureRequest(request, surfaceMap);
    return newRequest;
}

status_t Camera3Device::clearStreamingRequest(int64_t *lastFrameNumber) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    switch (mStatus) {
        case STATUS_ERROR:
            CLOGE("Device has encountered a serious error");
            return INVALID_OPERATION;
        case STATUS_UNINITIALIZED:
            CLOGE("Device not initialized");
            return INVALID_OPERATION;
        case STATUS_UNCONFIGURED:
        case STATUS_CONFIGURED:
        case STATUS_ACTIVE:
            // OK
            break;
        default:
            SET_ERR_L("Unexpected status: %d", mStatus);
            return INVALID_OPERATION;
    }
    ALOGV("Camera %s: Clearing repeating request", mId.string());

    return mRequestThread->clearRepeatingRequests(lastFrameNumber);
}

status_t Camera3Device::waitUntilRequestReceived(int32_t requestId, nsecs_t timeout) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);

    return mRequestThread->waitUntilRequestProcessed(requestId, timeout);
}

status_t Camera3Device::createInputStream(
        uint32_t width, uint32_t height, int format, int *id) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    nsecs_t maxExpectedDuration = getExpectedInFlightDuration();
    Mutex::Autolock l(mLock);
    ALOGV("Camera %s: Creating new input stream %d: %d x %d, format %d",
            mId.string(), mNextStreamId, width, height, format);

    status_t res;
    bool wasActive = false;

    switch (mStatus) {
        case STATUS_ERROR:
            ALOGE("%s: Device has encountered a serious error", __FUNCTION__);
            return INVALID_OPERATION;
        case STATUS_UNINITIALIZED:
            ALOGE("%s: Device not initialized", __FUNCTION__);
            return INVALID_OPERATION;
        case STATUS_UNCONFIGURED:
        case STATUS_CONFIGURED:
            // OK
            break;
        case STATUS_ACTIVE:
            ALOGV("%s: Stopping activity to reconfigure streams", __FUNCTION__);
            res = internalPauseAndWaitLocked(maxExpectedDuration);
            if (res != OK) {
                SET_ERR_L("Can't pause captures to reconfigure streams!");
                return res;
            }
            wasActive = true;
            break;
        default:
            SET_ERR_L("%s: Unexpected status: %d", mStatus);
            return INVALID_OPERATION;
    }
    assert(mStatus != STATUS_ACTIVE);

    if (mInputStream != 0) {
        ALOGE("%s: Cannot create more than 1 input stream", __FUNCTION__);
        return INVALID_OPERATION;
    }

    sp<Camera3InputStream> newStream = new Camera3InputStream(mNextStreamId,
                width, height, format);
    newStream->setStatusTracker(mStatusTracker);

    mInputStream = newStream;

    *id = mNextStreamId++;

    // Continue captures if active at start
    if (wasActive) {
        ALOGV("%s: Restarting activity to reconfigure streams", __FUNCTION__);
        // Reuse current operating mode and session parameters for new stream config
        res = configureStreamsLocked(mOperatingMode, mSessionParams);
        if (res != OK) {
            ALOGE("%s: Can't reconfigure device for new stream %d: %s (%d)",
                    __FUNCTION__, mNextStreamId, strerror(-res), res);
            return res;
        }
        internalResumeLocked();
    }

    ALOGV("Camera %s: Created input stream", mId.string());
    return OK;
}

status_t Camera3Device::createStream(sp<Surface> consumer,
            uint32_t width, uint32_t height, int format,
            android_dataspace dataSpace, camera3_stream_rotation_t rotation, int *id,
            const String8& physicalCameraId,
            std::vector<int> *surfaceIds, int streamSetId, bool isShared, uint64_t consumerUsage) {
    ATRACE_CALL();

    if (consumer == nullptr) {
        ALOGE("%s: consumer must not be null", __FUNCTION__);
        return BAD_VALUE;
    }

    std::vector<sp<Surface>> consumers;
    consumers.push_back(consumer);

    return createStream(consumers, /*hasDeferredConsumer*/ false, width, height,
            format, dataSpace, rotation, id, physicalCameraId, surfaceIds, streamSetId,
            isShared, consumerUsage);
}

status_t Camera3Device::createStream(const std::vector<sp<Surface>>& consumers,
        bool hasDeferredConsumer, uint32_t width, uint32_t height, int format,
        android_dataspace dataSpace, camera3_stream_rotation_t rotation, int *id,
        const String8& physicalCameraId,
        std::vector<int> *surfaceIds, int streamSetId, bool isShared, uint64_t consumerUsage) {
    ATRACE_CALL();

    Mutex::Autolock il(mInterfaceLock);
    nsecs_t maxExpectedDuration = getExpectedInFlightDuration();
    Mutex::Autolock l(mLock);
    ALOGV("Camera %s: Creating new stream %d: %d x %d, format %d, dataspace %d rotation %d"
            " consumer usage %" PRIu64 ", isShared %d, physicalCameraId %s", mId.string(),
            mNextStreamId, width, height, format, dataSpace, rotation, consumerUsage, isShared,
            physicalCameraId.string());

    status_t res;
    bool wasActive = false;

    switch (mStatus) {
        case STATUS_ERROR:
            CLOGE("Device has encountered a serious error");
            return INVALID_OPERATION;
        case STATUS_UNINITIALIZED:
            CLOGE("Device not initialized");
            return INVALID_OPERATION;
        case STATUS_UNCONFIGURED:
        case STATUS_CONFIGURED:
            // OK
            break;
        case STATUS_ACTIVE:
            ALOGV("%s: Stopping activity to reconfigure streams", __FUNCTION__);
            res = internalPauseAndWaitLocked(maxExpectedDuration);
            if (res != OK) {
                SET_ERR_L("Can't pause captures to reconfigure streams!");
                return res;
            }
            wasActive = true;
            break;
        default:
            SET_ERR_L("Unexpected status: %d", mStatus);
            return INVALID_OPERATION;
    }
    assert(mStatus != STATUS_ACTIVE);

    sp<Camera3OutputStream> newStream;

    if (consumers.size() == 0 && !hasDeferredConsumer) {
        ALOGE("%s: Number of consumers cannot be smaller than 1", __FUNCTION__);
        return BAD_VALUE;
    }

    if (hasDeferredConsumer && format != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
        ALOGE("Deferred consumer stream creation only support IMPLEMENTATION_DEFINED format");
        return BAD_VALUE;
    }

    if (format == HAL_PIXEL_FORMAT_BLOB) {
        ssize_t blobBufferSize;
        if (dataSpace == HAL_DATASPACE_DEPTH) {
            blobBufferSize = getPointCloudBufferSize();
            if (blobBufferSize <= 0) {
                SET_ERR_L("Invalid point cloud buffer size %zd", blobBufferSize);
                return BAD_VALUE;
            }
        } else if (dataSpace == static_cast<android_dataspace>(HAL_DATASPACE_JPEG_APP_SEGMENTS)) {
            blobBufferSize = width * height;
        } else {
            blobBufferSize = getJpegBufferSize(width, height);
            if (blobBufferSize <= 0) {
                SET_ERR_L("Invalid jpeg buffer size %zd", blobBufferSize);
                return BAD_VALUE;
            }
        }
        newStream = new Camera3OutputStream(mNextStreamId, consumers[0],
                width, height, blobBufferSize, format, dataSpace, rotation,
                mTimestampOffset, physicalCameraId, streamSetId);
    } else if (format == HAL_PIXEL_FORMAT_RAW_OPAQUE) {
        ssize_t rawOpaqueBufferSize = getRawOpaqueBufferSize(width, height);
        if (rawOpaqueBufferSize <= 0) {
            SET_ERR_L("Invalid RAW opaque buffer size %zd", rawOpaqueBufferSize);
            return BAD_VALUE;
        }
        newStream = new Camera3OutputStream(mNextStreamId, consumers[0],
                width, height, rawOpaqueBufferSize, format, dataSpace, rotation,
                mTimestampOffset, physicalCameraId, streamSetId);
    } else if (isShared) {
        newStream = new Camera3SharedOutputStream(mNextStreamId, consumers,
                width, height, format, consumerUsage, dataSpace, rotation,
                mTimestampOffset, physicalCameraId, streamSetId,
                mUseHalBufManager);
    } else if (consumers.size() == 0 && hasDeferredConsumer) {
        newStream = new Camera3OutputStream(mNextStreamId,
                width, height, format, consumerUsage, dataSpace, rotation,
                mTimestampOffset, physicalCameraId, streamSetId);
    } else {
        newStream = new Camera3OutputStream(mNextStreamId, consumers[0],
                width, height, format, dataSpace, rotation,
                mTimestampOffset, physicalCameraId, streamSetId);
    }

    size_t consumerCount = consumers.size();
    for (size_t i = 0; i < consumerCount; i++) {
        int id = newStream->getSurfaceId(consumers[i]);
        if (id < 0) {
            SET_ERR_L("Invalid surface id");
            return BAD_VALUE;
        }
        if (surfaceIds != nullptr) {
            surfaceIds->push_back(id);
        }
    }

    newStream->setStatusTracker(mStatusTracker);

    newStream->setBufferManager(mBufferManager);

    res = mOutputStreams.add(mNextStreamId, newStream);
    if (res < 0) {
        SET_ERR_L("Can't add new stream to set: %s (%d)", strerror(-res), res);
        return res;
    }

    *id = mNextStreamId++;
    mNeedConfig = true;

    // Continue captures if active at start
    if (wasActive) {
        ALOGV("%s: Restarting activity to reconfigure streams", __FUNCTION__);
        // Reuse current operating mode and session parameters for new stream config
        res = configureStreamsLocked(mOperatingMode, mSessionParams);
        if (res != OK) {
            CLOGE("Can't reconfigure device for new stream %d: %s (%d)",
                    mNextStreamId, strerror(-res), res);
            return res;
        }
        internalResumeLocked();
    }
    ALOGV("Camera %s: Created new stream", mId.string());
    return OK;
}

status_t Camera3Device::getStreamInfo(int id, StreamInfo *streamInfo) {
    ATRACE_CALL();
    if (nullptr == streamInfo) {
        return BAD_VALUE;
    }
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    switch (mStatus) {
        case STATUS_ERROR:
            CLOGE("Device has encountered a serious error");
            return INVALID_OPERATION;
        case STATUS_UNINITIALIZED:
            CLOGE("Device not initialized!");
            return INVALID_OPERATION;
        case STATUS_UNCONFIGURED:
        case STATUS_CONFIGURED:
        case STATUS_ACTIVE:
            // OK
            break;
        default:
            SET_ERR_L("Unexpected status: %d", mStatus);
            return INVALID_OPERATION;
    }

    sp<Camera3StreamInterface> stream = mOutputStreams.get(id);
    if (stream == nullptr) {
        CLOGE("Stream %d is unknown", id);
        return BAD_VALUE;
    }

    streamInfo->width  = stream->getWidth();
    streamInfo->height = stream->getHeight();
    streamInfo->format = stream->getFormat();
    streamInfo->dataSpace = stream->getDataSpace();
    streamInfo->formatOverridden = stream->isFormatOverridden();
    streamInfo->originalFormat = stream->getOriginalFormat();
    streamInfo->dataSpaceOverridden = stream->isDataSpaceOverridden();
    streamInfo->originalDataSpace = stream->getOriginalDataSpace();
    return OK;
}

status_t Camera3Device::setStreamTransform(int id,
        int transform) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    switch (mStatus) {
        case STATUS_ERROR:
            CLOGE("Device has encountered a serious error");
            return INVALID_OPERATION;
        case STATUS_UNINITIALIZED:
            CLOGE("Device not initialized");
            return INVALID_OPERATION;
        case STATUS_UNCONFIGURED:
        case STATUS_CONFIGURED:
        case STATUS_ACTIVE:
            // OK
            break;
        default:
            SET_ERR_L("Unexpected status: %d", mStatus);
            return INVALID_OPERATION;
    }

    sp<Camera3OutputStreamInterface> stream = mOutputStreams.get(id);
    if (stream == nullptr) {
        CLOGE("Stream %d does not exist", id);
        return BAD_VALUE;
    }
    return stream->setTransform(transform);
}

status_t Camera3Device::deleteStream(int id) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);
    status_t res;

    ALOGV("%s: Camera %s: Deleting stream %d", __FUNCTION__, mId.string(), id);

    // CameraDevice semantics require device to already be idle before
    // deleteStream is called, unlike for createStream.
    if (mStatus == STATUS_ACTIVE) {
        ALOGW("%s: Camera %s: Device not idle", __FUNCTION__, mId.string());
        return -EBUSY;
    }

    if (mStatus == STATUS_ERROR) {
        ALOGW("%s: Camera %s: deleteStream not allowed in ERROR state",
                __FUNCTION__, mId.string());
        return -EBUSY;
    }

    sp<Camera3StreamInterface> deletedStream;
    sp<Camera3StreamInterface> stream = mOutputStreams.get(id);
    if (mInputStream != NULL && id == mInputStream->getId()) {
        deletedStream = mInputStream;
        mInputStream.clear();
    } else {
        if (stream == nullptr) {
            CLOGE("Stream %d does not exist", id);
            return BAD_VALUE;
        }
    }

    // Delete output stream or the output part of a bi-directional stream.
    if (stream != nullptr) {
        deletedStream = stream;
        mOutputStreams.remove(id);
    }

    // Free up the stream endpoint so that it can be used by some other stream
    res = deletedStream->disconnect();
    if (res != OK) {
        SET_ERR_L("Can't disconnect deleted stream %d", id);
        // fall through since we want to still list the stream as deleted.
    }
    mDeletedStreams.add(deletedStream);
    mNeedConfig = true;

    return res;
}

status_t Camera3Device::configureStreams(const CameraMetadata& sessionParams, int operatingMode) {
    ATRACE_CALL();
    ALOGV("%s: E", __FUNCTION__);

    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    // In case the client doesn't include any session parameter, try a
    // speculative configuration using the values from the last cached
    // default request.
    if (sessionParams.isEmpty() &&
            ((mLastTemplateId > 0) && (mLastTemplateId < CAMERA3_TEMPLATE_COUNT)) &&
            (!mRequestTemplateCache[mLastTemplateId].isEmpty())) {
        ALOGV("%s: Speculative session param configuration with template id: %d", __func__,
                mLastTemplateId);
        return filterParamsAndConfigureLocked(mRequestTemplateCache[mLastTemplateId],
                operatingMode);
    }

    return filterParamsAndConfigureLocked(sessionParams, operatingMode);
}

status_t Camera3Device::filterParamsAndConfigureLocked(const CameraMetadata& sessionParams,
        int operatingMode) {
    //Filter out any incoming session parameters
    const CameraMetadata params(sessionParams);
    camera_metadata_entry_t availableSessionKeys = mDeviceInfo.find(
            ANDROID_REQUEST_AVAILABLE_SESSION_KEYS);
    CameraMetadata filteredParams(availableSessionKeys.count);
    camera_metadata_t *meta = const_cast<camera_metadata_t *>(
            filteredParams.getAndLock());
    set_camera_metadata_vendor_id(meta, mVendorTagId);
    filteredParams.unlock(meta);
    if (availableSessionKeys.count > 0) {
        for (size_t i = 0; i < availableSessionKeys.count; i++) {
            camera_metadata_ro_entry entry = params.find(
                    availableSessionKeys.data.i32[i]);
            if (entry.count > 0) {
                filteredParams.update(entry);
            }
        }
    }

    return configureStreamsLocked(operatingMode, filteredParams);
}

status_t Camera3Device::getInputBufferProducer(
        sp<IGraphicBufferProducer> *producer) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    if (producer == NULL) {
        return BAD_VALUE;
    } else if (mInputStream == NULL) {
        return INVALID_OPERATION;
    }

    return mInputStream->getInputBufferProducer(producer);
}

status_t Camera3Device::createDefaultRequest(int templateId,
        CameraMetadata *request) {
    ATRACE_CALL();
    ALOGV("%s: for template %d", __FUNCTION__, templateId);

    if (templateId <= 0 || templateId >= CAMERA3_TEMPLATE_COUNT) {
        android_errorWriteWithInfoLog(CameraService::SN_EVENT_LOG_ID, "26866110",
                CameraThreadState::getCallingUid(), nullptr, 0);
        return BAD_VALUE;
    }

    Mutex::Autolock il(mInterfaceLock);

    {
        Mutex::Autolock l(mLock);
        switch (mStatus) {
            case STATUS_ERROR:
                CLOGE("Device has encountered a serious error");
                return INVALID_OPERATION;
            case STATUS_UNINITIALIZED:
                CLOGE("Device is not initialized!");
                return INVALID_OPERATION;
            case STATUS_UNCONFIGURED:
            case STATUS_CONFIGURED:
            case STATUS_ACTIVE:
                // OK
                break;
            default:
                SET_ERR_L("Unexpected status: %d", mStatus);
                return INVALID_OPERATION;
        }

        if (!mRequestTemplateCache[templateId].isEmpty()) {
            *request = mRequestTemplateCache[templateId];
            mLastTemplateId = templateId;
            return OK;
        }
    }

    camera_metadata_t *rawRequest;
    status_t res = mInterface->constructDefaultRequestSettings(
            (camera3_request_template_t) templateId, &rawRequest);

    {
        Mutex::Autolock l(mLock);
        if (res == BAD_VALUE) {
            ALOGI("%s: template %d is not supported on this camera device",
                  __FUNCTION__, templateId);
            return res;
        } else if (res != OK) {
            CLOGE("Unable to construct request template %d: %s (%d)",
                    templateId, strerror(-res), res);
            return res;
        }

        set_camera_metadata_vendor_id(rawRequest, mVendorTagId);
        mRequestTemplateCache[templateId].acquire(rawRequest);

        // Override the template request with zoomRatioMapper
        res = mZoomRatioMappers[mId.c_str()].initZoomRatioInTemplate(
                &mRequestTemplateCache[templateId]);
        if (res != OK) {
            CLOGE("Failed to update zoom ratio for template %d: %s (%d)",
                    templateId, strerror(-res), res);
            return res;
        }

        // Fill in JPEG_QUALITY if not available
        if (!mRequestTemplateCache[templateId].exists(ANDROID_JPEG_QUALITY)) {
            static const uint8_t kDefaultJpegQuality = 95;
            mRequestTemplateCache[templateId].update(ANDROID_JPEG_QUALITY,
                    &kDefaultJpegQuality, 1);
        }

        *request = mRequestTemplateCache[templateId];
        mLastTemplateId = templateId;
    }
    return OK;
}

status_t Camera3Device::waitUntilDrained() {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    nsecs_t maxExpectedDuration = getExpectedInFlightDuration();
    Mutex::Autolock l(mLock);

    return waitUntilDrainedLocked(maxExpectedDuration);
}

status_t Camera3Device::waitUntilDrainedLocked(nsecs_t maxExpectedDuration) {
    switch (mStatus) {
        case STATUS_UNINITIALIZED:
        case STATUS_UNCONFIGURED:
            ALOGV("%s: Already idle", __FUNCTION__);
            return OK;
        case STATUS_CONFIGURED:
            // To avoid race conditions, check with tracker to be sure
        case STATUS_ERROR:
        case STATUS_ACTIVE:
            // Need to verify shut down
            break;
        default:
            SET_ERR_L("Unexpected status: %d",mStatus);
            return INVALID_OPERATION;
    }
    ALOGV("%s: Camera %s: Waiting until idle (%" PRIi64 "ns)", __FUNCTION__, mId.string(),
            maxExpectedDuration);
    status_t res = waitUntilStateThenRelock(/*active*/ false, maxExpectedDuration);
    if (res != OK) {
        SET_ERR_L("Error waiting for HAL to drain: %s (%d)", strerror(-res),
                res);
    }
    return res;
}


void Camera3Device::internalUpdateStatusLocked(Status status) {
    mStatus = status;
    mRecentStatusUpdates.add(mStatus);
    mStatusChanged.broadcast();
}

// Pause to reconfigure
status_t Camera3Device::internalPauseAndWaitLocked(nsecs_t maxExpectedDuration) {
    if (mRequestThread.get() != nullptr) {
        mRequestThread->setPaused(true);
    } else {
        return NO_INIT;
    }

    ALOGV("%s: Camera %s: Internal wait until idle (% " PRIi64 " ns)", __FUNCTION__, mId.string(),
          maxExpectedDuration);
    status_t res = waitUntilStateThenRelock(/*active*/ false, maxExpectedDuration);
    if (res != OK) {
        SET_ERR_L("Can't idle device in %f seconds!",
                maxExpectedDuration/1e9);
    }

    return res;
}

// Resume after internalPauseAndWaitLocked
status_t Camera3Device::internalResumeLocked() {
    status_t res;

    mRequestThread->setPaused(false);

    ALOGV("%s: Camera %s: Internal wait until active (% " PRIi64 " ns)", __FUNCTION__, mId.string(),
            kActiveTimeout);
    res = waitUntilStateThenRelock(/*active*/ true, kActiveTimeout);
    if (res != OK) {
        SET_ERR_L("Can't transition to active in %f seconds!",
                kActiveTimeout/1e9);
    }
    mPauseStateNotify = false;
    return OK;
}

status_t Camera3Device::waitUntilStateThenRelock(bool active, nsecs_t timeout) {
    status_t res = OK;

    size_t startIndex = 0;
    if (mStatusWaiters == 0) {
        // Clear the list of recent statuses if there are no existing threads waiting on updates to
        // this status list
        mRecentStatusUpdates.clear();
    } else {
        // If other threads are waiting on updates to this status list, set the position of the
        // first element that this list will check rather than clearing the list.
        startIndex = mRecentStatusUpdates.size();
    }

    mStatusWaiters++;

    bool signalPipelineDrain = false;
    if (!active && mUseHalBufManager) {
        auto streamIds = mOutputStreams.getStreamIds();
        if (mStatus == STATUS_ACTIVE) {
            mRequestThread->signalPipelineDrain(streamIds);
            signalPipelineDrain = true;
        }
        mRequestBufferSM.onWaitUntilIdle();
    }

    bool stateSeen = false;
    do {
        if (active == (mStatus == STATUS_ACTIVE)) {
            // Desired state is current
            break;
        }

        res = mStatusChanged.waitRelative(mLock, timeout);
        if (res != OK) break;

        // This is impossible, but if not, could result in subtle deadlocks and invalid state
        // transitions.
        LOG_ALWAYS_FATAL_IF(startIndex > mRecentStatusUpdates.size(),
                "%s: Skipping status updates in Camera3Device, may result in deadlock.",
                __FUNCTION__);

        // Encountered desired state since we began waiting
        for (size_t i = startIndex; i < mRecentStatusUpdates.size(); i++) {
            if (active == (mRecentStatusUpdates[i] == STATUS_ACTIVE) ) {
                stateSeen = true;
                break;
            }
        }
    } while (!stateSeen);

    if (signalPipelineDrain) {
        mRequestThread->resetPipelineDrain();
    }

    mStatusWaiters--;

    return res;
}


status_t Camera3Device::setNotifyCallback(wp<NotificationListener> listener) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> l(mOutputLock);

    if (listener != NULL && mListener != NULL) {
        ALOGW("%s: Replacing old callback listener", __FUNCTION__);
    }
    mListener = listener;
    mRequestThread->setNotificationListener(listener);
    mPreparerThread->setNotificationListener(listener);

    return OK;
}

bool Camera3Device::willNotify3A() {
    return false;
}

status_t Camera3Device::waitForNextFrame(nsecs_t timeout) {
    ATRACE_CALL();
    std::unique_lock<std::mutex> l(mOutputLock);

    while (mResultQueue.empty()) {
        auto st = mResultSignal.wait_for(l, std::chrono::nanoseconds(timeout));
        if (st == std::cv_status::timeout) {
            return TIMED_OUT;
        }
    }
    return OK;
}

status_t Camera3Device::getNextResult(CaptureResult *frame) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> l(mOutputLock);

    if (mResultQueue.empty()) {
        return NOT_ENOUGH_DATA;
    }

    if (frame == NULL) {
        ALOGE("%s: argument cannot be NULL", __FUNCTION__);
        return BAD_VALUE;
    }

    CaptureResult &result = *(mResultQueue.begin());
    frame->mResultExtras = result.mResultExtras;
    frame->mMetadata.acquire(result.mMetadata);
    frame->mPhysicalMetadatas = std::move(result.mPhysicalMetadatas);
    mResultQueue.erase(mResultQueue.begin());

    return OK;
}

status_t Camera3Device::triggerAutofocus(uint32_t id) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);

    ALOGV("%s: Triggering autofocus, id %d", __FUNCTION__, id);
    // Mix-in this trigger into the next request and only the next request.
    RequestTrigger trigger[] = {
        {
            ANDROID_CONTROL_AF_TRIGGER,
            ANDROID_CONTROL_AF_TRIGGER_START
        },
        {
            ANDROID_CONTROL_AF_TRIGGER_ID,
            static_cast<int32_t>(id)
        }
    };

    return mRequestThread->queueTrigger(trigger,
                                        sizeof(trigger)/sizeof(trigger[0]));
}

status_t Camera3Device::triggerCancelAutofocus(uint32_t id) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);

    ALOGV("%s: Triggering cancel autofocus, id %d", __FUNCTION__, id);
    // Mix-in this trigger into the next request and only the next request.
    RequestTrigger trigger[] = {
        {
            ANDROID_CONTROL_AF_TRIGGER,
            ANDROID_CONTROL_AF_TRIGGER_CANCEL
        },
        {
            ANDROID_CONTROL_AF_TRIGGER_ID,
            static_cast<int32_t>(id)
        }
    };

    return mRequestThread->queueTrigger(trigger,
                                        sizeof(trigger)/sizeof(trigger[0]));
}

status_t Camera3Device::triggerPrecaptureMetering(uint32_t id) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);

    ALOGV("%s: Triggering precapture metering, id %d", __FUNCTION__, id);
    // Mix-in this trigger into the next request and only the next request.
    RequestTrigger trigger[] = {
        {
            ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
            ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START
        },
        {
            ANDROID_CONTROL_AE_PRECAPTURE_ID,
            static_cast<int32_t>(id)
        }
    };

    return mRequestThread->queueTrigger(trigger,
                                        sizeof(trigger)/sizeof(trigger[0]));
}

status_t Camera3Device::flush(int64_t *frameNumber) {
    ATRACE_CALL();
    ALOGV("%s: Camera %s: Flushing all requests", __FUNCTION__, mId.string());
    Mutex::Autolock il(mInterfaceLock);

    {
        Mutex::Autolock l(mLock);

        // b/116514106 "disconnect()" can get called twice for the same device. The
        // camera device will not be initialized during the second run.
        if (mStatus == STATUS_UNINITIALIZED) {
            return OK;
        }

        mRequestThread->clear(/*out*/frameNumber);
    }

    return mRequestThread->flush();
}

status_t Camera3Device::prepare(int streamId) {
    return prepare(camera3::Camera3StreamInterface::ALLOCATE_PIPELINE_MAX, streamId);
}

status_t Camera3Device::prepare(int maxCount, int streamId) {
    ATRACE_CALL();
    ALOGV("%s: Camera %s: Preparing stream %d", __FUNCTION__, mId.string(), streamId);
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    sp<Camera3StreamInterface> stream = mOutputStreams.get(streamId);
    if (stream == nullptr) {
        CLOGE("Stream %d does not exist", streamId);
        return BAD_VALUE;
    }

    if (stream->isUnpreparable() || stream->hasOutstandingBuffers() ) {
        CLOGE("Stream %d has already been a request target", streamId);
        return BAD_VALUE;
    }

    if (mRequestThread->isStreamPending(stream)) {
        CLOGE("Stream %d is already a target in a pending request", streamId);
        return BAD_VALUE;
    }

    return mPreparerThread->prepare(maxCount, stream);
}

status_t Camera3Device::tearDown(int streamId) {
    ATRACE_CALL();
    ALOGV("%s: Camera %s: Tearing down stream %d", __FUNCTION__, mId.string(), streamId);
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    sp<Camera3StreamInterface> stream = mOutputStreams.get(streamId);
    if (stream == nullptr) {
        CLOGE("Stream %d does not exist", streamId);
        return BAD_VALUE;
    }

    if (stream->hasOutstandingBuffers() || mRequestThread->isStreamPending(stream)) {
        CLOGE("Stream %d is a target of a in-progress request", streamId);
        return BAD_VALUE;
    }

    return stream->tearDown();
}

status_t Camera3Device::addBufferListenerForStream(int streamId,
        wp<Camera3StreamBufferListener> listener) {
    ATRACE_CALL();
    ALOGV("%s: Camera %s: Adding buffer listener for stream %d", __FUNCTION__, mId.string(), streamId);
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    sp<Camera3StreamInterface> stream = mOutputStreams.get(streamId);
    if (stream == nullptr) {
        CLOGE("Stream %d does not exist", streamId);
        return BAD_VALUE;
    }
    stream->addBufferListener(listener);

    return OK;
}

/**
 * Methods called by subclasses
 */

void Camera3Device::notifyStatus(bool idle) {
    ATRACE_CALL();
    {
        // Need mLock to safely update state and synchronize to current
        // state of methods in flight.
        Mutex::Autolock l(mLock);
        // We can get various system-idle notices from the status tracker
        // while starting up. Only care about them if we've actually sent
        // in some requests recently.
        if (mStatus != STATUS_ACTIVE && mStatus != STATUS_CONFIGURED) {
            return;
        }
        ALOGV("%s: Camera %s: Now %s, pauseState: %s", __FUNCTION__, mId.string(),
                idle ? "idle" : "active", mPauseStateNotify ? "true" : "false");
        internalUpdateStatusLocked(idle ? STATUS_CONFIGURED : STATUS_ACTIVE);

        // Skip notifying listener if we're doing some user-transparent
        // state changes
        if (mPauseStateNotify) return;
    }

    sp<NotificationListener> listener;
    {
        std::lock_guard<std::mutex> l(mOutputLock);
        listener = mListener.promote();
    }
    if (idle && listener != NULL) {
        listener->notifyIdle();
    }
}

status_t Camera3Device::setConsumerSurfaces(int streamId,
        const std::vector<sp<Surface>>& consumers, std::vector<int> *surfaceIds) {
    ATRACE_CALL();
    ALOGV("%s: Camera %s: set consumer surface for stream %d",
            __FUNCTION__, mId.string(), streamId);

    if (surfaceIds == nullptr) {
        return BAD_VALUE;
    }

    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    if (consumers.size() == 0) {
        CLOGE("No consumer is passed!");
        return BAD_VALUE;
    }

    sp<Camera3OutputStreamInterface> stream = mOutputStreams.get(streamId);
    if (stream == nullptr) {
        CLOGE("Stream %d is unknown", streamId);
        return BAD_VALUE;
    }

    // isConsumerConfigurationDeferred will be off after setConsumers
    bool isDeferred = stream->isConsumerConfigurationDeferred();
    status_t res = stream->setConsumers(consumers);
    if (res != OK) {
        CLOGE("Stream %d set consumer failed (error %d %s) ", streamId, res, strerror(-res));
        return res;
    }

    for (auto &consumer : consumers) {
        int id = stream->getSurfaceId(consumer);
        if (id < 0) {
            CLOGE("Invalid surface id!");
            return BAD_VALUE;
        }
        surfaceIds->push_back(id);
    }

    if (isDeferred) {
        if (!stream->isConfiguring()) {
            CLOGE("Stream %d was already fully configured.", streamId);
            return INVALID_OPERATION;
        }

        res = stream->finishConfiguration();
        if (res != OK) {
            // If finishConfiguration fails due to abandoned surface, do not set
            // device to error state.
            bool isSurfaceAbandoned =
                    (res == NO_INIT || res == DEAD_OBJECT) && stream->isAbandoned();
            if (!isSurfaceAbandoned) {
                SET_ERR_L("Can't finish configuring output stream %d: %s (%d)",
                        stream->getId(), strerror(-res), res);
            }
            return res;
        }
    }

    return OK;
}

status_t Camera3Device::updateStream(int streamId, const std::vector<sp<Surface>> &newSurfaces,
        const std::vector<OutputStreamInfo> &outputInfo,
        const std::vector<size_t> &removedSurfaceIds, KeyedVector<sp<Surface>, size_t> *outputMap) {
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    sp<Camera3OutputStreamInterface> stream = mOutputStreams.get(streamId);
    if (stream == nullptr) {
        CLOGE("Stream %d is unknown", streamId);
        return BAD_VALUE;
    }

    for (const auto &it : removedSurfaceIds) {
        if (mRequestThread->isOutputSurfacePending(streamId, it)) {
            CLOGE("Shared surface still part of a pending request!");
            return -EBUSY;
        }
    }

    status_t res = stream->updateStream(newSurfaces, outputInfo, removedSurfaceIds, outputMap);
    if (res != OK) {
        CLOGE("Stream %d failed to update stream (error %d %s) ",
              streamId, res, strerror(-res));
        if (res == UNKNOWN_ERROR) {
            SET_ERR_L("%s: Stream update failed to revert to previous output configuration!",
                    __FUNCTION__);
        }
        return res;
    }

    return res;
}

status_t Camera3Device::dropStreamBuffers(bool dropping, int streamId) {
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);

    sp<Camera3OutputStreamInterface> stream = mOutputStreams.get(streamId);
    if (stream == nullptr) {
        ALOGE("%s: Stream %d is not found.", __FUNCTION__, streamId);
        return BAD_VALUE;
    }
    return stream->dropBuffers(dropping);
}

/**
 * Camera3Device private methods
 */

sp<Camera3Device::CaptureRequest> Camera3Device::createCaptureRequest(
        const PhysicalCameraSettingsList &request, const SurfaceMap &surfaceMap) {
    ATRACE_CALL();

    sp<CaptureRequest> newRequest = new CaptureRequest();
    newRequest->mSettingsList = request;

    camera_metadata_entry_t inputStreams =
            newRequest->mSettingsList.begin()->metadata.find(ANDROID_REQUEST_INPUT_STREAMS);
    if (inputStreams.count > 0) {
        if (mInputStream == NULL ||
                mInputStream->getId() != inputStreams.data.i32[0]) {
            CLOGE("Request references unknown input stream %d",
                    inputStreams.data.u8[0]);
            return NULL;
        }

        if (mInputStream->isConfiguring()) {
            SET_ERR_L("%s: input stream %d is not configured!",
                    __FUNCTION__, mInputStream->getId());
            return NULL;
        }
        // Check if stream prepare is blocking requests.
        if (mInputStream->isBlockedByPrepare()) {
            CLOGE("Request references an input stream that's being prepared!");
            return NULL;
        }

        newRequest->mInputStream = mInputStream;
        newRequest->mSettingsList.begin()->metadata.erase(ANDROID_REQUEST_INPUT_STREAMS);
    }

    camera_metadata_entry_t streams =
            newRequest->mSettingsList.begin()->metadata.find(ANDROID_REQUEST_OUTPUT_STREAMS);
    if (streams.count == 0) {
        CLOGE("Zero output streams specified!");
        return NULL;
    }

    for (size_t i = 0; i < streams.count; i++) {
        sp<Camera3OutputStreamInterface> stream = mOutputStreams.get(streams.data.i32[i]);
        if (stream == nullptr) {
            CLOGE("Request references unknown stream %d",
                    streams.data.i32[i]);
            return NULL;
        }
        // It is illegal to include a deferred consumer output stream into a request
        auto iter = surfaceMap.find(streams.data.i32[i]);
        if (iter != surfaceMap.end()) {
            const std::vector<size_t>& surfaces = iter->second;
            for (const auto& surface : surfaces) {
                if (stream->isConsumerConfigurationDeferred(surface)) {
                    CLOGE("Stream %d surface %zu hasn't finished configuration yet "
                          "due to deferred consumer", stream->getId(), surface);
                    return NULL;
                }
            }
            newRequest->mOutputSurfaces[streams.data.i32[i]] = surfaces;
        }

        if (stream->isConfiguring()) {
            SET_ERR_L("%s: stream %d is not configured!", __FUNCTION__, stream->getId());
            return NULL;
        }
        // Check if stream prepare is blocking requests.
        if (stream->isBlockedByPrepare()) {
            CLOGE("Request references an output stream that's being prepared!");
            return NULL;
        }

        newRequest->mOutputStreams.push(stream);
    }
    newRequest->mSettingsList.begin()->metadata.erase(ANDROID_REQUEST_OUTPUT_STREAMS);
    newRequest->mBatchSize = 1;

    auto rotateAndCropEntry =
            newRequest->mSettingsList.begin()->metadata.find(ANDROID_SCALER_ROTATE_AND_CROP);
    if (rotateAndCropEntry.count > 0 &&
            rotateAndCropEntry.data.u8[0] == ANDROID_SCALER_ROTATE_AND_CROP_AUTO) {
        newRequest->mRotateAndCropAuto = true;
    } else {
        newRequest->mRotateAndCropAuto = false;
    }

    auto zoomRatioEntry =
            newRequest->mSettingsList.begin()->metadata.find(ANDROID_CONTROL_ZOOM_RATIO);
    if (zoomRatioEntry.count > 0 &&
            zoomRatioEntry.data.f[0] == 1.0f) {
        newRequest->mZoomRatioIs1x = true;
    } else {
        newRequest->mZoomRatioIs1x = false;
    }

    return newRequest;
}

void Camera3Device::cancelStreamsConfigurationLocked() {
    int res = OK;
    if (mInputStream != NULL && mInputStream->isConfiguring()) {
        res = mInputStream->cancelConfiguration();
        if (res != OK) {
            CLOGE("Can't cancel configuring input stream %d: %s (%d)",
                    mInputStream->getId(), strerror(-res), res);
        }
    }

    for (size_t i = 0; i < mOutputStreams.size(); i++) {
        sp<Camera3OutputStreamInterface> outputStream = mOutputStreams[i];
        if (outputStream->isConfiguring()) {
            res = outputStream->cancelConfiguration();
            if (res != OK) {
                CLOGE("Can't cancel configuring output stream %d: %s (%d)",
                        outputStream->getId(), strerror(-res), res);
            }
        }
    }

    // Return state to that at start of call, so that future configures
    // properly clean things up
    internalUpdateStatusLocked(STATUS_UNCONFIGURED);
    mNeedConfig = true;

    res = mPreparerThread->resume();
    if (res != OK) {
        ALOGE("%s: Camera %s: Preparer thread failed to resume!", __FUNCTION__, mId.string());
    }
}

bool Camera3Device::checkAbandonedStreamsLocked() {
    if ((mInputStream.get() != nullptr) && (mInputStream->isAbandoned())) {
        return true;
    }

    for (size_t i = 0; i < mOutputStreams.size(); i++) {
        auto stream = mOutputStreams[i];
        if ((stream.get() != nullptr) && (stream->isAbandoned())) {
            return true;
        }
    }

    return false;
}

bool Camera3Device::reconfigureCamera(const CameraMetadata& sessionParams, int clientStatusId) {
    ATRACE_CALL();
    bool ret = false;

    Mutex::Autolock il(mInterfaceLock);
    nsecs_t maxExpectedDuration = getExpectedInFlightDuration();

    Mutex::Autolock l(mLock);
    if (checkAbandonedStreamsLocked()) {
        ALOGW("%s: Abandoned stream detected, session parameters can't be applied correctly!",
                __FUNCTION__);
        return true;
    }

    status_t rc = NO_ERROR;
    bool markClientActive = false;
    if (mStatus == STATUS_ACTIVE) {
        markClientActive = true;
        mPauseStateNotify = true;
        mStatusTracker->markComponentIdle(clientStatusId, Fence::NO_FENCE);

        rc = internalPauseAndWaitLocked(maxExpectedDuration);
    }

    if (rc == NO_ERROR) {
        mNeedConfig = true;
        rc = configureStreamsLocked(mOperatingMode, sessionParams, /*notifyRequestThread*/ false);
        if (rc == NO_ERROR) {
            ret = true;
            mPauseStateNotify = false;
            //Moving to active state while holding 'mLock' is important.
            //There could be pending calls to 'create-/deleteStream' which
            //will trigger another stream configuration while the already
            //present streams end up with outstanding buffers that will
            //not get drained.
            internalUpdateStatusLocked(STATUS_ACTIVE);
        } else if (rc == DEAD_OBJECT) {
            // DEAD_OBJECT can be returned if either the consumer surface is
            // abandoned, or the HAL has died.
            // - If the HAL has died, configureStreamsLocked call will set
            // device to error state,
            // - If surface is abandoned, we should not set device to error
            // state.
            ALOGE("Failed to re-configure camera due to abandoned surface");
        } else {
            SET_ERR_L("Failed to re-configure camera: %d", rc);
        }
    } else {
        ALOGE("%s: Failed to pause streaming: %d", __FUNCTION__, rc);
    }

    if (markClientActive) {
        mStatusTracker->markComponentActive(clientStatusId);
    }

    return ret;
}

status_t Camera3Device::configureStreamsLocked(int operatingMode,
        const CameraMetadata& sessionParams, bool notifyRequestThread) {
    ATRACE_CALL();
    status_t res;

    if (mStatus != STATUS_UNCONFIGURED && mStatus != STATUS_CONFIGURED) {
        CLOGE("Not idle");
        return INVALID_OPERATION;
    }

    if (operatingMode < 0) {
        CLOGE("Invalid operating mode: %d", operatingMode);
        return BAD_VALUE;
    }

    bool isConstrainedHighSpeed =
            static_cast<int>(StreamConfigurationMode::CONSTRAINED_HIGH_SPEED_MODE) ==
            operatingMode;

    if (mOperatingMode != operatingMode) {
        mNeedConfig = true;
        mIsConstrainedHighSpeedConfiguration = isConstrainedHighSpeed;
        mOperatingMode = operatingMode;
    }

    // In case called from configureStreams, abort queued input buffers not belonging to
    // any pending requests.
    if (mInputStream != NULL && notifyRequestThread) {
        while (true) {
            camera3_stream_buffer_t inputBuffer;
            status_t res = mInputStream->getInputBuffer(&inputBuffer,
                    /*respectHalLimit*/ false);
            if (res != OK) {
                // Exhausted acquiring all input buffers.
                break;
            }

            inputBuffer.status = CAMERA3_BUFFER_STATUS_ERROR;
            res = mInputStream->returnInputBuffer(inputBuffer);
            if (res != OK) {
                ALOGE("%s: %d: couldn't return input buffer while clearing input queue: "
                        "%s (%d)", __FUNCTION__, __LINE__, strerror(-res), res);
            }
        }
    }

    if (!mNeedConfig) {
        ALOGV("%s: Skipping config, no stream changes", __FUNCTION__);
        return OK;
    }

    // Workaround for device HALv3.2 or older spec bug - zero streams requires
    // adding a dummy stream instead.
    // TODO: Bug: 17321404 for fixing the HAL spec and removing this workaround.
    if (mOutputStreams.size() == 0) {
        addDummyStreamLocked();
    } else {
        tryRemoveDummyStreamLocked();
    }

    // Start configuring the streams
    ALOGV("%s: Camera %s: Starting stream configuration", __FUNCTION__, mId.string());

    mPreparerThread->pause();

    camera3_stream_configuration config;
    config.operation_mode = mOperatingMode;
    config.num_streams = (mInputStream != NULL) + mOutputStreams.size();

    Vector<camera3_stream_t*> streams;
    streams.setCapacity(config.num_streams);
    std::vector<uint32_t> bufferSizes(config.num_streams, 0);


    if (mInputStream != NULL) {
        camera3_stream_t *inputStream;
        inputStream = mInputStream->startConfiguration();
        if (inputStream == NULL) {
            CLOGE("Can't start input stream configuration");
            cancelStreamsConfigurationLocked();
            return INVALID_OPERATION;
        }
        streams.add(inputStream);
    }

    for (size_t i = 0; i < mOutputStreams.size(); i++) {

        // Don't configure bidi streams twice, nor add them twice to the list
        if (mOutputStreams[i].get() ==
            static_cast<Camera3StreamInterface*>(mInputStream.get())) {

            config.num_streams--;
            continue;
        }

        camera3_stream_t *outputStream;
        outputStream = mOutputStreams[i]->startConfiguration();
        if (outputStream == NULL) {
            CLOGE("Can't start output stream configuration");
            cancelStreamsConfigurationLocked();
            return INVALID_OPERATION;
        }
        streams.add(outputStream);

        if (outputStream->format == HAL_PIXEL_FORMAT_BLOB) {
            size_t k = i + ((mInputStream != nullptr) ? 1 : 0); // Input stream if present should
                                                                // always occupy the initial entry.
            if (outputStream->data_space == HAL_DATASPACE_V0_JFIF) {
                bufferSizes[k] = static_cast<uint32_t>(
                        getJpegBufferSize(outputStream->width, outputStream->height));
            } else if (outputStream->data_space ==
                    static_cast<android_dataspace>(HAL_DATASPACE_JPEG_APP_SEGMENTS)) {
                bufferSizes[k] = outputStream->width * outputStream->height;
            } else {
                ALOGW("%s: Blob dataSpace %d not supported",
                        __FUNCTION__, outputStream->data_space);
            }
        }
    }

    config.streams = streams.editArray();

    // Do the HAL configuration; will potentially touch stream
    // max_buffers, usage, and priv fields, as well as data_space and format
    // fields for IMPLEMENTATION_DEFINED formats.

    const camera_metadata_t *sessionBuffer = sessionParams.getAndLock();
    res = mInterface->configureStreams(sessionBuffer, &config, bufferSizes);
    sessionParams.unlock(sessionBuffer);

    if (res == BAD_VALUE) {
        // HAL rejected this set of streams as unsupported, clean up config
        // attempt and return to unconfigured state
        CLOGE("Set of requested inputs/outputs not supported by HAL");
        cancelStreamsConfigurationLocked();
        return BAD_VALUE;
    } else if (res != OK) {
        // Some other kind of error from configure_streams - this is not
        // expected
        SET_ERR_L("Unable to configure streams with HAL: %s (%d)",
                strerror(-res), res);
        return res;
    }

    // Finish all stream configuration immediately.
    // TODO: Try to relax this later back to lazy completion, which should be
    // faster

    if (mInputStream != NULL && mInputStream->isConfiguring()) {
        bool streamReConfigured = false;
        res = mInputStream->finishConfiguration(&streamReConfigured);
        if (res != OK) {
            CLOGE("Can't finish configuring input stream %d: %s (%d)",
                    mInputStream->getId(), strerror(-res), res);
            cancelStreamsConfigurationLocked();
            if ((res == NO_INIT || res == DEAD_OBJECT) && mInputStream->isAbandoned()) {
                return DEAD_OBJECT;
            }
            return BAD_VALUE;
        }
        if (streamReConfigured) {
            mInterface->onStreamReConfigured(mInputStream->getId());
        }
    }

    for (size_t i = 0; i < mOutputStreams.size(); i++) {
        sp<Camera3OutputStreamInterface> outputStream = mOutputStreams[i];
        if (outputStream->isConfiguring() && !outputStream->isConsumerConfigurationDeferred()) {
            bool streamReConfigured = false;
            res = outputStream->finishConfiguration(&streamReConfigured);
            if (res != OK) {
                CLOGE("Can't finish configuring output stream %d: %s (%d)",
                        outputStream->getId(), strerror(-res), res);
                cancelStreamsConfigurationLocked();
                if ((res == NO_INIT || res == DEAD_OBJECT) && outputStream->isAbandoned()) {
                    return DEAD_OBJECT;
                }
                return BAD_VALUE;
            }
            if (streamReConfigured) {
                mInterface->onStreamReConfigured(outputStream->getId());
            }
        }
    }

    // Request thread needs to know to avoid using repeat-last-settings protocol
    // across configure_streams() calls
    if (notifyRequestThread) {
        mRequestThread->configurationComplete(mIsConstrainedHighSpeedConfiguration, sessionParams);
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("camera.fifo.disable", value, "0");
    int32_t disableFifo = atoi(value);
    if (disableFifo != 1) {
        // Boost priority of request thread to SCHED_FIFO.
        pid_t requestThreadTid = mRequestThread->getTid();
        res = requestPriority(getpid(), requestThreadTid,
                kRequestThreadPriority, /*isForApp*/ false, /*asynchronous*/ false);
        if (res != OK) {
            ALOGW("Can't set realtime priority for request processing thread: %s (%d)",
                    strerror(-res), res);
        } else {
            ALOGD("Set real time priority for request queue thread (tid %d)", requestThreadTid);
        }
    }

    // Update device state
    const camera_metadata_t *newSessionParams = sessionParams.getAndLock();
    const camera_metadata_t *currentSessionParams = mSessionParams.getAndLock();
    bool updateSessionParams = (newSessionParams != currentSessionParams) ? true : false;
    sessionParams.unlock(newSessionParams);
    mSessionParams.unlock(currentSessionParams);
    if (updateSessionParams)  {
        mSessionParams = sessionParams;
    }

    mNeedConfig = false;

    internalUpdateStatusLocked((mDummyStreamId == NO_STREAM) ?
            STATUS_CONFIGURED : STATUS_UNCONFIGURED);

    ALOGV("%s: Camera %s: Stream configuration complete", __FUNCTION__, mId.string());

    // tear down the deleted streams after configure streams.
    mDeletedStreams.clear();

    auto rc = mPreparerThread->resume();
    if (rc != OK) {
        SET_ERR_L("%s: Camera %s: Preparer thread failed to resume!", __FUNCTION__, mId.string());
        return rc;
    }

    if (mDummyStreamId == NO_STREAM) {
        mRequestBufferSM.onStreamsConfigured();
    }

    return OK;
}

status_t Camera3Device::addDummyStreamLocked() {
    ATRACE_CALL();
    status_t res;

    if (mDummyStreamId != NO_STREAM) {
        // Should never be adding a second dummy stream when one is already
        // active
        SET_ERR_L("%s: Camera %s: A dummy stream already exists!",
                __FUNCTION__, mId.string());
        return INVALID_OPERATION;
    }

    ALOGV("%s: Camera %s: Adding a dummy stream", __FUNCTION__, mId.string());

    sp<Camera3OutputStreamInterface> dummyStream =
            new Camera3DummyStream(mNextStreamId);

    res = mOutputStreams.add(mNextStreamId, dummyStream);
    if (res < 0) {
        SET_ERR_L("Can't add dummy stream to set: %s (%d)", strerror(-res), res);
        return res;
    }

    mDummyStreamId = mNextStreamId;
    mNextStreamId++;

    return OK;
}

status_t Camera3Device::tryRemoveDummyStreamLocked() {
    ATRACE_CALL();
    status_t res;

    if (mDummyStreamId == NO_STREAM) return OK;
    if (mOutputStreams.size() == 1) return OK;

    ALOGV("%s: Camera %s: Removing the dummy stream", __FUNCTION__, mId.string());

    // Ok, have a dummy stream and there's at least one other output stream,
    // so remove the dummy

    sp<Camera3StreamInterface> deletedStream = mOutputStreams.get(mDummyStreamId);
    if (deletedStream == nullptr) {
        SET_ERR_L("Dummy stream %d does not appear to exist", mDummyStreamId);
        return INVALID_OPERATION;
    }
    mOutputStreams.remove(mDummyStreamId);

    // Free up the stream endpoint so that it can be used by some other stream
    res = deletedStream->disconnect();
    if (res != OK) {
        SET_ERR_L("Can't disconnect deleted dummy stream %d", mDummyStreamId);
        // fall through since we want to still list the stream as deleted.
    }
    mDeletedStreams.add(deletedStream);
    mDummyStreamId = NO_STREAM;

    return res;
}

void Camera3Device::setErrorState(const char *fmt, ...) {
    ATRACE_CALL();
    Mutex::Autolock l(mLock);
    va_list args;
    va_start(args, fmt);

    setErrorStateLockedV(fmt, args);

    va_end(args);
}

void Camera3Device::setErrorStateV(const char *fmt, va_list args) {
    ATRACE_CALL();
    Mutex::Autolock l(mLock);
    setErrorStateLockedV(fmt, args);
}

void Camera3Device::setErrorStateLocked(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    setErrorStateLockedV(fmt, args);

    va_end(args);
}

void Camera3Device::setErrorStateLockedV(const char *fmt, va_list args) {
    // Print out all error messages to log
    String8 errorCause = String8::formatV(fmt, args);
    ALOGE("Camera %s: %s", mId.string(), errorCause.string());

    // But only do error state transition steps for the first error
    if (mStatus == STATUS_ERROR || mStatus == STATUS_UNINITIALIZED) return;

    mErrorCause = errorCause;

    if (mRequestThread != nullptr) {
        mRequestThread->setPaused(true);
    }
    internalUpdateStatusLocked(STATUS_ERROR);

    // Notify upstream about a device error
    sp<NotificationListener> listener = mListener.promote();
    if (listener != NULL) {
        listener->notifyError(hardware::camera2::ICameraDeviceCallbacks::ERROR_CAMERA_DEVICE,
                CaptureResultExtras());
    }

    // Save stack trace. View by dumping it later.
    CameraTraces::saveTrace();
    // TODO: consider adding errorCause and client pid/procname
}

/**
 * In-flight request management
 */

status_t Camera3Device::registerInFlight(uint32_t frameNumber,
        int32_t numBuffers, CaptureResultExtras resultExtras, bool hasInput,
        bool hasAppCallback, nsecs_t maxExpectedDuration,
        std::set<String8>& physicalCameraIds, bool isStillCapture,
        bool isZslCapture, bool rotateAndCropAuto, const std::set<std::string>& cameraIdsWithZoom,
        const SurfaceMap& outputSurfaces) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> l(mInFlightLock);

    ssize_t res;
    res = mInFlightMap.add(frameNumber, InFlightRequest(numBuffers, resultExtras, hasInput,
            hasAppCallback, maxExpectedDuration, physicalCameraIds, isStillCapture, isZslCapture,
            rotateAndCropAuto, cameraIdsWithZoom, outputSurfaces));
    if (res < 0) return res;

    if (mInFlightMap.size() == 1) {
        // Hold a separate dedicated tracker lock to prevent race with disconnect and also
        // avoid a deadlock during reprocess requests.
        Mutex::Autolock l(mTrackerLock);
        if (mStatusTracker != nullptr) {
            mStatusTracker->markComponentActive(mInFlightStatusId);
        }
    }

    mExpectedInflightDuration += maxExpectedDuration;
    return OK;
}

void Camera3Device::onInflightEntryRemovedLocked(nsecs_t duration) {
    // Indicate idle inFlightMap to the status tracker
    if (mInFlightMap.size() == 0) {
        mRequestBufferSM.onInflightMapEmpty();
        // Hold a separate dedicated tracker lock to prevent race with disconnect and also
        // avoid a deadlock during reprocess requests.
        Mutex::Autolock l(mTrackerLock);
        if (mStatusTracker != nullptr) {
            mStatusTracker->markComponentIdle(mInFlightStatusId, Fence::NO_FENCE);
        }
    }
    mExpectedInflightDuration -= duration;
}

void Camera3Device::checkInflightMapLengthLocked() {
    // Sanity check - if we have too many in-flight frames with long total inflight duration,
    // something has likely gone wrong. This might still be legit only if application send in
    // a long burst of long exposure requests.
    if (mExpectedInflightDuration > kMinWarnInflightDuration) {
        if (!mIsConstrainedHighSpeedConfiguration && mInFlightMap.size() > kInFlightWarnLimit) {
            CLOGW("In-flight list too large: %zu, total inflight duration %" PRIu64,
                    mInFlightMap.size(), mExpectedInflightDuration);
        } else if (mIsConstrainedHighSpeedConfiguration && mInFlightMap.size() >
                kInFlightWarnLimitHighSpeed) {
            CLOGW("In-flight list too large for high speed configuration: %zu,"
                    "total inflight duration %" PRIu64,
                    mInFlightMap.size(), mExpectedInflightDuration);
        }
    }
}

void Camera3Device::onInflightMapFlushedLocked() {
    mExpectedInflightDuration = 0;
}

void Camera3Device::removeInFlightMapEntryLocked(int idx) {
    ATRACE_HFR_CALL();
    nsecs_t duration = mInFlightMap.valueAt(idx).maxExpectedDuration;
    mInFlightMap.removeItemsAt(idx, 1);

    onInflightEntryRemovedLocked(duration);
}


void Camera3Device::flushInflightRequests() {
    ATRACE_CALL();
    sp<NotificationListener> listener;
    {
        std::lock_guard<std::mutex> l(mOutputLock);
        listener = mListener.promote();
    }

    FlushInflightReqStates states {
        mId, mInFlightLock, mInFlightMap, mUseHalBufManager,
        listener, *this, *mInterface, *this};

    camera3::flushInflightRequests(states);
}

CameraMetadata Camera3Device::getLatestRequestLocked() {
    ALOGV("%s", __FUNCTION__);

    CameraMetadata retVal;

    if (mRequestThread != NULL) {
        retVal = mRequestThread->getLatestRequest();
    }

    return retVal;
}

void Camera3Device::monitorMetadata(TagMonitor::eventSource source,
        int64_t frameNumber, nsecs_t timestamp, const CameraMetadata& metadata,
        const std::unordered_map<std::string, CameraMetadata>& physicalMetadata) {

    mTagMonitor.monitorMetadata(source, frameNumber, timestamp, metadata,
            physicalMetadata);
}

/**
 * HalInterface inner class methods
 */

Camera3Device::HalInterface::HalInterface(
            sp<ICameraDeviceSession> &session,
            std::shared_ptr<RequestMetadataQueue> queue,
            bool useHalBufManager, bool supportOfflineProcessing) :
        mHidlSession(session),
        mRequestMetadataQueue(queue),
        mUseHalBufManager(useHalBufManager),
        mIsReconfigurationQuerySupported(true),
        mSupportOfflineProcessing(supportOfflineProcessing) {
    // Check with hardware service manager if we can downcast these interfaces
    // Somewhat expensive, so cache the results at startup
    auto castResult_3_6 = device::V3_6::ICameraDeviceSession::castFrom(mHidlSession);
    if (castResult_3_6.isOk()) {
        mHidlSession_3_6 = castResult_3_6;
    }
    auto castResult_3_5 = device::V3_5::ICameraDeviceSession::castFrom(mHidlSession);
    if (castResult_3_5.isOk()) {
        mHidlSession_3_5 = castResult_3_5;
    }
    auto castResult_3_4 = device::V3_4::ICameraDeviceSession::castFrom(mHidlSession);
    if (castResult_3_4.isOk()) {
        mHidlSession_3_4 = castResult_3_4;
    }
    auto castResult_3_3 = device::V3_3::ICameraDeviceSession::castFrom(mHidlSession);
    if (castResult_3_3.isOk()) {
        mHidlSession_3_3 = castResult_3_3;
    }
}

Camera3Device::HalInterface::HalInterface() :
        mUseHalBufManager(false),
        mSupportOfflineProcessing(false) {}

Camera3Device::HalInterface::HalInterface(const HalInterface& other) :
        mHidlSession(other.mHidlSession),
        mRequestMetadataQueue(other.mRequestMetadataQueue),
        mUseHalBufManager(other.mUseHalBufManager),
        mSupportOfflineProcessing(other.mSupportOfflineProcessing) {}

bool Camera3Device::HalInterface::valid() {
    return (mHidlSession != nullptr);
}

void Camera3Device::HalInterface::clear() {
    mHidlSession_3_6.clear();
    mHidlSession_3_5.clear();
    mHidlSession_3_4.clear();
    mHidlSession_3_3.clear();
    mHidlSession.clear();
}

status_t Camera3Device::HalInterface::constructDefaultRequestSettings(
        camera3_request_template_t templateId,
        /*out*/ camera_metadata_t **requestTemplate) {
    ATRACE_NAME("CameraHal::constructDefaultRequestSettings");
    if (!valid()) return INVALID_OPERATION;
    status_t res = OK;

    common::V1_0::Status status;

    auto requestCallback = [&status, &requestTemplate]
            (common::V1_0::Status s, const device::V3_2::CameraMetadata& request) {
            status = s;
            if (status == common::V1_0::Status::OK) {
                const camera_metadata *r =
                        reinterpret_cast<const camera_metadata_t*>(request.data());
                size_t expectedSize = request.size();
                int ret = validate_camera_metadata_structure(r, &expectedSize);
                if (ret == OK || ret == CAMERA_METADATA_VALIDATION_SHIFTED) {
                    *requestTemplate = clone_camera_metadata(r);
                    if (*requestTemplate == nullptr) {
                        ALOGE("%s: Unable to clone camera metadata received from HAL",
                                __FUNCTION__);
                        status = common::V1_0::Status::INTERNAL_ERROR;
                    }
                } else {
                    ALOGE("%s: Malformed camera metadata received from HAL", __FUNCTION__);
                    status = common::V1_0::Status::INTERNAL_ERROR;
                }
            }
        };
    hardware::Return<void> err;
    RequestTemplate id;
    switch (templateId) {
        case CAMERA3_TEMPLATE_PREVIEW:
            id = RequestTemplate::PREVIEW;
            break;
        case CAMERA3_TEMPLATE_STILL_CAPTURE:
            id = RequestTemplate::STILL_CAPTURE;
            break;
        case CAMERA3_TEMPLATE_VIDEO_RECORD:
            id = RequestTemplate::VIDEO_RECORD;
            break;
        case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
            id = RequestTemplate::VIDEO_SNAPSHOT;
            break;
        case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
            id = RequestTemplate::ZERO_SHUTTER_LAG;
            break;
        case CAMERA3_TEMPLATE_MANUAL:
            id = RequestTemplate::MANUAL;
            break;
        default:
            // Unknown template ID, or this HAL is too old to support it
            return BAD_VALUE;
    }
    err = mHidlSession->constructDefaultRequestSettings(id, requestCallback);

    if (!err.isOk()) {
        ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
        res = DEAD_OBJECT;
    } else {
        res = CameraProviderManager::mapToStatusT(status);
    }

    return res;
}

bool Camera3Device::HalInterface::isReconfigurationRequired(CameraMetadata& oldSessionParams,
        CameraMetadata& newSessionParams) {
    // We do reconfiguration by default;
    bool ret = true;
    if ((mHidlSession_3_5 != nullptr) && mIsReconfigurationQuerySupported) {
        android::hardware::hidl_vec<uint8_t> oldParams, newParams;
        camera_metadata_t* oldSessioMeta = const_cast<camera_metadata_t*>(
                oldSessionParams.getAndLock());
        camera_metadata_t* newSessioMeta = const_cast<camera_metadata_t*>(
                newSessionParams.getAndLock());
        oldParams.setToExternal(reinterpret_cast<uint8_t*>(oldSessioMeta),
                get_camera_metadata_size(oldSessioMeta));
        newParams.setToExternal(reinterpret_cast<uint8_t*>(newSessioMeta),
                get_camera_metadata_size(newSessioMeta));
        hardware::camera::common::V1_0::Status callStatus;
        bool required;
        auto hidlCb = [&callStatus, &required] (hardware::camera::common::V1_0::Status s,
                bool requiredFlag) {
            callStatus = s;
            required = requiredFlag;
        };
        auto err = mHidlSession_3_5->isReconfigurationRequired(oldParams, newParams, hidlCb);
        oldSessionParams.unlock(oldSessioMeta);
        newSessionParams.unlock(newSessioMeta);
        if (err.isOk()) {
            switch (callStatus) {
                case hardware::camera::common::V1_0::Status::OK:
                    ret = required;
                    break;
                case hardware::camera::common::V1_0::Status::METHOD_NOT_SUPPORTED:
                    mIsReconfigurationQuerySupported = false;
                    ret = true;
                    break;
                default:
                    ALOGV("%s: Reconfiguration query failed: %d", __FUNCTION__, callStatus);
                    ret = true;
            }
        } else {
            ALOGE("%s: Unexpected binder error: %s", __FUNCTION__, err.description().c_str());
            ret = true;
        }
    }

    return ret;
}

status_t Camera3Device::HalInterface::configureStreams(const camera_metadata_t *sessionParams,
        camera3_stream_configuration *config, const std::vector<uint32_t>& bufferSizes) {
    ATRACE_NAME("CameraHal::configureStreams");
    if (!valid()) return INVALID_OPERATION;
    status_t res = OK;

    // Convert stream config to HIDL
    std::set<int> activeStreams;
    device::V3_2::StreamConfiguration requestedConfiguration3_2;
    device::V3_4::StreamConfiguration requestedConfiguration3_4;
    requestedConfiguration3_2.streams.resize(config->num_streams);
    requestedConfiguration3_4.streams.resize(config->num_streams);
    for (size_t i = 0; i < config->num_streams; i++) {
        device::V3_2::Stream &dst3_2 = requestedConfiguration3_2.streams[i];
        device::V3_4::Stream &dst3_4 = requestedConfiguration3_4.streams[i];
        camera3_stream_t *src = config->streams[i];

        Camera3Stream* cam3stream = Camera3Stream::cast(src);
        cam3stream->setBufferFreedListener(this);
        int streamId = cam3stream->getId();
        StreamType streamType;
        switch (src->stream_type) {
            case CAMERA3_STREAM_OUTPUT:
                streamType = StreamType::OUTPUT;
                break;
            case CAMERA3_STREAM_INPUT:
                streamType = StreamType::INPUT;
                break;
            default:
                ALOGE("%s: Stream %d: Unsupported stream type %d",
                        __FUNCTION__, streamId, config->streams[i]->stream_type);
                return BAD_VALUE;
        }
        dst3_2.id = streamId;
        dst3_2.streamType = streamType;
        dst3_2.width = src->width;
        dst3_2.height = src->height;
        dst3_2.usage = mapToConsumerUsage(cam3stream->getUsage());
        dst3_2.rotation = mapToStreamRotation((camera3_stream_rotation_t) src->rotation);
        // For HidlSession version 3.5 or newer, the format and dataSpace sent
        // to HAL are original, not the overriden ones.
        if (mHidlSession_3_5 != nullptr) {
            dst3_2.format = mapToPixelFormat(cam3stream->isFormatOverridden() ?
                    cam3stream->getOriginalFormat() : src->format);
            dst3_2.dataSpace = mapToHidlDataspace(cam3stream->isDataSpaceOverridden() ?
                    cam3stream->getOriginalDataSpace() : src->data_space);
        } else {
            dst3_2.format = mapToPixelFormat(src->format);
            dst3_2.dataSpace = mapToHidlDataspace(src->data_space);
        }
        dst3_4.v3_2 = dst3_2;
        dst3_4.bufferSize = bufferSizes[i];
        if (src->physical_camera_id != nullptr) {
            dst3_4.physicalCameraId = src->physical_camera_id;
        }

        activeStreams.insert(streamId);
        // Create Buffer ID map if necessary
        mBufferRecords.tryCreateBufferCache(streamId);
    }
    // remove BufferIdMap for deleted streams
    mBufferRecords.removeInactiveBufferCaches(activeStreams);

    StreamConfigurationMode operationMode;
    res = mapToStreamConfigurationMode(
            (camera3_stream_configuration_mode_t) config->operation_mode,
            /*out*/ &operationMode);
    if (res != OK) {
        return res;
    }
    requestedConfiguration3_2.operationMode = operationMode;
    requestedConfiguration3_4.operationMode = operationMode;
    requestedConfiguration3_4.sessionParams.setToExternal(
            reinterpret_cast<uint8_t*>(const_cast<camera_metadata_t*>(sessionParams)),
            get_camera_metadata_size(sessionParams));

    // Invoke configureStreams
    device::V3_3::HalStreamConfiguration finalConfiguration;
    device::V3_4::HalStreamConfiguration finalConfiguration3_4;
    device::V3_6::HalStreamConfiguration finalConfiguration3_6;
    common::V1_0::Status status;

    auto configStream34Cb = [&status, &finalConfiguration3_4]
            (common::V1_0::Status s, const device::V3_4::HalStreamConfiguration& halConfiguration) {
                finalConfiguration3_4 = halConfiguration;
                status = s;
            };

    auto configStream36Cb = [&status, &finalConfiguration3_6]
            (common::V1_0::Status s, const device::V3_6::HalStreamConfiguration& halConfiguration) {
                finalConfiguration3_6 = halConfiguration;
                status = s;
            };

    auto postprocConfigStream34 = [&finalConfiguration, &finalConfiguration3_4]
            (hardware::Return<void>& err) -> status_t {
                if (!err.isOk()) {
                    ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
                    return DEAD_OBJECT;
                }
                finalConfiguration.streams.resize(finalConfiguration3_4.streams.size());
                for (size_t i = 0; i < finalConfiguration3_4.streams.size(); i++) {
                    finalConfiguration.streams[i] = finalConfiguration3_4.streams[i].v3_3;
                }
                return OK;
            };

    auto postprocConfigStream36 = [&finalConfiguration, &finalConfiguration3_6]
            (hardware::Return<void>& err) -> status_t {
                if (!err.isOk()) {
                    ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
                    return DEAD_OBJECT;
                }
                finalConfiguration.streams.resize(finalConfiguration3_6.streams.size());
                for (size_t i = 0; i < finalConfiguration3_6.streams.size(); i++) {
                    finalConfiguration.streams[i] = finalConfiguration3_6.streams[i].v3_4.v3_3;
                }
                return OK;
            };

    // See which version of HAL we have
    if (mHidlSession_3_6 != nullptr) {
        ALOGV("%s: v3.6 device found", __FUNCTION__);
        device::V3_5::StreamConfiguration requestedConfiguration3_5;
        requestedConfiguration3_5.v3_4 = requestedConfiguration3_4;
        requestedConfiguration3_5.streamConfigCounter = mNextStreamConfigCounter++;
        auto err = mHidlSession_3_6->configureStreams_3_6(
                requestedConfiguration3_5, configStream36Cb);
        res = postprocConfigStream36(err);
        if (res != OK) {
            return res;
        }
    } else if (mHidlSession_3_5 != nullptr) {
        ALOGV("%s: v3.5 device found", __FUNCTION__);
        device::V3_5::StreamConfiguration requestedConfiguration3_5;
        requestedConfiguration3_5.v3_4 = requestedConfiguration3_4;
        requestedConfiguration3_5.streamConfigCounter = mNextStreamConfigCounter++;
        auto err = mHidlSession_3_5->configureStreams_3_5(
                requestedConfiguration3_5, configStream34Cb);
        res = postprocConfigStream34(err);
        if (res != OK) {
            return res;
        }
    } else if (mHidlSession_3_4 != nullptr) {
        // We do; use v3.4 for the call
        ALOGV("%s: v3.4 device found", __FUNCTION__);
        auto err = mHidlSession_3_4->configureStreams_3_4(
                requestedConfiguration3_4, configStream34Cb);
        res = postprocConfigStream34(err);
        if (res != OK) {
            return res;
        }
    } else if (mHidlSession_3_3 != nullptr) {
        // We do; use v3.3 for the call
        ALOGV("%s: v3.3 device found", __FUNCTION__);
        auto err = mHidlSession_3_3->configureStreams_3_3(requestedConfiguration3_2,
            [&status, &finalConfiguration]
            (common::V1_0::Status s, const device::V3_3::HalStreamConfiguration& halConfiguration) {
                finalConfiguration = halConfiguration;
                status = s;
            });
        if (!err.isOk()) {
            ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
            return DEAD_OBJECT;
        }
    } else {
        // We don't; use v3.2 call and construct a v3.3 HalStreamConfiguration
        ALOGV("%s: v3.2 device found", __FUNCTION__);
        HalStreamConfiguration finalConfiguration_3_2;
        auto err = mHidlSession->configureStreams(requestedConfiguration3_2,
                [&status, &finalConfiguration_3_2]
                (common::V1_0::Status s, const HalStreamConfiguration& halConfiguration) {
                    finalConfiguration_3_2 = halConfiguration;
                    status = s;
                });
        if (!err.isOk()) {
            ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
            return DEAD_OBJECT;
        }
        finalConfiguration.streams.resize(finalConfiguration_3_2.streams.size());
        for (size_t i = 0; i < finalConfiguration_3_2.streams.size(); i++) {
            finalConfiguration.streams[i].v3_2 = finalConfiguration_3_2.streams[i];
            finalConfiguration.streams[i].overrideDataSpace =
                    requestedConfiguration3_2.streams[i].dataSpace;
        }
    }

    if (status != common::V1_0::Status::OK ) {
        return CameraProviderManager::mapToStatusT(status);
    }

    // And convert output stream configuration from HIDL

    for (size_t i = 0; i < config->num_streams; i++) {
        camera3_stream_t *dst = config->streams[i];
        int streamId = Camera3Stream::cast(dst)->getId();

        // Start scan at i, with the assumption that the stream order matches
        size_t realIdx = i;
        bool found = false;
        size_t halStreamCount = finalConfiguration.streams.size();
        for (size_t idx = 0; idx < halStreamCount; idx++) {
            if (finalConfiguration.streams[realIdx].v3_2.id == streamId) {
                found = true;
                break;
            }
            realIdx = (realIdx >= halStreamCount - 1) ? 0 : realIdx + 1;
        }
        if (!found) {
            ALOGE("%s: Stream %d not found in stream configuration response from HAL",
                    __FUNCTION__, streamId);
            return INVALID_OPERATION;
        }
        device::V3_3::HalStream &src = finalConfiguration.streams[realIdx];
        device::V3_6::HalStream &src_36 = finalConfiguration3_6.streams[realIdx];

        Camera3Stream* dstStream = Camera3Stream::cast(dst);
        int overrideFormat = mapToFrameworkFormat(src.v3_2.overrideFormat);
        android_dataspace overrideDataSpace = mapToFrameworkDataspace(src.overrideDataSpace);

        if (mHidlSession_3_6 != nullptr) {
            dstStream->setOfflineProcessingSupport(src_36.supportOffline);
        }

        if (dstStream->getOriginalFormat() != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            dstStream->setFormatOverride(false);
            dstStream->setDataSpaceOverride(false);
            if (dst->format != overrideFormat) {
                ALOGE("%s: Stream %d: Format override not allowed for format 0x%x", __FUNCTION__,
                        streamId, dst->format);
            }
            if (dst->data_space != overrideDataSpace) {
                ALOGE("%s: Stream %d: DataSpace override not allowed for format 0x%x", __FUNCTION__,
                        streamId, dst->format);
            }
        } else {
            bool needFormatOverride =
                    requestedConfiguration3_2.streams[i].format != src.v3_2.overrideFormat;
            bool needDataspaceOverride =
                    requestedConfiguration3_2.streams[i].dataSpace != src.overrideDataSpace;
            // Override allowed with IMPLEMENTATION_DEFINED
            dstStream->setFormatOverride(needFormatOverride);
            dstStream->setDataSpaceOverride(needDataspaceOverride);
            dst->format = overrideFormat;
            dst->data_space = overrideDataSpace;
        }

        if (dst->stream_type == CAMERA3_STREAM_INPUT) {
            if (src.v3_2.producerUsage != 0) {
                ALOGE("%s: Stream %d: INPUT streams must have 0 for producer usage",
                        __FUNCTION__, streamId);
                return INVALID_OPERATION;
            }
            dstStream->setUsage(
                    mapConsumerToFrameworkUsage(src.v3_2.consumerUsage));
        } else {
            // OUTPUT
            if (src.v3_2.consumerUsage != 0) {
                ALOGE("%s: Stream %d: OUTPUT streams must have 0 for consumer usage",
                        __FUNCTION__, streamId);
                return INVALID_OPERATION;
            }
            dstStream->setUsage(
                    mapProducerToFrameworkUsage(src.v3_2.producerUsage));
        }
        dst->max_buffers = src.v3_2.maxBuffers;
    }

    return res;
}

status_t Camera3Device::HalInterface::wrapAsHidlRequest(camera3_capture_request_t* request,
        /*out*/device::V3_2::CaptureRequest* captureRequest,
        /*out*/std::vector<native_handle_t*>* handlesCreated,
        /*out*/std::vector<std::pair<int32_t, int32_t>>* inflightBuffers) {
    ATRACE_CALL();
    if (captureRequest == nullptr || handlesCreated == nullptr || inflightBuffers == nullptr) {
        ALOGE("%s: captureRequest (%p), handlesCreated (%p), and inflightBuffers(%p) "
                "must not be null", __FUNCTION__, captureRequest, handlesCreated, inflightBuffers);
        return BAD_VALUE;
    }

    captureRequest->frameNumber = request->frame_number;

    captureRequest->fmqSettingsSize = 0;

    {
        if (request->input_buffer != nullptr) {
            int32_t streamId = Camera3Stream::cast(request->input_buffer->stream)->getId();
            buffer_handle_t buf = *(request->input_buffer->buffer);
            auto pair = getBufferId(buf, streamId);
            bool isNewBuffer = pair.first;
            uint64_t bufferId = pair.second;
            captureRequest->inputBuffer.streamId = streamId;
            captureRequest->inputBuffer.bufferId = bufferId;
            captureRequest->inputBuffer.buffer = (isNewBuffer) ? buf : nullptr;
            captureRequest->inputBuffer.status = BufferStatus::OK;
            native_handle_t *acquireFence = nullptr;
            if (request->input_buffer->acquire_fence != -1) {
                acquireFence = native_handle_create(1,0);
                acquireFence->data[0] = request->input_buffer->acquire_fence;
                handlesCreated->push_back(acquireFence);
            }
            captureRequest->inputBuffer.acquireFence = acquireFence;
            captureRequest->inputBuffer.releaseFence = nullptr;

            mBufferRecords.pushInflightBuffer(captureRequest->frameNumber, streamId,
                    request->input_buffer->buffer);
            inflightBuffers->push_back(std::make_pair(captureRequest->frameNumber, streamId));
        } else {
            captureRequest->inputBuffer.streamId = -1;
            captureRequest->inputBuffer.bufferId = BUFFER_ID_NO_BUFFER;
        }

        captureRequest->outputBuffers.resize(request->num_output_buffers);
        for (size_t i = 0; i < request->num_output_buffers; i++) {
            const camera3_stream_buffer_t *src = request->output_buffers + i;
            StreamBuffer &dst = captureRequest->outputBuffers[i];
            int32_t streamId = Camera3Stream::cast(src->stream)->getId();
            if (src->buffer != nullptr) {
                buffer_handle_t buf = *(src->buffer);
                auto pair = getBufferId(buf, streamId);
                bool isNewBuffer = pair.first;
                dst.bufferId = pair.second;
                dst.buffer = isNewBuffer ? buf : nullptr;
                native_handle_t *acquireFence = nullptr;
                if (src->acquire_fence != -1) {
                    acquireFence = native_handle_create(1,0);
                    acquireFence->data[0] = src->acquire_fence;
                    handlesCreated->push_back(acquireFence);
                }
                dst.acquireFence = acquireFence;
            } else if (mUseHalBufManager) {
                // HAL buffer management path
                dst.bufferId = BUFFER_ID_NO_BUFFER;
                dst.buffer = nullptr;
                dst.acquireFence = nullptr;
            } else {
                ALOGE("%s: cannot send a null buffer in capture request!", __FUNCTION__);
                return BAD_VALUE;
            }
            dst.streamId = streamId;
            dst.status = BufferStatus::OK;
            dst.releaseFence = nullptr;

            // Output buffers are empty when using HAL buffer manager
            if (!mUseHalBufManager) {
                mBufferRecords.pushInflightBuffer(
                        captureRequest->frameNumber, streamId, src->buffer);
                inflightBuffers->push_back(std::make_pair(captureRequest->frameNumber, streamId));
            }
        }
    }
    return OK;
}

void Camera3Device::HalInterface::cleanupNativeHandles(
        std::vector<native_handle_t*> *handles, bool closeFd) {
    if (handles == nullptr) {
        return;
    }
    if (closeFd) {
        for (auto& handle : *handles) {
            native_handle_close(handle);
        }
    }
    for (auto& handle : *handles) {
        native_handle_delete(handle);
    }
    handles->clear();
    return;
}

status_t Camera3Device::HalInterface::processBatchCaptureRequests(
        std::vector<camera3_capture_request_t*>& requests,/*out*/uint32_t* numRequestProcessed) {
    ATRACE_NAME("CameraHal::processBatchCaptureRequests");
    if (!valid()) return INVALID_OPERATION;

    sp<device::V3_4::ICameraDeviceSession> hidlSession_3_4;
    auto castResult_3_4 = device::V3_4::ICameraDeviceSession::castFrom(mHidlSession);
    if (castResult_3_4.isOk()) {
        hidlSession_3_4 = castResult_3_4;
    }

    hardware::hidl_vec<device::V3_2::CaptureRequest> captureRequests;
    hardware::hidl_vec<device::V3_4::CaptureRequest> captureRequests_3_4;
    size_t batchSize = requests.size();
    if (hidlSession_3_4 != nullptr) {
        captureRequests_3_4.resize(batchSize);
    } else {
        captureRequests.resize(batchSize);
    }
    std::vector<native_handle_t*> handlesCreated;
    std::vector<std::pair<int32_t, int32_t>> inflightBuffers;

    status_t res = OK;
    for (size_t i = 0; i < batchSize; i++) {
        if (hidlSession_3_4 != nullptr) {
            res = wrapAsHidlRequest(requests[i], /*out*/&captureRequests_3_4[i].v3_2,
                    /*out*/&handlesCreated, /*out*/&inflightBuffers);
        } else {
            res = wrapAsHidlRequest(requests[i], /*out*/&captureRequests[i],
                    /*out*/&handlesCreated, /*out*/&inflightBuffers);
        }
        if (res != OK) {
            mBufferRecords.popInflightBuffers(inflightBuffers);
            cleanupNativeHandles(&handlesCreated);
            return res;
        }
    }

    std::vector<device::V3_2::BufferCache> cachesToRemove;
    {
        std::lock_guard<std::mutex> lock(mFreedBuffersLock);
        for (auto& pair : mFreedBuffers) {
            // The stream might have been removed since onBufferFreed
            if (mBufferRecords.isStreamCached(pair.first)) {
                cachesToRemove.push_back({pair.first, pair.second});
            }
        }
        mFreedBuffers.clear();
    }

    common::V1_0::Status status = common::V1_0::Status::INTERNAL_ERROR;
    *numRequestProcessed = 0;

    // Write metadata to FMQ.
    for (size_t i = 0; i < batchSize; i++) {
        camera3_capture_request_t* request = requests[i];
        device::V3_2::CaptureRequest* captureRequest;
        if (hidlSession_3_4 != nullptr) {
            captureRequest = &captureRequests_3_4[i].v3_2;
        } else {
            captureRequest = &captureRequests[i];
        }

        if (request->settings != nullptr) {
            size_t settingsSize = get_camera_metadata_size(request->settings);
            if (mRequestMetadataQueue != nullptr && mRequestMetadataQueue->write(
                    reinterpret_cast<const uint8_t*>(request->settings), settingsSize)) {
                captureRequest->settings.resize(0);
                captureRequest->fmqSettingsSize = settingsSize;
            } else {
                if (mRequestMetadataQueue != nullptr) {
                    ALOGW("%s: couldn't utilize fmq, fallback to hwbinder", __FUNCTION__);
                }
                captureRequest->settings.setToExternal(
                        reinterpret_cast<uint8_t*>(const_cast<camera_metadata_t*>(request->settings)),
                        get_camera_metadata_size(request->settings));
                captureRequest->fmqSettingsSize = 0u;
            }
        } else {
            // A null request settings maps to a size-0 CameraMetadata
            captureRequest->settings.resize(0);
            captureRequest->fmqSettingsSize = 0u;
        }

        if (hidlSession_3_4 != nullptr) {
            captureRequests_3_4[i].physicalCameraSettings.resize(request->num_physcam_settings);
            for (size_t j = 0; j < request->num_physcam_settings; j++) {
                if (request->physcam_settings != nullptr) {
                    size_t settingsSize = get_camera_metadata_size(request->physcam_settings[j]);
                    if (mRequestMetadataQueue != nullptr && mRequestMetadataQueue->write(
                                reinterpret_cast<const uint8_t*>(request->physcam_settings[j]),
                                settingsSize)) {
                        captureRequests_3_4[i].physicalCameraSettings[j].settings.resize(0);
                        captureRequests_3_4[i].physicalCameraSettings[j].fmqSettingsSize =
                            settingsSize;
                    } else {
                        if (mRequestMetadataQueue != nullptr) {
                            ALOGW("%s: couldn't utilize fmq, fallback to hwbinder", __FUNCTION__);
                        }
                        captureRequests_3_4[i].physicalCameraSettings[j].settings.setToExternal(
                                reinterpret_cast<uint8_t*>(const_cast<camera_metadata_t*>(
                                        request->physcam_settings[j])),
                                get_camera_metadata_size(request->physcam_settings[j]));
                        captureRequests_3_4[i].physicalCameraSettings[j].fmqSettingsSize = 0u;
                    }
                } else {
                    captureRequests_3_4[i].physicalCameraSettings[j].fmqSettingsSize = 0u;
                    captureRequests_3_4[i].physicalCameraSettings[j].settings.resize(0);
                }
                captureRequests_3_4[i].physicalCameraSettings[j].physicalCameraId =
                    request->physcam_id[j];
            }
        }
    }

    hardware::details::return_status err;
    auto resultCallback =
        [&status, &numRequestProcessed] (auto s, uint32_t n) {
                status = s;
                *numRequestProcessed = n;
        };
    if (hidlSession_3_4 != nullptr) {
        err = hidlSession_3_4->processCaptureRequest_3_4(captureRequests_3_4, cachesToRemove,
                                                         resultCallback);
    } else {
        err = mHidlSession->processCaptureRequest(captureRequests, cachesToRemove,
                                                  resultCallback);
    }
    if (!err.isOk()) {
        ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
        status = common::V1_0::Status::CAMERA_DISCONNECTED;
    }

    if (status == common::V1_0::Status::OK && *numRequestProcessed != batchSize) {
        ALOGE("%s: processCaptureRequest returns OK but processed %d/%zu requests",
                __FUNCTION__, *numRequestProcessed, batchSize);
        status = common::V1_0::Status::INTERNAL_ERROR;
    }

    res = CameraProviderManager::mapToStatusT(status);
    if (res == OK) {
        if (mHidlSession->isRemote()) {
            // Only close acquire fence FDs when the HIDL transaction succeeds (so the FDs have been
            // sent to camera HAL processes)
            cleanupNativeHandles(&handlesCreated, /*closeFd*/true);
        } else {
            // In passthrough mode the FDs are now owned by HAL
            cleanupNativeHandles(&handlesCreated);
        }
    } else {
        mBufferRecords.popInflightBuffers(inflightBuffers);
        cleanupNativeHandles(&handlesCreated);
    }
    return res;
}

status_t Camera3Device::HalInterface::flush() {
    ATRACE_NAME("CameraHal::flush");
    if (!valid()) return INVALID_OPERATION;
    status_t res = OK;

    auto err = mHidlSession->flush();
    if (!err.isOk()) {
        ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
        res = DEAD_OBJECT;
    } else {
        res = CameraProviderManager::mapToStatusT(err);
    }

    return res;
}

status_t Camera3Device::HalInterface::dump(int /*fd*/) {
    ATRACE_NAME("CameraHal::dump");
    if (!valid()) return INVALID_OPERATION;

    // Handled by CameraProviderManager::dump

    return OK;
}

status_t Camera3Device::HalInterface::close() {
    ATRACE_NAME("CameraHal::close()");
    if (!valid()) return INVALID_OPERATION;
    status_t res = OK;

    auto err = mHidlSession->close();
    // Interface will be dead shortly anyway, so don't log errors
    if (!err.isOk()) {
        res = DEAD_OBJECT;
    }

    return res;
}

void Camera3Device::HalInterface::signalPipelineDrain(const std::vector<int>& streamIds) {
    ATRACE_NAME("CameraHal::signalPipelineDrain");
    if (!valid() || mHidlSession_3_5 == nullptr) {
        ALOGE("%s called on invalid camera!", __FUNCTION__);
        return;
    }

    auto err = mHidlSession_3_5->signalStreamFlush(streamIds, mNextStreamConfigCounter - 1);
    if (!err.isOk()) {
        ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
        return;
    }
}

status_t Camera3Device::HalInterface::switchToOffline(
        const std::vector<int32_t>& streamsToKeep,
        /*out*/hardware::camera::device::V3_6::CameraOfflineSessionInfo* offlineSessionInfo,
        /*out*/sp<hardware::camera::device::V3_6::ICameraOfflineSession>* offlineSession,
        /*out*/camera3::BufferRecords* bufferRecords) {
    ATRACE_NAME("CameraHal::switchToOffline");
    if (!valid() || mHidlSession_3_6 == nullptr) {
        ALOGE("%s called on invalid camera!", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (offlineSessionInfo == nullptr || offlineSession == nullptr || bufferRecords == nullptr) {
        ALOGE("%s: output arguments must not be null!", __FUNCTION__);
        return INVALID_OPERATION;
    }

    common::V1_0::Status status = common::V1_0::Status::INTERNAL_ERROR;
    auto resultCallback =
        [&status, &offlineSessionInfo, &offlineSession] (auto s, auto info, auto session) {
                status = s;
                *offlineSessionInfo = info;
                *offlineSession = session;
        };
    auto err = mHidlSession_3_6->switchToOffline(streamsToKeep, resultCallback);

    if (!err.isOk()) {
        ALOGE("%s: Transaction error: %s", __FUNCTION__, err.description().c_str());
        return DEAD_OBJECT;
    }

    status_t ret = CameraProviderManager::mapToStatusT(status);
    if (ret != OK) {
        return ret;
    }

    // TODO: assert no ongoing requestBuffer/returnBuffer call here
    // TODO: update RequestBufferStateMachine to block requestBuffer/returnBuffer once HAL
    //       returns from switchToOffline.


    // Validate buffer caches
    std::vector<int32_t> streams;
    streams.reserve(offlineSessionInfo->offlineStreams.size());
    for (auto offlineStream : offlineSessionInfo->offlineStreams) {
        int32_t id = offlineStream.id;
        streams.push_back(id);
        // Verify buffer caches
        std::vector<uint64_t> bufIds(offlineStream.circulatingBufferIds.begin(),
                offlineStream.circulatingBufferIds.end());
        if (!verifyBufferIds(id, bufIds)) {
            ALOGE("%s: stream ID %d buffer cache records mismatch!", __FUNCTION__, id);
            return UNKNOWN_ERROR;
        }
    }

    // Move buffer records
    bufferRecords->takeBufferCaches(mBufferRecords, streams);
    bufferRecords->takeInflightBufferMap(mBufferRecords);
    bufferRecords->takeRequestedBufferMap(mBufferRecords);
    return ret;
}

void Camera3Device::HalInterface::getInflightBufferKeys(
        std::vector<std::pair<int32_t, int32_t>>* out) {
    mBufferRecords.getInflightBufferKeys(out);
    return;
}

void Camera3Device::HalInterface::getInflightRequestBufferKeys(
        std::vector<uint64_t>* out) {
    mBufferRecords.getInflightRequestBufferKeys(out);
    return;
}

bool Camera3Device::HalInterface::verifyBufferIds(
        int32_t streamId, std::vector<uint64_t>& bufIds) {
    return mBufferRecords.verifyBufferIds(streamId, bufIds);
}

status_t Camera3Device::HalInterface::popInflightBuffer(
        int32_t frameNumber, int32_t streamId,
        /*out*/ buffer_handle_t **buffer) {
    return mBufferRecords.popInflightBuffer(frameNumber, streamId, buffer);
}

status_t Camera3Device::HalInterface::pushInflightRequestBuffer(
        uint64_t bufferId, buffer_handle_t* buf, int32_t streamId) {
    return mBufferRecords.pushInflightRequestBuffer(bufferId, buf, streamId);
}

// Find and pop a buffer_handle_t based on bufferId
status_t Camera3Device::HalInterface::popInflightRequestBuffer(
        uint64_t bufferId,
        /*out*/ buffer_handle_t** buffer,
        /*optional out*/ int32_t* streamId) {
    return mBufferRecords.popInflightRequestBuffer(bufferId, buffer, streamId);
}

std::pair<bool, uint64_t> Camera3Device::HalInterface::getBufferId(
        const buffer_handle_t& buf, int streamId) {
    return mBufferRecords.getBufferId(buf, streamId);
}

void Camera3Device::HalInterface::onBufferFreed(
        int streamId, const native_handle_t* handle) {
    uint32_t bufferId = mBufferRecords.removeOneBufferCache(streamId, handle);
    std::lock_guard<std::mutex> lock(mFreedBuffersLock);
    if (bufferId != BUFFER_ID_NO_BUFFER) {
        mFreedBuffers.push_back(std::make_pair(streamId, bufferId));
    }
}

void Camera3Device::HalInterface::onStreamReConfigured(int streamId) {
    std::vector<uint64_t> bufIds = mBufferRecords.clearBufferCaches(streamId);
    std::lock_guard<std::mutex> lock(mFreedBuffersLock);
    for (auto bufferId : bufIds) {
        mFreedBuffers.push_back(std::make_pair(streamId, bufferId));
    }
}

/**
 * RequestThread inner class methods
 */

Camera3Device::RequestThread::RequestThread(wp<Camera3Device> parent,
        sp<StatusTracker> statusTracker,
        sp<HalInterface> interface, const Vector<int32_t>& sessionParamKeys,
        bool useHalBufManager) :
        Thread(/*canCallJava*/false),
        mParent(parent),
        mStatusTracker(statusTracker),
        mInterface(interface),
        mListener(nullptr),
        mId(getId(parent)),
        mReconfigured(false),
        mDoPause(false),
        mPaused(true),
        mNotifyPipelineDrain(false),
        mFrameNumber(0),
        mLatestRequestId(NAME_NOT_FOUND),
        mCurrentAfTriggerId(0),
        mCurrentPreCaptureTriggerId(0),
        mRotateAndCropOverride(ANDROID_SCALER_ROTATE_AND_CROP_NONE),
        mRepeatingLastFrameNumber(
            hardware::camera2::ICameraDeviceUser::NO_IN_FLIGHT_REPEATING_FRAMES),
        mPrepareVideoStream(false),
        mConstrainedMode(false),
        mRequestLatency(kRequestLatencyBinSize),
        mSessionParamKeys(sessionParamKeys),
        mLatestSessionParams(sessionParamKeys.size()),
        mUseHalBufManager(useHalBufManager) {
    mStatusId = statusTracker->addComponent();
}

Camera3Device::RequestThread::~RequestThread() {}

void Camera3Device::RequestThread::setNotificationListener(
        wp<NotificationListener> listener) {
    ATRACE_CALL();
    Mutex::Autolock l(mRequestLock);
    mListener = listener;
}

void Camera3Device::RequestThread::configurationComplete(bool isConstrainedHighSpeed,
        const CameraMetadata& sessionParams) {
    ATRACE_CALL();
    Mutex::Autolock l(mRequestLock);
    mReconfigured = true;
    mLatestSessionParams = sessionParams;
    // Prepare video stream for high speed recording.
    mPrepareVideoStream = isConstrainedHighSpeed;
    mConstrainedMode = isConstrainedHighSpeed;
}

status_t Camera3Device::RequestThread::queueRequestList(
        List<sp<CaptureRequest> > &requests,
        /*out*/
        int64_t *lastFrameNumber) {
    ATRACE_CALL();
    Mutex::Autolock l(mRequestLock);
    for (List<sp<CaptureRequest> >::iterator it = requests.begin(); it != requests.end();
            ++it) {
        mRequestQueue.push_back(*it);
    }

    if (lastFrameNumber != NULL) {
        *lastFrameNumber = mFrameNumber + mRequestQueue.size() - 1;
        ALOGV("%s: requestId %d, mFrameNumber %" PRId32 ", lastFrameNumber %" PRId64 ".",
              __FUNCTION__, (*(requests.begin()))->mResultExtras.requestId, mFrameNumber,
              *lastFrameNumber);
    }

    unpauseForNewRequests();

    return OK;
}


status_t Camera3Device::RequestThread::queueTrigger(
        RequestTrigger trigger[],
        size_t count) {
    ATRACE_CALL();
    Mutex::Autolock l(mTriggerMutex);
    status_t ret;

    for (size_t i = 0; i < count; ++i) {
        ret = queueTriggerLocked(trigger[i]);

        if (ret != OK) {
            return ret;
        }
    }

    return OK;
}

const String8& Camera3Device::RequestThread::getId(const wp<Camera3Device> &device) {
    static String8 deadId("<DeadDevice>");
    sp<Camera3Device> d = device.promote();
    if (d != nullptr) return d->mId;
    return deadId;
}

status_t Camera3Device::RequestThread::queueTriggerLocked(
        RequestTrigger trigger) {

    uint32_t tag = trigger.metadataTag;
    ssize_t index = mTriggerMap.indexOfKey(tag);

    switch (trigger.getTagType()) {
        case TYPE_BYTE:
        // fall-through
        case TYPE_INT32:
            break;
        default:
            ALOGE("%s: Type not supported: 0x%x", __FUNCTION__,
                    trigger.getTagType());
            return INVALID_OPERATION;
    }

    /**
     * Collect only the latest trigger, since we only have 1 field
     * in the request settings per trigger tag, and can't send more than 1
     * trigger per request.
     */
    if (index != NAME_NOT_FOUND) {
        mTriggerMap.editValueAt(index) = trigger;
    } else {
        mTriggerMap.add(tag, trigger);
    }

    return OK;
}

status_t Camera3Device::RequestThread::setRepeatingRequests(
        const RequestList &requests,
        /*out*/
        int64_t *lastFrameNumber) {
    ATRACE_CALL();
    Mutex::Autolock l(mRequestLock);
    if (lastFrameNumber != NULL) {
        *lastFrameNumber = mRepeatingLastFrameNumber;
    }
    mRepeatingRequests.clear();
    mRepeatingRequests.insert(mRepeatingRequests.begin(),
            requests.begin(), requests.end());

    unpauseForNewRequests();

    mRepeatingLastFrameNumber = hardware::camera2::ICameraDeviceUser::NO_IN_FLIGHT_REPEATING_FRAMES;
    return OK;
}

bool Camera3Device::RequestThread::isRepeatingRequestLocked(const sp<CaptureRequest>& requestIn) {
    if (mRepeatingRequests.empty()) {
        return false;
    }
    int32_t requestId = requestIn->mResultExtras.requestId;
    const RequestList &repeatRequests = mRepeatingRequests;
    // All repeating requests are guaranteed to have same id so only check first quest
    const sp<CaptureRequest> firstRequest = *repeatRequests.begin();
    return (firstRequest->mResultExtras.requestId == requestId);
}

status_t Camera3Device::RequestThread::clearRepeatingRequests(/*out*/int64_t *lastFrameNumber) {
    ATRACE_CALL();
    Mutex::Autolock l(mRequestLock);
    return clearRepeatingRequestsLocked(lastFrameNumber);

}

status_t Camera3Device::RequestThread::clearRepeatingRequestsLocked(/*out*/int64_t *lastFrameNumber) {
    mRepeatingRequests.clear();
    if (lastFrameNumber != NULL) {
        *lastFrameNumber = mRepeatingLastFrameNumber;
    }
    mRepeatingLastFrameNumber = hardware::camera2::ICameraDeviceUser::NO_IN_FLIGHT_REPEATING_FRAMES;
    return OK;
}

status_t Camera3Device::RequestThread::clear(
        /*out*/int64_t *lastFrameNumber) {
    ATRACE_CALL();
    Mutex::Autolock l(mRequestLock);
    ALOGV("RequestThread::%s:", __FUNCTION__);

    mRepeatingRequests.clear();

    // Send errors for all requests pending in the request queue, including
    // pending repeating requests
    sp<NotificationListener> listener = mListener.promote();
    if (listener != NULL) {
        for (RequestList::iterator it = mRequestQueue.begin();
                 it != mRequestQueue.end(); ++it) {
            // Abort the input buffers for reprocess requests.
            if ((*it)->mInputStream != NULL) {
                camera3_stream_buffer_t inputBuffer;
                status_t res = (*it)->mInputStream->getInputBuffer(&inputBuffer,
                        /*respectHalLimit*/ false);
                if (res != OK) {
                    ALOGW("%s: %d: couldn't get input buffer while clearing the request "
                            "list: %s (%d)", __FUNCTION__, __LINE__, strerror(-res), res);
                } else {
                    inputBuffer.status = CAMERA3_BUFFER_STATUS_ERROR;
                    res = (*it)->mInputStream->returnInputBuffer(inputBuffer);
                    if (res != OK) {
                        ALOGE("%s: %d: couldn't return input buffer while clearing the request "
                                "list: %s (%d)", __FUNCTION__, __LINE__, strerror(-res), res);
                    }
                }
            }
            // Set the frame number this request would have had, if it
            // had been submitted; this frame number will not be reused.
            // The requestId and burstId fields were set when the request was
            // submitted originally (in convertMetadataListToRequestListLocked)
            (*it)->mResultExtras.frameNumber = mFrameNumber++;
            listener->notifyError(hardware::camera2::ICameraDeviceCallbacks::ERROR_CAMERA_REQUEST,
                    (*it)->mResultExtras);
        }
    }
    mRequestQueue.clear();

    Mutex::Autolock al(mTriggerMutex);
    mTriggerMap.clear();
    if (lastFrameNumber != NULL) {
        *lastFrameNumber = mRepeatingLastFrameNumber;
    }
    mRepeatingLastFrameNumber = hardware::camera2::ICameraDeviceUser::NO_IN_FLIGHT_REPEATING_FRAMES;
    mRequestSignal.signal();
    return OK;
}

status_t Camera3Device::RequestThread::flush() {
    ATRACE_CALL();
    Mutex::Autolock l(mFlushLock);

    return mInterface->flush();
}

void Camera3Device::RequestThread::setPaused(bool paused) {
    ATRACE_CALL();
    Mutex::Autolock l(mPauseLock);
    mDoPause = paused;
    mDoPauseSignal.signal();
}

status_t Camera3Device::RequestThread::waitUntilRequestProcessed(
        int32_t requestId, nsecs_t timeout) {
    ATRACE_CALL();
    Mutex::Autolock l(mLatestRequestMutex);
    status_t res;
    while (mLatestRequestId != requestId) {
        nsecs_t startTime = systemTime();

        res = mLatestRequestSignal.waitRelative(mLatestRequestMutex, timeout);
        if (res != OK) return res;

        timeout -= (systemTime() - startTime);
    }

    return OK;
}

void Camera3Device::RequestThread::requestExit() {
    // Call parent to set up shutdown
    Thread::requestExit();
    // The exit from any possible waits
    mDoPauseSignal.signal();
    mRequestSignal.signal();

    mRequestLatency.log("ProcessCaptureRequest latency histogram");
    mRequestLatency.reset();
}

void Camera3Device::RequestThread::checkAndStopRepeatingRequest() {
    ATRACE_CALL();
    bool surfaceAbandoned = false;
    int64_t lastFrameNumber = 0;
    sp<NotificationListener> listener;
    {
        Mutex::Autolock l(mRequestLock);
        // Check all streams needed by repeating requests are still valid. Otherwise, stop
        // repeating requests.
        for (const auto& request : mRepeatingRequests) {
            for (const auto& s : request->mOutputStreams) {
                if (s->isAbandoned()) {
                    surfaceAbandoned = true;
                    clearRepeatingRequestsLocked(&lastFrameNumber);
                    break;
                }
            }
            if (surfaceAbandoned) {
                break;
            }
        }
        listener = mListener.promote();
    }

    if (listener != NULL && surfaceAbandoned) {
        listener->notifyRepeatingRequestError(lastFrameNumber);
    }
}

bool Camera3Device::RequestThread::sendRequestsBatch() {
    ATRACE_CALL();
    status_t res;
    size_t batchSize = mNextRequests.size();
    std::vector<camera3_capture_request_t*> requests(batchSize);
    uint32_t numRequestProcessed = 0;
    for (size_t i = 0; i < batchSize; i++) {
        requests[i] = &mNextRequests.editItemAt(i).halRequest;
        ATRACE_ASYNC_BEGIN("frame capture", mNextRequests[i].halRequest.frame_number);
    }

    res = mInterface->processBatchCaptureRequests(requests, &numRequestProcessed);

    bool triggerRemoveFailed = false;
    NextRequest& triggerFailedRequest = mNextRequests.editItemAt(0);
    for (size_t i = 0; i < numRequestProcessed; i++) {
        NextRequest& nextRequest = mNextRequests.editItemAt(i);
        nextRequest.submitted = true;

        updateNextRequest(nextRequest);

        if (!triggerRemoveFailed) {
            // Remove any previously queued triggers (after unlock)
            status_t removeTriggerRes = removeTriggers(mPrevRequest);
            if (removeTriggerRes != OK) {
                triggerRemoveFailed = true;
                triggerFailedRequest = nextRequest;
            }
        }
    }

    if (triggerRemoveFailed) {
        SET_ERR("RequestThread: Unable to remove triggers "
              "(capture request %d, HAL device: %s (%d)",
              triggerFailedRequest.halRequest.frame_number, strerror(-res), res);
        cleanUpFailedRequests(/*sendRequestError*/ false);
        return false;
    }

    if (res != OK) {
        // Should only get a failure here for malformed requests or device-level
        // errors, so consider all errors fatal.  Bad metadata failures should
        // come through notify.
        SET_ERR("RequestThread: Unable to submit capture request %d to HAL device: %s (%d)",
                mNextRequests[numRequestProcessed].halRequest.frame_number,
                strerror(-res), res);
        cleanUpFailedRequests(/*sendRequestError*/ false);
        return false;
    }
    return true;
}

nsecs_t Camera3Device::RequestThread::calculateMaxExpectedDuration(const camera_metadata_t *request) {
    nsecs_t maxExpectedDuration = kDefaultExpectedDuration;
    camera_metadata_ro_entry_t e = camera_metadata_ro_entry_t();
    find_camera_metadata_ro_entry(request,
            ANDROID_CONTROL_AE_MODE,
            &e);
    if (e.count == 0) return maxExpectedDuration;

    switch (e.data.u8[0]) {
        case ANDROID_CONTROL_AE_MODE_OFF:
            find_camera_metadata_ro_entry(request,
                    ANDROID_SENSOR_EXPOSURE_TIME,
                    &e);
            if (e.count > 0) {
                maxExpectedDuration = e.data.i64[0];
            }
            find_camera_metadata_ro_entry(request,
                    ANDROID_SENSOR_FRAME_DURATION,
                    &e);
            if (e.count > 0) {
                maxExpectedDuration = std::max(e.data.i64[0], maxExpectedDuration);
            }
            break;
        default:
            find_camera_metadata_ro_entry(request,
                    ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                    &e);
            if (e.count > 1) {
                maxExpectedDuration = 1e9 / e.data.u8[0];
            }
            break;
    }

    return maxExpectedDuration;
}

bool Camera3Device::RequestThread::skipHFRTargetFPSUpdate(int32_t tag,
        const camera_metadata_ro_entry_t& newEntry, const camera_metadata_entry_t& currentEntry) {
    if (mConstrainedMode && (ANDROID_CONTROL_AE_TARGET_FPS_RANGE == tag) &&
            (newEntry.count == currentEntry.count) && (currentEntry.count == 2) &&
            (currentEntry.data.i32[1] == newEntry.data.i32[1])) {
        return true;
    }

    return false;
}

void Camera3Device::RequestThread::updateNextRequest(NextRequest& nextRequest) {
    // Update the latest request sent to HAL
    if (nextRequest.halRequest.settings != NULL) { // Don't update if they were unchanged
        Mutex::Autolock al(mLatestRequestMutex);

        camera_metadata_t* cloned = clone_camera_metadata(nextRequest.halRequest.settings);
        mLatestRequest.acquire(cloned);

        mLatestPhysicalRequest.clear();
        for (uint32_t i = 0; i < nextRequest.halRequest.num_physcam_settings; i++) {
            cloned = clone_camera_metadata(nextRequest.halRequest.physcam_settings[i]);
            mLatestPhysicalRequest.emplace(nextRequest.halRequest.physcam_id[i],
                    CameraMetadata(cloned));
        }

        sp<Camera3Device> parent = mParent.promote();
        if (parent != NULL) {
            parent->monitorMetadata(TagMonitor::REQUEST,
                    nextRequest.halRequest.frame_number,
                    0, mLatestRequest, mLatestPhysicalRequest);
        }
    }

    if (nextRequest.halRequest.settings != NULL) {
        nextRequest.captureRequest->mSettingsList.begin()->metadata.unlock(
                nextRequest.halRequest.settings);
    }

    cleanupPhysicalSettings(nextRequest.captureRequest, &nextRequest.halRequest);
}

bool Camera3Device::RequestThread::updateSessionParameters(const CameraMetadata& settings) {
    ATRACE_CALL();
    bool updatesDetected = false;

    CameraMetadata updatedParams(mLatestSessionParams);
    for (auto tag : mSessionParamKeys) {
        camera_metadata_ro_entry entry = settings.find(tag);
        camera_metadata_entry lastEntry = updatedParams.find(tag);

        if (entry.count > 0) {
            bool isDifferent = false;
            if (lastEntry.count > 0) {
                // Have a last value, compare to see if changed
                if (lastEntry.type == entry.type &&
                        lastEntry.count == entry.count) {
                    // Same type and count, compare values
                    size_t bytesPerValue = camera_metadata_type_size[lastEntry.type];
                    size_t entryBytes = bytesPerValue * lastEntry.count;
                    int cmp = memcmp(entry.data.u8, lastEntry.data.u8, entryBytes);
                    if (cmp != 0) {
                        isDifferent = true;
                    }
                } else {
                    // Count or type has changed
                    isDifferent = true;
                }
            } else {
                // No last entry, so always consider to be different
                isDifferent = true;
            }

            if (isDifferent) {
                ALOGV("%s: Session parameter tag id %d changed", __FUNCTION__, tag);
                if (!skipHFRTargetFPSUpdate(tag, entry, lastEntry)) {
                    updatesDetected = true;
                }
                updatedParams.update(entry);
            }
        } else if (lastEntry.count > 0) {
            // Value has been removed
            ALOGV("%s: Session parameter tag id %d removed", __FUNCTION__, tag);
            updatedParams.erase(tag);
            updatesDetected = true;
        }
    }

    bool reconfigureRequired;
    if (updatesDetected) {
        reconfigureRequired = mInterface->isReconfigurationRequired(mLatestSessionParams,
                updatedParams);
        mLatestSessionParams = updatedParams;
    } else {
        reconfigureRequired = false;
    }

    return reconfigureRequired;
}

bool Camera3Device::RequestThread::threadLoop() {
    ATRACE_CALL();
    status_t res;
    // Any function called from threadLoop() must not hold mInterfaceLock since
    // it could lead to deadlocks (disconnect() -> hold mInterfaceMutex -> wait for request thread
    // to finish -> request thread waits on mInterfaceMutex) http://b/143513518

    // Handle paused state.
    if (waitIfPaused()) {
        return true;
    }

    // Wait for the next batch of requests.
    waitForNextRequestBatch();
    if (mNextRequests.size() == 0) {
        return true;
    }

    // Get the latest request ID, if any
    int latestRequestId;
    camera_metadata_entry_t requestIdEntry = mNextRequests[mNextRequests.size() - 1].
            captureRequest->mSettingsList.begin()->metadata.find(ANDROID_REQUEST_ID);
    if (requestIdEntry.count > 0) {
        latestRequestId = requestIdEntry.data.i32[0];
    } else {
        ALOGW("%s: Did not have android.request.id set in the request.", __FUNCTION__);
        latestRequestId = NAME_NOT_FOUND;
    }

    // 'mNextRequests' will at this point contain either a set of HFR batched requests
    //  or a single request from streaming or burst. In either case the first element
    //  should contain the latest camera settings that we need to check for any session
    //  parameter updates.
    if (updateSessionParameters(mNextRequests[0].captureRequest->mSettingsList.begin()->metadata)) {
        res = OK;

        //Input stream buffers are already acquired at this point so an input stream
        //will not be able to move to idle state unless we force it.
        if (mNextRequests[0].captureRequest->mInputStream != nullptr) {
            res = mNextRequests[0].captureRequest->mInputStream->forceToIdle();
            if (res != OK) {
                ALOGE("%s: Failed to force idle input stream: %d", __FUNCTION__, res);
                cleanUpFailedRequests(/*sendRequestError*/ false);
                return false;
            }
        }

        if (res == OK) {
            sp<Camera3Device> parent = mParent.promote();
            if (parent != nullptr) {
                mReconfigured |= parent->reconfigureCamera(mLatestSessionParams, mStatusId);
            }
            setPaused(false);

            if (mNextRequests[0].captureRequest->mInputStream != nullptr) {
                mNextRequests[0].captureRequest->mInputStream->restoreConfiguredState();
                if (res != OK) {
                    ALOGE("%s: Failed to restore configured input stream: %d", __FUNCTION__, res);
                    cleanUpFailedRequests(/*sendRequestError*/ false);
                    return false;
                }
            }
        }
    }

    // Prepare a batch of HAL requests and output buffers.
    res = prepareHalRequests();
    if (res == TIMED_OUT) {
        // Not a fatal error if getting output buffers time out.
        cleanUpFailedRequests(/*sendRequestError*/ true);
        // Check if any stream is abandoned.
        checkAndStopRepeatingRequest();
        return true;
    } else if (res != OK) {
        cleanUpFailedRequests(/*sendRequestError*/ false);
        return false;
    }

    // Inform waitUntilRequestProcessed thread of a new request ID
    {
        Mutex::Autolock al(mLatestRequestMutex);

        mLatestRequestId = latestRequestId;
        mLatestRequestSignal.signal();
    }

    // Submit a batch of requests to HAL.
    // Use flush lock only when submitting multilple requests in a batch.
    // TODO: The problem with flush lock is flush() will be blocked by process_capture_request()
    // which may take a long time to finish so synchronizing flush() and
    // process_capture_request() defeats the purpose of cancelling requests ASAP with flush().
    // For now, only synchronize for high speed recording and we should figure something out for
    // removing the synchronization.
    bool useFlushLock = mNextRequests.size() > 1;

    if (useFlushLock) {
        mFlushLock.lock();
    }

    ALOGVV("%s: %d: submitting %zu requests in a batch.", __FUNCTION__, __LINE__,
            mNextRequests.size());

    sp<Camera3Device> parent = mParent.promote();
    if (parent != nullptr) {
        parent->mRequestBufferSM.onSubmittingRequest();
    }

    bool submitRequestSuccess = false;
    nsecs_t tRequestStart = systemTime(SYSTEM_TIME_MONOTONIC);
    submitRequestSuccess = sendRequestsBatch();

    nsecs_t tRequestEnd = systemTime(SYSTEM_TIME_MONOTONIC);
    mRequestLatency.add(tRequestStart, tRequestEnd);

    if (useFlushLock) {
        mFlushLock.unlock();
    }

    // Unset as current request
    {
        Mutex::Autolock l(mRequestLock);
        mNextRequests.clear();
    }
    mRequestSubmittedSignal.signal();

    return submitRequestSuccess;
}

status_t Camera3Device::RequestThread::prepareHalRequests() {
    ATRACE_CALL();

    bool batchedRequest = mNextRequests[0].captureRequest->mBatchSize > 1;
    for (size_t i = 0; i < mNextRequests.size(); i++) {
        auto& nextRequest = mNextRequests.editItemAt(i);
        sp<CaptureRequest> captureRequest = nextRequest.captureRequest;
        camera3_capture_request_t* halRequest = &nextRequest.halRequest;
        Vector<camera3_stream_buffer_t>* outputBuffers = &nextRequest.outputBuffers;

        // Prepare a request to HAL
        halRequest->frame_number = captureRequest->mResultExtras.frameNumber;

        // Insert any queued triggers (before metadata is locked)
        status_t res = insertTriggers(captureRequest);
        if (res < 0) {
            SET_ERR("RequestThread: Unable to insert triggers "
                    "(capture request %d, HAL device: %s (%d)",
                    halRequest->frame_number, strerror(-res), res);
            return INVALID_OPERATION;
        }

        int triggerCount = res;
        bool triggersMixedIn = (triggerCount > 0 || mPrevTriggers > 0);
        mPrevTriggers = triggerCount;

        bool rotateAndCropChanged = overrideAutoRotateAndCrop(captureRequest);

        // If the request is the same as last, or we had triggers last time
        bool newRequest =
                (mPrevRequest != captureRequest || triggersMixedIn || rotateAndCropChanged) &&
                // Request settings are all the same within one batch, so only treat the first
                // request in a batch as new
                !(batchedRequest && i > 0);
        if (newRequest) {
            std::set<std::string> cameraIdsWithZoom;
            /**
             * HAL workaround:
             * Insert a dummy trigger ID if a trigger is set but no trigger ID is
             */
            res = addDummyTriggerIds(captureRequest);
            if (res != OK) {
                SET_ERR("RequestThread: Unable to insert dummy trigger IDs "
                        "(capture request %d, HAL device: %s (%d)",
                        halRequest->frame_number, strerror(-res), res);
                return INVALID_OPERATION;
            }

            {
                // Correct metadata regions for distortion correction if enabled
                sp<Camera3Device> parent = mParent.promote();
                if (parent != nullptr) {
                    List<PhysicalCameraSettings>::iterator it;
                    for (it = captureRequest->mSettingsList.begin();
                            it != captureRequest->mSettingsList.end(); it++) {
                        if (parent->mDistortionMappers.find(it->cameraId) ==
                                parent->mDistortionMappers.end()) {
                            continue;
                        }

                        if (!captureRequest->mDistortionCorrectionUpdated) {
                            res = parent->mDistortionMappers[it->cameraId].correctCaptureRequest(
                                    &(it->metadata));
                            if (res != OK) {
                                SET_ERR("RequestThread: Unable to correct capture requests "
                                        "for lens distortion for request %d: %s (%d)",
                                        halRequest->frame_number, strerror(-res), res);
                                return INVALID_OPERATION;
                            }
                            captureRequest->mDistortionCorrectionUpdated = true;
                        }
                    }

                    for (it = captureRequest->mSettingsList.begin();
                            it != captureRequest->mSettingsList.end(); it++) {
                        if (parent->mZoomRatioMappers.find(it->cameraId) ==
                                parent->mZoomRatioMappers.end()) {
                            continue;
                        }

                        if (!captureRequest->mZoomRatioIs1x) {
                            cameraIdsWithZoom.insert(it->cameraId);
                        }

                        if (!captureRequest->mZoomRatioUpdated) {
                            res = parent->mZoomRatioMappers[it->cameraId].updateCaptureRequest(
                                    &(it->metadata));
                            if (res != OK) {
                                SET_ERR("RequestThread: Unable to correct capture requests "
                                        "for zoom ratio for request %d: %s (%d)",
                                        halRequest->frame_number, strerror(-res), res);
                                return INVALID_OPERATION;
                            }
                            captureRequest->mZoomRatioUpdated = true;
                        }
                    }
                    if (captureRequest->mRotateAndCropAuto &&
                            !captureRequest->mRotationAndCropUpdated) {
                        for (it = captureRequest->mSettingsList.begin();
                                it != captureRequest->mSettingsList.end(); it++) {
                            auto mapper = parent->mRotateAndCropMappers.find(it->cameraId);
                            if (mapper != parent->mRotateAndCropMappers.end()) {
                                res = mapper->second.updateCaptureRequest(&(it->metadata));
                                if (res != OK) {
                                    SET_ERR("RequestThread: Unable to correct capture requests "
                                            "for rotate-and-crop for request %d: %s (%d)",
                                            halRequest->frame_number, strerror(-res), res);
                                    return INVALID_OPERATION;
                                }
                            }
                        }
                        captureRequest->mRotationAndCropUpdated = true;
                    }
                }
            }

            /**
             * The request should be presorted so accesses in HAL
             *   are O(logn). Sidenote, sorting a sorted metadata is nop.
             */
            captureRequest->mSettingsList.begin()->metadata.sort();
            halRequest->settings = captureRequest->mSettingsList.begin()->metadata.getAndLock();
            mPrevRequest = captureRequest;
            mPrevCameraIdsWithZoom = cameraIdsWithZoom;
            ALOGVV("%s: Request settings are NEW", __FUNCTION__);

            IF_ALOGV() {
                camera_metadata_ro_entry_t e = camera_metadata_ro_entry_t();
                find_camera_metadata_ro_entry(
                        halRequest->settings,
                        ANDROID_CONTROL_AF_TRIGGER,
                        &e
                );
                if (e.count > 0) {
                    ALOGV("%s: Request (frame num %d) had AF trigger 0x%x",
                          __FUNCTION__,
                          halRequest->frame_number,
                          e.data.u8[0]);
                }
            }
        } else {
            // leave request.settings NULL to indicate 'reuse latest given'
            ALOGVV("%s: Request settings are REUSED",
                   __FUNCTION__);
        }

        if (captureRequest->mSettingsList.size() > 1) {
            halRequest->num_physcam_settings = captureRequest->mSettingsList.size() - 1;
            halRequest->physcam_id = new const char* [halRequest->num_physcam_settings];
            if (newRequest) {
                halRequest->physcam_settings =
                    new const camera_metadata* [halRequest->num_physcam_settings];
            } else {
                halRequest->physcam_settings = nullptr;
            }
            auto it = ++captureRequest->mSettingsList.begin();
            size_t i = 0;
            for (; it != captureRequest->mSettingsList.end(); it++, i++) {
                halRequest->physcam_id[i] = it->cameraId.c_str();
                if (newRequest) {
                    it->metadata.sort();
                    halRequest->physcam_settings[i] = it->metadata.getAndLock();
                }
            }
        }

        uint32_t totalNumBuffers = 0;

        // Fill in buffers
        if (captureRequest->mInputStream != NULL) {
            halRequest->input_buffer = &captureRequest->mInputBuffer;
            totalNumBuffers += 1;
        } else {
            halRequest->input_buffer = NULL;
        }

        outputBuffers->insertAt(camera3_stream_buffer_t(), 0,
                captureRequest->mOutputStreams.size());
        halRequest->output_buffers = outputBuffers->array();
        std::set<String8> requestedPhysicalCameras;

        sp<Camera3Device> parent = mParent.promote();
        if (parent == NULL) {
            // Should not happen, and nowhere to send errors to, so just log it
            CLOGE("RequestThread: Parent is gone");
            return INVALID_OPERATION;
        }
        nsecs_t waitDuration = kBaseGetBufferWait + parent->getExpectedInFlightDuration();

        SurfaceMap uniqueSurfaceIdMap;
        for (size_t j = 0; j < captureRequest->mOutputStreams.size(); j++) {
            sp<Camera3OutputStreamInterface> outputStream =
                    captureRequest->mOutputStreams.editItemAt(j);
            int streamId = outputStream->getId();

            // Prepare video buffers for high speed recording on the first video request.
            if (mPrepareVideoStream && outputStream->isVideoStream()) {
                // Only try to prepare video stream on the first video request.
                mPrepareVideoStream = false;

                res = outputStream->startPrepare(Camera3StreamInterface::ALLOCATE_PIPELINE_MAX,
                        false /*blockRequest*/);
                while (res == NOT_ENOUGH_DATA) {
                    res = outputStream->prepareNextBuffer();
                }
                if (res != OK) {
                    ALOGW("%s: Preparing video buffers for high speed failed: %s (%d)",
                        __FUNCTION__, strerror(-res), res);
                    outputStream->cancelPrepare();
                }
            }

            std::vector<size_t> uniqueSurfaceIds;
            res = outputStream->getUniqueSurfaceIds(
                    captureRequest->mOutputSurfaces[streamId],
                    &uniqueSurfaceIds);
            // INVALID_OPERATION is normal output for streams not supporting surfaceIds
            if (res != OK && res != INVALID_OPERATION) {
                ALOGE("%s: failed to query stream %d unique surface IDs",
                        __FUNCTION__, streamId);
                return res;
            }
            if (res == OK) {
                uniqueSurfaceIdMap.insert({streamId, std::move(uniqueSurfaceIds)});
            }

            if (mUseHalBufManager) {
                if (outputStream->isAbandoned()) {
                    ALOGV("%s: stream %d is abandoned, skipping request", __FUNCTION__, streamId);
                    return TIMED_OUT;
                }
                // HAL will request buffer through requestStreamBuffer API
                camera3_stream_buffer_t& buffer = outputBuffers->editItemAt(j);
                buffer.stream = outputStream->asHalStream();
                buffer.buffer = nullptr;
                buffer.status = CAMERA3_BUFFER_STATUS_OK;
                buffer.acquire_fence = -1;
                buffer.release_fence = -1;
            } else {
                res = outputStream->getBuffer(&outputBuffers->editItemAt(j),
                        waitDuration,
                        captureRequest->mOutputSurfaces[streamId]);
                if (res != OK) {
                    // Can't get output buffer from gralloc queue - this could be due to
                    // abandoned queue or other consumer misbehavior, so not a fatal
                    // error
                    ALOGV("RequestThread: Can't get output buffer, skipping request:"
                            " %s (%d)", strerror(-res), res);

                    return TIMED_OUT;
                }
            }

            {
                sp<Camera3Device> parent = mParent.promote();
                if (parent != nullptr) {
                    const String8& streamCameraId = outputStream->getPhysicalCameraId();
                    for (const auto& settings : captureRequest->mSettingsList) {
                        if ((streamCameraId.isEmpty() &&
                                parent->getId() == settings.cameraId.c_str()) ||
                                streamCameraId == settings.cameraId.c_str()) {
                            outputStream->fireBufferRequestForFrameNumber(
                                    captureRequest->mResultExtras.frameNumber,
                                    settings.metadata);
                        }
                    }
                }
            }

            String8 physicalCameraId = outputStream->getPhysicalCameraId();

            if (!physicalCameraId.isEmpty()) {
                // Physical stream isn't supported for input request.
                if (halRequest->input_buffer) {
                    CLOGE("Physical stream is not supported for input request");
                    return INVALID_OPERATION;
                }
                requestedPhysicalCameras.insert(physicalCameraId);
            }
            halRequest->num_output_buffers++;
        }
        totalNumBuffers += halRequest->num_output_buffers;

        // Log request in the in-flight queue
        // If this request list is for constrained high speed recording (not
        // preview), and the current request is not the last one in the batch,
        // do not send callback to the app.
        bool hasCallback = true;
        if (batchedRequest && i != mNextRequests.size()-1) {
            hasCallback = false;
        }
        bool isStillCapture = false;
        bool isZslCapture = false;
        if (!mNextRequests[0].captureRequest->mSettingsList.begin()->metadata.isEmpty()) {
            camera_metadata_ro_entry_t e = camera_metadata_ro_entry_t();
            find_camera_metadata_ro_entry(halRequest->settings, ANDROID_CONTROL_CAPTURE_INTENT, &e);
            if ((e.count > 0) && (e.data.u8[0] == ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE)) {
                isStillCapture = true;
                ATRACE_ASYNC_BEGIN("still capture", mNextRequests[i].halRequest.frame_number);
            }

            find_camera_metadata_ro_entry(halRequest->settings, ANDROID_CONTROL_ENABLE_ZSL, &e);
            if ((e.count > 0) && (e.data.u8[0] == ANDROID_CONTROL_ENABLE_ZSL_TRUE)) {
                isZslCapture = true;
            }
        }
        res = parent->registerInFlight(halRequest->frame_number,
                totalNumBuffers, captureRequest->mResultExtras,
                /*hasInput*/halRequest->input_buffer != NULL,
                hasCallback,
                calculateMaxExpectedDuration(halRequest->settings),
                requestedPhysicalCameras, isStillCapture, isZslCapture,
                captureRequest->mRotateAndCropAuto, mPrevCameraIdsWithZoom,
                (mUseHalBufManager) ? uniqueSurfaceIdMap :
                                      SurfaceMap{});
        ALOGVV("%s: registered in flight requestId = %" PRId32 ", frameNumber = %" PRId64
               ", burstId = %" PRId32 ".",
                __FUNCTION__,
                captureRequest->mResultExtras.requestId, captureRequest->mResultExtras.frameNumber,
                captureRequest->mResultExtras.burstId);
        if (res != OK) {
            SET_ERR("RequestThread: Unable to register new in-flight request:"
                    " %s (%d)", strerror(-res), res);
            return INVALID_OPERATION;
        }
    }

    return OK;
}

CameraMetadata Camera3Device::RequestThread::getLatestRequest() const {
    ATRACE_CALL();
    Mutex::Autolock al(mLatestRequestMutex);

    ALOGV("RequestThread::%s", __FUNCTION__);

    return mLatestRequest;
}

bool Camera3Device::RequestThread::isStreamPending(
        sp<Camera3StreamInterface>& stream) {
    ATRACE_CALL();
    Mutex::Autolock l(mRequestLock);

    for (const auto& nextRequest : mNextRequests) {
        if (!nextRequest.submitted) {
            for (const auto& s : nextRequest.captureRequest->mOutputStreams) {
                if (stream == s) return true;
            }
            if (stream == nextRequest.captureRequest->mInputStream) return true;
        }
    }

    for (const auto& request : mRequestQueue) {
        for (const auto& s : request->mOutputStreams) {
            if (stream == s) return true;
        }
        if (stream == request->mInputStream) return true;
    }

    for (const auto& request : mRepeatingRequests) {
        for (const auto& s : request->mOutputStreams) {
            if (stream == s) return true;
        }
        if (stream == request->mInputStream) return true;
    }

    return false;
}

bool Camera3Device::RequestThread::isOutputSurfacePending(int streamId, size_t surfaceId) {
    ATRACE_CALL();
    Mutex::Autolock l(mRequestLock);

    for (const auto& nextRequest : mNextRequests) {
        for (const auto& s : nextRequest.captureRequest->mOutputSurfaces) {
            if (s.first == streamId) {
                const auto &it = std::find(s.second.begin(), s.second.end(), surfaceId);
                if (it != s.second.end()) {
                    return true;
                }
            }
        }
    }

    for (const auto& request : mRequestQueue) {
        for (const auto& s : request->mOutputSurfaces) {
            if (s.first == streamId) {
                const auto &it = std::find(s.second.begin(), s.second.end(), surfaceId);
                if (it != s.second.end()) {
                    return true;
                }
            }
        }
    }

    for (const auto& request : mRepeatingRequests) {
        for (const auto& s : request->mOutputSurfaces) {
            if (s.first == streamId) {
                const auto &it = std::find(s.second.begin(), s.second.end(), surfaceId);
                if (it != s.second.end()) {
                    return true;
                }
            }
        }
    }

    return false;
}

void Camera3Device::RequestThread::signalPipelineDrain(const std::vector<int>& streamIds) {
    if (!mUseHalBufManager) {
        ALOGE("%s called for camera device not supporting HAL buffer management", __FUNCTION__);
        return;
    }

    Mutex::Autolock pl(mPauseLock);
    if (mPaused) {
        mInterface->signalPipelineDrain(streamIds);
        return;
    }
    // If request thread is still busy, wait until paused then notify HAL
    mNotifyPipelineDrain = true;
    mStreamIdsToBeDrained = streamIds;
}

void Camera3Device::RequestThread::resetPipelineDrain() {
    Mutex::Autolock pl(mPauseLock);
    mNotifyPipelineDrain = false;
    mStreamIdsToBeDrained.clear();
}

void Camera3Device::RequestThread::clearPreviousRequest() {
    Mutex::Autolock l(mRequestLock);
    mPrevRequest.clear();
}

status_t Camera3Device::RequestThread::switchToOffline(
        const std::vector<int32_t>& streamsToKeep,
        /*out*/hardware::camera::device::V3_6::CameraOfflineSessionInfo* offlineSessionInfo,
        /*out*/sp<hardware::camera::device::V3_6::ICameraOfflineSession>* offlineSession,
        /*out*/camera3::BufferRecords* bufferRecords) {
    Mutex::Autolock l(mRequestLock);
    clearRepeatingRequestsLocked(/*lastFrameNumber*/nullptr);

    // Wait until request thread is fully stopped
    // TBD: check if request thread is being paused by other APIs (shouldn't be)

    // We could also check for mRepeatingRequests.empty(), but the API interface
    // is serialized by Camera3Device::mInterfaceLock so no one should be able to submit any
    // new requests during the call; hence skip that check.
    bool queueEmpty = mNextRequests.empty() && mRequestQueue.empty();
    while (!queueEmpty) {
        status_t res = mRequestSubmittedSignal.waitRelative(mRequestLock, kRequestSubmitTimeout);
        if (res == TIMED_OUT) {
            ALOGE("%s: request thread failed to submit one request within timeout!", __FUNCTION__);
            return res;
        } else if (res != OK) {
            ALOGE("%s: request thread failed to submit a request: %s (%d)!",
                    __FUNCTION__, strerror(-res), res);
            return res;
        }
        queueEmpty = mNextRequests.empty() && mRequestQueue.empty();
    }

    return mInterface->switchToOffline(
            streamsToKeep, offlineSessionInfo, offlineSession, bufferRecords);
}

status_t Camera3Device::RequestThread::setRotateAndCropAutoBehavior(
        camera_metadata_enum_android_scaler_rotate_and_crop_t rotateAndCropValue) {
    ATRACE_CALL();
    Mutex::Autolock l(mTriggerMutex);
    if (rotateAndCropValue == ANDROID_SCALER_ROTATE_AND_CROP_AUTO) {
        return BAD_VALUE;
    }
    mRotateAndCropOverride = rotateAndCropValue;
    return OK;
}

nsecs_t Camera3Device::getExpectedInFlightDuration() {
    ATRACE_CALL();
    std::lock_guard<std::mutex> l(mInFlightLock);
    return mExpectedInflightDuration > kMinInflightDuration ?
            mExpectedInflightDuration : kMinInflightDuration;
}

void Camera3Device::RequestThread::cleanupPhysicalSettings(sp<CaptureRequest> request,
        camera3_capture_request_t *halRequest) {
    if ((request == nullptr) || (halRequest == nullptr)) {
        ALOGE("%s: Invalid request!", __FUNCTION__);
        return;
    }

    if (halRequest->num_physcam_settings > 0) {
        if (halRequest->physcam_id != nullptr) {
            delete [] halRequest->physcam_id;
            halRequest->physcam_id = nullptr;
        }
        if (halRequest->physcam_settings != nullptr) {
            auto it = ++(request->mSettingsList.begin());
            size_t i = 0;
            for (; it != request->mSettingsList.end(); it++, i++) {
                it->metadata.unlock(halRequest->physcam_settings[i]);
            }
            delete [] halRequest->physcam_settings;
            halRequest->physcam_settings = nullptr;
        }
    }
}

void Camera3Device::RequestThread::cleanUpFailedRequests(bool sendRequestError) {
    if (mNextRequests.empty()) {
        return;
    }

    for (auto& nextRequest : mNextRequests) {
        // Skip the ones that have been submitted successfully.
        if (nextRequest.submitted) {
            continue;
        }

        sp<CaptureRequest> captureRequest = nextRequest.captureRequest;
        camera3_capture_request_t* halRequest = &nextRequest.halRequest;
        Vector<camera3_stream_buffer_t>* outputBuffers = &nextRequest.outputBuffers;

        if (halRequest->settings != NULL) {
            captureRequest->mSettingsList.begin()->metadata.unlock(halRequest->settings);
        }

        cleanupPhysicalSettings(captureRequest, halRequest);

        if (captureRequest->mInputStream != NULL) {
            captureRequest->mInputBuffer.status = CAMERA3_BUFFER_STATUS_ERROR;
            captureRequest->mInputStream->returnInputBuffer(captureRequest->mInputBuffer);
        }

        // No output buffer can be returned when using HAL buffer manager
        if (!mUseHalBufManager) {
            for (size_t i = 0; i < halRequest->num_output_buffers; i++) {
                //Buffers that failed processing could still have
                //valid acquire fence.
                int acquireFence = (*outputBuffers)[i].acquire_fence;
                if (0 <= acquireFence) {
                    close(acquireFence);
                    outputBuffers->editItemAt(i).acquire_fence = -1;
                }
                outputBuffers->editItemAt(i).status = CAMERA3_BUFFER_STATUS_ERROR;
                captureRequest->mOutputStreams.editItemAt(i)->returnBuffer((*outputBuffers)[i], 0,
                        /*timestampIncreasing*/true, std::vector<size_t> (),
                        captureRequest->mResultExtras.frameNumber);
            }
        }

        if (sendRequestError) {
            Mutex::Autolock l(mRequestLock);
            sp<NotificationListener> listener = mListener.promote();
            if (listener != NULL) {
                listener->notifyError(
                        hardware::camera2::ICameraDeviceCallbacks::ERROR_CAMERA_REQUEST,
                        captureRequest->mResultExtras);
            }
        }

        // Remove yet-to-be submitted inflight request from inflightMap
        {
          sp<Camera3Device> parent = mParent.promote();
          if (parent != NULL) {
              std::lock_guard<std::mutex> l(parent->mInFlightLock);
              ssize_t idx = parent->mInFlightMap.indexOfKey(captureRequest->mResultExtras.frameNumber);
              if (idx >= 0) {
                  ALOGV("%s: Remove inflight request from queue: frameNumber %" PRId64,
                        __FUNCTION__, captureRequest->mResultExtras.frameNumber);
                  parent->removeInFlightMapEntryLocked(idx);
              }
          }
        }
    }

    Mutex::Autolock l(mRequestLock);
    mNextRequests.clear();
}

void Camera3Device::RequestThread::waitForNextRequestBatch() {
    ATRACE_CALL();
    // Optimized a bit for the simple steady-state case (single repeating
    // request), to avoid putting that request in the queue temporarily.
    Mutex::Autolock l(mRequestLock);

    assert(mNextRequests.empty());

    NextRequest nextRequest;
    nextRequest.captureRequest = waitForNextRequestLocked();
    if (nextRequest.captureRequest == nullptr) {
        return;
    }

    nextRequest.halRequest = camera3_capture_request_t();
    nextRequest.submitted = false;
    mNextRequests.add(nextRequest);

    // Wait for additional requests
    const size_t batchSize = nextRequest.captureRequest->mBatchSize;

    for (size_t i = 1; i < batchSize; i++) {
        NextRequest additionalRequest;
        additionalRequest.captureRequest = waitForNextRequestLocked();
        if (additionalRequest.captureRequest == nullptr) {
            break;
        }

        additionalRequest.halRequest = camera3_capture_request_t();
        additionalRequest.submitted = false;
        mNextRequests.add(additionalRequest);
    }

    if (mNextRequests.size() < batchSize) {
        ALOGE("RequestThread: only get %zu out of %zu requests. Skipping requests.",
                mNextRequests.size(), batchSize);
        cleanUpFailedRequests(/*sendRequestError*/true);
    }

    return;
}

sp<Camera3Device::CaptureRequest>
        Camera3Device::RequestThread::waitForNextRequestLocked() {
    status_t res;
    sp<CaptureRequest> nextRequest;

    while (mRequestQueue.empty()) {
        if (!mRepeatingRequests.empty()) {
            // Always atomically enqueue all requests in a repeating request
            // list. Guarantees a complete in-sequence set of captures to
            // application.
            const RequestList &requests = mRepeatingRequests;
            RequestList::const_iterator firstRequest =
                    requests.begin();
            nextRequest = *firstRequest;
            mRequestQueue.insert(mRequestQueue.end(),
                    ++firstRequest,
                    requests.end());
            // No need to wait any longer

            mRepeatingLastFrameNumber = mFrameNumber + requests.size() - 1;

            break;
        }

        res = mRequestSignal.waitRelative(mRequestLock, kRequestTimeout);

        if ((mRequestQueue.empty() && mRepeatingRequests.empty()) ||
                exitPending()) {
            Mutex::Autolock pl(mPauseLock);
            if (mPaused == false) {
                ALOGV("%s: RequestThread: Going idle", __FUNCTION__);
                mPaused = true;
                if (mNotifyPipelineDrain) {
                    mInterface->signalPipelineDrain(mStreamIdsToBeDrained);
                    mNotifyPipelineDrain = false;
                    mStreamIdsToBeDrained.clear();
                }
                // Let the tracker know
                sp<StatusTracker> statusTracker = mStatusTracker.promote();
                if (statusTracker != 0) {
                    statusTracker->markComponentIdle(mStatusId, Fence::NO_FENCE);
                }
                sp<Camera3Device> parent = mParent.promote();
                if (parent != nullptr) {
                    parent->mRequestBufferSM.onRequestThreadPaused();
                }
            }
            // Stop waiting for now and let thread management happen
            return NULL;
        }
    }

    if (nextRequest == NULL) {
        // Don't have a repeating request already in hand, so queue
        // must have an entry now.
        RequestList::iterator firstRequest =
                mRequestQueue.begin();
        nextRequest = *firstRequest;
        mRequestQueue.erase(firstRequest);
        if (mRequestQueue.empty() && !nextRequest->mRepeating) {
            sp<NotificationListener> listener = mListener.promote();
            if (listener != NULL) {
                listener->notifyRequestQueueEmpty();
            }
        }
    }

    // In case we've been unpaused by setPaused clearing mDoPause, need to
    // update internal pause state (capture/setRepeatingRequest unpause
    // directly).
    Mutex::Autolock pl(mPauseLock);
    if (mPaused) {
        ALOGV("%s: RequestThread: Unpaused", __FUNCTION__);
        sp<StatusTracker> statusTracker = mStatusTracker.promote();
        if (statusTracker != 0) {
            statusTracker->markComponentActive(mStatusId);
        }
    }
    mPaused = false;

    // Check if we've reconfigured since last time, and reset the preview
    // request if so. Can't use 'NULL request == repeat' across configure calls.
    if (mReconfigured) {
        mPrevRequest.clear();
        mReconfigured = false;
    }

    if (nextRequest != NULL) {
        nextRequest->mResultExtras.frameNumber = mFrameNumber++;
        nextRequest->mResultExtras.afTriggerId = mCurrentAfTriggerId;
        nextRequest->mResultExtras.precaptureTriggerId = mCurrentPreCaptureTriggerId;

        // Since RequestThread::clear() removes buffers from the input stream,
        // get the right buffer here before unlocking mRequestLock
        if (nextRequest->mInputStream != NULL) {
            res = nextRequest->mInputStream->getInputBuffer(&nextRequest->mInputBuffer);
            if (res != OK) {
                // Can't get input buffer from gralloc queue - this could be due to
                // disconnected queue or other producer misbehavior, so not a fatal
                // error
                ALOGE("%s: Can't get input buffer, skipping request:"
                        " %s (%d)", __FUNCTION__, strerror(-res), res);

                sp<NotificationListener> listener = mListener.promote();
                if (listener != NULL) {
                    listener->notifyError(
                            hardware::camera2::ICameraDeviceCallbacks::ERROR_CAMERA_REQUEST,
                            nextRequest->mResultExtras);
                }
                return NULL;
            }
        }
    }

    return nextRequest;
}

bool Camera3Device::RequestThread::waitIfPaused() {
    ATRACE_CALL();
    status_t res;
    Mutex::Autolock l(mPauseLock);
    while (mDoPause) {
        if (mPaused == false) {
            mPaused = true;
            ALOGV("%s: RequestThread: Paused", __FUNCTION__);
            if (mNotifyPipelineDrain) {
                mInterface->signalPipelineDrain(mStreamIdsToBeDrained);
                mNotifyPipelineDrain = false;
                mStreamIdsToBeDrained.clear();
            }
            // Let the tracker know
            sp<StatusTracker> statusTracker = mStatusTracker.promote();
            if (statusTracker != 0) {
                statusTracker->markComponentIdle(mStatusId, Fence::NO_FENCE);
            }
            sp<Camera3Device> parent = mParent.promote();
            if (parent != nullptr) {
                parent->mRequestBufferSM.onRequestThreadPaused();
            }
        }

        res = mDoPauseSignal.waitRelative(mPauseLock, kRequestTimeout);
        if (res == TIMED_OUT || exitPending()) {
            return true;
        }
    }
    // We don't set mPaused to false here, because waitForNextRequest needs
    // to further manage the paused state in case of starvation.
    return false;
}

void Camera3Device::RequestThread::unpauseForNewRequests() {
    ATRACE_CALL();
    // With work to do, mark thread as unpaused.
    // If paused by request (setPaused), don't resume, to avoid
    // extra signaling/waiting overhead to waitUntilPaused
    mRequestSignal.signal();
    Mutex::Autolock p(mPauseLock);
    if (!mDoPause) {
        ALOGV("%s: RequestThread: Going active", __FUNCTION__);
        if (mPaused) {
            sp<StatusTracker> statusTracker = mStatusTracker.promote();
            if (statusTracker != 0) {
                statusTracker->markComponentActive(mStatusId);
            }
        }
        mPaused = false;
    }
}

void Camera3Device::RequestThread::setErrorState(const char *fmt, ...) {
    sp<Camera3Device> parent = mParent.promote();
    if (parent != NULL) {
        va_list args;
        va_start(args, fmt);

        parent->setErrorStateV(fmt, args);

        va_end(args);
    }
}

status_t Camera3Device::RequestThread::insertTriggers(
        const sp<CaptureRequest> &request) {
    ATRACE_CALL();
    Mutex::Autolock al(mTriggerMutex);

    sp<Camera3Device> parent = mParent.promote();
    if (parent == NULL) {
        CLOGE("RequestThread: Parent is gone");
        return DEAD_OBJECT;
    }

    CameraMetadata &metadata = request->mSettingsList.begin()->metadata;
    size_t count = mTriggerMap.size();

    for (size_t i = 0; i < count; ++i) {
        RequestTrigger trigger = mTriggerMap.valueAt(i);
        uint32_t tag = trigger.metadataTag;

        if (tag == ANDROID_CONTROL_AF_TRIGGER_ID || tag == ANDROID_CONTROL_AE_PRECAPTURE_ID) {
            bool isAeTrigger = (trigger.metadataTag == ANDROID_CONTROL_AE_PRECAPTURE_ID);
            uint32_t triggerId = static_cast<uint32_t>(trigger.entryValue);
            if (isAeTrigger) {
                request->mResultExtras.precaptureTriggerId = triggerId;
                mCurrentPreCaptureTriggerId = triggerId;
            } else {
                request->mResultExtras.afTriggerId = triggerId;
                mCurrentAfTriggerId = triggerId;
            }
            continue;
        }

        camera_metadata_entry entry = metadata.find(tag);

        if (entry.count > 0) {
            /**
             * Already has an entry for this trigger in the request.
             * Rewrite it with our requested trigger value.
             */
            RequestTrigger oldTrigger = trigger;

            oldTrigger.entryValue = entry.data.u8[0];

            mTriggerReplacedMap.add(tag, oldTrigger);
        } else {
            /**
             * More typical, no trigger entry, so we just add it
             */
            mTriggerRemovedMap.add(tag, trigger);
        }

        status_t res;

        switch (trigger.getTagType()) {
            case TYPE_BYTE: {
                uint8_t entryValue = static_cast<uint8_t>(trigger.entryValue);
                res = metadata.update(tag,
                                      &entryValue,
                                      /*count*/1);
                break;
            }
            case TYPE_INT32:
                res = metadata.update(tag,
                                      &trigger.entryValue,
                                      /*count*/1);
                break;
            default:
                ALOGE("%s: Type not supported: 0x%x",
                      __FUNCTION__,
                      trigger.getTagType());
                return INVALID_OPERATION;
        }

        if (res != OK) {
            ALOGE("%s: Failed to update request metadata with trigger tag %s"
                  ", value %d", __FUNCTION__, trigger.getTagName(),
                  trigger.entryValue);
            return res;
        }

        ALOGV("%s: Mixed in trigger %s, value %d", __FUNCTION__,
              trigger.getTagName(),
              trigger.entryValue);
    }

    mTriggerMap.clear();

    return count;
}

status_t Camera3Device::RequestThread::removeTriggers(
        const sp<CaptureRequest> &request) {
    ATRACE_CALL();
    Mutex::Autolock al(mTriggerMutex);

    CameraMetadata &metadata = request->mSettingsList.begin()->metadata;

    /**
     * Replace all old entries with their old values.
     */
    for (size_t i = 0; i < mTriggerReplacedMap.size(); ++i) {
        RequestTrigger trigger = mTriggerReplacedMap.valueAt(i);

        status_t res;

        uint32_t tag = trigger.metadataTag;
        switch (trigger.getTagType()) {
            case TYPE_BYTE: {
                uint8_t entryValue = static_cast<uint8_t>(trigger.entryValue);
                res = metadata.update(tag,
                                      &entryValue,
                                      /*count*/1);
                break;
            }
            case TYPE_INT32:
                res = metadata.update(tag,
                                      &trigger.entryValue,
                                      /*count*/1);
                break;
            default:
                ALOGE("%s: Type not supported: 0x%x",
                      __FUNCTION__,
                      trigger.getTagType());
                return INVALID_OPERATION;
        }

        if (res != OK) {
            ALOGE("%s: Failed to restore request metadata with trigger tag %s"
                  ", trigger value %d", __FUNCTION__,
                  trigger.getTagName(), trigger.entryValue);
            return res;
        }
    }
    mTriggerReplacedMap.clear();

    /**
     * Remove all new entries.
     */
    for (size_t i = 0; i < mTriggerRemovedMap.size(); ++i) {
        RequestTrigger trigger = mTriggerRemovedMap.valueAt(i);
        status_t res = metadata.erase(trigger.metadataTag);

        if (res != OK) {
            ALOGE("%s: Failed to erase metadata with trigger tag %s"
                  ", trigger value %d", __FUNCTION__,
                  trigger.getTagName(), trigger.entryValue);
            return res;
        }
    }
    mTriggerRemovedMap.clear();

    return OK;
}

status_t Camera3Device::RequestThread::addDummyTriggerIds(
        const sp<CaptureRequest> &request) {
    // Trigger ID 0 had special meaning in the HAL2 spec, so avoid it here
    static const int32_t dummyTriggerId = 1;
    status_t res;

    CameraMetadata &metadata = request->mSettingsList.begin()->metadata;

    // If AF trigger is active, insert a dummy AF trigger ID if none already
    // exists
    camera_metadata_entry afTrigger = metadata.find(ANDROID_CONTROL_AF_TRIGGER);
    camera_metadata_entry afId = metadata.find(ANDROID_CONTROL_AF_TRIGGER_ID);
    if (afTrigger.count > 0 &&
            afTrigger.data.u8[0] != ANDROID_CONTROL_AF_TRIGGER_IDLE &&
            afId.count == 0) {
        res = metadata.update(ANDROID_CONTROL_AF_TRIGGER_ID, &dummyTriggerId, 1);
        if (res != OK) return res;
    }

    // If AE precapture trigger is active, insert a dummy precapture trigger ID
    // if none already exists
    camera_metadata_entry pcTrigger =
            metadata.find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
    camera_metadata_entry pcId = metadata.find(ANDROID_CONTROL_AE_PRECAPTURE_ID);
    if (pcTrigger.count > 0 &&
            pcTrigger.data.u8[0] != ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE &&
            pcId.count == 0) {
        res = metadata.update(ANDROID_CONTROL_AE_PRECAPTURE_ID,
                &dummyTriggerId, 1);
        if (res != OK) return res;
    }

    return OK;
}

bool Camera3Device::RequestThread::overrideAutoRotateAndCrop(
        const sp<CaptureRequest> &request) {
    ATRACE_CALL();

    if (request->mRotateAndCropAuto) {
        Mutex::Autolock l(mTriggerMutex);
        CameraMetadata &metadata = request->mSettingsList.begin()->metadata;

        auto rotateAndCropEntry = metadata.find(ANDROID_SCALER_ROTATE_AND_CROP);
        if (rotateAndCropEntry.count > 0) {
            if (rotateAndCropEntry.data.u8[0] == mRotateAndCropOverride) {
                return false;
            } else {
                rotateAndCropEntry.data.u8[0] = mRotateAndCropOverride;
                return true;
            }
        } else {
            uint8_t rotateAndCrop_u8 = mRotateAndCropOverride;
            metadata.update(ANDROID_SCALER_ROTATE_AND_CROP,
                    &rotateAndCrop_u8, 1);
            return true;
        }
    }
    return false;
}

/**
 * PreparerThread inner class methods
 */

Camera3Device::PreparerThread::PreparerThread() :
        Thread(/*canCallJava*/false), mListener(nullptr),
        mActive(false), mCancelNow(false), mCurrentMaxCount(0), mCurrentPrepareComplete(false) {
}

Camera3Device::PreparerThread::~PreparerThread() {
    Thread::requestExitAndWait();
    if (mCurrentStream != nullptr) {
        mCurrentStream->cancelPrepare();
        ATRACE_ASYNC_END("stream prepare", mCurrentStream->getId());
        mCurrentStream.clear();
    }
    clear();
}

status_t Camera3Device::PreparerThread::prepare(int maxCount, sp<Camera3StreamInterface>& stream) {
    ATRACE_CALL();
    status_t res;

    Mutex::Autolock l(mLock);
    sp<NotificationListener> listener = mListener.promote();

    res = stream->startPrepare(maxCount, true /*blockRequest*/);
    if (res == OK) {
        // No preparation needed, fire listener right off
        ALOGV("%s: Stream %d already prepared", __FUNCTION__, stream->getId());
        if (listener != NULL) {
            listener->notifyPrepared(stream->getId());
        }
        return OK;
    } else if (res != NOT_ENOUGH_DATA) {
        return res;
    }

    // Need to prepare, start up thread if necessary
    if (!mActive) {
        // mRunning will change to false before the thread fully shuts down, so wait to be sure it
        // isn't running
        Thread::requestExitAndWait();
        res = Thread::run("C3PrepThread", PRIORITY_BACKGROUND);
        if (res != OK) {
            ALOGE("%s: Unable to start preparer stream: %d (%s)", __FUNCTION__, res, strerror(-res));
            if (listener != NULL) {
                listener->notifyPrepared(stream->getId());
            }
            return res;
        }
        mCancelNow = false;
        mActive = true;
        ALOGV("%s: Preparer stream started", __FUNCTION__);
    }

    // queue up the work
    mPendingStreams.emplace(maxCount, stream);
    ALOGV("%s: Stream %d queued for preparing", __FUNCTION__, stream->getId());

    return OK;
}

void Camera3Device::PreparerThread::pause() {
    ATRACE_CALL();

    Mutex::Autolock l(mLock);

    std::unordered_map<int, sp<camera3::Camera3StreamInterface> > pendingStreams;
    pendingStreams.insert(mPendingStreams.begin(), mPendingStreams.end());
    sp<camera3::Camera3StreamInterface> currentStream = mCurrentStream;
    int currentMaxCount = mCurrentMaxCount;
    mPendingStreams.clear();
    mCancelNow = true;
    while (mActive) {
        auto res = mThreadActiveSignal.waitRelative(mLock, kActiveTimeout);
        if (res == TIMED_OUT) {
            ALOGE("%s: Timed out waiting on prepare thread!", __FUNCTION__);
            return;
        } else if (res != OK) {
            ALOGE("%s: Encountered an error: %d waiting on prepare thread!", __FUNCTION__, res);
            return;
        }
    }

    //Check whether the prepare thread was able to complete the current
    //stream. In case work is still pending emplace it along with the rest
    //of the streams in the pending list.
    if (currentStream != nullptr) {
        if (!mCurrentPrepareComplete) {
            pendingStreams.emplace(currentMaxCount, currentStream);
        }
    }

    mPendingStreams.insert(pendingStreams.begin(), pendingStreams.end());
    for (const auto& it : mPendingStreams) {
        it.second->cancelPrepare();
    }
}

status_t Camera3Device::PreparerThread::resume() {
    ATRACE_CALL();
    status_t res;

    Mutex::Autolock l(mLock);
    sp<NotificationListener> listener = mListener.promote();

    if (mActive) {
        ALOGE("%s: Trying to resume an already active prepare thread!", __FUNCTION__);
        return NO_INIT;
    }

    auto it = mPendingStreams.begin();
    for (; it != mPendingStreams.end();) {
        res = it->second->startPrepare(it->first, true /*blockRequest*/);
        if (res == OK) {
            if (listener != NULL) {
                listener->notifyPrepared(it->second->getId());
            }
            it = mPendingStreams.erase(it);
        } else if (res != NOT_ENOUGH_DATA) {
            ALOGE("%s: Unable to start preparer stream: %d (%s)", __FUNCTION__,
                    res, strerror(-res));
            it = mPendingStreams.erase(it);
        } else {
            it++;
        }
    }

    if (mPendingStreams.empty()) {
        return OK;
    }

    res = Thread::run("C3PrepThread", PRIORITY_BACKGROUND);
    if (res != OK) {
        ALOGE("%s: Unable to start preparer stream: %d (%s)",
                __FUNCTION__, res, strerror(-res));
        return res;
    }
    mCancelNow = false;
    mActive = true;
    ALOGV("%s: Preparer stream started", __FUNCTION__);

    return OK;
}

status_t Camera3Device::PreparerThread::clear() {
    ATRACE_CALL();
    Mutex::Autolock l(mLock);

    for (const auto& it : mPendingStreams) {
        it.second->cancelPrepare();
    }
    mPendingStreams.clear();
    mCancelNow = true;

    return OK;
}

void Camera3Device::PreparerThread::setNotificationListener(wp<NotificationListener> listener) {
    ATRACE_CALL();
    Mutex::Autolock l(mLock);
    mListener = listener;
}

bool Camera3Device::PreparerThread::threadLoop() {
    status_t res;
    {
        Mutex::Autolock l(mLock);
        if (mCurrentStream == nullptr) {
            // End thread if done with work
            if (mPendingStreams.empty()) {
                ALOGV("%s: Preparer stream out of work", __FUNCTION__);
                // threadLoop _must not_ re-acquire mLock after it sets mActive to false; would
                // cause deadlock with prepare()'s requestExitAndWait triggered by !mActive.
                mActive = false;
                mThreadActiveSignal.signal();
                return false;
            }

            // Get next stream to prepare
            auto it = mPendingStreams.begin();
            mCurrentStream = it->second;
            mCurrentMaxCount = it->first;
            mCurrentPrepareComplete = false;
            mPendingStreams.erase(it);
            ATRACE_ASYNC_BEGIN("stream prepare", mCurrentStream->getId());
            ALOGV("%s: Preparing stream %d", __FUNCTION__, mCurrentStream->getId());
        } else if (mCancelNow) {
            mCurrentStream->cancelPrepare();
            ATRACE_ASYNC_END("stream prepare", mCurrentStream->getId());
            ALOGV("%s: Cancelling stream %d prepare", __FUNCTION__, mCurrentStream->getId());
            mCurrentStream.clear();
            mCancelNow = false;
            return true;
        }
    }

    res = mCurrentStream->prepareNextBuffer();
    if (res == NOT_ENOUGH_DATA) return true;
    if (res != OK) {
        // Something bad happened; try to recover by cancelling prepare and
        // signalling listener anyway
        ALOGE("%s: Stream %d returned error %d (%s) during prepare", __FUNCTION__,
                mCurrentStream->getId(), res, strerror(-res));
        mCurrentStream->cancelPrepare();
    }

    // This stream has finished, notify listener
    Mutex::Autolock l(mLock);
    sp<NotificationListener> listener = mListener.promote();
    if (listener != NULL) {
        ALOGV("%s: Stream %d prepare done, signaling listener", __FUNCTION__,
                mCurrentStream->getId());
        listener->notifyPrepared(mCurrentStream->getId());
    }

    ATRACE_ASYNC_END("stream prepare", mCurrentStream->getId());
    mCurrentStream.clear();
    mCurrentPrepareComplete = true;

    return true;
}

status_t Camera3Device::RequestBufferStateMachine::initialize(
        sp<camera3::StatusTracker> statusTracker) {
    if (statusTracker == nullptr) {
        ALOGE("%s: statusTracker is null", __FUNCTION__);
        return BAD_VALUE;
    }

    std::lock_guard<std::mutex> lock(mLock);
    mStatusTracker = statusTracker;
    mRequestBufferStatusId = statusTracker->addComponent();
    return OK;
}

bool Camera3Device::RequestBufferStateMachine::startRequestBuffer() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mStatus == RB_STATUS_READY || mStatus == RB_STATUS_PENDING_STOP) {
        mRequestBufferOngoing = true;
        notifyTrackerLocked(/*active*/true);
        return true;
    }
    return false;
}

void Camera3Device::RequestBufferStateMachine::endRequestBuffer() {
    std::lock_guard<std::mutex> lock(mLock);
    if (!mRequestBufferOngoing) {
        ALOGE("%s called without a successful startRequestBuffer call first!", __FUNCTION__);
        return;
    }
    mRequestBufferOngoing = false;
    if (mStatus == RB_STATUS_PENDING_STOP) {
        checkSwitchToStopLocked();
    }
    notifyTrackerLocked(/*active*/false);
}

void Camera3Device::RequestBufferStateMachine::onStreamsConfigured() {
    std::lock_guard<std::mutex> lock(mLock);
    mSwitchedToOffline = false;
    mStatus = RB_STATUS_READY;
    return;
}

void Camera3Device::RequestBufferStateMachine::onSubmittingRequest() {
    std::lock_guard<std::mutex> lock(mLock);
    mRequestThreadPaused = false;
    // inflight map register actually happens in prepareHalRequest now, but it is close enough
    // approximation.
    mInflightMapEmpty = false;
    if (mStatus == RB_STATUS_STOPPED) {
        mStatus = RB_STATUS_READY;
    }
    return;
}

void Camera3Device::RequestBufferStateMachine::onRequestThreadPaused() {
    std::lock_guard<std::mutex> lock(mLock);
    mRequestThreadPaused = true;
    if (mStatus == RB_STATUS_PENDING_STOP) {
        checkSwitchToStopLocked();
    }
    return;
}

void Camera3Device::RequestBufferStateMachine::onInflightMapEmpty() {
    std::lock_guard<std::mutex> lock(mLock);
    mInflightMapEmpty = true;
    if (mStatus == RB_STATUS_PENDING_STOP) {
        checkSwitchToStopLocked();
    }
    return;
}

void Camera3Device::RequestBufferStateMachine::onWaitUntilIdle() {
    std::lock_guard<std::mutex> lock(mLock);
    if (!checkSwitchToStopLocked()) {
        mStatus = RB_STATUS_PENDING_STOP;
    }
    return;
}

bool Camera3Device::RequestBufferStateMachine::onSwitchToOfflineSuccess() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mRequestBufferOngoing) {
        ALOGE("%s: HAL must not be requesting buffer after HAL returns switchToOffline!",
                __FUNCTION__);
        return false;
    }
    mSwitchedToOffline = true;
    mInflightMapEmpty = true;
    mRequestThreadPaused = true;
    mStatus = RB_STATUS_STOPPED;
    return true;
}

void Camera3Device::RequestBufferStateMachine::notifyTrackerLocked(bool active) {
    sp<StatusTracker> statusTracker = mStatusTracker.promote();
    if (statusTracker != nullptr) {
        if (active) {
            statusTracker->markComponentActive(mRequestBufferStatusId);
        } else {
            statusTracker->markComponentIdle(mRequestBufferStatusId, Fence::NO_FENCE);
        }
    }
}

bool Camera3Device::RequestBufferStateMachine::checkSwitchToStopLocked() {
    if (mInflightMapEmpty && mRequestThreadPaused && !mRequestBufferOngoing) {
        mStatus = RB_STATUS_STOPPED;
        return true;
    }
    return false;
}

bool Camera3Device::startRequestBuffer() {
    return mRequestBufferSM.startRequestBuffer();
}

void Camera3Device::endRequestBuffer() {
    mRequestBufferSM.endRequestBuffer();
}

nsecs_t Camera3Device::getWaitDuration() {
    return kBaseGetBufferWait + getExpectedInFlightDuration();
}

void Camera3Device::getInflightBufferKeys(std::vector<std::pair<int32_t, int32_t>>* out) {
    mInterface->getInflightBufferKeys(out);
}

void Camera3Device::getInflightRequestBufferKeys(std::vector<uint64_t>* out) {
    mInterface->getInflightRequestBufferKeys(out);
}

std::vector<sp<Camera3StreamInterface>> Camera3Device::getAllStreams() {
    std::vector<sp<Camera3StreamInterface>> ret;
    bool hasInputStream = mInputStream != nullptr;
    ret.reserve(mOutputStreams.size() + mDeletedStreams.size() + ((hasInputStream) ? 1 : 0));
    if (hasInputStream) {
        ret.push_back(mInputStream);
    }
    for (size_t i = 0; i < mOutputStreams.size(); i++) {
        ret.push_back(mOutputStreams[i]);
    }
    for (size_t i = 0; i < mDeletedStreams.size(); i++) {
        ret.push_back(mDeletedStreams[i]);
    }
    return ret;
}

status_t Camera3Device::switchToOffline(
        const std::vector<int32_t>& streamsToKeep,
        /*out*/ sp<CameraOfflineSessionBase>* session) {
    ATRACE_CALL();
    if (session == nullptr) {
        ALOGE("%s: session must not be null", __FUNCTION__);
        return BAD_VALUE;
    }

    Mutex::Autolock il(mInterfaceLock);

    bool hasInputStream = mInputStream != nullptr;
    int32_t inputStreamId = hasInputStream ? mInputStream->getId() : -1;
    bool inputStreamSupportsOffline = hasInputStream ?
            mInputStream->getOfflineProcessingSupport() : false;
    auto outputStreamIds = mOutputStreams.getStreamIds();
    auto streamIds = outputStreamIds;
    if (hasInputStream) {
        streamIds.push_back(mInputStream->getId());
    }

    // Check all streams in streamsToKeep supports offline mode
    for (auto id : streamsToKeep) {
        if (std::find(streamIds.begin(), streamIds.end(), id) == streamIds.end()) {
            ALOGE("%s: Unknown stream ID %d", __FUNCTION__, id);
            return BAD_VALUE;
        } else if (id == inputStreamId) {
            if (!inputStreamSupportsOffline) {
                ALOGE("%s: input stream %d cannot be switched to offline",
                        __FUNCTION__, id);
                return BAD_VALUE;
            }
        } else {
            sp<camera3::Camera3OutputStreamInterface> stream = mOutputStreams.get(id);
            if (!stream->getOfflineProcessingSupport()) {
                ALOGE("%s: output stream %d cannot be switched to offline",
                        __FUNCTION__, id);
                return BAD_VALUE;
            }
        }
    }

    // TODO: block surface sharing and surface group streams until we can support them

    // Stop repeating request, wait until all remaining requests are submitted, then call into
    // HAL switchToOffline
    hardware::camera::device::V3_6::CameraOfflineSessionInfo offlineSessionInfo;
    sp<hardware::camera::device::V3_6::ICameraOfflineSession> offlineSession;
    camera3::BufferRecords bufferRecords;
    status_t ret = mRequestThread->switchToOffline(
            streamsToKeep, &offlineSessionInfo, &offlineSession, &bufferRecords);

    if (ret != OK) {
        SET_ERR("Switch to offline failed: %s (%d)", strerror(-ret), ret);
        return ret;
    }

    bool succ = mRequestBufferSM.onSwitchToOfflineSuccess();
    if (!succ) {
        SET_ERR("HAL must not be calling requestStreamBuffers call");
        // TODO: block ALL callbacks from HAL till app configured new streams?
        return UNKNOWN_ERROR;
    }

    // Verify offlineSessionInfo
    std::vector<int32_t> offlineStreamIds;
    offlineStreamIds.reserve(offlineSessionInfo.offlineStreams.size());
    for (auto offlineStream : offlineSessionInfo.offlineStreams) {
        // verify stream IDs
        int32_t id = offlineStream.id;
        if (std::find(streamIds.begin(), streamIds.end(), id) == streamIds.end()) {
            SET_ERR("stream ID %d not found!", id);
            return UNKNOWN_ERROR;
        }

        // When not using HAL buf manager, only allow streams requested by app to be preserved
        if (!mUseHalBufManager) {
            if (std::find(streamsToKeep.begin(), streamsToKeep.end(), id) == streamsToKeep.end()) {
                SET_ERR("stream ID %d must not be switched to offline!", id);
                return UNKNOWN_ERROR;
            }
        }

        offlineStreamIds.push_back(id);
        sp<Camera3StreamInterface> stream = (id == inputStreamId) ?
                static_cast<sp<Camera3StreamInterface>>(mInputStream) :
                static_cast<sp<Camera3StreamInterface>>(mOutputStreams.get(id));
        // Verify number of outstanding buffers
        if (stream->getOutstandingBuffersCount() != offlineStream.numOutstandingBuffers) {
            SET_ERR("Offline stream %d # of remaining buffer mismatch: (%zu,%d) (service/HAL)",
                    id, stream->getOutstandingBuffersCount(), offlineStream.numOutstandingBuffers);
            return UNKNOWN_ERROR;
        }
    }

    // Verify all streams to be deleted don't have any outstanding buffers
    if (hasInputStream && std::find(offlineStreamIds.begin(), offlineStreamIds.end(),
                inputStreamId) == offlineStreamIds.end()) {
        if (mInputStream->hasOutstandingBuffers()) {
            SET_ERR("Input stream %d still has %zu outstanding buffer!",
                    inputStreamId, mInputStream->getOutstandingBuffersCount());
            return UNKNOWN_ERROR;
        }
    }

    for (const auto& outStreamId : outputStreamIds) {
        if (std::find(offlineStreamIds.begin(), offlineStreamIds.end(),
                outStreamId) == offlineStreamIds.end()) {
            auto outStream = mOutputStreams.get(outStreamId);
            if (outStream->hasOutstandingBuffers()) {
                SET_ERR("Output stream %d still has %zu outstanding buffer!",
                        outStreamId, outStream->getOutstandingBuffersCount());
                return UNKNOWN_ERROR;
            }
        }
    }

    InFlightRequestMap offlineReqs;
    // Verify inflight requests and their pending buffers
    {
        std::lock_guard<std::mutex> l(mInFlightLock);
        for (auto offlineReq : offlineSessionInfo.offlineRequests) {
            int idx = mInFlightMap.indexOfKey(offlineReq.frameNumber);
            if (idx == NAME_NOT_FOUND) {
                SET_ERR("Offline request frame number %d not found!", offlineReq.frameNumber);
                return UNKNOWN_ERROR;
            }

            const auto& inflightReq = mInFlightMap.valueAt(idx);
            // TODO: check specific stream IDs
            size_t numBuffersLeft = static_cast<size_t>(inflightReq.numBuffersLeft);
            if (numBuffersLeft != offlineReq.pendingStreams.size()) {
                SET_ERR("Offline request # of remaining buffer mismatch: (%d,%d) (service/HAL)",
                        inflightReq.numBuffersLeft, offlineReq.pendingStreams.size());
                return UNKNOWN_ERROR;
            }
            offlineReqs.add(offlineReq.frameNumber, inflightReq);
        }
    }

    // Create Camera3OfflineSession and transfer object ownership
    //   (streams, inflight requests, buffer caches)
    camera3::StreamSet offlineStreamSet;
    sp<camera3::Camera3Stream> inputStream;
    for (auto offlineStream : offlineSessionInfo.offlineStreams) {
        int32_t id = offlineStream.id;
        if (mInputStream != nullptr && id == mInputStream->getId()) {
            inputStream = mInputStream;
        } else {
            offlineStreamSet.add(id, mOutputStreams.get(id));
        }
    }

    // TODO: check if we need to lock before copying states
    //       though technically no other thread should be talking to Camera3Device at this point
    Camera3OfflineStates offlineStates(
            mTagMonitor, mVendorTagId, mUseHalBufManager, mNeedFixupMonochromeTags,
            mUsePartialResult, mNumPartialResults, mLastCompletedRegularFrameNumber,
            mLastCompletedReprocessFrameNumber, mLastCompletedZslFrameNumber,
            mNextResultFrameNumber, mNextReprocessResultFrameNumber,
            mNextZslStillResultFrameNumber, mNextShutterFrameNumber,
            mNextReprocessShutterFrameNumber, mNextZslStillShutterFrameNumber,
            mDeviceInfo, mPhysicalDeviceInfoMap, mDistortionMappers,
            mZoomRatioMappers, mRotateAndCropMappers);

    *session = new Camera3OfflineSession(mId, inputStream, offlineStreamSet,
            std::move(bufferRecords), offlineReqs, offlineStates, offlineSession);

    // Delete all streams that has been transferred to offline session
    Mutex::Autolock l(mLock);
    for (auto offlineStream : offlineSessionInfo.offlineStreams) {
        int32_t id = offlineStream.id;
        if (mInputStream != nullptr && id == mInputStream->getId()) {
            mInputStream.clear();
        } else {
            mOutputStreams.remove(id);
        }
    }

    // disconnect all other streams and switch to UNCONFIGURED state
    if (mInputStream != nullptr) {
        ret = mInputStream->disconnect();
        if (ret != OK) {
            SET_ERR_L("disconnect input stream failed!");
            return UNKNOWN_ERROR;
        }
    }

    for (auto streamId : mOutputStreams.getStreamIds()) {
        sp<Camera3StreamInterface> stream = mOutputStreams.get(streamId);
        ret = stream->disconnect();
        if (ret != OK) {
            SET_ERR_L("disconnect output stream %d failed!", streamId);
            return UNKNOWN_ERROR;
        }
    }

    mInputStream.clear();
    mOutputStreams.clear();
    mNeedConfig = true;
    internalUpdateStatusLocked(STATUS_UNCONFIGURED);
    mOperatingMode = NO_MODE;
    mIsConstrainedHighSpeedConfiguration = false;
    mRequestThread->clearPreviousRequest();

    return OK;
    // TO be done by CameraDeviceClient/Camera3OfflineSession
    // register the offline client to camera service
    // Setup result passthing threads etc
    // Initialize offline session so HAL can start sending callback to it (result Fmq)
    // TODO: check how many onIdle callback will be sent
    // Java side to make sure the CameraCaptureSession is properly closed
}

void Camera3Device::getOfflineStreamIds(std::vector<int> *offlineStreamIds) {
    ATRACE_CALL();

    if (offlineStreamIds == nullptr) {
        return;
    }

    Mutex::Autolock il(mInterfaceLock);

    auto streamIds = mOutputStreams.getStreamIds();
    bool hasInputStream = mInputStream != nullptr;
    if (hasInputStream && mInputStream->getOfflineProcessingSupport()) {
        offlineStreamIds->push_back(mInputStream->getId());
    }

    for (const auto & streamId : streamIds) {
        sp<camera3::Camera3OutputStreamInterface> stream = mOutputStreams.get(streamId);
        // Streams that use the camera buffer manager are currently not supported in
        // offline mode
        if (stream->getOfflineProcessingSupport() &&
                (stream->getStreamSetId() == CAMERA3_STREAM_SET_ID_INVALID)) {
            offlineStreamIds->push_back(streamId);
        }
    }
}

status_t Camera3Device::setRotateAndCropAutoBehavior(
    camera_metadata_enum_android_scaler_rotate_and_crop_t rotateAndCropValue) {
    ATRACE_CALL();
    Mutex::Autolock il(mInterfaceLock);
    Mutex::Autolock l(mLock);
    if (mRequestThread == nullptr) {
        return INVALID_OPERATION;
    }
    return mRequestThread->setRotateAndCropAutoBehavior(rotateAndCropValue);
}

}; // namespace android
