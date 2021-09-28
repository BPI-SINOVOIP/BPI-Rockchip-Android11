/*
 * Copyright (C) 2015 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "NativeCamera"
#include <log/log.h>

#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <unistd.h>
#include <assert.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <set>

#include <android/native_window_jni.h>

#include "camera/NdkCameraError.h"
#include "camera/NdkCameraManager.h"
#include "camera/NdkCameraMetadata.h"
#include "camera/NdkCameraDevice.h"
#include "camera/NdkCameraCaptureSession.h"
#include "media/NdkImage.h"
#include "media/NdkImageReader.h"

#define LOG_ERROR(buf, ...) sprintf(buf, __VA_ARGS__); \
                            ALOGE("%s", buf);

namespace {
    const int MAX_ERROR_STRING_LEN = 512;
    char errorString[MAX_ERROR_STRING_LEN];
}

template <>
struct std::default_delete<ACameraMetadata> {
    inline void operator()(ACameraMetadata* chars) const { ACameraMetadata_free(chars); }
};

class CameraServiceListener {
  public:
    typedef std::set<std::pair<std::string, std::string>> StringPairSet;

    static void onAvailable(void* obj, const char* cameraId) {
        ALOGV("Camera %s onAvailable", cameraId);
        if (obj == nullptr) {
            return;
        }
        CameraServiceListener* thiz = reinterpret_cast<CameraServiceListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnAvailableCount++;
        thiz->mAvailableMap[cameraId] = true;
        return;
    }

    static void onUnavailable(void* obj, const char* cameraId) {
        ALOGV("Camera %s onUnavailable", cameraId);
        if (obj == nullptr) {
            return;
        }
        CameraServiceListener* thiz = reinterpret_cast<CameraServiceListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnUnavailableCount++;
        thiz->mAvailableMap[cameraId] = false;
        return;
    }


    static void onCameraAccessPrioritiesChanged(void* obj) {
        ALOGV("onCameraAccessPrioritiesChanged");
        if (obj == nullptr) {
            return;
        }
        CameraServiceListener* thiz = reinterpret_cast<CameraServiceListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnCameraAccessPrioritiesChangedCount++;
        return;
    }

    static void onPhysicalCameraAvailable(void* obj, const char* cameraId,
            const char* physicalCameraId) {
        ALOGV("Camera %s : %s onAvailable", cameraId, physicalCameraId);
        if (obj == nullptr) {
            return;
        }
        CameraServiceListener* thiz = reinterpret_cast<CameraServiceListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnPhysicalCameraAvailableCount++;
        return;
    }

    static void onPhysicalCameraUnavailable(void* obj, const char* cameraId,
            const char* physicalCameraId) {
        ALOGV("Camera %s : %s onUnavailable", cameraId, physicalCameraId);
        if (obj == nullptr) {
            return;
        }
        CameraServiceListener* thiz = reinterpret_cast<CameraServiceListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mUnavailablePhysicalCameras.emplace(cameraId, physicalCameraId);
        return;
    }


    void resetCount() {
        std::lock_guard<std::mutex> lock(mMutex);
        mOnAvailableCount = 0;
        mOnUnavailableCount = 0;
        mOnCameraAccessPrioritiesChangedCount = 0;
        mOnPhysicalCameraAvailableCount = 0;
        mUnavailablePhysicalCameras.clear();
        return;
    }

    int getAvailableCount() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnAvailableCount;
    }

    int getUnavailableCount() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnUnavailableCount;
    }

    int getCameraAccessPrioritiesChangedCount() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnCameraAccessPrioritiesChangedCount;
    }

    int getPhysicalCameraAvailableCount() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnPhysicalCameraAvailableCount;
    }

    StringPairSet getUnavailablePhysicalCameras() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mUnavailablePhysicalCameras;
    }

    bool isAvailable(const char* cameraId) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mAvailableMap.count(cameraId) == 0) {
            return false;
        }
        return mAvailableMap[cameraId];
    }

  private:
    std::mutex mMutex;
    int mOnAvailableCount = 0;
    int mOnUnavailableCount = 0;
    int mOnCameraAccessPrioritiesChangedCount = 0;
    int mOnPhysicalCameraAvailableCount = 0;
    std::map<std::string, bool> mAvailableMap;
    StringPairSet mUnavailablePhysicalCameras;
};

class CameraDeviceListener {
  public:
    static void onDisconnected(void* obj, ACameraDevice* device) {
        ALOGV("Camera %s is disconnected!", ACameraDevice_getId(device));
        if (obj == nullptr) {
            return;
        }
        CameraDeviceListener* thiz = reinterpret_cast<CameraDeviceListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnDisconnect++;
        return;
    }

    static void onError(void* obj, ACameraDevice* device, int errorCode) {
        ALOGV("Camera %s receive error %d!", ACameraDevice_getId(device), errorCode);
        if (obj == nullptr) {
            return;
        }
        CameraDeviceListener* thiz = reinterpret_cast<CameraDeviceListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnError++;
        thiz->mLatestError = errorCode;
        return;
    }

  private:
    std::mutex mMutex;
    int mOnDisconnect = 0;
    int mOnError = 0;
    int mLatestError = 0;
};

class CaptureSessionListener {

  public:
    static void onClosed(void* obj, ACameraCaptureSession *session) {
        // TODO: might want an API to query cameraId even session is closed?
        ALOGV("Session %p is closed!", session);
        if (obj == nullptr) {
            return;
        }
        CaptureSessionListener* thiz = reinterpret_cast<CaptureSessionListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mIsClosed = true;
        thiz->mOnClosed++; // Should never > 1
    }

    static void onReady(void* obj, ACameraCaptureSession *session) {
        ALOGV("%s", __FUNCTION__);
        if (obj == nullptr) {
            return;
        }
        CaptureSessionListener* thiz = reinterpret_cast<CaptureSessionListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        ACameraDevice* device = nullptr;
        camera_status_t ret = ACameraCaptureSession_getDevice(session, &device);
        // There will be one onReady fired after session closed
        if (ret != ACAMERA_OK && !thiz->mIsClosed) {
            ALOGE("%s Getting camera device from session callback failed!",
                    __FUNCTION__);
            thiz->mInError = true;
        }
        ALOGV("Session for camera %s is ready!", ACameraDevice_getId(device));
        thiz->mIsIdle = true;
        thiz->mOnReady++;
    }

    static void onActive(void* obj, ACameraCaptureSession *session) {
        ALOGV("%s", __FUNCTION__);
        if (obj == nullptr) {
            return;
        }
        CaptureSessionListener* thiz = reinterpret_cast<CaptureSessionListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        ACameraDevice* device = nullptr;
        camera_status_t ret = ACameraCaptureSession_getDevice(session, &device);
        if (ret != ACAMERA_OK) {
            ALOGE("%s Getting camera device from session callback failed!",
                    __FUNCTION__);
            thiz->mInError = true;
        }
        ALOGV("Session for camera %s is busy!", ACameraDevice_getId(device));
        thiz->mIsIdle = false;
        thiz->mOnActive;
    }

    bool isClosed() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mIsClosed;
    }

    bool isIdle() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mIsIdle;
    }

    bool isInError() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mInError;
    }

    int onClosedCount()  {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnClosed;
    }

    int onReadyCount()  {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnReady;
    }

    int onActiveCount()  {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnActive;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mMutex);
        mIsClosed = false;
        mIsIdle = true;
        mInError = false;
        mOnClosed = 0;
        mOnReady = 0;
        mOnActive = 0;
    }

  private:
    std::mutex mMutex;
    bool mIsClosed = false;
    bool mIsIdle = true;
    bool mInError = false; // should always stay false
    int mOnClosed = 0;
    int mOnReady = 0;
    int mOnActive = 0;
};

class CaptureResultListener {
  public:
    ~CaptureResultListener() {
        std::unique_lock<std::mutex> l(mMutex);
        clearSavedRequestsLocked();
        clearFailedLostFrameNumbersLocked();
    }

    static void onCaptureStart(void* /*obj*/, ACameraCaptureSession* /*session*/,
            const ACaptureRequest* /*request*/, int64_t /*timestamp*/) {
        //Not used for now
    }

    static void onCaptureProgressed(void* /*obj*/, ACameraCaptureSession* /*session*/,
            ACaptureRequest* /*request*/, const ACameraMetadata* /*result*/) {
        //Not used for now
    }

    static void onCaptureCompleted(void* obj, ACameraCaptureSession* /*session*/,
            ACaptureRequest* request, const ACameraMetadata* result) {
        ALOGV("%s", __FUNCTION__);
        if ((obj == nullptr) || (result == nullptr)) {
            return;
        }
        CaptureResultListener* thiz = reinterpret_cast<CaptureResultListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        ACameraMetadata_const_entry entry;
        auto ret = ACameraMetadata_getConstEntry(result, ACAMERA_SYNC_FRAME_NUMBER, &entry);
        if (ret != ACAMERA_OK) {
            ALOGE("Error: Sync frame number missing from result!");
            return;
        }

        ACameraMetadata* copy = ACameraMetadata_copy(result);
        ACameraMetadata_const_entry entryCopy;
        ret = ACameraMetadata_getConstEntry(copy, ACAMERA_SYNC_FRAME_NUMBER, &entryCopy);
        if (ret != ACAMERA_OK) {
            ALOGE("Error: Sync frame number missing from result copy!");
            return;
        }

        if (entry.data.i64[0] != entryCopy.data.i64[0]) {
            ALOGE("Error: Sync frame number %" PRId64 " mismatch result copy %" PRId64,
                    entry.data.i64[0], entryCopy.data.i64[0]);
            return;
        }
        ACameraMetadata_free(copy);

        if (thiz->mSaveCompletedRequests) {
            thiz->mCompletedRequests.push_back(ACaptureRequest_copy(request));
        }

        thiz->mLastCompletedFrameNumber = entry.data.i64[0];
        thiz->mResultCondition.notify_one();
    }

    static void onLogicalCameraCaptureCompleted(void* obj, ACameraCaptureSession* /*session*/,
            ACaptureRequest* request, const ACameraMetadata* result,
            size_t physicalResultCount, const char** physicalCameraIds,
            const ACameraMetadata** physicalResults) {
        ALOGV("%s", __FUNCTION__);
        if ((obj == nullptr) || (result == nullptr)) {
            return;
        }
        CaptureResultListener* thiz = reinterpret_cast<CaptureResultListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        ACameraMetadata_const_entry entry;
        auto ret = ACameraMetadata_getConstEntry(result, ACAMERA_SYNC_FRAME_NUMBER, &entry);
        if (ret != ACAMERA_OK) {
            ALOGE("Error: Sync frame number missing from result!");
            return;
        }

        ACameraMetadata* copy = ACameraMetadata_copy(result);
        ACameraMetadata_const_entry entryCopy;
        ret = ACameraMetadata_getConstEntry(copy, ACAMERA_SYNC_FRAME_NUMBER, &entryCopy);
        if (ret != ACAMERA_OK) {
            ALOGE("Error: Sync frame number missing from result copy!");
            return;
        }

        if (entry.data.i64[0] != entryCopy.data.i64[0]) {
            ALOGE("Error: Sync frame number %" PRId64 " mismatch result copy %" PRId64,
                    entry.data.i64[0], entryCopy.data.i64[0]);
            return;
        }

        if (thiz->mRegisteredPhysicalIds.size() != physicalResultCount) {
            ALOGE("Error: Number of registered physical camera Ids %zu is different than received"
                    " physical camera Ids %zu", thiz->mRegisteredPhysicalIds.size(),
                    physicalResultCount);
            return;
        }
        for (size_t i = 0; i < physicalResultCount; i++) {
            if (physicalCameraIds[i] == nullptr) {
                ALOGE("Error: Invalid physical camera id in capture result");
                return;
            }
            if (physicalResults[i] == nullptr) {
                ALOGE("Error: Invalid physical camera metadata in capture result");
                return;
            }
            ACameraMetadata_const_entry physicalEntry;
            auto ret = ACameraMetadata_getConstEntry(physicalResults[i], ACAMERA_SYNC_FRAME_NUMBER,
                    &physicalEntry);
            if (ret != ACAMERA_OK) {
                ALOGE("Error: Sync frame number missing from physical camera result metadata!");
                return;
            }
            if (physicalEntry.data.i64[0] != entryCopy.data.i64[0]) {
                ALOGE("Error: Physical camera sync frame number %" PRId64
                        " mismatch result copy %" PRId64,
                        physicalEntry.data.i64[0], entryCopy.data.i64[0]);
                return;
            }

            auto foundId = std::find(thiz->mRegisteredPhysicalIds.begin(),
                    thiz->mRegisteredPhysicalIds.end(), physicalCameraIds[i]);
            if (foundId == thiz->mRegisteredPhysicalIds.end()) {
                ALOGE("Error: Returned physical camera Id %s is not registered",
                        physicalCameraIds[i]);
                return;
            }
        }
        ACameraMetadata_free(copy);

        if (thiz->mSaveCompletedRequests) {
            thiz->mCompletedRequests.push_back(ACaptureRequest_copy(request));
        }

        thiz->mLastCompletedFrameNumber = entry.data.i64[0];
        thiz->mResultCondition.notify_one();
    }

    static void onCaptureFailed(void* obj, ACameraCaptureSession* /*session*/,
            ACaptureRequest* /*request*/, ACameraCaptureFailure* failure) {
        ALOGV("%s", __FUNCTION__);
        if ((obj == nullptr) || (failure == nullptr)) {
            return;
        }
        CaptureResultListener* thiz = reinterpret_cast<CaptureResultListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mFailedFrameNumbers.insert(failure->frameNumber);
        thiz->mResultCondition.notify_one();
    }

    static void onLogicalCameraCaptureFailed(void* obj, ACameraCaptureSession* /*session*/,
            ACaptureRequest* /*request*/, ALogicalCameraCaptureFailure* failure) {
        ALOGV("%s", __FUNCTION__);
        if ((obj == nullptr) || (failure == nullptr)) {
            return;
        }
        if (failure->physicalCameraId != nullptr) {
            ALOGV("%s: physicalCameraId: %s", __FUNCTION__, failure->physicalCameraId);
        }
        CaptureResultListener* thiz = reinterpret_cast<CaptureResultListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mFailedFrameNumbers.insert(failure->captureFailure.frameNumber);
        thiz->mResultCondition.notify_one();
    }

    static void onCaptureSequenceCompleted(void* obj, ACameraCaptureSession* /*session*/,
            int sequenceId, int64_t frameNumber) {
        ALOGV("%s", __FUNCTION__);
        if (obj == nullptr) {
            return;
        }
        CaptureResultListener* thiz = reinterpret_cast<CaptureResultListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mLastSequenceIdCompleted = sequenceId;
        thiz->mLastSequenceFrameNumber = frameNumber;
        thiz->mResultCondition.notify_one();
    }

    static void onCaptureSequenceAborted(void* /*obj*/, ACameraCaptureSession* /*session*/,
            int /*sequenceId*/) {
        //Not used for now
    }

    static void onCaptureBufferLost(void* obj, ACameraCaptureSession* /*session*/,
            ACaptureRequest* /*request*/, ANativeWindow* /*window*/, int64_t frameNumber) {
        ALOGV("%s", __FUNCTION__);
        if (obj == nullptr) {
            return;
        }
        CaptureResultListener* thiz = reinterpret_cast<CaptureResultListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mBufferLostFrameNumbers.insert(frameNumber);
        thiz->mResultCondition.notify_one();
    }

    int64_t getCaptureSequenceLastFrameNumber(int64_t sequenceId, uint32_t timeoutSec) {
        int64_t ret = -1;
        std::unique_lock<std::mutex> l(mMutex);

        while (mLastSequenceIdCompleted != sequenceId) {
            auto timeout = std::chrono::system_clock::now() +
                           std::chrono::seconds(timeoutSec);
            if (std::cv_status::timeout == mResultCondition.wait_until(l, timeout)) {
                break;
            }
        }

        if (mLastSequenceIdCompleted == sequenceId) {
            ret = mLastSequenceFrameNumber;
        }

        return ret;
    }

    bool waitForFrameNumber(int64_t frameNumber, uint32_t timeoutSec) {
        bool ret = false;
        std::unique_lock<std::mutex> l(mMutex);

        while ((mLastCompletedFrameNumber != frameNumber) &&
                !checkForFailureOrLossLocked(frameNumber)) {
            auto timeout = std::chrono::system_clock::now() +
                           std::chrono::seconds(timeoutSec);
            if (std::cv_status::timeout == mResultCondition.wait_until(l, timeout)) {
                break;
            }
        }

        if ((mLastCompletedFrameNumber == frameNumber) ||
                checkForFailureOrLossLocked(frameNumber)) {
            ret = true;
        }

        return ret;
    }

    void setRequestSave(bool enable) {
        std::unique_lock<std::mutex> l(mMutex);
        if (!enable) {
            clearSavedRequestsLocked();
        }
        mSaveCompletedRequests = enable;
    }

    // The lifecycle of returned ACaptureRequest* is still managed by CaptureResultListener
    void getCompletedRequests(std::vector<ACaptureRequest*>* out) {
        std::unique_lock<std::mutex> l(mMutex);
        *out = mCompletedRequests;
    }

    void registerPhysicalResults(size_t physicalIdCnt, const char*const* physicalOutputs) {
        std::unique_lock<std::mutex> l(mMutex);
        mRegisteredPhysicalIds.clear();
        for (size_t i = 0; i < physicalIdCnt; i++) {
            mRegisteredPhysicalIds.push_back(physicalOutputs[i]);
        }
    }

    bool checkForFailureOrLoss(int64_t frameNumber) {
        std::lock_guard<std::mutex> lock(mMutex);
        return checkForFailureOrLossLocked(frameNumber);
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mMutex);
        mLastSequenceIdCompleted = -1;
        mLastSequenceFrameNumber = -1;
        mLastCompletedFrameNumber = -1;
        mSaveCompletedRequests = false;
        clearSavedRequestsLocked();
        clearFailedLostFrameNumbersLocked();
    }

  private:
    std::mutex mMutex;
    std::condition_variable mResultCondition;
    int mLastSequenceIdCompleted = -1;
    int64_t mLastSequenceFrameNumber = -1;
    int64_t mLastCompletedFrameNumber = -1;
    std::set<int64_t> mFailedFrameNumbers, mBufferLostFrameNumbers;
    bool    mSaveCompletedRequests = false;
    std::vector<ACaptureRequest*> mCompletedRequests;
    // Registered physical camera Ids that are being requested upon.
    std::vector<std::string> mRegisteredPhysicalIds;

    void clearSavedRequestsLocked() {
        for (ACaptureRequest* req : mCompletedRequests) {
            ACaptureRequest_free(req);
        }
        mCompletedRequests.clear();
    }

    void clearFailedLostFrameNumbersLocked() {
        mFailedFrameNumbers.clear();
        mBufferLostFrameNumbers.clear();
    }

    bool checkForFailureOrLossLocked(int64_t frameNumber) {
        return (mFailedFrameNumbers.find(frameNumber) != mFailedFrameNumbers.end()) ||
                (mBufferLostFrameNumbers.find(frameNumber) != mBufferLostFrameNumbers.end());
    }
};

class ImageReaderListener {
  public:
    ImageReaderListener() {
        mBufferTs.insert(mLastBufferTs);
    }

    // count, acquire, validate, and delete AImage when a new image is available
    static void validateImageCb(void* obj, AImageReader* reader) {
        ALOGV("%s", __FUNCTION__);
        if (obj == nullptr) {
            return;
        }
        ImageReaderListener* thiz = reinterpret_cast<ImageReaderListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnImageAvailableCount++;

        AImage* img = nullptr;
        media_status_t ret = AImageReader_acquireNextImage(reader, &img);
        if (ret != AMEDIA_OK || img == nullptr) {
            ALOGE("%s: acquire image from reader %p failed! ret: %d, img %p",
                    __FUNCTION__, reader, ret, img);
            return;
        }

        // TODO: validate image content
        int32_t format = -1;
        ret = AImage_getFormat(img, &format);
        if (ret != AMEDIA_OK || format == -1) {
            ALOGE("%s: get format for image %p failed! ret: %d, format %d",
                    __FUNCTION__, img, ret, format);
        }

        // Save jpeg to SD card
        if (thiz->mDumpFilePathBase && format == AIMAGE_FORMAT_JPEG) {
            int32_t numPlanes = 0;
            ret = AImage_getNumberOfPlanes(img, &numPlanes);
            if (ret != AMEDIA_OK || numPlanes != 1) {
                ALOGE("%s: get numPlanes for image %p failed! ret: %d, numPlanes %d",
                        __FUNCTION__, img, ret, numPlanes);
                AImage_delete(img);
                return;
            }

            int32_t width = -1, height = -1;
            ret = AImage_getWidth(img, &width);
            if (ret != AMEDIA_OK || width <= 0) {
                ALOGE("%s: get width for image %p failed! ret: %d, width %d",
                        __FUNCTION__, img, ret, width);
                AImage_delete(img);
                return;
            }

            ret = AImage_getHeight(img, &height);
            if (ret != AMEDIA_OK || height <= 0) {
                ALOGE("%s: get height for image %p failed! ret: %d, height %d",
                        __FUNCTION__, img, ret, height);
                AImage_delete(img);
                return;
            }

            uint8_t* data = nullptr;
            int dataLength = 0;
            ret =  AImage_getPlaneData(img, /*planeIdx*/0, &data, &dataLength);
            if (ret != AMEDIA_OK || data == nullptr || dataLength <= 0) {
                ALOGE("%s: get jpeg data for image %p failed! ret: %d, data %p, len %d",
                        __FUNCTION__, img, ret, data, dataLength);
                AImage_delete(img);
                return;
            }

#if 0
            char dumpFilePath[512];
            sprintf(dumpFilePath, "%s/%dx%d.jpg", thiz->mDumpFilePathBase, width, height);
            ALOGI("Writing jpeg file to %s", dumpFilePath);
            FILE* file = fopen(dumpFilePath,"w+");

            if (file != nullptr) {
                fwrite(data, 1, dataLength, file);
                fflush(file);
                fclose(file);
            }
#endif
        }

        AImage_delete(img);
    }

    // count, acquire image but not delete the image
    static void acquireImageCb(void* obj, AImageReader* reader) {
        ALOGV("%s", __FUNCTION__);
        if (obj == nullptr) {
            return;
        }
        ImageReaderListener* thiz = reinterpret_cast<ImageReaderListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);
        thiz->mOnImageAvailableCount++;

        // Acquire, but not closing.
        AImage* img = nullptr;
        media_status_t ret = AImageReader_acquireNextImage(reader, &img);
        if (ret != AMEDIA_OK || img == nullptr) {
            ALOGE("%s: acquire image from reader %p failed! ret: %d, img %p",
                    __FUNCTION__, reader, ret, img);
            return;
        }
        return;
    }

    int onImageAvailableCount() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mOnImageAvailableCount;
    }

    void setDumpFilePathBase(const char* path) {
        std::lock_guard<std::mutex> lock(mMutex);
        mDumpFilePathBase = path;
    }

    // acquire image, query the corresponding timestamp but not delete the image
    static void signalImageCb(void* obj, AImageReader* reader) {
        ALOGV("%s", __FUNCTION__);
        if (obj == nullptr) {
            return;
        }
        ImageReaderListener* thiz = reinterpret_cast<ImageReaderListener*>(obj);
        std::lock_guard<std::mutex> lock(thiz->mMutex);

        AImage* img = nullptr;
        media_status_t ret = AImageReader_acquireNextImage(reader, &img);
        if (ret != AMEDIA_OK || img == nullptr) {
            ALOGE("%s: acquire image from reader %p failed! ret: %d, img %p",
                    __FUNCTION__, reader, ret, img);
            thiz->mBufferCondition.notify_one();
            return;
        }

        int64_t currentTs = -1;
        ret = AImage_getTimestamp(img, &currentTs);
        if (ret != AMEDIA_OK || currentTs == -1) {
            ALOGE("%s: acquire image from reader %p failed! ret: %d", __FUNCTION__, reader, ret);
            AImage_delete(img);
            thiz->mBufferCondition.notify_one();
            return;
        }

        thiz->mBufferTs.insert(currentTs);
        thiz->mBufferCondition.notify_one();
        return;
    }

    bool waitForNextBuffer(uint32_t timeoutSec) {
        bool ret = false;
        std::unique_lock<std::mutex> l(mMutex);

        auto it = mBufferTs.find(mLastBufferTs);
        if (it == mBufferTs.end()) {
            ALOGE("%s: Last buffer timestamp: %" PRId64 " not found!", __FUNCTION__, mLastBufferTs);
            return false;
        }
        it++;

        if (it == mBufferTs.end()) {
            auto timeout = std::chrono::system_clock::now() + std::chrono::seconds(timeoutSec);
            if (std::cv_status::no_timeout == mBufferCondition.wait_until(l, timeout)) {
                it = mBufferTs.find(mLastBufferTs);
                it++;
            }
        }

        if (it != mBufferTs.end()) {
            mLastBufferTs = *it;
            ret = true;
        }

        return ret;
    }

  private:
    // TODO: add mReader to make sure each listener is associated to one reader?
    std::mutex mMutex;
    int mOnImageAvailableCount = 0;
    const char* mDumpFilePathBase = nullptr;
    std::condition_variable mBufferCondition;
    int64_t mLastBufferTs = -1;
    std::set<int64_t> mBufferTs;
};

class StaticInfo {
  public:
    explicit StaticInfo(ACameraMetadata* chars) : mChars(chars) {}

    bool isColorOutputSupported() {
        return isCapabilitySupported(ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE);
    }

    bool isCapabilitySupported(acamera_metadata_enum_android_request_available_capabilities_t cap) {
        ACameraMetadata_const_entry entry;
        ACameraMetadata_getConstEntry(mChars, ACAMERA_REQUEST_AVAILABLE_CAPABILITIES, &entry);
        for (uint32_t i = 0; i < entry.count; i++) {
            if (entry.data.u8[i] == cap) {
                return true;
            }
        }
        return false;
    }

    int64_t getMinFrameDurationFor(int64_t format, int64_t width, int64_t height) {
        int32_t minFrameDurationTag = (format == AIMAGE_FORMAT_HEIC) ?
                ACAMERA_HEIC_AVAILABLE_HEIC_MIN_FRAME_DURATIONS :
                (format == AIMAGE_FORMAT_DEPTH_JPEG) ?
                ACAMERA_DEPTH_AVAILABLE_DYNAMIC_DEPTH_MIN_FRAME_DURATIONS :
                ACAMERA_SCALER_AVAILABLE_MIN_FRAME_DURATIONS;
        return getDurationFor(minFrameDurationTag, format, width, height);
    }

    int64_t getStallDurationFor(int64_t format, int64_t width, int64_t height) {
        int32_t stallDurationTag = (format == AIMAGE_FORMAT_HEIC) ?
                ACAMERA_HEIC_AVAILABLE_HEIC_STALL_DURATIONS : (format == AIMAGE_FORMAT_DEPTH_JPEG) ?
                ACAMERA_DEPTH_AVAILABLE_DYNAMIC_DEPTH_STALL_DURATIONS :
                ACAMERA_SCALER_AVAILABLE_STALL_DURATIONS;
        return getDurationFor(stallDurationTag, format, width, height);
    }

    bool getMaxSizeForFormat(int32_t format, int32_t *width, int32_t *height) {
        ACameraMetadata_const_entry entry;

        int32_t streamConfigTag, streamConfigOutputTag;
        switch (format) {
            case AIMAGE_FORMAT_HEIC:
                streamConfigTag = ACAMERA_HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS;
                streamConfigOutputTag = ACAMERA_HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS_OUTPUT;
                break;
            case AIMAGE_FORMAT_DEPTH_JPEG:
                streamConfigTag = ACAMERA_DEPTH_AVAILABLE_DYNAMIC_DEPTH_STREAM_CONFIGURATIONS;
                streamConfigOutputTag =
                        ACAMERA_DEPTH_AVAILABLE_DYNAMIC_DEPTH_STREAM_CONFIGURATIONS_OUTPUT;
                break;
            case AIMAGE_FORMAT_JPEG:
            case AIMAGE_FORMAT_Y8:
            default:
                streamConfigTag = ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS;
                streamConfigOutputTag = ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
                break;
        }

        bool supported = false;
        camera_status_t status = ACameraMetadata_getConstEntry(mChars, streamConfigTag, &entry);
        if (status == ACAMERA_ERROR_METADATA_NOT_FOUND) {
            return supported;
        }

       int32_t w = 0, h = 0;
        for (uint32_t i = 0; i < entry.count; i += 4) {
            if (entry.data.i32[i] == format &&
                    entry.data.i32[i+3] == streamConfigOutputTag &&
                    entry.data.i32[i+1] * entry.data.i32[i+2] > w * h) {
                w = entry.data.i32[i+1];
                h = entry.data.i32[i+2];
                supported = true;
            }
        }

        if (supported) {
            *width = w;
            *height = h;
        }
        return supported;
    }

    bool isSizeSupportedForFormat(int32_t format, int32_t width, int32_t height) {
        ACameraMetadata_const_entry entry;

        int32_t streamConfigTag, streamConfigOutputTag;
        switch (format) {
            case AIMAGE_FORMAT_HEIC:
                streamConfigTag = ACAMERA_HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS;
                streamConfigOutputTag = ACAMERA_HEIC_AVAILABLE_HEIC_STREAM_CONFIGURATIONS_OUTPUT;
                break;
            case AIMAGE_FORMAT_JPEG:
            case AIMAGE_FORMAT_Y8:
            default:
                streamConfigTag = ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS;
                streamConfigOutputTag = ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
                break;
        }

        auto ret = ACameraMetadata_getConstEntry(mChars, streamConfigTag, &entry);
        if (ret != ACAMERA_OK) {
            return false;
        }
        for (uint32_t i = 0; i < entry.count; i += 4) {
            if (entry.data.i32[i] == format &&
                    entry.data.i32[i+3] == streamConfigOutputTag &&
                    entry.data.i32[i+1] == width &&
                    entry.data.i32[i+2] == height) {
                return true;
            }
        }
        return false;
    }
  private:
    int64_t getDurationFor(uint32_t tag, int64_t format, int64_t width, int64_t height) {
        if (tag != ACAMERA_SCALER_AVAILABLE_MIN_FRAME_DURATIONS &&
                tag != ACAMERA_SCALER_AVAILABLE_STALL_DURATIONS &&
                tag != ACAMERA_DEPTH_AVAILABLE_DEPTH_MIN_FRAME_DURATIONS &&
                tag != ACAMERA_DEPTH_AVAILABLE_DEPTH_STALL_DURATIONS &&
                tag != ACAMERA_HEIC_AVAILABLE_HEIC_MIN_FRAME_DURATIONS &&
                tag != ACAMERA_HEIC_AVAILABLE_HEIC_STALL_DURATIONS &&
                tag != ACAMERA_DEPTH_AVAILABLE_DYNAMIC_DEPTH_MIN_FRAME_DURATIONS &&
                tag != ACAMERA_DEPTH_AVAILABLE_DYNAMIC_DEPTH_STALL_DURATIONS) {
            return -1;
        }
        ACameraMetadata_const_entry entry;
        ACameraMetadata_getConstEntry(mChars, tag, &entry);
        for (uint32_t i = 0; i < entry.count; i += 4) {
            if (entry.data.i64[i] == format &&
                    entry.data.i64[i+1] == width &&
                    entry.data.i64[i+2] == height) {
                return entry.data.i64[i+3];
            }
        }
        return -1;
    }
    const ACameraMetadata* mChars;
};

class PreviewTestCase {
  public:
    ~PreviewTestCase() {
        resetCamera();
        deInit();
        if (mCameraManager) {
            ACameraManager_delete(mCameraManager);
            mCameraManager = nullptr;
        }
    }

    PreviewTestCase() {
        // create is guaranteed to succeed;
        createManager();
    }

    // Free all resources except camera manager
    void resetCamera() {
        mSessionListener.reset();
        mResultListener.reset();
        if (mSession) {
            ACameraCaptureSession_close(mSession);
            mSession = nullptr;
        }
        if (mDevice) {
            ACameraDevice_close(mDevice);
            mDevice = nullptr;
        }
        if (mImgReader) {
            AImageReader_delete(mImgReader);
            // No need to call ANativeWindow_release on imageReaderAnw
            mImgReaderAnw = nullptr;
            mImgReader = nullptr;
        }
        if (mPreviewAnw) {
            ANativeWindow_release(mPreviewAnw);
            mPreviewAnw = nullptr;
        }
        if (mOutputs) {
            ACaptureSessionOutputContainer_free(mOutputs);
            mOutputs = nullptr;
        }
        if (mPreviewOutput) {
            ACaptureSessionOutput_free(mPreviewOutput);
            mPreviewOutput = nullptr;
        }
        if (mImgReaderOutput) {
            ACaptureSessionOutput_free(mImgReaderOutput);
            mImgReaderOutput = nullptr;
        }
        if (mPreviewRequest) {
            ACaptureRequest_free(mPreviewRequest);
            mPreviewRequest = nullptr;
        }
        if (mStillRequest) {
            ACaptureRequest_free(mStillRequest);
            mStillRequest = nullptr;
        }
        if (mReqPreviewOutput) {
            ACameraOutputTarget_free(mReqPreviewOutput);
            mReqPreviewOutput = nullptr;
        }
        if (mReqImgReaderOutput) {
            ACameraOutputTarget_free(mReqImgReaderOutput);
            mReqImgReaderOutput = nullptr;
        }

        mImgReaderInited = false;
        mPreviewInited = false;
    }

    camera_status_t initWithErrorLog() {
        return initWithErrorLog(nullptr /*env*/, nullptr /*jOverrideCameraId*/);
    }

    camera_status_t initWithErrorLog(JNIEnv* env, jstring jOverrideCameraId) {
        camera_status_t ret = ACameraManager_getCameraIdList(
                mCameraManager, &mCameraIdList);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Get camera id list failed: ret %d", ret);
            return ret;
        }

        if (env != nullptr && jOverrideCameraId != nullptr) {
            mOverrideCameraId = env->GetStringUTFChars(jOverrideCameraId, 0);
        }
        ret = ACameraManager_registerAvailabilityCallback(mCameraManager, &mServiceCb);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Register availability callback failed: ret %d", ret);
            return ret;
        }
        mMgrInited = true;
        mJOverrideCameraId = jOverrideCameraId;
        mJNIEnv = env;
        return ACAMERA_OK;
    }

    camera_status_t deInit () {
        if (!mMgrInited) {
            return ACAMERA_OK;
        }

        camera_status_t ret = ACameraManager_unregisterAvailabilityCallback(
                mCameraManager, &mServiceCb);
        if (ret != ACAMERA_OK) {
            ALOGE("Unregister availability callback failed: ret %d", ret);
            return ret;
        }

        if (mCameraIdList) {
            ACameraManager_deleteCameraIdList(mCameraIdList);
            mCameraIdList = nullptr;
        }
        mMgrInited = false;
        if (mOverrideCameraId != nullptr && mJNIEnv != nullptr) {
            mJNIEnv->ReleaseStringUTFChars(mJOverrideCameraId, mOverrideCameraId);
            mOverrideCameraId = nullptr;
            mJOverrideCameraId = nullptr;
            mJNIEnv = nullptr;
        }
        return ACAMERA_OK;
    }

    int getNumCameras() {
        if (!mMgrInited || !mCameraIdList) {
            return -1;
        }
        if (mOverrideCameraId != nullptr) {
            return 1;
        }
        return mCameraIdList->numCameras;
    }

    const char* getCameraId(int idx) {
        if (!mMgrInited || !mCameraIdList || idx < 0 || idx >= mCameraIdList->numCameras) {
            return nullptr;
        }
        if (mOverrideCameraId != nullptr) {
            if (idx >= 1) {
                return nullptr;
            } else {
                return mOverrideCameraId;
            }
        }
        return mCameraIdList->cameraIds[idx];
    }

    // Caller is responsible to free returned characteristics metadata
    ACameraMetadata* getCameraChars(int idx) {
        if (!mMgrInited || !mCameraIdList || idx < 0 || idx >= mCameraIdList->numCameras) {
            return nullptr;
        }
        const char* cameraId = mCameraIdList->cameraIds[idx];
        if (mOverrideCameraId != nullptr) {
            if (idx >= 1) {
                return nullptr;
            } else {
                cameraId = mOverrideCameraId;
            }
        }

        ACameraMetadata* chars;
        camera_status_t ret = ACameraManager_getCameraCharacteristics(
                mCameraManager, cameraId, &chars);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Get camera characteristics failed: ret %d", ret);
            return nullptr;
        }
        return chars;
    }

    // Caller is responsible to free returned characteristics metadata.
    ACameraMetadata* getCameraChars(const char* id) {
        if (!mMgrInited || id == nullptr) {
            return nullptr;
        }

        ACameraMetadata* chars;
        camera_status_t ret = ACameraManager_getCameraCharacteristics(mCameraManager, id, &chars);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Get camera characteristics failed: ret %d", ret);
            return nullptr;
        }
        return chars;
    }

    camera_status_t updateOutput(JNIEnv* env, ACaptureSessionOutput *output) {
        if (mSession == nullptr) {
            ALOGE("Testcase cannot update output configuration session %p",
                    mSession);
            return ACAMERA_ERROR_UNKNOWN;
        }

        return ACameraCaptureSession_updateSharedOutput(mSession, output);
    }

    camera_status_t openCamera(const char* cameraId) {
        if (mDevice) {
            ALOGE("Cannot open camera before closing previously open one");
            return ACAMERA_ERROR_INVALID_PARAMETER;
        }
        mCameraId = cameraId;
        return ACameraManager_openCamera(mCameraManager, cameraId, &mDeviceCb, &mDevice);
    }

    camera_status_t closeCamera() {
        camera_status_t ret = ACameraDevice_close(mDevice);
        mDevice = nullptr;
        return ret;
    }

    bool isCameraAvailable(const char* cameraId) {
        if (!mMgrInited) {
            ALOGE("Camera service listener has not been registered!");
        }
        return mServiceListener.isAvailable(cameraId);
    }

    media_status_t initImageReaderWithErrorLog(
            int32_t width, int32_t height, int32_t format, int32_t maxImages,
            AImageReader_ImageListener* listener) {
        if (mImgReader || mImgReaderAnw) {
            LOG_ERROR(errorString, "Cannot init image reader before closing existing one");
            return AMEDIA_ERROR_UNKNOWN;
        }

        media_status_t ret = initImageReaderWithErrorLog(
                width, height, format, maxImages, listener, &mImgReader,
                &mImgReaderAnw);
        if (ret != AMEDIA_OK) {
            return ret;
        }

        mImgReaderInited = true;
        return AMEDIA_OK;
    }

    media_status_t initImageReaderWithErrorLog(
            int32_t width, int32_t height, int32_t format, int32_t maxImages,
            AImageReader_ImageListener* listener, AImageReader **imgReader,
            ANativeWindow **imgReaderAnw) {

        media_status_t ret = AImageReader_new(
                width, height, format,
                maxImages, imgReader);
        if (ret != AMEDIA_OK) {
            LOG_ERROR(errorString, "Create image reader. ret %d", ret);
            return ret;
        }
        if (*imgReader == nullptr) {
            LOG_ERROR(errorString, "null image reader created");
            return AMEDIA_ERROR_UNKNOWN;
        }

        ret = AImageReader_setImageListener(*imgReader, listener);
        if (ret != AMEDIA_OK) {
            LOG_ERROR(errorString, "Set AImageReader listener failed. ret %d", ret);
            return ret;
        }

        ret = AImageReader_getWindow(*imgReader, imgReaderAnw);
        if (ret != AMEDIA_OK) {
            LOG_ERROR(errorString, "AImageReader_getWindow failed. ret %d", ret);
            return ret;
        }
        if (*imgReaderAnw == nullptr) {
            LOG_ERROR(errorString, "Null ANW from AImageReader!");
            return AMEDIA_ERROR_UNKNOWN;
        }
        return AMEDIA_OK;
    }

    ANativeWindow* initPreviewAnw(JNIEnv* env, jobject jSurface) {
        if (mPreviewAnw) {
            ALOGE("Cannot init preview twice!");
            return nullptr;
        }
        mPreviewAnw =  ANativeWindow_fromSurface(env, jSurface);
        mPreviewInited = true;
        return mPreviewAnw;
    }

    camera_status_t createCaptureSessionWithLog(bool isPreviewShared = false,
            ACaptureRequest *sessionParameters = nullptr) {
        std::vector<ACaptureSessionOutput*> extraOutputs;
        return createCaptureSessionWithLog(extraOutputs, isPreviewShared, sessionParameters);
    }

    camera_status_t createCaptureSessionOutputContainer(
            const std::vector<ACaptureSessionOutput*> extraOutputs,
            ACaptureSessionOutputContainer** outputs,
            bool isPreviewShared = false, ACaptureRequest *sessionParameters = nullptr) {
        if (!mMgrInited || (!mImgReaderInited && !mPreviewInited) || !outputs) {
            LOG_ERROR(errorString, "Cannot create session output container. mgrInit %d "
                    "readerInit %d previewInit %d outputs %p",
                    mMgrInited, mImgReaderInited, mPreviewInited, outputs);
            return ACAMERA_ERROR_UNKNOWN;
        }

        camera_status_t ret = ACaptureSessionOutputContainer_create(outputs);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Create capture session output container failed. ret %d", ret);
            return ret;
        }

        if (mImgReaderInited) {
            ret = ACaptureSessionOutput_create(mImgReaderAnw, &mImgReaderOutput);
            if (ret != ACAMERA_OK || mImgReaderOutput == nullptr) {
                LOG_ERROR(errorString,
                        "Session image reader output create fail! ret %d output %p",
                        ret, mImgReaderOutput);
                if (ret == ACAMERA_OK) {
                    ret = ACAMERA_ERROR_UNKNOWN; // ret OK but output is null
                }
                return ret;
            }

            ret = ACaptureSessionOutputContainer_add(*outputs, mImgReaderOutput);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Session image reader output add failed! ret %d", ret);
                return ret;
            }
        }

        for (auto extraOutput : extraOutputs) {
            ret = ACaptureSessionOutputContainer_add(*outputs, extraOutput);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Session image reader output add failed! ret %d", ret);
                return ret;
            }
        }

        if (mPreviewInited) {
            if (isPreviewShared) {
                ret = ACaptureSessionSharedOutput_create(mPreviewAnw, &mPreviewOutput);
            } else {
                ret = ACaptureSessionOutput_create(mPreviewAnw, &mPreviewOutput);
            }
            if (ret != ACAMERA_OK || mPreviewOutput == nullptr) {
                LOG_ERROR(errorString,
                        "Session preview output create fail! ret %d output %p",
                        ret, mPreviewOutput);
                if (ret == ACAMERA_OK) {
                    ret = ACAMERA_ERROR_UNKNOWN; // ret OK but output is null
                }
                return ret;
            }

            ret = ACaptureSessionOutputContainer_add(*outputs, mPreviewOutput);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Session preview output add failed! ret %d", ret);
                return ret;
            }
        }
        return ret;
    }

    camera_status_t createCaptureSessionWithLog(
            const std::vector<ACaptureSessionOutput*> extraOutputs,
            bool isPreviewShared = false, ACaptureRequest *sessionParameters = nullptr,
            bool sessionConfigurationDefault = true) {
        if (mSession) {
            LOG_ERROR(errorString, "Cannot create session before closing existing one");
            return ACAMERA_ERROR_UNKNOWN;
        }

        camera_status_t ret = createCaptureSessionOutputContainer(
                extraOutputs, &mOutputs, isPreviewShared, sessionParameters);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Failed to create session output container! ret %d", ret);
            return ret;
        }

        ret = ACameraDevice_isSessionConfigurationSupported(mDevice, mOutputs);
        if (ret != ACAMERA_OK && ret != ACAMERA_ERROR_UNSUPPORTED_OPERATION &&
                ret != ACAMERA_ERROR_STREAM_CONFIGURE_FAIL) {
            LOG_ERROR(errorString, "isSessionConfigurationSupported must return either OK "
                    ", UNSUPPORTED_OPERATION, or STREAM_CONFIGURE_FAIL, but returns %d", ret);
            return ret;
        }

        // If session configuration is not supported by default, return early
        // when camera device doesn't explicitly claim support.
        if (ret != ACAMERA_OK && !sessionConfigurationDefault) {
            return ret;
        }

        ret = ACameraDevice_createCaptureSessionWithSessionParameters(
                mDevice, mOutputs, sessionParameters, &mSessionCb, &mSession);
        if (ret != ACAMERA_OK || mSession == nullptr) {
            LOG_ERROR(errorString, "Create session for camera %s failed. ret %d session %p",
                    mCameraId, ret, mSession);
            if (ret == ACAMERA_OK) {
                ret = ACAMERA_ERROR_UNKNOWN; // ret OK but session is null
            }
            return ret;
        }

        return ACAMERA_OK;
    }

    void closeSession() {
        if (mSession != nullptr) {
            ACameraCaptureSession_close(mSession);
        }
        if (mOutputs) {
            ACaptureSessionOutputContainer_free(mOutputs);
            mOutputs = nullptr;
        }
        if (mPreviewOutput) {
            ACaptureSessionOutput_free(mPreviewOutput);
            mPreviewOutput = nullptr;
        }
        if (mImgReaderOutput) {
            ACaptureSessionOutput_free(mImgReaderOutput);
            mImgReaderOutput = nullptr;
        }
        mSession = nullptr;
    }

    camera_status_t createRequestsWithErrorLog() {
        std::vector<ACameraOutputTarget*> extraOutputs;
        return createRequestsWithErrorLog(extraOutputs);
    }

    camera_status_t createRequestsWithErrorLog(
                const std::vector<ACameraOutputTarget*> extraOutputs,
                const ACameraIdList* physicalCameraIdList = nullptr) {
        if (mPreviewRequest || mStillRequest) {
            LOG_ERROR(errorString, "Cannot create requests before deleteing existing one");
            return ACAMERA_ERROR_UNKNOWN;
        }

        if (mDevice == nullptr || (!mPreviewInited && !mImgReaderInited)) {
            LOG_ERROR(errorString,
                    "Cannot create request. device %p previewInit %d readeInit %d",
                    mDevice, mPreviewInited, mImgReaderInited);
            return ACAMERA_ERROR_UNKNOWN;
        }

        camera_status_t ret;
        bool usePhysicalSettings = (physicalCameraIdList != nullptr);
        if (mPreviewInited) {
            if (!usePhysicalSettings) {
                ret = ACameraDevice_createCaptureRequest(
                        mDevice, TEMPLATE_PREVIEW, &mPreviewRequest);
            } else {
                ret = ACameraDevice_createCaptureRequest_withPhysicalIds(
                        mDevice, TEMPLATE_PREVIEW, physicalCameraIdList, &mPreviewRequest);
            }
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Camera %s create preview request failed. ret %d",
                        mCameraId, ret);
                return ret;
            }

            if (usePhysicalSettings) {
                for (int i = 0; i < physicalCameraIdList->numCameras; i++) {
                    // Check physical camera specific metadata functions.
                    uint8_t aeMode = ACAMERA_CONTROL_AE_MODE_ON;
                    ret = ACaptureRequest_setEntry_physicalCamera_u8(mPreviewRequest,
                            physicalCameraIdList->cameraIds[i], ACAMERA_CONTROL_AE_MODE,
                            1 /*count*/, &aeMode);
                    if (ret != ACAMERA_OK) {
                        LOG_ERROR(errorString,
                            "Error: Camera %s update AE mode key fail. ret %d",
                            physicalCameraIdList->cameraIds[i], ret);
                        return ret;
                    }

                    ACameraMetadata_const_entry entry;
                    ret = ACaptureRequest_getConstEntry_physicalCamera(mPreviewRequest,
                                physicalCameraIdList->cameraIds[i],
                                ACAMERA_CONTROL_AE_MODE, &entry);
                    if (ret != ACAMERA_OK) {
                        LOG_ERROR(errorString, "Get AE mode key for physicalCamera %s failed."
                                " ret %d", physicalCameraIdList->cameraIds[i], ret);
                        return ret;
                    }
                    if (entry.data.u8[0] != aeMode) {
                        LOG_ERROR(errorString,
                            "Error: AE mode key is not updated. expect %d but get %d",
                            aeMode, entry.data.u8[0]);
                        return ACAMERA_ERROR_UNKNOWN;
                    }
                }
            }

            ret = ACameraOutputTarget_create(mPreviewAnw, &mReqPreviewOutput);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString,
                        "Camera %s create request preview output target failed. ret %d",
                        mCameraId, ret);
                return ret;
            }

            ret = ACaptureRequest_addTarget(mPreviewRequest, mReqPreviewOutput);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Camera %s add preview request output failed. ret %d",
                        mCameraId, ret);
                return ret;
            }

            // Add extraOutputs to the request
            for (auto extraOutput : extraOutputs) {
                ret = ACaptureRequest_addTarget(mPreviewRequest, extraOutput);
                if (ret != ACAMERA_OK) {
                    LOG_ERROR(errorString, "Camera %s add extra request output failed. ret %d",
                            mCameraId, ret);
                    return ret;
                }
            }
        } else {
            ALOGI("Preview not inited. Will not create preview request!");
        }

        if (mImgReaderInited) {
            ret = ACameraDevice_createCaptureRequest(
                    mDevice, TEMPLATE_STILL_CAPTURE, &mStillRequest);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Camera %s create still request failed. ret %d",
                        mCameraId, ret);
                return ret;
            }

            ret = ACameraOutputTarget_create(mImgReaderAnw, &mReqImgReaderOutput);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString,
                        "Camera %s create request reader output target failed. ret %d",
                        mCameraId, ret);
                return ret;
            }

            ret = ACaptureRequest_addTarget(mStillRequest, mReqImgReaderOutput);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Camera %s add still request output failed. ret %d",
                        mCameraId, ret);
                return ret;
            }

            if (mPreviewInited) {
                ret = ACaptureRequest_addTarget(mStillRequest, mReqPreviewOutput);
                if (ret != ACAMERA_OK) {
                    LOG_ERROR(errorString,
                            "Camera %s add still request preview output failed. ret %d",
                            mCameraId, ret);
                    return ret;
                }
            }
        } else {
            ALOGI("AImageReader not inited. Will not create still request!");
        }

        return ACAMERA_OK;
    }

    // The output ACaptureRequest* is still managed by testcase class
    camera_status_t getStillRequest(ACaptureRequest** out) {
        if (mStillRequest == nullptr) {
            ALOGE("Camera %s Still capture request hasn't been created", mCameraId);
            return ACAMERA_ERROR_INVALID_PARAMETER;
        }
        *out = mStillRequest;
        return ACAMERA_OK;
    }

    camera_status_t getPreviewRequest(ACaptureRequest** out) {
        if (mPreviewRequest == nullptr) {
            ALOGE("Camera %s Preview capture request hasn't been created", mCameraId);
            return ACAMERA_ERROR_INVALID_PARAMETER;
        }
        *out = mPreviewRequest;
        return ACAMERA_OK;
    }

    camera_status_t startPreview(int *sequenceId = nullptr, size_t physicalIdCnt = 0,
            const char*const* extraPhysicalOutputs = nullptr) {
        if (mSession == nullptr || mPreviewRequest == nullptr) {
            ALOGE("Testcase cannot start preview: session %p, preview request %p",
                    mSession, mPreviewRequest);
            return ACAMERA_ERROR_UNKNOWN;
        }
        int previewSeqId;
        camera_status_t ret;
        if (sequenceId == nullptr) {
            ret = ACameraCaptureSession_setRepeatingRequest(
                   mSession, nullptr, 1, &mPreviewRequest, &previewSeqId);
        } else if (physicalIdCnt == 0) {
            ret = ACameraCaptureSession_setRepeatingRequest(
                   mSession, &mResultCb, 1, &mPreviewRequest, sequenceId);
        } else {
            if (extraPhysicalOutputs == nullptr) {
                ALOGE("Testcase missing valid physical camera Ids for logical camera");
                return ACAMERA_ERROR_INVALID_PARAMETER;
            }
            CaptureResultListener* resultListener =
                    static_cast<CaptureResultListener*>(mLogicalCameraResultCb.context);
            resultListener->registerPhysicalResults(physicalIdCnt, extraPhysicalOutputs);
            ret = ACameraCaptureSession_logicalCamera_setRepeatingRequest(
                    mSession, &mLogicalCameraResultCb, 1, &mPreviewRequest, sequenceId);
        }
        return ret;
    }

    camera_status_t startRepeatingRequest(int *sequenceId, ACaptureRequest *request,
            ACameraCaptureSession_captureCallbacks *resultCb) {
        if (mSession == nullptr || request == nullptr || resultCb == nullptr) {
            ALOGE("Testcase cannot start repeating request: session %p, request %p resultCb %p",
                    mSession, request, resultCb);
            return ACAMERA_ERROR_UNKNOWN;
        }

        return ACameraCaptureSession_setRepeatingRequest(mSession, resultCb, 1, &request,
                sequenceId);
    }

    camera_status_t stopPreview() {
        if (mSession == nullptr) {
            ALOGE("Testcase cannot stop preview: session %p", mSession);
            return ACAMERA_ERROR_UNKNOWN;
        }
        return ACameraCaptureSession_stopRepeating(mSession);
    }

    camera_status_t updateRepeatingRequest(ACaptureRequest *updatedRequest,
            int *sequenceId = nullptr) {
        if (mSession == nullptr || updatedRequest == nullptr) {
            ALOGE("Testcase cannot update repeating request: session %p, updated request %p",
                    mSession, updatedRequest);
            return ACAMERA_ERROR_UNKNOWN;
        }

        int previewSeqId;
        camera_status_t ret;
        if (sequenceId == nullptr) {
            ret = ACameraCaptureSession_setRepeatingRequest(
                    mSession, nullptr, 1, &updatedRequest, &previewSeqId);
        } else {
            ret = ACameraCaptureSession_setRepeatingRequest(
                    mSession, &mResultCb, 1, &updatedRequest, sequenceId);
        }
        return ret;
    }

    int64_t getCaptureSequenceLastFrameNumber(int64_t sequenceId, uint32_t timeoutSec) {
        return mResultListener.getCaptureSequenceLastFrameNumber(sequenceId, timeoutSec);
    }

    bool waitForFrameNumber(int64_t frameNumber, uint32_t timeoutSec) {
        return mResultListener.waitForFrameNumber(frameNumber, timeoutSec);
    }

    camera_status_t takePicture() {
        if (mSession == nullptr || mStillRequest == nullptr) {
            ALOGE("Testcase cannot take picture: session %p, still request %p",
                    mSession, mStillRequest);
            return ACAMERA_ERROR_UNKNOWN;
        }
        int seqId;
        return ACameraCaptureSession_capture(
                mSession, nullptr, 1, &mStillRequest, &seqId);
    }

    camera_status_t capture(ACaptureRequest* request,
            ACameraCaptureSession_captureCallbacks* listener,
            /*out*/int* seqId) {
        if (mSession == nullptr || request == nullptr) {
            ALOGE("Testcase cannot capture session: session %p, request %p",
                    mSession, request);
            return ACAMERA_ERROR_UNKNOWN;
        }
        return ACameraCaptureSession_capture(
                mSession, listener, 1, &request, seqId);
    }

    camera_status_t resetWithErrorLog() {
        camera_status_t ret;

        closeSession();

        for (int i = 0; i < 50; i++) {
            usleep(100000); // sleep 100ms
            if (mSessionListener.isClosed()) {
                ALOGI("Session take ~%d ms to close", i*100);
                break;
            }
        }

        if (!mSessionListener.isClosed() || mSessionListener.onClosedCount() != 1) {
            LOG_ERROR(errorString,
                    "Session for camera %s close error. isClosde %d close count %d",
                    mCameraId, mSessionListener.isClosed(), mSessionListener.onClosedCount());
            return ACAMERA_ERROR_UNKNOWN;
        }
        mSessionListener.reset();
        mResultListener.reset();

        ret = closeCamera();
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Close camera device %s failure. ret %d", mCameraId, ret);
            return ret;
        }

        resetCamera();
        return ACAMERA_OK;
    }

    CaptureSessionListener* getSessionListener() {
        return &mSessionListener;
    }

    ACameraDevice* getCameraDevice() {
        return mDevice;
    }

    ACaptureSessionOutput *getPreviewOutput() {
        return mPreviewOutput;
    }

  private:
    ACameraManager* createManager() {
        if (!mCameraManager) {
            mCameraManager = ACameraManager_create();
        }
        return mCameraManager;
    }

    CameraServiceListener mServiceListener;
    ACameraManager_AvailabilityCallbacks mServiceCb {
        &mServiceListener,
        CameraServiceListener::onAvailable,
        CameraServiceListener::onUnavailable
    };
    CameraDeviceListener mDeviceListener;
    ACameraDevice_StateCallbacks mDeviceCb {
        &mDeviceListener,
        CameraDeviceListener::onDisconnected,
        CameraDeviceListener::onError
    };
    CaptureSessionListener mSessionListener;
    ACameraCaptureSession_stateCallbacks mSessionCb {
        &mSessionListener,
        CaptureSessionListener::onClosed,
        CaptureSessionListener::onReady,
        CaptureSessionListener::onActive
    };

    CaptureResultListener mResultListener;
    ACameraCaptureSession_captureCallbacks mResultCb {
        &mResultListener,
        CaptureResultListener::onCaptureStart,
        CaptureResultListener::onCaptureProgressed,
        CaptureResultListener::onCaptureCompleted,
        CaptureResultListener::onCaptureFailed,
        CaptureResultListener::onCaptureSequenceCompleted,
        CaptureResultListener::onCaptureSequenceAborted,
        CaptureResultListener::onCaptureBufferLost
    };

    ACameraCaptureSession_logicalCamera_captureCallbacks mLogicalCameraResultCb {
        &mResultListener,
        CaptureResultListener::onCaptureStart,
        CaptureResultListener::onCaptureProgressed,
        CaptureResultListener::onLogicalCameraCaptureCompleted,
        CaptureResultListener::onLogicalCameraCaptureFailed,
        CaptureResultListener::onCaptureSequenceCompleted,
        CaptureResultListener::onCaptureSequenceAborted,
        CaptureResultListener::onCaptureBufferLost
    };

    ACameraIdList* mCameraIdList = nullptr;
    ACameraDevice* mDevice = nullptr;
    AImageReader* mImgReader = nullptr;
    ANativeWindow* mImgReaderAnw = nullptr;
    ANativeWindow* mPreviewAnw = nullptr;
    ACameraManager* mCameraManager = nullptr;
    ACaptureSessionOutputContainer* mOutputs = nullptr;
    ACaptureSessionOutput* mPreviewOutput = nullptr;
    ACaptureSessionOutput* mImgReaderOutput = nullptr;
    ACameraCaptureSession* mSession = nullptr;
    ACaptureRequest* mPreviewRequest = nullptr;
    ACaptureRequest* mStillRequest = nullptr;
    ACameraOutputTarget* mReqPreviewOutput = nullptr;
    ACameraOutputTarget* mReqImgReaderOutput = nullptr;
    const char* mCameraId;
    JNIEnv* mJNIEnv = nullptr;
    jstring mJOverrideCameraId;
    const char* mOverrideCameraId = nullptr;

    bool mMgrInited = false; // cameraId, serviceListener
    bool mImgReaderInited = false;
    bool mPreviewInited = false;
};

jint throwAssertionError(JNIEnv* env, const char* message)
{
    jclass assertionClass;
    const char* className = "junit/framework/AssertionFailedError";

    assertionClass = env->FindClass(className);
    if (assertionClass == nullptr) {
        ALOGE("Native throw error: cannot find class %s", className);
        return -1;
    }
    return env->ThrowNew(assertionClass, message);
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraManagerTest_\
testCameraManagerGetAndCloseNative(
        JNIEnv* env, jclass /*clazz*/) {
    bool pass = false;
    ALOGV("%s", __FUNCTION__);
    ACameraManager* cameraManager2 = nullptr;
    ACameraManager* cameraManager3 = nullptr;
    ACameraManager* cameraManager4 = nullptr;
    camera_status_t ret = ACAMERA_OK;
    ACameraManager* cameraManager = ACameraManager_create();
    if (cameraManager == nullptr) {
        LOG_ERROR(errorString, "ACameraManager_create returns nullptr");
        goto cleanup;
    }
    ACameraManager_delete(cameraManager);
    cameraManager = nullptr;

    // Test get/close multiple instances
    cameraManager = ACameraManager_create();
    cameraManager2 = ACameraManager_create();
    if (cameraManager2 == nullptr) {
        LOG_ERROR(errorString, "ACameraManager_create 2 returns nullptr");
        goto cleanup;
    }
    ACameraManager_delete(cameraManager);
    cameraManager = nullptr;
    cameraManager3 = ACameraManager_create();
    if (cameraManager3 == nullptr) {
        LOG_ERROR(errorString, "ACameraManager_create 3 returns nullptr");
        goto cleanup;
    }
    cameraManager4 = ACameraManager_create();
        if (cameraManager4 == nullptr) {
        LOG_ERROR(errorString, "ACameraManager_create 4 returns nullptr");
        goto cleanup;
    }
    ACameraManager_delete(cameraManager3);
    ACameraManager_delete(cameraManager2);
    ACameraManager_delete(cameraManager4);

    pass = true;
cleanup:
    if (cameraManager) {
        ACameraManager_delete(cameraManager);
    }
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "fail");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraManagerTest_\
testCameraManagerGetCameraIdsNative(
        JNIEnv* env, jclass /*clazz*/) {
    ALOGV("%s", __FUNCTION__);
    bool pass = false;
    ACameraManager* mgr = ACameraManager_create();
    ACameraIdList *cameraIdList = nullptr;
    camera_status_t ret = ACameraManager_getCameraIdList(mgr, &cameraIdList);
    if (ret != ACAMERA_OK || cameraIdList == nullptr) {
        LOG_ERROR(errorString, "Get camera id list failed: ret %d, cameraIdList %p",
                ret, cameraIdList);
        goto cleanup;
    }
    ALOGI("Number of cameras: %d", cameraIdList->numCameras);
    for (int i = 0; i < cameraIdList->numCameras; i++) {
        ALOGI("Camera ID: %s", cameraIdList->cameraIds[i]);
    }
    ACameraManager_deleteCameraIdList(cameraIdList);
    cameraIdList = nullptr;

    pass = true;
cleanup:
    if (mgr) {
        ACameraManager_delete(mgr);
    }
    if (cameraIdList) {
        ACameraManager_deleteCameraIdList(cameraIdList);
    }
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "fail");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraManagerTest_\
testCameraManagerExtendedAvailabilityCallbackNative(
        JNIEnv* env, jclass /*clazz*/) {
    ALOGV("%s", __FUNCTION__);
    bool pass = false;
    ACameraManager* mgr = ACameraManager_create();
    ACameraIdList *cameraIdList = nullptr;
    camera_status_t ret = ACameraManager_getCameraIdList(mgr, &cameraIdList);
    int numCameras = cameraIdList->numCameras;
    CameraServiceListener listener;
    CameraServiceListener::StringPairSet unavailablePhysicalCameras;
    CameraServiceListener::StringPairSet physicalCameraIdPairs;
    ACameraManager_ExtendedAvailabilityCallbacks cbs {
            {
                &listener,
                CameraServiceListener::onAvailable,
                CameraServiceListener::onUnavailable
            },
            CameraServiceListener::onCameraAccessPrioritiesChanged,
            CameraServiceListener::onPhysicalCameraAvailable,
            CameraServiceListener::onPhysicalCameraUnavailable,
            {}
    };

    ret = ACameraManager_registerExtendedAvailabilityCallback(mgr, &cbs);
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Register extended availability callback failed: ret %d", ret);
        goto cleanup;
    }
    sleep(1); // sleep a second to give some time for callbacks to happen

    // Should at least get onAvailable for each camera once
    if (listener.getAvailableCount() < numCameras) {
        LOG_ERROR(errorString, "Expect at least %d available callback but only got %d",
                numCameras, listener.getAvailableCount());
        goto cleanup;
    }

    {
        int availablePhysicalCamera = listener.getPhysicalCameraAvailableCount();
        if (availablePhysicalCamera > 0) {
            LOG_ERROR(errorString, "Expect no available callback, but got %d",
                    availablePhysicalCamera);
        }
    }

    unavailablePhysicalCameras = listener.getUnavailablePhysicalCameras();
    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = cameraIdList->cameraIds[i];
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }
        ACameraMetadata* c;
        ret = ACameraManager_getCameraCharacteristics(mgr, cameraId, &c);
        if (ret != ACAMERA_OK || c == nullptr) {
            LOG_ERROR(errorString, "Get camera %s characteristics failure", cameraId);
            goto cleanup;
        }
        std::unique_ptr<ACameraMetadata> chars(c);

        size_t physicalCameraCnt = 0;
        const char *const* physicalCameraIds = nullptr;
        if (!ACameraMetadata_isLogicalMultiCamera(
                chars.get(), &physicalCameraCnt, &physicalCameraIds)) {
            continue;
        }
        for (size_t j = 0; j < physicalCameraCnt; j++) {
            physicalCameraIdPairs.emplace(cameraId, physicalCameraIds[j]);
        }
    }
    for (const auto& unavailIdPair : unavailablePhysicalCameras) {
        bool validPair = false;
        for (const auto& idPair : physicalCameraIdPairs) {
            if (idPair.first == unavailIdPair.first && idPair.second == unavailIdPair.second) {
                validPair = true;
                break;
            }
        }
        if (!validPair) {
            LOG_ERROR(errorString, "Expect valid unavailable physical cameras, but got %s : %s",
                    unavailIdPair.first.c_str(), unavailIdPair.second.c_str());
            goto cleanup;
        }
    }

    ret = ACameraManager_unregisterExtendedAvailabilityCallback(mgr, &cbs);
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Unregister extended availability callback failed: ret %d", ret);
        goto cleanup;
    }
    pass = true;
cleanup:
    if (cameraIdList) {
        ACameraManager_deleteCameraIdList(cameraIdList);
    }
    if (mgr) {
        ACameraManager_delete(mgr);
    }
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraManagerTest_\
testCameraManagerAvailabilityCallbackNative(
        JNIEnv* env, jclass /*clazz*/) {
    ALOGV("%s", __FUNCTION__);
    bool pass = false;
    ACameraManager* mgr = ACameraManager_create();
    ACameraIdList *cameraIdList = nullptr;
    camera_status_t ret = ACameraManager_getCameraIdList(mgr, &cameraIdList);
    int numCameras = cameraIdList->numCameras;
    CameraServiceListener listener;
    ACameraManager_AvailabilityCallbacks cbs {
            &listener,
            CameraServiceListener::onAvailable,
            CameraServiceListener::onUnavailable};
    ret = ACameraManager_registerAvailabilityCallback(mgr, &cbs);
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Register availability callback failed: ret %d", ret);
        goto cleanup;
    }
    sleep(1); // sleep a second to give some time for callbacks to happen

    // Should at least get onAvailable for each camera once
    if (listener.getAvailableCount() < numCameras) {
        LOG_ERROR(errorString, "Expect at least %d available callback but only got %d",
                numCameras, listener.getAvailableCount());
        goto cleanup;
    }

    ret = ACameraManager_unregisterAvailabilityCallback(mgr, &cbs);
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Unregister availability callback failed: ret %d", ret);
        goto cleanup;
    }
    pass = true;
cleanup:
    if (cameraIdList) {
        ACameraManager_deleteCameraIdList(cameraIdList);
    }
    if (mgr) {
        ACameraManager_delete(mgr);
    }
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraManagerTest_\
testCameraManagerCharacteristicsNative(
        JNIEnv* env, jclass /*clazz*/) {
    ALOGV("%s", __FUNCTION__);
    bool pass = false;
    ACameraManager* mgr = ACameraManager_create();
    ACameraIdList *cameraIdList = nullptr;
    ACameraMetadata* chars = nullptr;
    ACameraMetadata* copy = nullptr;
    int numCameras = 0;
    camera_status_t ret = ACameraManager_getCameraIdList(mgr, &cameraIdList);
    if (ret != ACAMERA_OK || cameraIdList == nullptr) {
        LOG_ERROR(errorString, "Get camera id list failed: ret %d, cameraIdList %p",
                ret, cameraIdList);
        goto cleanup;
    }
    numCameras = cameraIdList->numCameras;

    for (int i = 0; i < numCameras; i++) {
        ret = ACameraManager_getCameraCharacteristics(
                mgr, cameraIdList->cameraIds[i], &chars);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Get camera characteristics failed: ret %d", ret);
            goto cleanup;
        }

        int32_t numTags = 0;
        const uint32_t* tags = nullptr;
        ret = ACameraMetadata_getAllTags(chars, &numTags, &tags);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Get camera characteristics tags failed: ret %d", ret);
            goto cleanup;
        }

        for (int tid = 0; tid < numTags; tid++) {
            uint32_t tagId = tags[tid];
            ALOGV("%s camera characteristics contains key %u", __FUNCTION__, tagId);
            uint32_t sectionId = tagId >> 16;
            if (sectionId >= ACAMERA_SECTION_COUNT && sectionId < ACAMERA_VENDOR) {
                LOG_ERROR(errorString, "Unknown tagId %u, sectionId %u", tagId, sectionId);
                goto cleanup;
            }
        }

        ACameraMetadata_const_entry entry;
        ret = ACameraMetadata_getConstEntry(chars, ACAMERA_REQUEST_AVAILABLE_CAPABILITIES, &entry);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Get const available capabilities key failed. ret %d", ret);
            goto cleanup;
        }

        // Check the entry is actually legit
        if (entry.tag != ACAMERA_REQUEST_AVAILABLE_CAPABILITIES ||
                entry.count == 0 || entry.type != ACAMERA_TYPE_BYTE || entry.data.i32 == nullptr) {
            LOG_ERROR(errorString,
                    "Bad available capabilities key: tag: %d (expected %d), count %u (expect > 0), "
                    "type %d (expected %d), data %p (expected not null)",
                    entry.tag, ACAMERA_REQUEST_AVAILABLE_CAPABILITIES, entry.count,
                    entry.type, ACAMERA_TYPE_BYTE, entry.data.i32);
            goto cleanup;
        }
        // All camera supports BC except depth only cameras
        bool supportBC = false, supportDepth = false;
        for (uint32_t i = 0; i < entry.count; i++) {
            if (entry.data.u8[i] == ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE) {
                supportBC = true;
            }
            if (entry.data.u8[i] == ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT) {
                supportDepth = true;
            }
        }
        if (!(supportBC || supportDepth)) {
            LOG_ERROR(errorString, "Error: camera device %s does not support either BC or DEPTH",
                    cameraIdList->cameraIds[i]);
            goto cleanup;
        }

        // Check copy works
        copy = ACameraMetadata_copy(chars);

        // Compare copy with original
        ACameraMetadata_const_entry entryCopy;
        ret = ACameraMetadata_getConstEntry(
                copy, ACAMERA_REQUEST_AVAILABLE_CAPABILITIES, &entryCopy);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Get const available capabilities key failed. ret %d", ret);
            goto cleanup;
        }
        for (uint32_t i = 0; i < entry.count; i++) {
            if (entry.data.u8[i] != entryCopy.data.u8[i]) {
                LOG_ERROR(errorString,
                    "Copy of available capability key[%d]: %d mismatches original %d",
                    i, entryCopy.data.u8[i], entry.data.u8[i]);
                goto cleanup;
            }
        }

        // Check get unknown value fails
        uint32_t badTag = (uint32_t) ACAMERA_VENDOR_START - 1;
        ret = ACameraMetadata_getConstEntry(chars, badTag, &entry);
        if (ret == ACAMERA_OK) {
            LOG_ERROR(errorString, "Error: get unknown tag should fail!");
            goto cleanup;
        }

        ACameraMetadata_free(chars);
        ACameraMetadata_free(copy);
        chars = nullptr;
        copy = nullptr;
    }

    pass = true;
cleanup:
    if (chars) {
        ACameraMetadata_free(chars);
    }
    if (copy) {
        ACameraMetadata_free(copy);
    }
    ACameraManager_deleteCameraIdList(cameraIdList);
    ACameraManager_delete(mgr);
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDeviceOpenAndCloseNative(
        JNIEnv* env, jclass /*clazz*/, jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    int numCameras = 0;
    bool pass = false;
    PreviewTestCase testCase;

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto cleanup;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negavtive number of cameras: %d", numCameras);
        goto cleanup;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto cleanup;
        }

        ret = testCase.closeCamera();
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Close camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto cleanup;
        }
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto cleanup;
    }

    pass = true;
cleanup:
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDeviceCreateCaptureRequestNative(
        JNIEnv* env, jclass /*clazz*/, jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    bool pass = false;
    ACameraManager* mgr = ACameraManager_create();
    ACameraIdList* cameraIdList = nullptr;
    ACameraDevice* device = nullptr;
    ACaptureRequest* request = nullptr;
    ACameraMetadata* chars = nullptr;
    camera_status_t ret = ACameraManager_getCameraIdList(mgr, &cameraIdList);

    int numCameras = cameraIdList->numCameras;
    const char* overrideCameraId = nullptr;
    if (jOverrideCameraId != nullptr) {
        overrideCameraId = env->GetStringUTFChars(jOverrideCameraId, nullptr);
    }

    for (int i = 0; i < numCameras; i++) {
        CameraDeviceListener deviceListener;
        const char* cameraId = cameraIdList->cameraIds[i];
        if (overrideCameraId != nullptr && strcmp(overrideCameraId, cameraId)) {
            // Skip other cameras if overriding camera id to be tested.
            continue;
        }
        ACameraDevice_StateCallbacks deviceCb {
            &deviceListener,
            CameraDeviceListener::onDisconnected,
            CameraDeviceListener::onError
        };
        ret = ACameraManager_openCamera(mgr, cameraId, &deviceCb, &device);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        ret = ACameraManager_getCameraCharacteristics(mgr, cameraId, &chars);
        if (ret != ACAMERA_OK || chars == nullptr) {
            LOG_ERROR(errorString, "Get camera %s characteristics failure. ret %d, chars %p",
                    cameraId, ret, chars);
            goto cleanup;
        }
        StaticInfo staticInfo(chars);

        for (int t = TEMPLATE_PREVIEW; t <= TEMPLATE_MANUAL; t++) {
            ACameraDevice_request_template templateId =
                    static_cast<ACameraDevice_request_template>(t);
            ret = ACameraDevice_createCaptureRequest(device, templateId, &request);
            if (ret == ACAMERA_ERROR_INVALID_PARAMETER) {
                // template not supported. skip
                continue;
            }

            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Create capture request failed!: ret %d", ret);
                goto cleanup;
            }

            int32_t numTags = 0;
            const uint32_t* tags = nullptr;
            ret = ACaptureRequest_getAllTags(request, &numTags, &tags);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Get capture request tags failed: ret %d", ret);
                goto cleanup;
            }

            for (int tid = 0; tid < numTags; tid++) {
                uint32_t tagId = tags[tid];
                ALOGV("%s capture request contains key %u", __FUNCTION__, tagId);
                uint32_t sectionId = tagId >> 16;
                if (sectionId >= ACAMERA_SECTION_COUNT && sectionId < ACAMERA_VENDOR) {
                    LOG_ERROR(errorString, "Unknown tagId %u, sectionId %u", tagId, sectionId);
                    goto cleanup;
                }
            }

            void* context = nullptr;
            ret = ACaptureRequest_getUserContext(request, &context);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Get capture request context failed: ret %d", ret);
                goto cleanup;
            }
            if (context != nullptr) {
                LOG_ERROR(errorString, "Capture request context is not null: %p", context);
                goto cleanup;
            }

            intptr_t magic_num = 0xBEEF;
            ret = ACaptureRequest_setUserContext(request, (void*) magic_num);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Set capture request context failed: ret %d", ret);
                goto cleanup;
            }

            ret = ACaptureRequest_getUserContext(request, &context);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Get capture request context failed: ret %d", ret);
                goto cleanup;
            }
            if (context != (void*) magic_num) {
                LOG_ERROR(errorString, "Capture request context is wrong: %p", context);
                goto cleanup;
            }

            // try get/set capture request fields
            ACameraMetadata_const_entry entry;
            ret = ACaptureRequest_getConstEntry_physicalCamera(request, nullptr,
                    ACAMERA_CONTROL_AE_MODE, &entry);
            if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
                LOG_ERROR(errorString, "Get AE mode key for null physical id should fail. ret %d",
                        ret);
                goto cleanup;
            }
            ret = ACaptureRequest_getConstEntry_physicalCamera(request, cameraId,
                    ACAMERA_CONTROL_AE_MODE, &entry);
            if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
                LOG_ERROR(errorString, "Get AE mode key for physical id should fail. ret %d",
                        ret);
                goto cleanup;
            }
            ret = ACaptureRequest_getConstEntry(request, ACAMERA_CONTROL_AE_MODE, &entry);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Get AE mode key failed. ret %d", ret);
                goto cleanup;
            }

            if (entry.tag != ACAMERA_CONTROL_AE_MODE || entry.type != ACAMERA_TYPE_BYTE ||\
                    entry.count != 1) {
                LOG_ERROR(errorString,
                        "Bad AE mode key. tag 0x%x (expect 0x%x), type %d (expect %d), "
                        "count %d (expect %d)",
                        entry.tag, ACAMERA_CONTROL_AE_MODE, entry.type, ACAMERA_TYPE_BYTE,
                        entry.count, 1);
                goto cleanup;
            }
            if (t == TEMPLATE_MANUAL) {
                if (entry.data.u8[0] != ACAMERA_CONTROL_AE_MODE_OFF) {
                    LOG_ERROR(errorString, "Error: MANUAL template AE mode %d (expect %d)",
                            entry.data.u8[0], ACAMERA_CONTROL_AE_MODE_OFF);
                    goto cleanup;
                }
                // try set AE_MODE_ON
                uint8_t aeMode = ACAMERA_CONTROL_AE_MODE_ON;
                ret = ACaptureRequest_setEntry_physicalCamera_u8(
                        request, nullptr, ACAMERA_CONTROL_AE_MODE, /*count*/ 1, &aeMode);
                if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
                    LOG_ERROR(errorString, "Error: camera %s setEntry_physicalCamera_u8 should "
                            "fail. ret %d", cameraId, ret);
                    goto cleanup;
                }
                ret = ACaptureRequest_setEntry_physicalCamera_u8(
                        request, cameraId, ACAMERA_CONTROL_AE_MODE, /*count*/ 1, &aeMode);
                if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
                    LOG_ERROR(errorString, "Error: camera %s setEntry_physicalCamera_u8 should "
                            "fail. ret %d", cameraId, ret);
                    goto cleanup;
                }
                ret = ACaptureRequest_setEntry_u8(
                        request, ACAMERA_CONTROL_AE_MODE, /*count*/ 1, &aeMode);
                if (ret != ACAMERA_OK) {
                    LOG_ERROR(errorString,
                            "Error: Camera %s template %d: update AE mode key fail. ret %d",
                            cameraId, t, ret);
                    goto cleanup;
                }
                ret = ACaptureRequest_getConstEntry(
                        request, ACAMERA_CONTROL_AE_MODE, &entry);
                if (ret != ACAMERA_OK) {
                    LOG_ERROR(errorString, "Get AE mode key failed. ret %d", ret);
                    goto cleanup;
                }
                if (entry.data.u8[0] != aeMode) {
                    LOG_ERROR(errorString,
                            "Error: AE mode key is not updated. expect %d but get %d",
                            aeMode, entry.data.u8[0]);
                    goto cleanup;
                }
            } else {
                if (staticInfo.isColorOutputSupported()) {
                    if (entry.data.u8[0] != ACAMERA_CONTROL_AE_MODE_ON) {
                        LOG_ERROR(errorString,
                                "Error: Template %d has wrong AE mode %d (expect %d)",
                                t, entry.data.u8[0], ACAMERA_CONTROL_AE_MODE_ON);
                        goto cleanup;
                    }
                    // try set AE_MODE_OFF
                    if (staticInfo.isCapabilitySupported(
                            ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) {
                        uint8_t aeMode = ACAMERA_CONTROL_AE_MODE_OFF;
                        ret = ACaptureRequest_setEntry_u8(
                                request, ACAMERA_CONTROL_AE_MODE, /*count*/ 1, &aeMode);
                        if (ret != ACAMERA_OK) {
                            LOG_ERROR(errorString,
                                    "Error: Camera %s template %d: update AE mode key fail. ret %d",
                                    cameraId, t, ret);
                            goto cleanup;
                        }
                        ret = ACaptureRequest_getConstEntry(
                                request, ACAMERA_CONTROL_AE_MODE, &entry);
                        if (ret != ACAMERA_OK) {
                            LOG_ERROR(errorString, "Get AE mode key failed. ret %d", ret);
                            goto cleanup;
                        }
                        if (entry.data.u8[0] != aeMode) {
                            LOG_ERROR(errorString,
                                    "Error: AE mode key is not updated. expect %d but get %d",
                                    aeMode, entry.data.u8[0]);
                            goto cleanup;
                        }
                    }
                }
            }
            ACaptureRequest_free(request);
            request = nullptr;
        }

        ACameraMetadata_free(chars);
        chars = nullptr;
        ACameraDevice_close(device);
        device = nullptr;
    }

    pass = true;
cleanup:
    if (cameraIdList) {
        ACameraManager_deleteCameraIdList(cameraIdList);
    }
    if (request) {
        ACaptureRequest_free(request);
    }
    if (chars) {
        ACameraMetadata_free(chars);
    }
    if (device) {
        ACameraDevice_close(device);
    }
    if (mgr) {
        ACameraManager_delete(mgr);
    }
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDeviceSessionOpenAndCloseNative(
        JNIEnv* env, jclass /*clazz*/, jobject jPreviewSurface,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    int numCameras = 0;
    bool pass = false;
    PreviewTestCase testCase;

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto cleanup;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negavtive number of cameras: %d", numCameras);
        goto cleanup;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }

        {
            ACameraMetadata* chars = testCase.getCameraChars(cameraId);
            StaticInfo staticInfo(chars);
            if (!staticInfo.isColorOutputSupported()) {
                ALOGI("%s: camera %s does not support color output. skipping",
                        __FUNCTION__, cameraId);
                ACameraMetadata_free(chars);
                continue;
            }
            ACameraMetadata_free(chars);
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto cleanup;
        }

        ANativeWindow* previewAnw = testCase.initPreviewAnw(env, jPreviewSurface);
        if (previewAnw == nullptr) {
            LOG_ERROR(errorString, "Null ANW from preview surface!");
            goto cleanup;
        }

        CaptureSessionListener* sessionListener = testCase.getSessionListener();
        if (sessionListener == nullptr) {
            LOG_ERROR(errorString, "Session listener camera %s is null", cameraId);
            goto cleanup;
        }

        // Try open/close session multiple times
        for (int j = 0; j < 5; j++) {
            ret = testCase.createCaptureSessionWithLog();
            if (ret != ACAMERA_OK) {
                // Don't log error here. testcase did it
                goto cleanup;
            }

            usleep(100000); // sleep to give some time for callbacks to happen

            if (!sessionListener->isIdle()) {
                LOG_ERROR(errorString, "Session for camera %s should be idle right after creation",
                        cameraId);
                goto cleanup;
            }

            testCase.closeSession();

            usleep(100000); // sleep to give some time for callbacks to happen
            if (!sessionListener->isClosed() || sessionListener->onClosedCount() != 1) {
                LOG_ERROR(errorString,
                        "Session for camera %s close error. isClosde %d close count %d",
                        cameraId, sessionListener->isClosed(), sessionListener->onClosedCount());
                goto cleanup;
            }
            sessionListener->reset();
        }

        // Try open/close really fast
        ret = testCase.createCaptureSessionWithLog();
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Create session for camera %s failed. ret %d",
                    cameraId, ret);
            goto cleanup;
        }
        testCase.closeSession();
        usleep(100000); // sleep to give some time for callbacks to happen
        if (!sessionListener->isClosed() || sessionListener->onClosedCount() != 1) {
            LOG_ERROR(errorString,
                    "Session for camera %s close error. isClosde %d close count %d",
                    cameraId, sessionListener->isClosed(), sessionListener->onClosedCount());
            goto cleanup;
        }

        ret = testCase.resetWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto cleanup;
        }
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto cleanup;
    }

    pass = true;
cleanup:
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDeviceSharedOutputUpdate(
        JNIEnv* env, jclass /*clazz*/, jobject jPreviewSurface, jobject jSharedSurface,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    int numCameras = 0;
    bool pass = false;
    PreviewTestCase testCase;
    int sequenceId = -1;
    int64_t lastFrameNumber = 0;
    bool frameArrived = false;
    ANativeWindow* previewAnw = nullptr;
    ANativeWindow* sharedAnw = ANativeWindow_fromSurface(env, jSharedSurface);
    ACaptureRequest* updatedRequest = nullptr;
    ACameraOutputTarget* reqPreviewOutput = nullptr;
    ACameraOutputTarget* reqSharedOutput = nullptr;
    ACaptureSessionOutput *previewOutput = nullptr;
    uint32_t timeoutSec = 1;
    uint32_t runPreviewSec = 2;

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto cleanup;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negavtive number of cameras: %d", numCameras);
        goto cleanup;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }

        {
            ACameraMetadata* chars = testCase.getCameraChars(cameraId);
            StaticInfo staticInfo(chars);
            if (!staticInfo.isColorOutputSupported()) {
                ALOGI("%s: camera %s does not support color output. skipping",
                        __FUNCTION__, cameraId);
                ACameraMetadata_free(chars);
                continue;
            }
            ACameraMetadata_free(chars);
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto cleanup;
        }

        previewAnw = testCase.initPreviewAnw(env, jPreviewSurface);
        if (previewAnw == nullptr) {
            LOG_ERROR(errorString, "Null ANW from preview surface!");
            goto cleanup;
        }

        ret = testCase.createCaptureSessionWithLog(true);
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.createRequestsWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.startPreview();
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Start preview failed!");
            goto cleanup;
        }

        sleep(runPreviewSec);

        previewOutput = testCase.getPreviewOutput();
        //Try some bad input
        ret = ACaptureSessionSharedOutput_add(previewOutput, previewAnw);
        if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
            LOG_ERROR(errorString, "ACaptureSessionSharedOutput_add should return invalid "
                    "parameter! %d", ret);
            goto cleanup;
        }

        ret = ACaptureSessionSharedOutput_remove(previewOutput, previewAnw);
        if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
            LOG_ERROR(errorString, "ACaptureSessionSharedOutput_remove should return invalid "
                    "parameter! %d", ret);
            goto cleanup;
        }

        //Now try with valid input
        ret = ACaptureSessionSharedOutput_add(previewOutput, sharedAnw);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "ACaptureSessionSharedOutput_add failed!")
            goto cleanup;
        }

        ret = testCase.updateOutput(env, previewOutput);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Failed to update output configuration!")
            goto cleanup;
        }

        ret = ACameraDevice_createCaptureRequest(
                testCase.getCameraDevice(), TEMPLATE_PREVIEW, &updatedRequest);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Camera %s create preview request failed. ret %d",
                    cameraId, ret);
            goto cleanup;
        }

        ret = ACameraOutputTarget_create(previewAnw, &reqPreviewOutput);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString,
                    "Camera %s create request preview output target failed. ret %d",
                    cameraId, ret);
            goto cleanup;
        }

        ret = ACaptureRequest_addTarget(updatedRequest, reqPreviewOutput);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Camera %s add preview request output failed. ret %d",
                    cameraId, ret);
            goto cleanup;
        }

        ret = ACameraOutputTarget_create(sharedAnw, &reqSharedOutput);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString,
                    "Camera %s create request preview output target failed. ret %d",
                    cameraId, ret);
            goto cleanup;
        }

        ret = ACaptureRequest_addTarget(updatedRequest, reqSharedOutput);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Camera %s add preview request output failed. ret %d",
                    cameraId, ret);
            goto cleanup;
        }

        ret = testCase.updateRepeatingRequest(updatedRequest, &sequenceId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Camera %s failed to update repeated request. ret %d",
                    cameraId, ret);
            goto cleanup;
        }

        sleep(runPreviewSec);

        ret = ACaptureSessionSharedOutput_remove(previewOutput, sharedAnw);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "ACaptureSessionSharedOutput_remove failed!");
            goto cleanup;
        }

        //Try removing shared output which still has pending camera requests
        ret = testCase.updateOutput(env, previewOutput);
        if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
            LOG_ERROR(errorString, "updateOutput should fail!");
            goto cleanup;
        }

        //Remove the shared output correctly by updating the repeating request
        //first
        ret = ACaptureRequest_removeTarget(updatedRequest, reqSharedOutput);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Camera %s remove target output failed. ret %d",
                    cameraId, ret);
            goto cleanup;
        }

        ret = testCase.updateRepeatingRequest(updatedRequest);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Camera %s failed to update repeated request. ret %d",
                    cameraId, ret);
            goto cleanup;
        }

        //Then wait for all old requests to flush
        lastFrameNumber = testCase.getCaptureSequenceLastFrameNumber(sequenceId, timeoutSec);
        if (lastFrameNumber < 0) {
            LOG_ERROR(errorString, "Camera %s failed to acquire last frame number!",
                    cameraId);
            goto cleanup;
        }

        frameArrived = testCase.waitForFrameNumber(lastFrameNumber, timeoutSec);
        if (!frameArrived) {
            LOG_ERROR(errorString, "Camera %s timed out waiting on last frame number!",
                    cameraId);
            goto cleanup;
        }

        ret = testCase.updateOutput(env, previewOutput);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "updateOutput failed!");
            goto cleanup;
        }

        sleep(runPreviewSec);

        ret = testCase.resetWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto cleanup;
        }
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto cleanup;
    }

    pass = true;

cleanup:

    if (updatedRequest != nullptr) {
        ACaptureRequest_free(updatedRequest);
        updatedRequest = nullptr;
    }

    if (reqPreviewOutput != nullptr) {
        ACameraOutputTarget_free(reqPreviewOutput);
        reqPreviewOutput = nullptr;
    }

    if (reqSharedOutput != nullptr) {
        ACameraOutputTarget_free(reqSharedOutput);
        reqSharedOutput = nullptr;
    }

    if (sharedAnw) {
        ANativeWindow_release(sharedAnw);
        sharedAnw = nullptr;
    }

    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDeviceSimplePreviewNative(
        JNIEnv* env, jclass /*clazz*/, jobject jPreviewSurface,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    int numCameras = 0;
    bool pass = false;
    PreviewTestCase testCase;

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto cleanup;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negavtive number of cameras: %d", numCameras);
        goto cleanup;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }

        {
            ACameraMetadata* chars = testCase.getCameraChars(cameraId);
            StaticInfo staticInfo(chars);
            if (!staticInfo.isColorOutputSupported()) {
                ALOGI("%s: camera %s does not support color output. skipping",
                        __FUNCTION__, cameraId);
                ACameraMetadata_free(chars);
                continue;
            }
            ACameraMetadata_free(chars);
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto cleanup;
        }

        ANativeWindow* previewAnw = testCase.initPreviewAnw(env, jPreviewSurface);
        if (previewAnw == nullptr) {
            LOG_ERROR(errorString, "Null ANW from preview surface!");
            goto cleanup;
        }

        ret = testCase.createCaptureSessionWithLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.createRequestsWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.startPreview();
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Start preview failed!");
            goto cleanup;
        }

        sleep(3);

        ret = testCase.resetWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto cleanup;
        }
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto cleanup;
    }

    pass = true;
cleanup:
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDevicePreviewWithSessionParametersNative(
        JNIEnv* env, jclass /*clazz*/, jobject jPreviewSurface,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    int numCameras = 0;
    bool pass = false;
    ACameraManager* mgr = ACameraManager_create();
    ACameraMetadata* chars = nullptr;
    PreviewTestCase testCase;

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto cleanup;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negavtive number of cameras: %d", numCameras);
        goto cleanup;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }

        ret = ACameraManager_getCameraCharacteristics(
                mgr, cameraId, &chars);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Get camera characteristics failed: ret %d", ret);
            goto cleanup;
        }

        StaticInfo staticInfo(chars);
        ACameraMetadata_const_entry sessionParamKeys{};
        ret = ACameraMetadata_getConstEntry(chars, ACAMERA_REQUEST_AVAILABLE_SESSION_KEYS,
                &sessionParamKeys);
        if ((ret != ACAMERA_OK) || (sessionParamKeys.count == 0) ||
                !staticInfo.isColorOutputSupported()) {
            ACameraMetadata_free(chars);
            chars = nullptr;
            continue;
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto cleanup;
        }

        ANativeWindow* previewAnw = testCase.initPreviewAnw(env, jPreviewSurface);
        if (previewAnw == nullptr) {
            LOG_ERROR(errorString, "Null ANW from preview surface!");
            goto cleanup;
        }

        ret = testCase.createRequestsWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ACaptureRequest *previewRequest = nullptr;
        ret = testCase.getPreviewRequest(&previewRequest);
        if ((ret != ACAMERA_OK) || (previewRequest == nullptr)) {
            LOG_ERROR(errorString, "Preview request query failed!");
            goto cleanup;
        }

        ret = testCase.createCaptureSessionWithLog(/*isPreviewShared*/ false, previewRequest);
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.startPreview();
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Start preview failed!");
            goto cleanup;
        }

        sleep(3);

        ret = testCase.resetWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto cleanup;
        }

        ACameraMetadata_free(chars);
        chars = nullptr;
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto cleanup;
    }

    pass = true;
cleanup:
    if (chars) {
        ACameraMetadata_free(chars);
    }
    ACameraManager_delete(mgr);
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

bool nativeCameraDeviceLogicalPhysicalStreaming(
        JNIEnv* env, jobject jPreviewSurface, bool usePhysicalSettings,
        jstring jOverrideCameraId) {
    const int NUM_TEST_IMAGES = 10;
    const int TEST_WIDTH  = 640;
    const int TEST_HEIGHT = 480;
    ALOGV("%s", __FUNCTION__);
    int numCameras = 0;
    bool pass = false;
    ACameraManager* mgr = ACameraManager_create();
    ACameraMetadata* chars = nullptr;
    media_status_t mediaRet = AMEDIA_ERROR_UNKNOWN;
    PreviewTestCase testCase;
    int64_t lastFrameNumber = 0;
    bool frameArrived = false;
    uint32_t timeoutSec = 1;
    uint32_t runPreviewSec = 2;

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto cleanup;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negavtive number of cameras: %d", numCameras);
        goto cleanup;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }

        if (chars != nullptr) {
            ACameraMetadata_free(chars);
            chars = nullptr;
        }
        chars = testCase.getCameraChars(i);
        if (chars == nullptr) {
            LOG_ERROR(errorString, "Get camera %s characteristics failure", cameraId);
            goto cleanup;
        }

        size_t physicalCameraCnt = 0;
        const char *const* physicalCameraIds = nullptr;
        if (!ACameraMetadata_isLogicalMultiCamera(
                chars, &physicalCameraCnt, &physicalCameraIds)) {
            continue;
        }
        if (physicalCameraCnt < 2) {
            LOG_ERROR(errorString, "Logical camera device %s only has %zu physical cameras",
                   cameraId, physicalCameraCnt);
            goto cleanup;
        }

        std::vector<const char*> candidateIds;
        for (size_t i = 0; i < physicalCameraCnt && candidateIds.size() < 2; i++) {
            ACameraMetadata* physicalChars = testCase.getCameraChars(physicalCameraIds[i]);
            if (physicalChars == nullptr) {
                LOG_ERROR(errorString,
                        "Get camera %s characteristics failure", physicalCameraIds[i]);
                goto cleanup;
            }
            StaticInfo info(physicalChars);
            bool testSizeSupported = info.isSizeSupportedForFormat(AIMAGE_FORMAT_YUV_420_888,
                    TEST_WIDTH, TEST_HEIGHT);
            ACameraMetadata_free(physicalChars);
            if (!testSizeSupported) {
                continue;
            }
            candidateIds.push_back(physicalCameraIds[i]);
        }
        if (candidateIds.size() < 2) {
            continue;
        }

        // Check physical camera request keys
        if (usePhysicalSettings) {
            ACameraMetadata_const_entry entry;
            camera_status_t status = ACameraMetadata_getConstEntry(
                    chars, ACAMERA_REQUEST_AVAILABLE_PHYSICAL_CAMERA_REQUEST_KEYS, &entry);
            if (status == ACAMERA_ERROR_METADATA_NOT_FOUND) {
                // No supported PHYSICAL_CAMERA_REQUEST_KEYS, skip test
                continue;
            } else if (status != ACAMERA_OK) {
                // Do not log error here. testcase did it.
                goto cleanup;
            } else if (entry.count == 0) {
                // No supported PHYSICAL_CAMERA_REQUEST_KEYS, skip test
                continue;
            }
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto cleanup;
        }

        std::vector<ImageReaderListener> readerListeners(2);
        std::vector<AImageReader_ImageListener> readerCbs;
        std::vector<AImageReader*> readers;
        std::vector<ANativeWindow*> readerAnws;
        std::vector<ACaptureSessionOutput*> readerSessionOutputs;
        std::vector<ACameraOutputTarget*> readerOutputs;
        for (size_t i = 0; i < 2; i++) {
            AImageReader_ImageListener readerCb {
                &readerListeners[i],
                ImageReaderListener::validateImageCb
            };
            readerCbs.push_back(readerCb);

            AImageReader* reader = nullptr;
            ANativeWindow* readerAnw = nullptr;
            ACaptureSessionOutput* readerSessionOutput = nullptr;
            ACameraOutputTarget* readerOutput = nullptr;
            mediaRet = testCase.initImageReaderWithErrorLog(
                    TEST_WIDTH, TEST_HEIGHT, AIMAGE_FORMAT_YUV_420_888, NUM_TEST_IMAGES,
                    &readerCb, &reader, &readerAnw);
            if (mediaRet != AMEDIA_OK) {
                // Don't log error here. testcase did it
                goto cleanup;
            }

            camera_status_t ret = ACaptureSessionPhysicalOutput_create(readerAnw,
                    candidateIds[i], &readerSessionOutput);
            if (ret != ACAMERA_OK || readerSessionOutput == nullptr) {
                if (ret == ACAMERA_OK) {
                    ret = ACAMERA_ERROR_UNKNOWN; // ret OK but output is null
                }
                // Don't log error here. testcase did it
                goto cleanup;
            }

            ret = ACameraOutputTarget_create(readerAnw, &readerOutput);
            if (ret != ACAMERA_OK) {
                // Don't log error here. testcase did it
                goto cleanup;
            }

            readers.push_back(reader);
            readerAnws.push_back(readerAnw);
            readerSessionOutputs.push_back(readerSessionOutput);
            readerOutputs.push_back(readerOutput);
        }

        ANativeWindow* previewAnw = testCase.initPreviewAnw(env, jPreviewSurface);
        if (previewAnw == nullptr) {
            LOG_ERROR(errorString, "Null ANW from preview surface!");
            goto cleanup;
        }

        ret = testCase.createCaptureSessionWithLog(readerSessionOutputs, false /*isPreviewShared*/,
                nullptr /*sessionParameters*/, false /*sessionConfigurationDefault*/);
        if (ret == ACAMERA_ERROR_UNSUPPORTED_OPERATION ||
                ret == ACAMERA_ERROR_STREAM_CONFIGURE_FAIL) {
            // Camera device doesn't support the stream combination, skip the
            // current camera.
            testCase.closeCamera();
            testCase.resetCamera();
            continue;
        } else if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        if (usePhysicalSettings) {
            std::vector<const char*> twoNullStr = {nullptr, nullptr};
            ACameraIdList nullCameraIdList = {2, twoNullStr.data()};
            ret = testCase.createRequestsWithErrorLog(readerOutputs, &nullCameraIdList);
            if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
                LOG_ERROR(errorString, "Null physical camera ids must fail createCaptureRequest. "
                        "ret %d", ret);
                goto cleanup;
            }

            std::string invalidId = "";
            std::vector<const char*> one0LengthStr = {invalidId.c_str()};
            ACameraIdList invalidCameraIdList = {1, one0LengthStr.data()};
            ret = testCase.createRequestsWithErrorLog(readerOutputs, &invalidCameraIdList);
            if (ret != ACAMERA_ERROR_INVALID_PARAMETER) {
                LOG_ERROR(errorString, "zero-length physical camera ids must fail "
                        "createCaptureRequest. ret %d", ret);
                goto cleanup;
            }

            ACameraIdList physicalCameraIdList = {2, candidateIds.data()};
            ret = testCase.createRequestsWithErrorLog(readerOutputs, &physicalCameraIdList);
        } else {
            ret = testCase.createRequestsWithErrorLog(readerOutputs);
        }
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ACaptureRequest *previewRequest = nullptr;
        ret = testCase.getPreviewRequest(&previewRequest);
        if ((ret != ACAMERA_OK) || (previewRequest == nullptr)) {
            LOG_ERROR(errorString, "Preview request query failed!");
            goto cleanup;
        }

        int sequenceId = 0;
        ret = testCase.startPreview(&sequenceId, 2, candidateIds.data());
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Start preview failed!");
            goto cleanup;
        }

        sleep(runPreviewSec);

        ret = testCase.stopPreview();
        if (ret != ACAMERA_OK) {
            ALOGE("%s: stopPreview failed", __FUNCTION__);
            LOG_ERROR(errorString, "stopPreview failed!");
            goto cleanup;
        }

        //Then wait for all old requests to flush
        lastFrameNumber = testCase.getCaptureSequenceLastFrameNumber(sequenceId, timeoutSec);
        if (lastFrameNumber < 0) {
            LOG_ERROR(errorString, "Camera %s failed to acquire last frame number!",
                    cameraId);
            goto cleanup;
        }

        frameArrived = testCase.waitForFrameNumber(lastFrameNumber, timeoutSec);
        if (!frameArrived) {
            LOG_ERROR(errorString, "Camera %s timed out waiting on last frame number!",
                    cameraId);
            goto cleanup;
        }

        ret = testCase.resetWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto cleanup;
        }
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto cleanup;
    }

    pass = true;
cleanup:
    if (chars) {
        ACameraMetadata_free(chars);
    }
    ACameraManager_delete(mgr);
    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDeviceLogicalPhysicalStreamingNative(
        JNIEnv* env, jclass /*clazz*/, jobject jPreviewSurface,
        jstring jOverrideCameraId) {
    return nativeCameraDeviceLogicalPhysicalStreaming(env,
            jPreviewSurface, false /*usePhysicalSettings*/,
            jOverrideCameraId);
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDeviceLogicalPhysicalSettingsNative(
        JNIEnv* env, jclass /*clazz*/, jobject jPreviewSurface,
        jstring jOverrideCameraId) {
    return nativeCameraDeviceLogicalPhysicalStreaming(env,
            jPreviewSurface, true /*usePhysicalSettings*/,
            jOverrideCameraId);
}

bool nativeImageReaderTestBase(
        JNIEnv* env, jstring jOutPath, jint format, AImageReader_ImageCallback cb,
        jstring jOverrideCameraId) {
    const int NUM_TEST_IMAGES = 10;
    const int TEST_WIDTH  = 640;
    const int TEST_HEIGHT = 480;
    media_status_t mediaRet = AMEDIA_ERROR_UNKNOWN;
    int numCameras = 0;
    bool pass = false;
    PreviewTestCase testCase;
    ACameraMetadata* chars = nullptr;

    const char* outPath = (jOutPath == nullptr) ? nullptr :
            env->GetStringUTFChars(jOutPath, nullptr);
    if (outPath != nullptr) {
        ALOGI("%s: out path is %s", __FUNCTION__, outPath);
    }

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto cleanup;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negavtive number of cameras: %d", numCameras);
        goto cleanup;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }

        {
            ACameraMetadata* chars = testCase.getCameraChars(cameraId);
            StaticInfo staticInfo(chars);
            if (!staticInfo.isColorOutputSupported()) {
                ALOGI("%s: camera %s does not support color output. skipping",
                        __FUNCTION__, cameraId);
                ACameraMetadata_free(chars);
                continue;
            }
            ACameraMetadata_free(chars);
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        chars = testCase.getCameraChars(i);
        if (chars == nullptr) {
            LOG_ERROR(errorString, "Get camera %s characteristics failure", cameraId);
            goto cleanup;
        }
        StaticInfo staticInfo(chars);

        usleep(200000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto cleanup;
        }

        ImageReaderListener readerListener;
        AImageReader_ImageListener readerCb { &readerListener, cb };
        readerListener.setDumpFilePathBase(outPath);

        int32_t testWidth, testHeight;
        switch (format) {
            case AIMAGE_FORMAT_JPEG:
                testWidth = TEST_WIDTH;
                testHeight = TEST_HEIGHT;
                break;
            case AIMAGE_FORMAT_Y8:
            case AIMAGE_FORMAT_HEIC:
            case AIMAGE_FORMAT_DEPTH_JPEG:
                if (!staticInfo.getMaxSizeForFormat(format, &testWidth, &testHeight)) {
                    // No corresponding format support, skip this device.
                    ACameraMetadata_free(chars);
                    chars = nullptr;
                    ret = testCase.closeCamera();
                    if (ret != ACAMERA_OK) {
                        LOG_ERROR(errorString, "Camera %s failed to close. ret %d ", cameraId, ret);
                        goto cleanup;
                    }

                    continue;
                }
                break;
            default:
                LOG_ERROR(errorString, "Testcase doesn't yet support format %d", format);
                goto cleanup;
        }
        mediaRet = testCase.initImageReaderWithErrorLog(
                testWidth, testHeight, format, NUM_TEST_IMAGES,
                &readerCb);
        if (mediaRet != AMEDIA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.createCaptureSessionWithLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.createRequestsWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        CaptureResultListener resultListener;
        ACameraCaptureSession_captureCallbacks resultCb {
            &resultListener,
            CaptureResultListener::onCaptureStart,
            CaptureResultListener::onCaptureProgressed,
            CaptureResultListener::onCaptureCompleted,
            CaptureResultListener::onCaptureFailed,
            CaptureResultListener::onCaptureSequenceCompleted,
            CaptureResultListener::onCaptureSequenceAborted,
            CaptureResultListener::onCaptureBufferLost
        };
        resultListener.setRequestSave(true);
        ACaptureRequest* requestTemplate = nullptr;
        ret = testCase.getStillRequest(&requestTemplate);
        if (ret != ACAMERA_OK || requestTemplate == nullptr) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        // Do some still capture
        int lastSeqId = -1;
        for (intptr_t capture = 0; capture < NUM_TEST_IMAGES; capture++) {
            ACaptureRequest* req = ACaptureRequest_copy(requestTemplate);
            ACaptureRequest_setUserContext(req, (void*) capture);
            int seqId;
            ret = testCase.capture(req, &resultCb, &seqId);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Camera %s capture(%" PRIdPTR ") failed. ret %d",
                        cameraId, capture, ret);
                goto cleanup;
            }
            if (capture == NUM_TEST_IMAGES - 1) {
                lastSeqId = seqId;
            }
            ACaptureRequest_free(req);
        }

        // wait until last sequence complete
        resultListener.getCaptureSequenceLastFrameNumber(lastSeqId, /*timeoutSec*/ 5);

        std::vector<ACaptureRequest*> completedRequests;
        resultListener.getCompletedRequests(&completedRequests);

        if (completedRequests.size() != NUM_TEST_IMAGES) {
            LOG_ERROR(errorString, "Camera %s fails to capture %d capture results. Got %zu",
                    cameraId, NUM_TEST_IMAGES, completedRequests.size());
            goto cleanup;
        }

        for (intptr_t i = 0; i < NUM_TEST_IMAGES; i++) {
            intptr_t userContext = -1;
            ret = ACaptureRequest_getUserContext(completedRequests[i], (void**) &userContext);
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Camera %s fails to get request user context", cameraId);
                goto cleanup;
            }

            if (userContext != i) {
                LOG_ERROR(errorString, "Camera %s fails to return matching user context. "
                        "Expect %" PRIdPTR ", got %" PRIdPTR, cameraId, i, userContext);
                goto cleanup;
            }
        }

        int64_t minFrameDurationNs = staticInfo.getMinFrameDurationFor(
                format, testWidth, testHeight);
        if (minFrameDurationNs < 0) {
            LOG_ERROR(errorString, "Get camera %s minFrameDuration failed", cameraId);
            goto cleanup;
        }
        int64_t stallDurationNs = (format == AIMAGE_FORMAT_Y8) ? 0 :
                staticInfo.getStallDurationFor(format, testWidth, testHeight);
        if (stallDurationNs < 0) {
            LOG_ERROR(errorString, "Get camera %s stallDuration failed", cameraId);
            goto cleanup;
        }

        int64_t expectedDurationNs = (minFrameDurationNs + stallDurationNs) * NUM_TEST_IMAGES;
        constexpr int64_t waitPerIterationUs = 100000;
        constexpr int64_t usToNs = 1000;
        int totalWaitIteration = 50;

        // Allow 1.5x margin
        if (expectedDurationNs * 3 / 2 > totalWaitIteration * waitPerIterationUs * usToNs) {
            totalWaitIteration = expectedDurationNs * 3 / 2 / waitPerIterationUs / usToNs;
        }

        // wait until all capture finished
        for (int i = 0; i < totalWaitIteration; i++) {
            usleep(waitPerIterationUs);
            if (readerListener.onImageAvailableCount() == NUM_TEST_IMAGES) {
                ALOGI("Session take ~%d ms to capture %d images",
                        i*100, NUM_TEST_IMAGES);
                break;
            }
        }

        if (readerListener.onImageAvailableCount() != NUM_TEST_IMAGES) {
            LOG_ERROR(errorString, "Camera %s timeout capturing %d images. Got %d",
                    cameraId, NUM_TEST_IMAGES, readerListener.onImageAvailableCount());
            goto cleanup;
        }

        ret = testCase.resetWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        usleep(200000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto cleanup;
        }
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto cleanup;
    }

    pass = true;

cleanup:
    if (outPath != nullptr) {
        env->ReleaseStringUTFChars(jOutPath, outPath);
    }

    if (chars != nullptr) {
        ACameraMetadata_free(chars);
        chars = nullptr;
    }

    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

// Test the camera NDK capture failure path by acquiring the maximum amount of
// ImageReader buffers available. Since there is no circulation of camera
// images, the registered output surface will eventually run out of free buffers
// and start reporting capture errors or lost buffers.
extern "C" jboolean
Java_android_hardware_camera2_cts_NativeCameraDeviceTest_\
testCameraDeviceCaptureFailureNative(JNIEnv* env, jclass /*clazz*/, jstring jOverrideCameraId) {
    const size_t NUM_TEST_IMAGES = 10;
    const size_t NUM_FAILED_FRAMES = 3; // Wait for at least 3 consecutive failed capture requests
    const int64_t NUM_TOTAL_FRAMES = 60; // Avoid waiting for more than 60 frames
    const size_t TEST_WIDTH  = 640;
    const size_t TEST_HEIGHT = 480;
    media_status_t mediaRet = AMEDIA_ERROR_UNKNOWN;
    int numCameras = 0;
    bool pass = false;
    PreviewTestCase testCase;
    uint32_t bufferTimeoutSec = 1;
    uint32_t timeoutSec = 10; // It is important to keep this timeout bigger than the framework
                              // timeout

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto exit;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negative number of cameras: %d", numCameras);
        goto exit;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto exit;
        }

        std::unique_ptr<ACameraMetadata> chars(testCase.getCameraChars(i));
        if (chars.get() == nullptr) {
            LOG_ERROR(errorString, "Get camera %s characteristics failure", cameraId);
            goto exit;
        }
        StaticInfo staticInfo(chars.get());

        if (!staticInfo.isColorOutputSupported()) {
            continue;
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto exit;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto exit;
        }

        ImageReaderListener readerListener;
        AImageReader_ImageListener readerCb =
                { &readerListener, ImageReaderListener::signalImageCb };
        mediaRet = testCase.initImageReaderWithErrorLog(TEST_WIDTH, TEST_HEIGHT,
                AIMAGE_FORMAT_YUV_420_888, NUM_TEST_IMAGES, &readerCb);
        if (mediaRet != AMEDIA_OK) {
            // Don't log error here. testcase did it
            goto exit;
        }

        ret = testCase.createCaptureSessionWithLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto exit;
        }

        ret = testCase.createRequestsWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto exit;
        }

        CaptureResultListener resultListener;
        ACameraCaptureSession_captureCallbacks resultCb {
            &resultListener,
            CaptureResultListener::onCaptureStart,
            CaptureResultListener::onCaptureProgressed,
            CaptureResultListener::onCaptureCompleted,
            CaptureResultListener::onCaptureFailed,
            CaptureResultListener::onCaptureSequenceCompleted,
            CaptureResultListener::onCaptureSequenceAborted,
            CaptureResultListener::onCaptureBufferLost
        };
        ACaptureRequest* requestTemplate = nullptr;
        ret = testCase.getStillRequest(&requestTemplate);
        if (ret != ACAMERA_OK || requestTemplate == nullptr) {
            // Don't log error here. testcase did it
            goto exit;
        }

        int seqId;
        ret = testCase.startRepeatingRequest(&seqId, requestTemplate, &resultCb);
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto exit;
        }

        size_t failedRequestCount;
        int64_t lastFrameNumber;
        int64_t lastFailedRequestNumber = -1;
        failedRequestCount = lastFrameNumber = 0;
        while ((failedRequestCount < NUM_FAILED_FRAMES) && (lastFrameNumber < NUM_TOTAL_FRAMES)) {
            auto frameArrived = resultListener.waitForFrameNumber(lastFrameNumber, timeoutSec);
            if (!frameArrived) {
                LOG_ERROR(errorString, "Camera %s timed out waiting on last frame number!",
                        cameraId);
                goto exit;
            }
            readerListener.waitForNextBuffer(bufferTimeoutSec);
            auto failedFrameNumber = resultListener.checkForFailureOrLoss(lastFrameNumber) ?
                    lastFrameNumber : -1;
            if (lastFailedRequestNumber != failedFrameNumber) {
                if ((lastFailedRequestNumber + 1) == failedFrameNumber) {
                    failedRequestCount++;
                } else {
                    failedRequestCount = 1;
                }
                lastFailedRequestNumber = failedFrameNumber;
            }
            lastFrameNumber++;
        }

        ret = testCase.stopPreview();
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "stopPreview failed!");
            goto exit;
        }

        ret = testCase.resetWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto exit;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto exit;
        }

        if (failedRequestCount < NUM_FAILED_FRAMES) {
            LOG_ERROR(errorString, "Unable to receive %zu consecutive capture failures within"
                    " %" PRId64 " capture requests", NUM_FAILED_FRAMES, NUM_TOTAL_FRAMES);
            goto exit;
        }
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto exit;
    }

    pass = true;

exit:

    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }

    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeImageReaderTest_\
testJpegNative(
        JNIEnv* env, jclass /*clazz*/, jstring jOutPath,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    return nativeImageReaderTestBase(env, jOutPath, AIMAGE_FORMAT_JPEG,
            ImageReaderListener::validateImageCb, jOverrideCameraId);
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeImageReaderTest_\
testY8Native(
        JNIEnv* env, jclass /*clazz*/, jstring jOutPath,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    return nativeImageReaderTestBase(env, jOutPath, AIMAGE_FORMAT_Y8,
            ImageReaderListener::validateImageCb, jOverrideCameraId);
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeImageReaderTest_\
testHeicNative(
        JNIEnv* env, jclass /*clazz*/, jstring jOutPath,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    return nativeImageReaderTestBase(env, jOutPath, AIMAGE_FORMAT_HEIC,
            ImageReaderListener::validateImageCb, jOverrideCameraId);
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeImageReaderTest_\
testDepthJpegNative(
        JNIEnv* env, jclass /*clazz*/, jstring jOutPath,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    return nativeImageReaderTestBase(env, jOutPath, AIMAGE_FORMAT_DEPTH_JPEG,
            ImageReaderListener::validateImageCb, jOverrideCameraId);
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeImageReaderTest_\
testImageReaderCloseAcquiredImagesNative(
        JNIEnv* env, jclass /*clazz*/,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    return nativeImageReaderTestBase(env, nullptr, AIMAGE_FORMAT_JPEG,
            ImageReaderListener::acquireImageCb, jOverrideCameraId);
}

template <>
struct std::default_delete<ACameraManager> {
    inline void operator()(ACameraManager* manager) const { ACameraManager_delete(manager); }
};

class AvailabilityContext {
    public:
        AvailabilityContext();
        ~AvailabilityContext();

        camera_status_t initialize();
        int getAcessCallbackCountAndReset();

    private:
        std::unique_ptr<ACameraManager> mCameraManager;
        std::unique_ptr<CameraServiceListener> mServiceListener;
        std::unique_ptr<ACameraManager_ExtendedAvailabilityCallbacks> mServiceCb;
};

AvailabilityContext::AvailabilityContext() :
    mCameraManager(ACameraManager_create()),
    mServiceListener(std::make_unique<CameraServiceListener>()),
    mServiceCb(std::make_unique<ACameraManager_ExtendedAvailabilityCallbacks>()) {
        mServiceCb->availabilityCallbacks.context = mServiceListener.get();
        mServiceCb->availabilityCallbacks.onCameraAvailable = CameraServiceListener::onAvailable;
        mServiceCb->availabilityCallbacks.onCameraUnavailable =
                CameraServiceListener::onUnavailable;
        mServiceCb->onCameraAccessPrioritiesChanged =
                CameraServiceListener::onCameraAccessPrioritiesChanged;
        mServiceCb->onPhysicalCameraAvailable =
                CameraServiceListener::onPhysicalCameraAvailable;
        mServiceCb->onPhysicalCameraUnavailable =
                CameraServiceListener::onPhysicalCameraUnavailable;
}

camera_status_t AvailabilityContext::initialize() {
    auto rc = ACameraManager_registerExtendedAvailabilityCallback(mCameraManager.get(),
            mServiceCb.get());
    if (rc != ACAMERA_OK) {
        LOG_ERROR(errorString, "Register availability callback failed: rc %d", rc);
        return rc;
    }

    ACameraIdList* cameraIdList = nullptr;
    rc = ACameraManager_getCameraIdList(mCameraManager.get(), &cameraIdList);
    if (rc != ACAMERA_OK) {
        LOG_ERROR(errorString, "Get camera id list failed: ret %d", rc);
        return rc;
    }
    ACameraManager_deleteCameraIdList(cameraIdList);

    return rc;
}

int AvailabilityContext::getAcessCallbackCountAndReset() {
    auto ret = mServiceListener->getCameraAccessPrioritiesChangedCount();
    mServiceListener->resetCount();
    return ret;
}

AvailabilityContext::~AvailabilityContext() {
    if (mServiceCb != nullptr) {
        camera_status_t ret = ACameraManager_unregisterExtendedAvailabilityCallback(
                mCameraManager.get(), mServiceCb.get());
        if (ret != ACAMERA_OK) {
            ALOGE("Unregister availability callback failed: ret %d", ret);
        }
    }
}

extern "C" jlong
Java_android_hardware_multiprocess_camera_cts_CameraEvictionTest_\
initializeAvailabilityCallbacksNative(
        JNIEnv* env, jclass /*clazz*/) {
    ALOGV("%s", __FUNCTION__);

    AvailabilityContext *ctx = new AvailabilityContext();

    auto rc = ctx->initialize();
    if (rc != ACAMERA_OK) {
        LOG_ERROR(errorString, "Availability context initialization failed: %d", rc);
        return 0;
    }

    return (jlong) ctx;
}

extern "C" jint
Java_android_hardware_multiprocess_camera_cts_CameraEvictionTest_\
getAccessCallbacksCountAndResetNative(
        JNIEnv* env, jclass /*clazz*/, jlong context) {
    ALOGV("%s", __FUNCTION__);

    if (context == 0) {
        LOG_ERROR(errorString, "Invalid availability context");
        return 0;
    }

    AvailabilityContext* ctx = reinterpret_cast<AvailabilityContext*>(context);
    return ctx->getAcessCallbackCountAndReset();
}

extern "C" void
Java_android_hardware_multiprocess_camera_cts_CameraEvictionTest_\
releaseAvailabilityCallbacksNative(
        JNIEnv* env, jclass /*clazz*/, jlong context) {
    ALOGV("%s", __FUNCTION__);

    if (context == 0) {
        return;
    }

    AvailabilityContext* ctx = reinterpret_cast<AvailabilityContext*>(context);
    delete ctx;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_NativeStillCaptureTest_\
testStillCaptureNative(
        JNIEnv* env, jclass /*clazz*/, jstring jOutPath, jobject jPreviewSurface,
        jstring jOverrideCameraId) {
    ALOGV("%s", __FUNCTION__);
    const int NUM_TEST_IMAGES = 10;
    const int TEST_WIDTH  = 640;
    const int TEST_HEIGHT = 480;
    media_status_t mediaRet = AMEDIA_ERROR_UNKNOWN;
    int numCameras = 0;
    bool pass = false;
    PreviewTestCase testCase;
    ACameraMetadata* chars = nullptr;

    const char* outPath = env->GetStringUTFChars(jOutPath, nullptr);
    ALOGI("%s: out path is %s", __FUNCTION__, outPath);

    camera_status_t ret = testCase.initWithErrorLog(env, jOverrideCameraId);
    if (ret != ACAMERA_OK) {
        // Don't log error here. testcase did it
        goto cleanup;
    }

    numCameras = testCase.getNumCameras();
    if (numCameras < 0) {
        LOG_ERROR(errorString, "Testcase returned negavtive number of cameras: %d", numCameras);
        goto cleanup;
    }

    for (int i = 0; i < numCameras; i++) {
        const char* cameraId = testCase.getCameraId(i);
        if (cameraId == nullptr) {
            LOG_ERROR(errorString, "Testcase returned null camera id for camera %d", i);
            goto cleanup;
        }

        {
            ACameraMetadata* chars = testCase.getCameraChars(cameraId);
            StaticInfo staticInfo(chars);
            if (!staticInfo.isColorOutputSupported()) {
                ALOGI("%s: camera %s does not support color output. skipping",
                        __FUNCTION__, cameraId);
                ACameraMetadata_free(chars);
                continue;
            }
            ACameraMetadata_free(chars);
        }

        ret = testCase.openCamera(cameraId);
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Open camera device %s failure. ret %d", cameraId, ret);
            goto cleanup;
        }

        chars = testCase.getCameraChars(i);
        if (chars == nullptr) {
            LOG_ERROR(errorString, "Get camera %s characteristics failure", cameraId);
            goto cleanup;
        }
        StaticInfo staticInfo(chars);

        usleep(100000); // sleep to give some time for callbacks to happen

        if (testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be unavailable now", cameraId);
            goto cleanup;
        }

        ImageReaderListener readerListener;
        AImageReader_ImageListener readerCb {
            &readerListener,
            ImageReaderListener::validateImageCb
        };
        readerListener.setDumpFilePathBase(outPath);
        mediaRet = testCase.initImageReaderWithErrorLog(
                TEST_WIDTH, TEST_HEIGHT, AIMAGE_FORMAT_JPEG, NUM_TEST_IMAGES,
                &readerCb);
        if (mediaRet != AMEDIA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ANativeWindow* previewAnw = testCase.initPreviewAnw(env, jPreviewSurface);
        if (previewAnw == nullptr) {
            LOG_ERROR(errorString, "Null ANW from preview surface!");
            goto cleanup;
        }

        ret = testCase.createCaptureSessionWithLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.createRequestsWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        ret = testCase.startPreview();
        if (ret != ACAMERA_OK) {
            LOG_ERROR(errorString, "Start preview failed!");
            goto cleanup;
        }

        // Let preview run some time
        sleep(3);

        // Do some still capture
        for (int capture = 0; capture < NUM_TEST_IMAGES; capture++) {
            ret = testCase.takePicture();
            if (ret != ACAMERA_OK) {
                LOG_ERROR(errorString, "Camera %s capture(%d) failed. ret %d",
                        cameraId, capture, ret);
                goto cleanup;
            }
        }

        int64_t minFrameDurationNs = staticInfo.getMinFrameDurationFor(
                AIMAGE_FORMAT_JPEG, TEST_WIDTH, TEST_HEIGHT);
        if (minFrameDurationNs < 0) {
            LOG_ERROR(errorString, "Get camera %s minFrameDuration failed", cameraId);
            goto cleanup;
        }
        int64_t stallDurationNs = staticInfo.getStallDurationFor(
                AIMAGE_FORMAT_JPEG, TEST_WIDTH, TEST_HEIGHT);
        if (stallDurationNs < 0) {
            LOG_ERROR(errorString, "Get camera %s stallDuration failed", cameraId);
            goto cleanup;
        }

        int64_t expectedDurationNs = (minFrameDurationNs + stallDurationNs) * NUM_TEST_IMAGES;
        constexpr int64_t waitPerIterationUs = 100000;
        constexpr int64_t usToNs = 1000;
        int totalWaitIteration = 50;

        // Allow 1.5x margin
        if (expectedDurationNs * 3 / 2 > totalWaitIteration * waitPerIterationUs * usToNs) {
            totalWaitIteration = expectedDurationNs * 3 / 2 / waitPerIterationUs / usToNs;
        }

        // wait until all capture finished
        for (int i = 0; i < totalWaitIteration; i++) {
            usleep(waitPerIterationUs);
            if (readerListener.onImageAvailableCount() == NUM_TEST_IMAGES) {
                ALOGI("Session take ~%d ms to capture %d images",
                        i*100, NUM_TEST_IMAGES);
                break;
            }
        }

        if (readerListener.onImageAvailableCount() != NUM_TEST_IMAGES) {
            LOG_ERROR(errorString, "Camera %s timeout capturing %d images. Got %d",
                    cameraId, NUM_TEST_IMAGES, readerListener.onImageAvailableCount());
            goto cleanup;
        }

        ret = testCase.resetWithErrorLog();
        if (ret != ACAMERA_OK) {
            // Don't log error here. testcase did it
            goto cleanup;
        }

        usleep(100000); // sleep to give some time for callbacks to happen

        if (!testCase.isCameraAvailable(cameraId)) {
            LOG_ERROR(errorString, "Camera %s should be available now", cameraId);
            goto cleanup;
        }
    }

    ret = testCase.deInit();
    if (ret != ACAMERA_OK) {
        LOG_ERROR(errorString, "Testcase deInit failed: ret %d", ret);
        goto cleanup;
    }

    pass = true;
cleanup:
    env->ReleaseStringUTFChars(jOutPath, outPath);

    if (chars != nullptr) {
        ACameraMetadata_free(chars);
        chars = nullptr;
    }

    ALOGI("%s %s", __FUNCTION__, pass ? "pass" : "failed");
    if (!pass) {
        throwAssertionError(env, errorString);
    }
    return pass;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_CaptureResultTest_\
validateACameraMetadataFromCameraMetadataCriticalTagsNative(
        JNIEnv* env, jclass /*clazz*/, jobject captureResult,
        jlong javaTimestamp) {
    ALOGV("%s", __FUNCTION__);
    ACameraMetadata* ndkResult =
        ACameraMetadata_fromCameraMetadata(env, captureResult);
    if (!ndkResult) {
        ALOGE("validateCriticalTags failed: "
              "ACameraMetadata_fromCameraMetadata returned nullptr.");
        return false;
    }

    camera_status_t ret;
    ACameraMetadata_const_entry entry;

    ret = ACameraMetadata_getConstEntry(ndkResult, ACAMERA_SENSOR_TIMESTAMP,
        &entry);

    if (ret != ACAMERA_OK) {
        ALOGE("validateCriticalTags failed: "
              "ACameraMetadata_getConstEntry returned %d.", ret);
        ACameraMetadata_free(ndkResult);
        return false;
    }
    if (entry.type != ACAMERA_TYPE_INT64) {
        ALOGE("validateCriticalTags failed: entry.type is %u but should be %u.",
              entry.type, ACAMERA_TYPE_INT64);
        ACameraMetadata_free(ndkResult);
        return false;
    }
    if (entry.count != 1) {
        ALOGE("validateCriticalTags failed: entry.count is %u but should be %u.",
              entry.count, 1);
        ACameraMetadata_free(ndkResult);
        return false;
    }
    if (entry.data.i64 == nullptr) {
        ALOGE("validateCriticalTags failed: entry.data.i64 is nullptr.");
        ACameraMetadata_free(ndkResult);
        return false;
    }

    const int64_t javaTimestampI64 = static_cast<int64_t>(javaTimestamp);
    const int64_t ndkTimestampI64 = *(entry.data.i64);
    ALOGV("javaTimestampI64 = %" PRId64 ", ndkTimestampI64 = %" PRId64,
          javaTimestampI64, ndkTimestampI64);

    ACameraMetadata_free(ndkResult);

    return (javaTimestampI64 == ndkTimestampI64);
}

static ACameraMetadata *sStashedMetadata = nullptr;

// Test holding on to a ACameraMetadata past a single local JNI call
extern "C" jboolean
Java_android_hardware_camera2_cts_CaptureResultTest_\
stashACameraMetadataFromCameraMetadataNative(
        JNIEnv* env, jclass /*clazz*/, jobject captureResult) {
    ALOGV("%s", __FUNCTION__);
    ACameraMetadata* ndkResult =
        ACameraMetadata_fromCameraMetadata(env, captureResult);
    if (ndkResult == nullptr) return false;
    sStashedMetadata = ndkResult;

    return true;
}

extern "C" jboolean
Java_android_hardware_camera2_cts_CaptureResultTest_\
validateStashedACameraMetadataFromCameraMetadataNative(
        JNIEnv* env, jclass /*clazz*/, jlong timestamp) {
    ALOGV("%s", __FUNCTION__);
    if (sStashedMetadata == nullptr) return false;

    camera_status_t ret;
    ACameraMetadata_const_entry entry;

    ret = ACameraMetadata_getConstEntry(sStashedMetadata, ACAMERA_SENSOR_TIMESTAMP,
        &entry);

    if (ret != ACAMERA_OK) {
        ALOGE("validateStashed failed: "
              "ACameraMetadata_getConstEntry returned %d.", ret);
        ACameraMetadata_free(sStashedMetadata);
        sStashedMetadata = nullptr;
        return false;
    }
    if (entry.type != ACAMERA_TYPE_INT64) {
        ALOGE("validateStashed failed: entry.type is %u but should be %u.",
              entry.type, ACAMERA_TYPE_INT64);
        ACameraMetadata_free(sStashedMetadata);
        sStashedMetadata = nullptr;
        return false;
    }
    if (entry.count != 1) {
        ALOGE("validateStashed failed: entry.count is %u but should be %u.",
              entry.count, 1);
        ACameraMetadata_free(sStashedMetadata);
        sStashedMetadata = nullptr;
        return false;
    }
    if (entry.data.i64 == nullptr) {
        ALOGE("validateStashed failed: entry.data.i64 is nullptr.");
        ACameraMetadata_free(sStashedMetadata);
        sStashedMetadata = nullptr;
        return false;
    }

    const int64_t javaTimestampI64 = static_cast<int64_t>(timestamp);
    const int64_t ndkTimestampI64 = *(entry.data.i64);

    ACameraMetadata_free(sStashedMetadata);
    sStashedMetadata = nullptr;
    ALOGV("javaTimestampI64 = %" PRId64 ", ndkTimestampI64 = %" PRId64,
          javaTimestampI64, ndkTimestampI64);
    return (javaTimestampI64 == ndkTimestampI64);

}



extern "C" jboolean
Java_android_hardware_camera2_cts_CameraManagerTest_\
validateACameraMetadataFromCameraMetadataCriticalTagsNative(
        JNIEnv* env, jclass /*clazz*/, jobject cameraCharacteristics,
        jint javaLensFacing) {
    ALOGV("%s", __FUNCTION__);
    ACameraMetadata* ndkCharacteristics =
        ACameraMetadata_fromCameraMetadata(env, cameraCharacteristics);
    if (!ndkCharacteristics) {
        ALOGE("validateCriticalTags failed: "
              "ACameraMetadata_fromCameraMetadata returned nullptr.");
        return false;
    }

    camera_status_t ret;
    ACameraMetadata_const_entry entry;

    ret = ACameraMetadata_getConstEntry(ndkCharacteristics,
        ACAMERA_LENS_FACING, &entry);
    ACameraMetadata_free(ndkCharacteristics);

    if (ret != ACAMERA_OK) {
        ALOGE("validateCriticalTags failed: "
              "ACameraMetadata_getConstEntry returned %d", ret);
        return false;
    }
    if (entry.type != ACAMERA_TYPE_BYTE) {
        ALOGE("validateCriticalTags failed: entry.type is %u but should be %u.",
              entry.type, ACAMERA_TYPE_BYTE);
        return false;
    }
    if (entry.count != 1) {
        ALOGE("validateCriticalTags failed: entry.count is %u but should be %u.",
              entry.count, 1);
        return false;
    }
    if (entry.data.u8 == nullptr) {
        ALOGE("validateCriticalTags failed: entry.data.u8 is nullptr.");
        return false;
    }

    const uint8_t javaLensFacingU8 = static_cast<uint8_t>(javaLensFacing);
    const uint8_t ndkLensFacingU8 = *(entry.data.u8);
    ALOGV("javaLensFacingU8 = %d, ndkLensFacingU8 = %d",
          javaLensFacingU8, ndkLensFacingU8);
    return (javaLensFacingU8 == ndkLensFacingU8);
}
