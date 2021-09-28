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

#ifndef HW_EMULATOR_CAMERA_EMULATED_QEMU_CAMERA3_H
#define HW_EMULATOR_CAMERA_EMULATED_QEMU_CAMERA3_H

/*
 * Contains declaration of a class EmulatedQemuCamera3 that encapsulates
 * functionality of a video capture device that implements version 3 of the
 * camera device interface.
 */

#include "EmulatedCamera3.h"
#include "fake-pipeline2/JpegCompressor.h"
#include "qemu-pipeline3/QemuSensor.h"

#include <CameraMetadata.h>
#include <utils/SortedVector.h>
#include <utils/List.h>
#include <utils/Mutex.h>

using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;

namespace android {

/*
 * Encapsulates functionality for a v3 HAL camera which interfaces with a video
 * capture device on the host computer.
 *
 * NOTE: Currently, resolutions larger than 640x480 are susceptible to
 * performance problems.
 *
 * TODO: Optimize to allow regular usage of higher resolutions.
 */
class EmulatedQemuCamera3 : public EmulatedCamera3,
        private QemuSensor::QemuSensorListener {
public:
    EmulatedQemuCamera3(int cameraId, struct hw_module_t* module,
                        GraphicBufferMapper* gbm);
    virtual ~EmulatedQemuCamera3();

    /*
     * Args:
     *     deviceName: File path where the capture device can be found (e.g.,
     *                 "/dev/video0").
     *     frameDims: Space-delimited resolutions with each dimension delimited
     *                by a comma (e.g., "640,480 320,240").
     *     facingDir: Contains either "front" or "back".
     */
    virtual status_t Initialize(const char *deviceName,
                                const char *frameDims,
                                const char *facingDir);

    /**************************************************************************
     * Camera Module API and Generic Hardware Device API Implementation
     *************************************************************************/
    virtual status_t connectCamera(hw_device_t **device);
    virtual status_t closeCamera();
    virtual status_t getCameraInfo(struct camera_info *info);

protected:
    /**************************************************************************
     * EmulatedCamera3 Abstract API Implementation
     *************************************************************************/
    virtual status_t configureStreams(camera3_stream_configuration *streamList);
    virtual status_t registerStreamBuffers(
            const camera3_stream_buffer_set *bufferSet);
    virtual const camera_metadata_t* constructDefaultRequestSettings(int type);
    virtual status_t processCaptureRequest(camera3_capture_request *request);
    virtual status_t flush();

private:
    /*
     * Get the requested capability set (from boot properties) for this camera
     * and populate "mCapabilities".
     */
    status_t getCameraCapabilities();

    /*
     * Extracts supported resolutions into "mResolutions".
     *
     * Args:
     *     frameDims: A string of space-delimited resolutions with each
     *                dimension delimited by a comma (e.g., "640,480 320,240").
     */
    void parseResolutions(const char *frameDims);

    bool hasCapability(AvailableCapabilities cap);

    /*
     * Build the static info metadata buffer for this device.
     */
    status_t constructStaticInfo();

    status_t process3A(CameraMetadata &settings);

    status_t doFakeAE(CameraMetadata &settings);
    status_t doFakeAF(CameraMetadata &settings);
    status_t doFakeAWB(CameraMetadata &settings);
    void     update3A(CameraMetadata &settings);

    /*
     * Signal from readout thread that it doesn't have anything to do.
     */
    void signalReadoutIdle();

    /*
     * Handle interrupt events from the sensor.
     */
    void onQemuSensorEvent(uint32_t frameNumber, Event e, nsecs_t timestamp);
    using EmulatedCamera3::Initialize;

private:
    /**************************************************************************
     * Static Configuration Information
     *************************************************************************/
    static const uint32_t kMaxRawStreamCount = 0;
    static const uint32_t kMaxProcessedStreamCount = 3;
    static const uint32_t kMaxJpegStreamCount = 1;
    static const uint32_t kMaxReprocessStreamCount = 0;
    static const uint32_t kMaxBufferCount = 3;
    // We need a positive stream ID to distinguish external buffers from
    // sensor-generated buffers which use a nonpositive ID. Otherwise, HAL3 has
    // no concept of a stream id.
    static const uint32_t kGenericStreamId = 1;
    static const int32_t kAvailableFormats[];
    static const int64_t kSyncWaitTimeout = 10000000;  // 10 ms
    static const int32_t kMaxSyncTimeoutCount = 1000;  // 1000 kSyncWaitTimeouts
    static const uint32_t kFenceTimeoutMs = 2000;  // 2 s
    static const nsecs_t kJpegTimeoutNs = 5000000000l;  // 5 s

    /**************************************************************************
     * Data Members
     *************************************************************************/

    // HAL interface serialization lock.
    Mutex mLock;

    const char *mDeviceName;
    bool mFacingBack;
    uint32_t mSensorWidth;
    uint32_t mSensorHeight;
    std::vector<std::pair<int32_t,int32_t>> mResolutions;


    SortedVector<AvailableCapabilities> mCapabilities;

    /*
     * Cache for default templates. Once one is requested, the pointer must be
     * valid at least until close() is called on the device.
     */
    camera_metadata_t *mDefaultTemplates[CAMERA3_TEMPLATE_COUNT];

    // Private stream information, stored in camera3_stream_t->priv.
    struct PrivateStreamInfo {
        bool alive;
    };

    // Shortcut to the input stream.
    camera3_stream_t* mInputStream;
    GraphicBufferMapper* mGBM;

    typedef List<camera3_stream_t*> StreamList;
    typedef List<camera3_stream_t*>::iterator StreamIterator;
    typedef Vector<camera3_stream_buffer> HalBufferVector;

    // All streams, including input stream.
    StreamList mStreams;

    // Cached settings from latest submitted request.
    CameraMetadata mPrevSettings;

    // Fake Hardware Interfaces
    sp<QemuSensor> mSensor;
    sp<JpegCompressor> mJpegCompressor;
    friend class JpegCompressor;

    /*
     * Processing thread for sending out results.
     */
    class ReadoutThread : public Thread, private JpegCompressor::JpegListener {
      public:
        ReadoutThread(EmulatedQemuCamera3 *parent);
        ~ReadoutThread();

        struct Request {
            uint32_t frameNumber;
            CameraMetadata settings;
            HalBufferVector *buffers;
            Buffers *sensorBuffers;
        };

        /*
         * Interface to Parent Class
         */

        /*
         * Place request in the in-flight queue to wait for sensor capture.
         */
        void queueCaptureRequest(const Request &r);

        /*
         * Test if the readout thread is idle (no in-flight requests, not
         * currently reading out anything).
         */
        bool isIdle();

        /*
         * Wait until isIdle is true.
         */
        status_t waitForReadout();

      private:
        static const nsecs_t kWaitPerLoop = 10000000L;  // 10 ms
        static const nsecs_t kMaxWaitLoops = 1000;
        static const size_t kMaxQueueSize = 2;

        EmulatedQemuCamera3 *mParent;
        Mutex mLock;

        List<Request> mInFlightQueue;
        Condition mInFlightSignal;
        bool mThreadActive;

        virtual bool threadLoop();

        // Only accessed by threadLoop.
        Request mCurrentRequest;

        Mutex mJpegLock;
        bool mJpegWaiting;
        camera3_stream_buffer mJpegHalBuffer;
        uint32_t mJpegFrameNumber;

        /*
         * Jpeg Completion Callbacks
         */
        virtual void onJpegDone(const StreamBuffer &jpegBuffer, bool success);
        virtual void onJpegInputDone(const StreamBuffer &inputBuffer);
    };

    sp<ReadoutThread> mReadoutThread;

    /** Fake 3A constants */

    static const nsecs_t kNormalExposureTime;
    static const nsecs_t kFacePriorityExposureTime;
    static const int     kNormalSensitivity;
    static const int     kFacePrioritySensitivity;
    // Rate of converging AE to new target value, as fraction of difference between
    // current and target value.
    static const float   kExposureTrackRate;
    // Minimum duration for precapture state. May be longer if slow to converge
    // to target exposure
    static const int     kPrecaptureMinFrames;
    // How often to restart AE 'scanning'
    static const int     kStableAeMaxFrames;
    // Maximum stop below 'normal' exposure time that we'll wander to while
    // pretending to converge AE. In powers of 2. (-2 == 1/4 as bright)
    static const float   kExposureWanderMin;
    // Maximum stop above 'normal' exposure time that we'll wander to while
    // pretending to converge AE. In powers of 2. (2 == 4x as bright)
    static const float   kExposureWanderMax;

    /** Fake 3A state */

    uint8_t mControlMode;
    bool    mFacePriority;
    uint8_t mAeState;
    uint8_t mAfState;
    uint8_t mAwbState;
    uint8_t mAeMode;
    uint8_t mAfMode;
    uint8_t mAwbMode;

    int     mAeCounter;
    nsecs_t mAeCurrentExposureTime;
    nsecs_t mAeTargetExposureTime;
    int     mAeCurrentSensitivity;
};

}; // end of namespace android

#endif // HW_EMULATOR_CAMERA_EMULATED_QEMU_CAMERA3_H
