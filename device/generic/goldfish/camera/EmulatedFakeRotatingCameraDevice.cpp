/*
 * Copyright (C) 2011 The Android Open Source Project
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
 * Contains implementation of a class EmulatedFakeRotatingCameraDevice that encapsulates
 * fake camera device.
 */

#define GL_GLEXT_PROTOTYPES
#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera_FakeDevice"
#define FAKE_CAMERA_SENSOR "FakeRotatingCameraSensor"
#include <log/log.h>
#include "EmulatedFakeCamera.h"
#include "EmulatedFakeRotatingCameraDevice.h"
#include <qemu_pipe_bp.h>

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ui/DisplayInfo.h>
#include <fcntl.h>

#undef min
#undef max
#include <algorithm>

namespace android {

// include the dots pattern directly, it is NV21 format
#include "acircles_pattern_1280_720.c"

// ----------------------------------------------------------------------------

static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE) {
    if (returnVal != EGL_TRUE) {
        ALOGE("%s() returned %d\n", op, returnVal);
    }

    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error
            = eglGetError()) {
        ALOGE("after %s() eglError (0x%x)\n", op, error);
    }
}

static signed clamp_rgb(signed value) {
    if (value > 255) {
        value = 255;
    } else if (value < 0) {
        value = 0;
    }
    return value;
}

static void rgba8888_to_nv21(uint8_t* input, uint8_t* output, int width, int height) {
    int align = 16;
    int yStride = (width + (align -1)) & ~(align-1);
    uint8_t* outputVU = output + height*yStride;
    for (int j = 0; j < height; ++j) {
        uint8_t* outputY = output + j*yStride;
        for (int i = 0; i < width; ++i) {
            uint8_t R = input[j*width*4 + i*4];
            uint8_t G = input[j*width*4 + i*4 + 1];
            uint8_t B = input[j*width*4 + i*4 + 2];
            uint8_t Y = clamp_rgb((77 * R + 150 * G +  29 * B) >> 8);
            *outputY++ = Y;
            bool jeven = (j & 1) == 0;
            bool ieven = (i & 1) == 0;
            if (jeven && ieven) {
                uint8_t V = clamp_rgb((( 128 * R - 107 * G - 21 * B) >> 8) + 128);
                uint8_t U = clamp_rgb((( -43 * R - 85 * G + 128 * B) >> 8) + 128);
                *outputVU++ = V;
                *outputVU++ = U;
            }
        }
    }
}

static void nv21_to_rgba8888(uint8_t* input, uint32_t * output, int width, int height) {
    int align = 16;
    int yStride = (width + (align -1)) & ~(align-1);
    uint8_t* inputVU = input + height*yStride;
    uint8_t Y, U, V;
    for (int j = 0; j < height; ++j) {
        uint8_t* inputY = input + j*yStride;
        for (int i = 0; i < width; ++i) {
            Y = *inputY++;
            bool jeven = (j & 1) == 0;
            bool ieven = (i & 1) == 0;
            if (jeven && ieven) {
                V = *inputVU++;
                U = *inputVU++;
            }
            *output++ = YUVToRGB32(Y,U,V);
        }
    }
}

void EmulatedFakeRotatingCameraDevice::render(int width, int height)
{
    update_scene((float)width, (float)height);
    create_texture_dotx(1280, 720);

    int w= 992/2;
    int h = 1280/2;
    const GLfloat verticesfloat[] = {
             -w,  -h,  0,
              w,  -h,  0,
              w,   h,  0,
             -w,   h,  0
    };

    const GLfloat texCoordsfloat[] = {
            0,       0,
            1.0f,    0,
            1.0f,    1.0f,
            0,       1.0f
    };

    const GLushort indices[] = { 0, 1, 2,  0, 2, 3 };

    glVertexPointer(3, GL_FLOAT, 0, verticesfloat);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoordsfloat);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    int nelem = sizeof(indices)/sizeof(indices[0]);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_TRIANGLES, nelem, GL_UNSIGNED_SHORT, indices);
    glFinish();
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, mPixelBuf);
}

static void get_color(uint32_t* img, int i, int j, int w, int h, int dw, uint32_t * color) {
    int mini = dw/2 - w/2;
    int minj = dw/2 - h/2;
    int maxi = mini + w -1;
    int maxj = minj + h -1;

    if ( i >= mini && i <= maxi && j >= minj && j <= maxj) {
        *color = img[i-mini + dw*(j-minj)];
    }
}

static void convert_to_square(uint32_t* src, uint32_t* dest, int sw, int sh, int dw) {
    for (int i=0; i < dw; ++i) {
        for (int j=0; j < dw; ++j) {
            uint32_t color=0;
            get_color(src, i, j, sw, sh, dw, &color);
            dest[i+j*dw] = color;
        }
    }
}

void EmulatedFakeRotatingCameraDevice::create_texture_dotx(int width, int height) {
    uint32_t* myrgba = new uint32_t[width * height];
    nv21_to_rgba8888(rawData, myrgba, width, height);
    uint32_t* myrgba2 = new uint32_t[width * width];
    convert_to_square(myrgba, myrgba2, width, height, width);

    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, width, 0, GL_RGBA, GL_UNSIGNED_BYTE, myrgba2);
    //glGenerateMipmapOES does not work on mac, dont use it.
    //glGenerateMipmapOES(GL_TEXTURE_2D);
    // need to use linear, otherwise the dots will have sharp edges
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    delete[] myrgba;
    delete[] myrgba2;
}


static void gluLookAt(float eyeX, float eyeY, float eyeZ,
        float centerX, float centerY, float centerZ, float upX, float upY,
        float upZ)
{
    // See the OpenGL GLUT documentation for gluLookAt for a description
    // of the algorithm. We implement it in a straightforward way:

    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;
    float flf = 1.0f / sqrt(fx * fx + fy * fy + fz * fz);
    fx *= flf;
    fy *= flf;
    fz *= flf;

    // compute s = f x up (x means "cross product")

    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;
    float slf = 1.0f / sqrt(sx * sx + sy * sy + sz * sz);
    sx *= slf;
    sy *= slf;
    sz *= slf;

    // compute u = s x f
    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;
    float ulf = 1.0f / sqrt(ux * ux + uy * uy + uz * uz);
    ux *= ulf;
    uy *= ulf;
    uz *= ulf;

    float m[16] ;
    m[0] = sx;
    m[1] = ux;
    m[2] = -fx;
    m[3] = 0.0f;

    m[4] = sy;
    m[5] = uy;
    m[6] = -fy;
    m[7] = 0.0f;

    m[8] = sz;
    m[9] = uz;
    m[10] = -fz;
    m[11] = 0.0f;

    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;

    glMultMatrixf(m);
    glTranslatef(-eyeX, -eyeY, -eyeZ);
}

void EmulatedFakeRotatingCameraDevice::update_scene(float width, float height)
{
    float ratio = width / height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-ratio/2.0, ratio/2.0, -1/2.0, 1/2.0, 1, 40000);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float up_x=-1;
    float up_y=0;
    float up_z=0;
    get_yawing(&up_x, &up_y, &up_z);
    float eye_x=0;
    float eye_y=0;
    float eye_z=2000;
    get_eye_x_y_z(&eye_x, &eye_y, &eye_z);
    gluLookAt( eye_x, eye_y, eye_z, 0, 0, 0, up_x, up_y, up_z);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

void EmulatedFakeRotatingCameraDevice::free_gl_surface(void)
{
    if (mEglDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent( EGL_NO_DISPLAY, EGL_NO_SURFACE,
                EGL_NO_SURFACE, EGL_NO_CONTEXT );
        eglDestroyContext( mEglDisplay, mEglContext );
        eglDestroySurface( mEglDisplay, mEglSurface );
        eglTerminate( mEglDisplay );
        mEglDisplay = EGL_NO_DISPLAY;
    }
}

void EmulatedFakeRotatingCameraDevice::init_sensor() {
    if (mSensorPipe >=0) return;
    // create a sensor pipe
    mSensorPipe = qemu_pipe_open_ns(NULL, FAKE_CAMERA_SENSOR, O_RDWR);
    if (mSensorPipe < 0) {
        ALOGE("cannot open %s", FAKE_CAMERA_SENSOR);
    } else {
        ALOGD("successfully opened %s", FAKE_CAMERA_SENSOR);
    }
}

void EmulatedFakeRotatingCameraDevice::read_sensor() {
    if (mSensorPipe < 0) return;
    char get[] = "get";
    int pipe_command_length = sizeof(get);
    qemu_pipe_write_fully(mSensorPipe, &pipe_command_length, sizeof(pipe_command_length));
    qemu_pipe_write_fully(mSensorPipe, get, pipe_command_length);
    qemu_pipe_read_fully(mSensorPipe, &pipe_command_length, sizeof(pipe_command_length));
    qemu_pipe_read_fully(mSensorPipe, &mSensorValues, pipe_command_length);
    assert(pipe_command_length == 9*sizeof(float));
    ALOGD("accel: %g %g %g; magnetic %g %g %g orientation %g %g %g",
            mSensorValues[SENSOR_VALUE_ACCEL_X], mSensorValues[SENSOR_VALUE_ACCEL_Y],
            mSensorValues[SENSOR_VALUE_ACCEL_Z],
            mSensorValues[SENSOR_VALUE_MAGNETIC_X], mSensorValues[SENSOR_VALUE_MAGNETIC_Y],
            mSensorValues[SENSOR_VALUE_MAGNETIC_Y],
            mSensorValues[SENSOR_VALUE_ROTATION_X], mSensorValues[SENSOR_VALUE_ROTATION_Y],
            mSensorValues[SENSOR_VALUE_ROTATION_Z]);
}

void EmulatedFakeRotatingCameraDevice::read_rotation_vector(double *yaw, double* pitch, double* roll) {
    read_sensor();
    *yaw = mSensorValues[SENSOR_VALUE_ROTATION_Z];
    *pitch = mSensorValues[SENSOR_VALUE_ROTATION_X];
    *roll = mSensorValues[SENSOR_VALUE_ROTATION_Y];
    return;
}

void EmulatedFakeRotatingCameraDevice::get_yawing(float* x, float* y, float*z) {
    double yaw, pitch, roll;
    read_rotation_vector(&yaw, &pitch, &roll);
    *x = sin((180+yaw)*3.14/180);
    *y = cos((180+yaw)*3.14/180);
    *z = 0;
    ALOGD("%s: yaw is %g, x %g y %g z %g", __func__, yaw, *x, *y, *z);
}

void EmulatedFakeRotatingCameraDevice::get_eye_x_y_z(float* x, float* y, float*z) {
    const float R=3500;
    //the coordinate of real camera is rotated (x-y swap)
    //and reverted (+/- swap)
    //
    //so rotation y is clockwise around x axis;
    //and rotation x is clockwise around y axis.
    const float theta_around_x = -mSensorValues[SENSOR_VALUE_ROTATION_Y];
    const float theta_around_y = -mSensorValues[SENSOR_VALUE_ROTATION_X];
    //apply x rotation first
    float y1 = -R*sin(theta_around_x*3.14/180);
    float z1 = R*cos(theta_around_x*3.14/180);
    //apply y rotation second
    float xz2 = z1 * sin(theta_around_y*3.14/180);
    float zz2 = z1 * cos(theta_around_y*3.14/180);
    *x = xz2;
    *y = y1;
    *z = zz2;

}

int EmulatedFakeRotatingCameraDevice::init_gl_surface(int width, int height)
{
    EGLint numConfigs = 1;
    EGLConfig myConfig = {0};

    if ( (mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY )
    {
        ALOGE("eglGetDisplay failed\n");
        return 0;
    }

    if ( eglInitialize(mEglDisplay, NULL, NULL) != EGL_TRUE )
    {
        ALOGE("eglInitialize failed\n");
        return 0;
    }

    {
        EGLint s_configAttribs[] = {
         EGL_SURFACE_TYPE, EGL_PBUFFER_BIT|EGL_WINDOW_BIT,
         EGL_RED_SIZE,       5,
         EGL_GREEN_SIZE,     6,
         EGL_BLUE_SIZE,      5,
         EGL_NONE
        };
        eglChooseConfig(mEglDisplay, s_configAttribs, &myConfig, 1, &numConfigs);
        EGLint attribs[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };
        mEglSurface = eglCreatePbufferSurface(mEglDisplay, myConfig, attribs);
        if (mEglSurface == EGL_NO_SURFACE) {
            ALOGE("eglCreatePbufferSurface error %x\n", eglGetError());
        }
    }

    if ( (mEglContext = eglCreateContext(mEglDisplay, myConfig, 0, 0)) == EGL_NO_CONTEXT )
    {
        ALOGE("eglCreateContext failed\n");
        return 0;
    }

    if ( eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext) != EGL_TRUE )
    {
        ALOGE("eglMakeCurrent failed\n");
        return 0;
    }

    int w, h;

    eglQuerySurface(mEglDisplay, mEglSurface, EGL_WIDTH, &w);
    checkEglError("eglQuerySurface");
    eglQuerySurface(mEglDisplay, mEglSurface, EGL_HEIGHT, &h);
    checkEglError("eglQuerySurface");

    ALOGD("Window dimensions: %d x %d\n", w, h);

    glDisable(GL_DITHER);
    glEnable(GL_CULL_FACE);

    return 1;
}

EmulatedFakeRotatingCameraDevice::EmulatedFakeRotatingCameraDevice(EmulatedFakeCamera* camera_hal)
    : EmulatedCameraDevice(camera_hal), mOpenglReady(false)
{
}

EmulatedFakeRotatingCameraDevice::~EmulatedFakeRotatingCameraDevice()
{
}

/****************************************************************************
 * Emulated camera device abstract interface implementation.
 ***************************************************************************/

status_t EmulatedFakeRotatingCameraDevice::connectDevice()
{
    ALOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    if (!isInitialized()) {
        ALOGE("%s: Fake camera device is not initialized.", __FUNCTION__);
        return EINVAL;
    }
    if (isConnected()) {
        ALOGW("%s: Fake camera device is already connected.", __FUNCTION__);
        return NO_ERROR;
    }

    /* There is no device to connect to. */
    mState = ECDS_CONNECTED;

    return NO_ERROR;
}

status_t EmulatedFakeRotatingCameraDevice::disconnectDevice()
{
    ALOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    if (!isConnected()) {
        ALOGW("%s: Fake camera device is already disconnected.", __FUNCTION__);
        return NO_ERROR;
    }
    if (isStarted()) {
        ALOGE("%s: Cannot disconnect from the started device.", __FUNCTION__);
        return EINVAL;
    }

    /* There is no device to disconnect from. */
    mState = ECDS_INITIALIZED;

    return NO_ERROR;
}

status_t EmulatedFakeRotatingCameraDevice::startDevice(int width,
                                               int height,
                                               uint32_t pix_fmt)
{
    ALOGE("%s width %d height %d", __FUNCTION__, width, height);

    Mutex::Autolock locker(&mObjectLock);
    if (!isConnected()) {
        ALOGE("%s: Fake camera device is not connected.", __FUNCTION__);
        return EINVAL;
    }
    if (isStarted()) {
        ALOGE("%s: Fake camera device is already started.", __FUNCTION__);
        return EINVAL;
    }

    /* Initialize the base class. */
    const status_t res =
        EmulatedCameraDevice::commonStartDevice(width, height, pix_fmt);

    mState = ECDS_STARTED;

    return res;
}

status_t EmulatedFakeRotatingCameraDevice::stopDevice()
{
    ALOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    if (!isStarted()) {
        ALOGW("%s: Fake camera device is not started.", __FUNCTION__);
        return NO_ERROR;
    }

    EmulatedCameraDevice::commonStopDevice();
    mState = ECDS_CONNECTED;

    if (mOpenglReady) {
        free_gl_surface();
        delete mPixelBuf;
        mOpenglReady=false;
    }
    if (mSensorPipe >= 0) {
        close(mSensorPipe);
        mSensorPipe = -1;
    }

    return NO_ERROR;
}

/****************************************************************************
 * Worker thread management overrides.
 ***************************************************************************/

bool EmulatedFakeRotatingCameraDevice::produceFrame(void* buffer,
                                                    int64_t* timestamp)
{
    if (mOpenglReady == false) {
        init_gl_surface(mFrameWidth, mFrameHeight);
        mOpenglReady = true;
        int width=mFrameWidth;
        int height = mFrameHeight;
        int kGlBytesPerPixel = 4;
        mPixelBuf = new uint8_t[width * height * kGlBytesPerPixel];
        init_sensor();
    }
    render(mFrameWidth, mFrameHeight);
    fillBuffer(buffer);
    return true;
}

/****************************************************************************
 * Fake camera device private API
 ***************************************************************************/

void EmulatedFakeRotatingCameraDevice::fillBuffer(void* buffer)
{
    uint8_t* currentFrame = reinterpret_cast<uint8_t*>(buffer);
    rgba8888_to_nv21(mPixelBuf, currentFrame, mFrameWidth, mFrameHeight);
    return;
}

}; /* namespace android */
