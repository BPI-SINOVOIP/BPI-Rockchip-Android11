/*
 * Copyright (C) 2017 The Android Open Source Project
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

// Uncomment LOG_NDEBUG to enable verbose logging, and uncomment both LOG_NDEBUG
// *and* LOG_NNDEBUG to enable very verbose logging.

//#define LOG_NDEBUG 0
//#define LOG_NNDEBUG 0

#define LOG_TAG "EmulatedCamera3_QemuSensor"
#define ATRACE_TAG ATRACE_TAG_CAMERA

#ifdef LOG_NNDEBUG
#define ALOGVV(...) ALOGV(__VA_ARGS__)
#else
#define ALOGVV(...) ((void)0)
#endif

#include "qemu-pipeline3/QemuSensor.h"
#include "system/camera_metadata.h"
#include <gralloc_cb_bp.h>

#include <cmath>
#include <cstdlib>
#include <linux/videodev2.h>
#include <log/log.h>
#include <cutils/properties.h>
#include <ui/Rect.h>
#include <utils/Trace.h>

namespace android {

const nsecs_t QemuSensor::kExposureTimeRange[2] =
        {1000L, 300000000L};  // 1 us - 0.3 sec
const nsecs_t QemuSensor::kFrameDurationRange[2] =
        {33331760L, 300000000L};  // ~1/30 s - 0.3 sec
const nsecs_t QemuSensor::kMinVerticalBlank = 10000L;

const int32_t QemuSensor::kSensitivityRange[2] = {100, 1600};
const uint32_t QemuSensor::kDefaultSensitivity = 100;

const char QemuSensor::kHostCameraVerString[] = "ro.kernel.qemu.camera_protocol_ver";

QemuSensor::QemuSensor(const char *deviceName, uint32_t width, uint32_t height,
                       GraphicBufferMapper* gbm):
        Thread(false),
        mWidth(width),
        mHeight(height),
        mActiveArray{0, 0, width, height},
        mLastRequestWidth(-1),
        mLastRequestHeight(-1),
        mCameraQemuClient(),
        mDeviceName(deviceName),
        mGBA(&GraphicBufferAllocator::get()),
        mGBM(gbm),
        mGotVSync(false),
        mFrameDuration(kFrameDurationRange[0]),
        mNextBuffers(nullptr),
        mFrameNumber(0),
        mCapturedBuffers(nullptr),
        mListener(nullptr) {
    mHostCameraVer = property_get_int32(kHostCameraVerString, 0);
    ALOGV("QemuSensor created with pixel array %d x %d", width, height);
}

QemuSensor::~QemuSensor() {
    shutDown();
}

status_t QemuSensor::startUp() {
    ALOGV("%s: Entered", __FUNCTION__);

    mCapturedBuffers = nullptr;
    status_t res = run("EmulatedQemuCamera3::QemuSensor",
            ANDROID_PRIORITY_URGENT_DISPLAY);

    if (res != OK) {
        ALOGE("Unable to start up sensor capture thread: %d", res);
    }

    char connect_str[256];
    snprintf(connect_str, sizeof(connect_str), "name=%s", mDeviceName);
    res = mCameraQemuClient.connectClient(connect_str);
    if (res != NO_ERROR) {
        return res;
    }

    res = mCameraQemuClient.queryConnect();
    if (res == NO_ERROR) {
        ALOGV("%s: Connected to device '%s'",
                __FUNCTION__, (const char*) mDeviceName);
        mState = ECDS_CONNECTED;
    } else {
        ALOGE("%s: Connection to device '%s' failed",
                __FUNCTION__, (const char*) mDeviceName);
    }

    return res;
}

status_t QemuSensor::shutDown() {
    ALOGV("%s: Entered", __FUNCTION__);

    status_t res = requestExitAndWait();
    if (res != OK) {
        ALOGE("Unable to shut down sensor capture thread: %d", res);
    }

    /* Stop the actual camera device. */
    res = mCameraQemuClient.queryStop();
    if (res == NO_ERROR) {
        mState = ECDS_CONNECTED;
        ALOGV("%s: Qemu camera device '%s' is stopped",
                __FUNCTION__, (const char*) mDeviceName);
    } else {
        ALOGE("%s: Unable to stop device '%s'",
                __FUNCTION__, (const char*) mDeviceName);
    }

    return res;
}

void QemuSensor::setFrameDuration(uint64_t ns) {
    Mutex::Autolock lock(mControlMutex);
    ALOGVV("Frame duration set to %f", ns/1000000.f);
    mFrameDuration = ns;
}

void QemuSensor::setDestinationBuffers(Buffers *buffers) {
    Mutex::Autolock lock(mControlMutex);
    mNextBuffers = buffers;
}

void QemuSensor::setFrameNumber(uint32_t frameNumber) {
    Mutex::Autolock lock(mControlMutex);
    mFrameNumber = frameNumber;
}

bool QemuSensor::waitForVSync(nsecs_t reltime) {
    int res;
    Mutex::Autolock lock(mControlMutex);

    mGotVSync = false;
    res = mVSync.waitRelative(mControlMutex, reltime);
    if (res != OK && res != TIMED_OUT) {
        ALOGE("%s: Error waiting for VSync signal: %d", __FUNCTION__, res);
        return false;
    }
    return mGotVSync;
}

bool QemuSensor::waitForNewFrame(nsecs_t reltime, nsecs_t *captureTime) {
    Mutex::Autolock lock(mReadoutMutex);
    if (mCapturedBuffers == nullptr) {
        int res;
        res = mReadoutAvailable.waitRelative(mReadoutMutex, reltime);
        if (res == TIMED_OUT) {
            return false;
        } else if (res != OK || mCapturedBuffers == nullptr) {
            ALOGE("Error waiting for sensor readout signal: %d", res);
            return false;
        }
    }
    mReadoutComplete.signal();

    *captureTime = mCaptureTime;
    mCapturedBuffers = nullptr;
    return true;
}

QemuSensor::QemuSensorListener::~QemuSensorListener() {
}

void QemuSensor::setQemuSensorListener(QemuSensorListener *listener) {
    Mutex::Autolock lock(mControlMutex);
    mListener = listener;
}

status_t QemuSensor::readyToRun() {
    ALOGV("Starting up sensor thread");
    mStartupTime = systemTime();
    mNextCaptureTime = 0;
    mNextCapturedBuffers = nullptr;
    return OK;
}

bool QemuSensor::threadLoop() {
    ATRACE_CALL();
    /*
     * Stages are out-of-order relative to a single frame's processing, but
     * in-order in time.
     */

    /*
     * Stage 1: Read in latest control parameters.
     */
    uint64_t frameDuration;
    Buffers *nextBuffers;
    uint32_t frameNumber;
    QemuSensorListener *listener = nullptr;
    {
        // Lock while we're grabbing readout variables.
        Mutex::Autolock lock(mControlMutex);
        frameDuration = mFrameDuration;
        nextBuffers = mNextBuffers;
        frameNumber = mFrameNumber;
        listener = mListener;
        // Don't reuse a buffer set.
        mNextBuffers = nullptr;

        // Signal VSync for start of readout.
        ALOGVV("QemuSensor VSync");
        mGotVSync = true;
        mVSync.signal();
    }

    /*
     * Stage 3: Read out latest captured image.
     */

    Buffers *capturedBuffers = nullptr;
    nsecs_t captureTime = 0;

    nsecs_t startRealTime = systemTime();
    /*
     * Stagefright cares about system time for timestamps, so base simulated
     * time on that.
     */
    nsecs_t simulatedTime = startRealTime;
    nsecs_t frameEndRealTime = startRealTime + frameDuration;

    if (mNextCapturedBuffers != nullptr) {
        ALOGVV("QemuSensor starting readout");
        /*
         * Pretend we're doing readout now; will signal once enough time has
         * elapsed.
         */
        capturedBuffers = mNextCapturedBuffers;
        captureTime = mNextCaptureTime;
    }

    /*
     * TODO: Move this signal to another thread to simulate readout time
     * properly.
     */
    if (capturedBuffers != nullptr) {
        ALOGVV("QemuSensor readout complete");
        Mutex::Autolock lock(mReadoutMutex);
        if (mCapturedBuffers != nullptr) {
            ALOGV("Waiting for readout thread to catch up!");
            mReadoutComplete.wait(mReadoutMutex);
        }

        mCapturedBuffers = capturedBuffers;
        mCaptureTime = captureTime;
        mReadoutAvailable.signal();
        capturedBuffers = nullptr;
    }

    /*
     * Stage 2: Capture new image.
     */
    mNextCaptureTime = simulatedTime;
    mNextCapturedBuffers = nextBuffers;

    if (mNextCapturedBuffers != nullptr) {

        int64_t timestamp = 0L;

        // Might be adding more buffers, so size isn't constant.
        for (size_t i = 0; i < mNextCapturedBuffers->size(); ++i) {
            const StreamBuffer &b = (*mNextCapturedBuffers)[i];
            ALOGVV("QemuSensor capturing buffer %d: stream %d,"
                    " %d x %d, format %x, stride %d, buf %p, img %p",
                    i, b.streamId, b.width, b.height, b.format, b.stride,
                    b.buffer, b.img);
            switch (b.format) {
                case HAL_PIXEL_FORMAT_RGB_888:
                    captureRGB(b.img, b.width, b.height, b.stride, &timestamp);
                    break;
                case HAL_PIXEL_FORMAT_RGBA_8888:
                    if (mHostCameraVer == 1) {
                        captureRGBA(b.width, b.height, b.stride, &timestamp, b.buffer);
                    } else {
                        captureRGBA(b.img, b.width, b.height, b.stride, &timestamp);
                    }
                    break;
                case HAL_PIXEL_FORMAT_BLOB:
                    if (b.dataSpace == HAL_DATASPACE_DEPTH) {
                        ALOGE("%s: Depth clouds unsupported", __FUNCTION__);
                    } else {
                        /*
                         * Add auxillary buffer of the right size. Assumes only
                         * one BLOB (JPEG) buffer is in mNextCapturedBuffers.
                         */
                        StreamBuffer bAux;
                        bAux.streamId = 0;
                        bAux.width = b.width;
                        bAux.height = b.height;
                        bAux.format = HAL_PIXEL_FORMAT_YCbCr_420_888;
                        bAux.stride = b.width;
                        if (mHostCameraVer == 1) {
                            const uint64_t usage =
                                GRALLOC_USAGE_HW_CAMERA_READ |
                                GRALLOC_USAGE_HW_CAMERA_WRITE |
                                GRALLOC_USAGE_HW_TEXTURE;
                            const uint64_t graphicBufferId = 0; // not used
                            const uint32_t layerCount = 1;
                            buffer_handle_t handle;
                            uint32_t stride;

                            status_t status = mGBA->allocate(
                                bAux.width, bAux.height, bAux.format,
                                layerCount, usage,
                                &handle, &stride,
                                graphicBufferId, "QemuSensor");
                            if (status != OK) {
                                LOG_ALWAYS_FATAL("allocate failed");
                            }

                            android_ycbcr ycbcr = {};
                            mGBM->lockYCbCr(handle,
                                            GRALLOC_USAGE_HW_CAMERA_WRITE,
                                            Rect(0, 0, bAux.width, bAux.height),
                                            &ycbcr);

                            bAux.buffer = new buffer_handle_t;
                            *bAux.buffer = handle;
                            bAux.img = (uint8_t*)ycbcr.y;
                        } else {
                            bAux.buffer = nullptr;
                            // TODO: Reuse these.
                            bAux.img = new uint8_t[b.width * b.height * 3];
                        }
                        mNextCapturedBuffers->push_back(bAux);
                    }
                    break;
                case HAL_PIXEL_FORMAT_YCbCr_420_888:
                    if (mHostCameraVer == 1) {
                        captureYU12(b.width, b.height, b.stride, &timestamp, b.buffer);
                    } else {
                        captureYU12(b.img, b.width, b.height, b.stride, &timestamp);
                    }
                    break;
                default:
                    ALOGE("%s: Unknown/unsupported format %x, no output",
                            __FUNCTION__, b.format);
                    break;
            }
        }
        if (timestamp != 0UL) {
          mNextCaptureTime = timestamp;
        }
        // Note: we have to do this after the actual capture so that the
        // capture time is accurate as reported from QEMU.
        if (listener != nullptr) {
            listener->onQemuSensorEvent(frameNumber, QemuSensorListener::EXPOSURE_START,
                                        mNextCaptureTime);
        }
    }

    ALOGVV("QemuSensor vertical blanking interval");
    nsecs_t workDoneRealTime = systemTime();
    const nsecs_t timeAccuracy = 2e6;  // 2 ms of imprecision is ok.
    if (workDoneRealTime < frameEndRealTime - timeAccuracy) {
        timespec t;
        t.tv_sec = (frameEndRealTime - workDoneRealTime) / 1000000000L;
        t.tv_nsec = (frameEndRealTime - workDoneRealTime) % 1000000000L;

        int ret;
        do {
            ret = nanosleep(&t, &t);
        } while (ret != 0);
    }
    ALOGVV("Frame cycle took %d ms, target %d ms",
            (int) ((systemTime() - startRealTime) / 1000000),
            (int) (frameDuration / 1000000));
    return true;
}

void QemuSensor::captureRGBA(uint8_t *img, uint32_t width, uint32_t height,
        uint32_t stride, int64_t *timestamp) {
    ATRACE_CALL();
    status_t res;
    if (width != (uint32_t)mLastRequestWidth ||
        height != (uint32_t)mLastRequestHeight) {
        ALOGI("%s: Dimensions for the current request (%dx%d) differ "
              "from the previous request (%dx%d). Restarting camera",
                __FUNCTION__, width, height, mLastRequestWidth,
                mLastRequestHeight);

        if (mLastRequestWidth != -1 || mLastRequestHeight != -1) {
            // We only need to stop the camera if this isn't the first request.

            // Stop the camera device.
            res = mCameraQemuClient.queryStop();
            if (res == NO_ERROR) {
                mState = ECDS_CONNECTED;
                ALOGV("%s: Qemu camera device '%s' is stopped",
                        __FUNCTION__, (const char*) mDeviceName);
            } else {
                ALOGE("%s: Unable to stop device '%s'",
                        __FUNCTION__, (const char*) mDeviceName);
            }
        }

        /*
         * Host Camera always assumes V4L2_PIX_FMT_RGB32 as the preview format,
         * and asks for the video format from the pixFmt parameter, which is
         * V4L2_PIX_FMT_YUV420 in our implementation.
         */
        uint32_t pixFmt = V4L2_PIX_FMT_YUV420;
        res = mCameraQemuClient.queryStart(pixFmt, width, height);
        if (res == NO_ERROR) {
            mLastRequestWidth = width;
            mLastRequestHeight = height;
            ALOGV("%s: Qemu camera device '%s' is started for %.4s[%dx%d] frames",
                    __FUNCTION__, (const char*) mDeviceName,
                    reinterpret_cast<const char*>(&pixFmt),
                    mWidth, mHeight);
            mState = ECDS_STARTED;
        } else {
            ALOGE("%s: Unable to start device '%s' for %.4s[%dx%d] frames",
                    __FUNCTION__, (const char*) mDeviceName,
                    reinterpret_cast<const char*>(&pixFmt),
                    mWidth, mHeight);
            return;
        }
    }
    if (width != stride) {
        ALOGW("%s: expect stride (%d), actual stride (%d)", __FUNCTION__,
              width, stride);
    }

    // Since the format is V4L2_PIX_FMT_RGB32, we need 4 bytes per pixel.
      size_t bufferSize = width * height * 4;
      // Apply no white balance or exposure compensation.
      float whiteBalance[] = {1.0f, 1.0f, 1.0f};
      float exposureCompensation = 1.0f;
      // Read from webcam.
      mCameraQemuClient.queryFrame(nullptr, img, 0, bufferSize, whiteBalance[0],
              whiteBalance[1], whiteBalance[2],
              exposureCompensation, timestamp);

    ALOGVV("RGBA sensor image captured");
}

void QemuSensor::captureRGBA(uint32_t width, uint32_t height,
        uint32_t stride, int64_t *timestamp, buffer_handle_t* handle) {
    ATRACE_CALL();
    status_t res;
    if (mLastRequestWidth == -1 || mLastRequestHeight == -1) {
        uint32_t pixFmt = V4L2_PIX_FMT_YUV420;
        res = mCameraQemuClient.queryStart();
        if (res == NO_ERROR) {
            mLastRequestWidth = width;
            mLastRequestHeight = height;
            ALOGV("%s: Qemu camera device '%s' is started for %.4s[%dx%d] frames",
                    __FUNCTION__, (const char*) mDeviceName,
                    reinterpret_cast<const char*>(&pixFmt),
                    mWidth, mHeight);
            mState = ECDS_STARTED;
        } else {
            ALOGE("%s: Unable to start device '%s' for %.4s[%dx%d] frames",
                    __FUNCTION__, (const char*) mDeviceName,
                    reinterpret_cast<const char*>(&pixFmt),
                    mWidth, mHeight);
            return;
        }
    }
    if (width != stride) {
        ALOGW("%s: expect stride (%d), actual stride (%d)", __FUNCTION__,
              width, stride);
    }

    float whiteBalance[] = {1.0f, 1.0f, 1.0f};
    float exposureCompensation = 1.0f;
    const cb_handle_t* cb = cb_handle_t::from(*handle);
    LOG_ALWAYS_FATAL_IF(!cb, "Unexpected buffer handle");
    const uint64_t offset = cb->getMmapedOffset();
    mCameraQemuClient.queryFrame(width, height, V4L2_PIX_FMT_RGB32, offset,
                                 whiteBalance[0], whiteBalance[1], whiteBalance[2],
                                 exposureCompensation, timestamp);

    ALOGVV("RGBA sensor image captured");
}

void QemuSensor::captureRGB(uint8_t *img, uint32_t width, uint32_t height, uint32_t stride, int64_t *timestamp) {
    ALOGE("%s: Not implemented", __FUNCTION__);
}

void QemuSensor::captureYU12(uint8_t *img, uint32_t width, uint32_t height, uint32_t stride,
                             int64_t *timestamp) {
    ATRACE_CALL();
    status_t res;
    if (width != (uint32_t)mLastRequestWidth ||
        height != (uint32_t)mLastRequestHeight) {
        ALOGI("%s: Dimensions for the current request (%dx%d) differ "
              "from the previous request (%dx%d). Restarting camera",
                __FUNCTION__, width, height, mLastRequestWidth,
                mLastRequestHeight);

        if (mLastRequestWidth != -1 || mLastRequestHeight != -1) {
            // We only need to stop the camera if this isn't the first request.
            // Stop the camera device.
            res = mCameraQemuClient.queryStop();
            if (res == NO_ERROR) {
                mState = ECDS_CONNECTED;
                ALOGV("%s: Qemu camera device '%s' is stopped",
                        __FUNCTION__, (const char*) mDeviceName);
            } else {
                ALOGE("%s: Unable to stop device '%s'",
                        __FUNCTION__, (const char*) mDeviceName);
            }
        }

        /*
         * Host Camera always assumes V4L2_PIX_FMT_RGB32 as the preview format,
         * and asks for the video format from the pixFmt parameter, which is
         * V4L2_PIX_FMT_YUV420 in our implementation.
         */
        uint32_t pixFmt = V4L2_PIX_FMT_YUV420;
        res = mCameraQemuClient.queryStart(pixFmt, width, height);
        if (res == NO_ERROR) {
            mLastRequestWidth = width;
            mLastRequestHeight = height;
            ALOGV("%s: Qemu camera device '%s' is started for %.4s[%dx%d] frames",
                    __FUNCTION__, (const char*) mDeviceName,
                    reinterpret_cast<const char*>(&pixFmt),
                    mWidth, mHeight);
            mState = ECDS_STARTED;
        } else {
            ALOGE("%s: Unable to start device '%s' for %.4s[%dx%d] frames",
                    __FUNCTION__, (const char*) mDeviceName,
                    reinterpret_cast<const char*>(&pixFmt),
                    mWidth, mHeight);
            return;
        }
    }
    if (width != stride) {
        ALOGW("%s: expect stride (%d), actual stride (%d)", __FUNCTION__,
              width, stride);
    }

    // Calculate the buffer size for YUV420.
    size_t bufferSize = (width * height * 12) / 8;
    // Apply no white balance or exposure compensation.
    float whiteBalance[] = {1.0f, 1.0f, 1.0f};
    float exposureCompensation = 1.0f;
    // Read video frame from webcam.
    mCameraQemuClient.queryFrame(img, nullptr, bufferSize, 0, whiteBalance[0],
            whiteBalance[1], whiteBalance[2],
            exposureCompensation, timestamp);

    ALOGVV("YUV420 sensor image captured");
}

void QemuSensor::captureYU12(uint32_t width, uint32_t height, uint32_t stride,
                             int64_t *timestamp, buffer_handle_t* handle) {
    ATRACE_CALL();
    status_t res;
    if (mLastRequestWidth == -1 || mLastRequestHeight == -1) {
        uint32_t pixFmt = V4L2_PIX_FMT_YUV420;
        res = mCameraQemuClient.queryStart();
        if (res == NO_ERROR) {
            mLastRequestWidth = width;
            mLastRequestHeight = height;
            ALOGV("%s: Qemu camera device '%s' is started for %.4s[%dx%d] frames",
                    __FUNCTION__, (const char*) mDeviceName,
                    reinterpret_cast<const char*>(&pixFmt),
                    mWidth, mHeight);
            mState = ECDS_STARTED;
        } else {
            ALOGE("%s: Unable to start device '%s' for %.4s[%dx%d] frames",
                    __FUNCTION__, (const char*) mDeviceName,
                    reinterpret_cast<const char*>(&pixFmt),
                    mWidth, mHeight);
            return;
        }
    }
    if (width != stride) {
        ALOGW("%s: expect stride (%d), actual stride (%d)", __FUNCTION__,
              width, stride);
    }

    float whiteBalance[] = {1.0f, 1.0f, 1.0f};
    float exposureCompensation = 1.0f;
    const cb_handle_t* cb = cb_handle_t::from(*handle);
    LOG_ALWAYS_FATAL_IF(!cb, "Unexpected buffer handle");
    const uint64_t offset = cb->getMmapedOffset();
    mCameraQemuClient.queryFrame(width, height, V4L2_PIX_FMT_YUV420, offset,
                                 whiteBalance[0], whiteBalance[1], whiteBalance[2],
                                 exposureCompensation, timestamp);
    ALOGVV("YUV420 sensor image captured");
}

}; // end of namespace android
