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
 * Contains implementation of a class PreviewWindow that encapsulates
 * functionality of a preview window set via set_preview_window camera HAL API.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera_Preview"
#include <log/log.h>
#include <ui/GraphicBuffer.h>
#include "EmulatedCameraDevice.h"
#include "PreviewWindow.h"

namespace android {

PreviewWindow::PreviewWindow(GraphicBufferMapper* gbm)
    : mPreviewWindow(NULL),
      mGBM(gbm),
      mPreviewFrameWidth(0),
      mPreviewFrameHeight(0),
      mPreviewEnabled(false)
{
}

PreviewWindow::~PreviewWindow()
{
}

/****************************************************************************
 * Camera API
 ***************************************************************************/

status_t PreviewWindow::setPreviewWindow(struct preview_stream_ops* window,
                                         int preview_fps)
{
    ALOGV("%s: current: %p -> new: %p", __FUNCTION__, mPreviewWindow, window);

    status_t res = NO_ERROR;
    Mutex::Autolock locker(&mObjectLock);

    /* Reset preview info. */
    mPreviewFrameWidth = mPreviewFrameHeight = 0;

    if (window != NULL) {
        /* The CPU will write each frame to the preview window buffer.
         * Note that we delay setting preview window buffer geometry until
         * frames start to come in. */
        res = window->set_usage(window, GRALLOC_USAGE_SW_WRITE_OFTEN);
        if (res != NO_ERROR) {
            window = NULL;
            res = -res; // set_usage returns a negative errno.
            ALOGE("%s: Error setting preview window usage %d -> %s",
                 __FUNCTION__, res, strerror(res));
        }
    }
    mPreviewWindow = window;

    return res;
}

status_t PreviewWindow::startPreview()
{
    ALOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    mPreviewEnabled = true;

    return NO_ERROR;
}

void PreviewWindow::stopPreview()
{
    ALOGV("%s", __FUNCTION__);

    Mutex::Autolock locker(&mObjectLock);
    mPreviewEnabled = false;
}

/****************************************************************************
 * Public API
 ***************************************************************************/

void PreviewWindow::onNextFrameAvailable(nsecs_t timestamp,
                                         EmulatedCameraDevice* camera_dev)
{
    int res;
    Mutex::Autolock locker(&mObjectLock);

    if (!isPreviewEnabled() || mPreviewWindow == NULL) {
        return;
    }

    /* Make sure that preview window dimensions are OK with the camera device */
    if (adjustPreviewDimensions(camera_dev)) {
        /* Need to set / adjust buffer geometry for the preview window.
         * Note that in the emulator preview window uses only RGB for pixel
         * formats. */
        ALOGV("%s: Adjusting preview windows %p geometry to %dx%d",
             __FUNCTION__, mPreviewWindow, mPreviewFrameWidth,
             mPreviewFrameHeight);
        res = mPreviewWindow->set_buffers_geometry(mPreviewWindow,
                                                   mPreviewFrameWidth,
                                                   mPreviewFrameHeight,
                                                   HAL_PIXEL_FORMAT_RGBA_8888);
        if (res != NO_ERROR) {
            ALOGE("%s: Error in set_buffers_geometry %d -> %s",
                 __FUNCTION__, -res, strerror(-res));
            return;
        }
    }

    /*
     * Push new frame to the preview window.
     */

    /* Dequeue preview window buffer for the frame. */
    buffer_handle_t* buffer = NULL;
    int stride = 0;
    res = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buffer, &stride);
    if (res != NO_ERROR || buffer == NULL) {
        ALOGE("%s: Unable to dequeue preview window buffer: %d -> %s",
            __FUNCTION__, -res, strerror(-res));
        return;
    }

    /* Let the preview window to lock the buffer. */
    res = mPreviewWindow->lock_buffer(mPreviewWindow, buffer);
    if (res != NO_ERROR) {
        ALOGE("%s: Unable to lock preview window buffer: %d -> %s",
             __FUNCTION__, -res, strerror(-res));
        mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
        return;
    }

    /* Now let the graphics framework to lock the buffer, and provide
     * us with the framebuffer data address. */
    void* img = NULL;

    status_t status = mGBM->lock(*buffer,
                                 GraphicBuffer::USAGE_SW_WRITE_OFTEN,
                                 Rect(0, 0, mPreviewFrameWidth, mPreviewFrameHeight),
                                 &img);
    if (status != OK) {
        ALOGE("%s: gralloc.lock failure: %d -> %s",
             __FUNCTION__, res, strerror(res));
        mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
        return;
    }

    int64_t frame_timestamp = 0L;
    /* Frames come in in YV12/NV12/NV21 format. Since preview window doesn't
     * supports those formats, we need to obtain the frame in RGB565. */
    res = camera_dev->getCurrentPreviewFrame(img, &frame_timestamp);
    if (res == NO_ERROR) {
        /* Show it. */
        mPreviewWindow->set_timestamp(mPreviewWindow,
                                      frame_timestamp != 0L ? frame_timestamp : timestamp);
        mPreviewWindow->enqueue_buffer(mPreviewWindow, buffer);
    } else {
        ALOGE("%s: Unable to obtain preview frame: %d", __FUNCTION__, res);
        mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
    }
    mGBM->unlock(*buffer);
}

/***************************************************************************
 * Private API
 **************************************************************************/

bool PreviewWindow::adjustPreviewDimensions(EmulatedCameraDevice* camera_dev)
{
    /* Match the cached frame dimensions against the actual ones. */
    if (mPreviewFrameWidth == camera_dev->getFrameWidth() &&
        mPreviewFrameHeight == camera_dev->getFrameHeight()) {
        /* They match. */
        return false;
    }

    /* They don't match: adjust the cache. */
    mPreviewFrameWidth = camera_dev->getFrameWidth();
    mPreviewFrameHeight = camera_dev->getFrameHeight();

    return true;
}

}; /* namespace android */
