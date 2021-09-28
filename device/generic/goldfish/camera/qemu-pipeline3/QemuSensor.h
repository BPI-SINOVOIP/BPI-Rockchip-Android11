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

/*
 * This class is an abstraction to treat a capture device (e.g., a webcam)
 * connected to the host computer as an image sensor.  The capture device must
 * support both 360x240 and 640x480 resolutions.
 *
 * The characteristics of this sensor don't correspond to any actual sensor,
 * but are not far off typical sensors.
 */

#ifndef HW_EMULATOR_CAMERA2_QEMU_SENSOR_H
#define HW_EMULATOR_CAMERA2_QEMU_SENSOR_H

#include "fake-pipeline2/Base.h"
#include "QemuClient.h"

#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>
#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <utils/Timers.h>

namespace android {

class EmulatedFakeCamera2;

class QemuSensor: private Thread, public virtual RefBase {
  public:
   /*
    * Args:
    *     deviceName: File path where the capture device can be found (e.g.,
    *                 "/dev/video0").
    *     width: Width of pixel array.
    *     height: Height of pixel array.
    */
    QemuSensor(const char *deviceName, uint32_t width, uint32_t height,
               GraphicBufferMapper* gbm);
    ~QemuSensor();

    /*
     * Power Control
     */

    status_t startUp();
    status_t shutDown();

    /*
     * Controls that can be updated every frame.
     */

    void setFrameDuration(uint64_t ns);

    /*
     * Each Buffer in "buffers" must be at least stride*height*2 bytes in size.
     */
    void setDestinationBuffers(Buffers *buffers);
    /*
     * To simplify tracking the sensor's current frame.
     */
    void setFrameNumber(uint32_t frameNumber);

    /*
     * Synchronizing with sensor operation (vertical sync).
     */

    /*
     * Wait until the sensor outputs its next vertical sync signal, meaning it
     * is starting readout of its latest frame of data.
     *
     * Returns:
     *     true if vertical sync is signaled; false if the wait timed out.
     */
    bool waitForVSync(nsecs_t reltime);

    /*
     * Wait until a new frame has been read out, and then return the time
     * capture started. May return immediately if a new frame has been pushed
     * since the last wait for a new frame.
     *
     * Returns:
     *     true if new frame is returned; false if timed out.
     */
    bool waitForNewFrame(nsecs_t reltime, nsecs_t *captureTime);

    /*
     * Interrupt event servicing from the sensor. Only triggers for sensor
     * cycles that have valid buffers to write to.
     */
    struct QemuSensorListener {
        enum Event {
            EXPOSURE_START,
        };

        virtual void onQemuSensorEvent(uint32_t frameNumber, Event e,
                nsecs_t timestamp) = 0;
        virtual ~QemuSensorListener();
    };

    void setQemuSensorListener(QemuSensorListener *listener);

    /*
     * Static Sensor Characteristics
     */
    const uint32_t mWidth, mHeight;
    const uint32_t mActiveArray[4];

    static const nsecs_t kExposureTimeRange[2];
    static const nsecs_t kFrameDurationRange[2];
    static const nsecs_t kMinVerticalBlank;

    static const int32_t kSensitivityRange[2];
    static const uint32_t kDefaultSensitivity;

    static const char kHostCameraVerString[];

  private:
    int32_t mLastRequestWidth, mLastRequestHeight;

    /*
     * Defines possible states of the emulated camera device object.
     */
    enum EmulatedCameraDeviceState {
        // Object has been constructed.
        ECDS_CONSTRUCTED,
        // Object has been initialized.
        ECDS_INITIALIZED,
        // Object has been connected to the physical device.
        ECDS_CONNECTED,
        // Camera device has been started.
        ECDS_STARTED,
    };
    // Object state.
    EmulatedCameraDeviceState mState;

    CameraQemuClient mCameraQemuClient;
    const char *mDeviceName;
    GraphicBufferAllocator* mGBA;
    GraphicBufferMapper*    mGBM;

    // Always lock before accessing control parameters.
    Mutex mControlMutex;
    /*
     * Control Parameters
     */
    Condition mVSync;
    bool mGotVSync;
    uint64_t mFrameDuration;
    Buffers *mNextBuffers;
    uint32_t mFrameNumber;

    // Always lock before accessing readout variables.
    Mutex mReadoutMutex;
    /*
     * Readout Variables
     */
    Condition mReadoutAvailable;
    Condition mReadoutComplete;
    Buffers *mCapturedBuffers;
    nsecs_t mCaptureTime;
    QemuSensorListener *mListener;

    // Time of sensor startup (used for simulation zero-time point).
    nsecs_t mStartupTime;
    int32_t mHostCameraVer;

  private:
    /*
     * Inherited Thread Virtual Overrides
     */
    virtual status_t readyToRun();
    /*
     * QemuSensor capture operation main loop.
     */
    virtual bool threadLoop();

    /*
     * Members only used by the processing thread.
     */
    nsecs_t mNextCaptureTime;
    Buffers *mNextCapturedBuffers;

    void captureRGBA(uint32_t width, uint32_t height, uint32_t stride,
                     int64_t *timestamp, buffer_handle_t* handle);
    void captureYU12(uint32_t width, uint32_t height, uint32_t stride,
                     int64_t *timestamp, buffer_handle_t* handle);
    void captureRGBA(uint8_t *img, uint32_t width, uint32_t height,
                     uint32_t stride, int64_t *timestamp);
    void captureYU12(uint8_t *img, uint32_t width, uint32_t height,
                     uint32_t stride, int64_t *timestamp);
    void captureRGB(uint8_t *img, uint32_t width, uint32_t height,
                    uint32_t stride, int64_t *timestamp);
};

}; // end of namespace android

#endif // HW_EMULATOR_CAMERA2_QEMU_SENSOR_H
