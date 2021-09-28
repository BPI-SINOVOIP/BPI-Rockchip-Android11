/*
 * Copyright (C) 2012 The Android Open Source Project
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
#define LOG_TAG "EmulatedCamera2_JpegCompressor"

#include <log/log.h>

#include <gralloc_cb_bp.h>
#include "JpegCompressor.h"
#include "../EmulatedFakeCamera2.h"
#include "../EmulatedFakeCamera3.h"
#include "../Exif.h"
#include "../Thumbnail.h"
#include "hardware/camera3.h"

namespace android {

JpegCompressor::JpegCompressor(GraphicBufferMapper* gbm):
        Thread(false),
        mIsBusy(false),
        mSynchronous(false),
        mBuffers(NULL),
        mListener(NULL),
        mGBM(gbm) {
}

JpegCompressor::~JpegCompressor() {
    Mutex::Autolock lock(mMutex);
}

status_t JpegCompressor::reserve() {
    Mutex::Autolock busyLock(mBusyMutex);
    if (mIsBusy) {
        ALOGE("%s: Already processing a buffer!", __FUNCTION__);
        return INVALID_OPERATION;
    }
    mIsBusy = true;
    return OK;
}

status_t JpegCompressor::start(Buffers *buffers, JpegListener *listener, CameraMetadata* settings) {
    if (listener == NULL) {
        ALOGE("%s: NULL listener not allowed!", __FUNCTION__);
        return BAD_VALUE;
    }
    Mutex::Autolock lock(mMutex);
    {
        Mutex::Autolock busyLock(mBusyMutex);

        if (!mIsBusy) {
            ALOGE("Called start without reserve() first!");
            return INVALID_OPERATION;
        }
        mSynchronous = false;
        mBuffers = buffers;
        mListener = listener;
        if (settings) {
              mSettings = *settings;
        }
    }

    status_t res;
    res = run("EmulatedFakeCamera2::JpegCompressor");
    if (res != OK) {
        ALOGE("%s: Unable to start up compression thread: %s (%d)",
                __FUNCTION__, strerror(-res), res);
        delete mBuffers;
    }
    return res;
}

status_t JpegCompressor::compressSynchronous(Buffers *buffers) {
    status_t res;

    Mutex::Autolock lock(mMutex);
    {
        Mutex::Autolock busyLock(mBusyMutex);

        if (mIsBusy) {
            ALOGE("%s: Already processing a buffer!", __FUNCTION__);
            return INVALID_OPERATION;
        }

        mIsBusy = true;
        mSynchronous = true;
        mBuffers = buffers;
    }

    res = compress();

    cleanUp();

    return res;
}

status_t JpegCompressor::cancel() {
    requestExitAndWait();
    return OK;
}

status_t JpegCompressor::readyToRun() {
    return OK;
}

bool JpegCompressor::threadLoop() {
    status_t res;
    ALOGV("%s: Starting compression thread", __FUNCTION__);

    res = compress();

    mListener->onJpegDone(mJpegBuffer, res == OK);

    cleanUp();

    return false;
}

status_t JpegCompressor::compress() {
    // Find source and target buffers. Assumes only one buffer matches
    // each condition!
    bool mFoundAux = false;
    int thumbWidth = 0, thumbHeight = 0;
    unsigned char thumbJpegQuality = 90;
    unsigned char jpegQuality = 90;
    camera_metadata_entry_t entry;

    for (size_t i = 0; i < mBuffers->size(); i++) {
        const StreamBuffer &b = (*mBuffers)[i];
        if (b.format == HAL_PIXEL_FORMAT_BLOB) {
            mJpegBuffer = b;
            mFoundJpeg = true;
        } else if (b.streamId <= 0) {
            mAuxBuffer = b;
            mFoundAux = true;
        }
        if (mFoundJpeg && mFoundAux) break;
    }
    if (!mFoundJpeg || !mFoundAux) {
        ALOGE("%s: Unable to find buffers for JPEG source/destination",
                __FUNCTION__);
        return BAD_VALUE;
    }

    // Create EXIF data and compress thumbnail
    ExifData* exifData = createExifData(mSettings, mAuxBuffer.width, mAuxBuffer.height);
    entry = mSettings.find(ANDROID_JPEG_THUMBNAIL_SIZE);
    if (entry.count > 0) {
        thumbWidth = entry.data.i32[0];
        thumbHeight = entry.data.i32[1];
    }
    entry = mSettings.find(ANDROID_JPEG_THUMBNAIL_QUALITY);
    if (entry.count > 0) {
        thumbJpegQuality = entry.data.u8[0];
    }
    if (thumbWidth > 0 && thumbHeight > 0) {
        createThumbnail(static_cast<const unsigned char*>(mAuxBuffer.img),
                        mAuxBuffer.width, mAuxBuffer.height,
                        thumbWidth, thumbHeight,
                        thumbJpegQuality, exifData);
    }

    // Compress the image
    entry = mSettings.find(ANDROID_JPEG_QUALITY);
    if (entry.count > 0) {
        jpegQuality = entry.data.u8[0];
    }
    NV21JpegCompressor nV21JpegCompressor;
    nV21JpegCompressor.compressRawImage((void*)mAuxBuffer.img,
                                         mAuxBuffer.width,
                                         mAuxBuffer.height,
                                         jpegQuality, exifData);
    nV21JpegCompressor.getCompressedImage((void*)mJpegBuffer.img);

    // Refer to /hardware/libhardware/include/hardware/camera3.h
    // Transport header for compressed JPEG buffers in output streams.
    camera3_jpeg_blob_t jpeg_blob;
    const cb_handle_t *cb = cb_handle_t::from(*mJpegBuffer.buffer);
    jpeg_blob.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
    jpeg_blob.jpeg_size = nV21JpegCompressor.getCompressedSize();
    memcpy(mJpegBuffer.img + cb->width - sizeof(camera3_jpeg_blob_t),
           &jpeg_blob, sizeof(camera3_jpeg_blob_t));

    freeExifData(exifData);
    return OK;
}

bool JpegCompressor::isBusy() {
    Mutex::Autolock busyLock(mBusyMutex);
    return mIsBusy;
}

bool JpegCompressor::isStreamInUse(uint32_t id) {
    Mutex::Autolock lock(mBusyMutex);

    if (mBuffers && mIsBusy) {
        for (size_t i = 0; i < mBuffers->size(); i++) {
            if ( (*mBuffers)[i].streamId == (int)id ) return true;
        }
    }
    return false;
}

bool JpegCompressor::waitForDone(nsecs_t timeout) {
    Mutex::Autolock lock(mBusyMutex);
    while (mIsBusy) {
        status_t res = mDone.waitRelative(mBusyMutex, timeout);
        if (res != OK) return false;
    }
    return true;
}

void JpegCompressor::cleanUp() {
    Mutex::Autolock lock(mBusyMutex);

    if (mFoundAux) {
        if (mAuxBuffer.streamId == 0) {
            if (mAuxBuffer.buffer == nullptr) {
                delete[] mAuxBuffer.img;
            } else {
                buffer_handle_t buffer = *mAuxBuffer.buffer;

                mGBM->unlock(buffer);
                mGBM->freeBuffer(buffer);
            }
        } else if (!mSynchronous) {
            mListener->onJpegInputDone(mAuxBuffer);
        }
    }
    if (!mSynchronous) {
        delete mBuffers;
    }

    mBuffers = NULL;

    mIsBusy = false;
    mDone.signal();
}

JpegCompressor::JpegListener::~JpegListener() {
}

} // namespace android
