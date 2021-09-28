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

#ifndef HW_EMULATOR_CAMERA_EMULATED_FAKE_CAMERA_ROTATE_DEVICE_H
#define HW_EMULATOR_CAMERA_EMULATED_FAKE_CAMERA_ROTATE_DEVICE_H

/*
 * Contains declaration of a class EmulatedFakeRotatingCameraDevice that encapsulates
 * a fake camera device.
 */

#include "Converters.h"
#include "EmulatedCameraDevice.h"

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

namespace android {

class EmulatedFakeCamera;

/* Encapsulates a fake camera device.
 * Fake camera device emulates a camera device by providing frames containing
 * an image rendered by opengl, that takes rotating input from host
 */
class EmulatedFakeRotatingCameraDevice : public EmulatedCameraDevice {
public:
    /* Constructs EmulatedFakeRotatingCameraDevice instance. */
    explicit EmulatedFakeRotatingCameraDevice(EmulatedFakeCamera* camera_hal);

    /* Destructs EmulatedFakeRotatingCameraDevice instance. */
    ~EmulatedFakeRotatingCameraDevice();

    /***************************************************************************
     * Emulated camera device abstract interface implementation.
     * See declarations of these methods in EmulatedCameraDevice class for
     * information on each of these methods.
     **************************************************************************/

public:
    /* Connects to the camera device.
     * Since there is no real device to connect to, this method does nothing,
     * but changes the state.
     */
    status_t connectDevice();

    /* Disconnects from the camera device.
     * Since there is no real device to disconnect from, this method does
     * nothing, but changes the state.
     */
    status_t disconnectDevice();

    /* Starts the camera device. */
    status_t startDevice(int width, int height, uint32_t pix_fmt);

    /* Stops the camera device. */
    status_t stopDevice();


protected:
    /* Implementation of the frame production routine. */
    bool produceFrame(void* buffer, int64_t* timestamp) override;

    /****************************************************************************
     * Fake camera device private API
     ***************************************************************************/

private:

    void fillBuffer(void* buffer);
    void render(int width, int height);
    int init_gl_surface(int width, int height);
    void get_eye_x_y_z(float* x, float* y, float*z);
    void get_yawing(float* x, float* y, float*z);
    void read_rotation_vector(double *yaw, double* pitch, double* roll);
    void read_sensor();
    void init_sensor();
    void free_gl_surface(void);
    void update_scene(float width, float height);
    void create_texture_dotx(int width, int height);

    bool mOpenglReady = false;
    EGLDisplay mEglDisplay;
    EGLSurface mEglSurface;
    EGLContext mEglContext;
    GLuint mTexture;
    uint8_t* mPixelBuf;// = new uint8_t[width * height * kGlBytesPerPixel];;
    int mSensorPipe = -1;
    enum SENSOR_VALUE_TYPE {
        SENSOR_VALUE_ACCEL_X=0,
        SENSOR_VALUE_ACCEL_Y=1,
        SENSOR_VALUE_ACCEL_Z=2,
        SENSOR_VALUE_MAGNETIC_X=3,
        SENSOR_VALUE_MAGNETIC_Y=4,
        SENSOR_VALUE_MAGNETIC_Z=5,
        SENSOR_VALUE_ROTATION_X=6,
        SENSOR_VALUE_ROTATION_Y=7,
        SENSOR_VALUE_ROTATION_Z=8,
    };

    float mSensorValues[9] = {0};

};

}; /* namespace android */

#endif  /* HW_EMULATOR_CAMERA_EMULATED_FAKE_CAMERA_ROTATE_DEVICE_H */
