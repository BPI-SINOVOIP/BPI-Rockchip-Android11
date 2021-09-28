/*
 * Copyright 2019 Rockchip Electronics Co. LTD
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
 *
 * author: rimon.xu@rock-chips.com
 *   date: 2019/12/31
 * module: sideband window
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "RTSidebandWindow"

#include "RTSidebandWindow.h"
#include "log/log.h"
#include <sys/time.h>
#include <utils/Timers.h>
#include <string.h>

#include "DrmVopRender.h"

namespace android {

#define DEFAULT_SIDEBAND_WIDTH          1280
#define DEFAULT_SIDEBAND_HEIGHT         720
#define DEFAULT_SIDEBAND_FORMAT         0x15        //NV12

#define MIN_BUFFER_COUNT_UNDEQUEUE      2

static const gralloc_module_t *g_gralloc_module = NULL;

RTSidebandWindow::RTSidebandWindow()
        : mAllocDevice(NULL),
          mVopRender(NULL),
          mThreadRunning(false),
          mMessageQueue("RenderThread", static_cast<int>(MESSAGE_ID_MAX)),
          mMessageThread(nullptr) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);

    memset(&mSidebandInfo, 0, sizeof(mSidebandInfo));
}

RTSidebandWindow::~RTSidebandWindow() {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
}

status_t RTSidebandWindow::init(RTSidebandInfo info) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    status_t    err = 0;
    bool        ready = false;

    if (!g_gralloc_module) {
        err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                            (const hw_module_t **)&g_gralloc_module);
        if (err < 0) {
            ALOGE("g_gralloc_module get failed");
            goto __FAILED;
        }
    }
    if (!mAllocDevice) {
        err = gralloc_open((const struct hw_module_t*)g_gralloc_module, &mAllocDevice);
        if (err < 0) {
            ALOGE("Unable to open gralloc alloc device error =%d", err);
            mAllocDevice = NULL;
            goto __FAILED;
        }
    }

    if (info.structSize != sizeof(RTSidebandInfo)) {
        ALOGE("sideband info struct size is invailed!");
        goto __FAILED;
    }

    memcpy(&mSidebandInfo, &info, sizeof(RTSidebandInfo));

    mVopRender = new DrmVopRender();
    ready = mVopRender->initialize();
    if (ready) {
        mVopRender->detect();
    }

    mMessageThread = std::unique_ptr<MessageThread>(new MessageThread(this, "VOP Render"));
    if (mMessageThread != NULL) {
        mMessageThread->run();
    }

    return 0;
__FAILED:
    return -1;
}

status_t RTSidebandWindow::release() {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    requestExitAndWait();
    if (mMessageThread != NULL) {
        mMessageThread.reset();
        mMessageThread = NULL;
    }

    while (mRenderingQueue.size() > 0) {
        buffer_handle_t tmpBuffer = mRenderingQueue.front();
        mRenderingQueue.erase(mRenderingQueue.begin());
        mAllocDevice->free(mAllocDevice,tmpBuffer);
    }

    mVopRender->deinitialize();
    if (mVopRender) {
        delete mVopRender;
        mVopRender = NULL;
    }
    if (mAllocDevice) {
        ALOGV("Closing gralloc0 device %p", mAllocDevice);
        gralloc_close(mAllocDevice);
    }
    return 0;
}

status_t RTSidebandWindow::start() {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    return 0;
}

status_t RTSidebandWindow::stop() {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    return 0;
}

status_t RTSidebandWindow::flush() {
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.id = MESSAGE_ID_FLUSH;
    status_t status = mMessageQueue.send(&msg);

    return status;
}

status_t RTSidebandWindow::allocateBuffer(buffer_handle_t *buffer) {
    ALOGE("%s %d in has register", __FUNCTION__, __LINE__);
    buffer_handle_t temp_buffer = NULL;
    int32_t stride = 0;
    mAllocDevice->alloc(mAllocDevice,
                        mSidebandInfo.width,
                        mSidebandInfo.height,
                        mSidebandInfo.format,
                        mSidebandInfo.usage,
                        &temp_buffer,
                        &stride);
    if (temp_buffer) {
        g_gralloc_module->registerBuffer(g_gralloc_module, temp_buffer);
    }
    *buffer = temp_buffer;
    return 0;
}

status_t RTSidebandWindow::allocateSidebandHandle(buffer_handle_t *handle) {
    buffer_handle_t temp_buffer = NULL;
    int32_t stride = 0;
    mAllocDevice->alloc(mAllocDevice,
                        DEFAULT_SIDEBAND_WIDTH,
                        DEFAULT_SIDEBAND_HEIGHT,
                        DEFAULT_SIDEBAND_FORMAT,
                        0,
                        &temp_buffer,
                        &stride);
    if (temp_buffer) {
        g_gralloc_module->registerBuffer(g_gralloc_module, temp_buffer);
    }
    *handle = temp_buffer;
    return 0;
}

status_t RTSidebandWindow::freeBuffer(buffer_handle_t *buffer) {
    ALOGE("%s %d in has unregister", __FUNCTION__, __LINE__);
    // android::Mutex::Autolock _l(mLock);
    if (*buffer) {
        g_gralloc_module->unregisterBuffer(g_gralloc_module, *buffer);
        mAllocDevice->free(mAllocDevice, *buffer);
    }

    return 0;
}

status_t RTSidebandWindow::remainBuffer(buffer_handle_t buffer) {
    ALOGV("remainBuffer buffer: %p", buffer);
    // android::Mutex::Autolock _l(mLock);
    mRenderingQueue.push_back(buffer);
    return 0;
}


status_t RTSidebandWindow::dequeueBuffer(buffer_handle_t *buffer) {
    buffer_handle_t tmpBuffer = NULL;
    status_t status = 0;
    ALOGV("dequeueBuffer size: %d", (int32_t)mRenderingQueue.size());
    if (mRenderingQueue.size() > MIN_BUFFER_COUNT_UNDEQUEUE) {
        tmpBuffer = mRenderingQueue.front();
        Message msg;
        memset(&msg, 0, sizeof(Message));
        msg.id = MESSAGE_ID_DEQUEUE_REQUEST;
        status = mMessageQueue.send(&msg);

    }
    *buffer = tmpBuffer;
    return status;
}

status_t RTSidebandWindow::queueBuffer(buffer_handle_t buffer) {
    (void)buffer;
    ALOGV("%s %d in buffer: %p queue size: %d", __FUNCTION__, __LINE__, buffer, (int32_t)mRenderingQueue.size());
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.id = MESSAGE_ID_RENDER_REQUEST;
    msg.streamBuffer.buffer = buffer;

    status_t status = mMessageQueue.send(&msg);
    return status;
}

status_t RTSidebandWindow::setBufferGeometry(int32_t width, int32_t height, int32_t format) {
    ALOGV("%s %d width=%d height=%d in", __FUNCTION__, __LINE__, width, height);
    mSidebandInfo.width = width;
    mSidebandInfo.height = height;
    mSidebandInfo.format = format;

    return 0;
}

status_t RTSidebandWindow::setCrop(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    mSidebandInfo.left = left;
    mSidebandInfo.top = top;
    mSidebandInfo.right = right;
    mSidebandInfo.bottom = bottom;

    return 0;
}


status_t RTSidebandWindow::requestExitAndWait()
{
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.id = MESSAGE_ID_EXIT;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    status |= mMessageThread->requestExitAndWait();
    return status;
}

void RTSidebandWindow::messageThreadLoop() {
    mThreadRunning = true;
    while (mThreadRunning) {
        status_t status = NO_ERROR;
        Message msg;
        mMessageQueue.receive(&msg);

        ALOGV("@%s, receive message id:%d", __FUNCTION__, msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
          status = handleMessageExit();
        break;
        case MESSAGE_ID_RENDER_REQUEST:
          status = handleRenderRequest(msg);
        break;
        case MESSAGE_ID_DEQUEUE_REQUEST:
          status = handleDequeueRequest(msg);
        break;
        case MESSAGE_ID_FLUSH:
          status = handleFlush();
        break;
        default:
          ALOGE("ERROR Unknown message %d", msg.id);
          status = BAD_VALUE;
        break;
        }

        if (status != NO_ERROR)
            ALOGE("error %d in handling message: %d", status, static_cast<int>(msg.id));
        ALOGV("@%s, finish message id:%d", __FUNCTION__, msg.id);
        mMessageQueue.reply(msg.id, status);
    }
}

status_t RTSidebandWindow::handleMessageExit() {
    mThreadRunning = false;
    return 0;
}

status_t RTSidebandWindow::handleRenderRequest(Message &msg) {
    buffer_handle_t buffer = msg.streamBuffer.buffer;
    ALOGV("%s %d buffer: %p in", __FUNCTION__, __LINE__, buffer);
    mVopRender->SetDrmPlane(0, mSidebandInfo.right - mSidebandInfo.left, mSidebandInfo.bottom - mSidebandInfo.top, buffer);

    mRenderingQueue.push_back(buffer);

    return 0;
}

status_t RTSidebandWindow::handleDequeueRequest(Message &msg) {
    (void)msg;
    mRenderingQueue.erase(mRenderingQueue.begin());
    return 0;
}

status_t RTSidebandWindow::handleFlush() {
    while (mRenderingQueue.size() > 0) {
        buffer_handle_t buffer = NULL;
        buffer = mRenderingQueue.front();
        mRenderingQueue.erase(mRenderingQueue.begin());
        freeBuffer(&buffer);
    }
    return 0;
}


}
