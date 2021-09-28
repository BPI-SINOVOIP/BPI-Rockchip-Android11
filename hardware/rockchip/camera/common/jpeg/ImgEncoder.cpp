/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "ImgEncoder"

#include "ImgEncoder.h"
#include "LogHelper.h"

NAMESPACE_DECLARATION {

ImgEncoder::ImgEncoder(int cameraid) :
    mCameraId(cameraid),
    mThumbOutBuf(nullptr),
    mJpegDataBuf(nullptr)
{
    LOGI("@%s", __FUNCTION__);
}

ImgEncoder::~ImgEncoder()
{
    LOGI("@%s", __FUNCTION__);
}

std::shared_ptr<CommonBuffer> ImgEncoder::createCommonBuffer(std::shared_ptr<CameraBuffer> cBuffer)
{
    std::shared_ptr<CommonBuffer> jBuffer = nullptr;
    if (cBuffer.get()) {
        BufferProps props;
        props.width  = cBuffer->width();
        props.height = cBuffer->height();
        props.size = cBuffer->size();
        props.stride = cBuffer->stride();
        props.format = cBuffer->v4l2Fmt();
        props.type   = BMT_HEAP;

        jBuffer = std::make_shared<CommonBuffer>(props, cBuffer->data());
    }

    return jBuffer;
}

/**
 * convertEncodePackage
 *
 * method for converting CameraBuffer based EncodePackage to CommonBuffer
 * based EncodePackage.
 */
void ImgEncoder::convertEncodePackage(EncodePackage& src, ImgEncoderCore::EncodePackage& dst)
{
    dst.main        = createCommonBuffer(src.main);
    dst.thumb       = createCommonBuffer(src.thumb);
    dst.jpegOut     = createCommonBuffer(src.jpegOut);
    dst.jpegSize    = src.jpegSize;
    dst.encodedData = createCommonBuffer(src.encodedData);
    dst.encodedDataSize = src.encodedDataSize;
    dst.thumbOut    = createCommonBuffer(src.thumbOut);
    dst.thumbSize   = src.thumbSize;
    dst.settings    = src.settings;
    dst.jpegDQTAddr = src.jpegDQTAddr;
    dst.padExif     = src.padExif;
    dst.encodeAll   = src.encodeAll;
}

void ImgEncoder::allocateOutputCameraBuffers(EncodePackage &pkg, ExifMetaData& metaData)
{
    int thumbwidth = metaData.mJpegSetting.thumbWidth;
    int thumbheight = metaData.mJpegSetting.thumbHeight;

    // Allocate buffer for main image jpeg output if in first time or resolution changed
    if (pkg.encodeAll && (mJpegDataBuf.get() == nullptr ||
                COMPARE_RESOLUTION(mJpegDataBuf, pkg.jpegOut))) {

        if (mJpegDataBuf.get() != nullptr)
            mJpegDataBuf.reset();

        LOGI("Allocating jpeg data buffer with %dx%d, stride:%d", pkg.jpegOut->width(),
                pkg.jpegOut->height(), pkg.jpegOut->stride());
        mJpegDataBuf = MemoryUtils::allocateHeapBuffer(pkg.jpegOut->width(),
                pkg.jpegOut->height(), pkg.jpegOut->stride(),
                pkg.jpegOut->v4l2Fmt(), mCameraId, pkg.jpegOut->size());
    }
    pkg.encodedData = mJpegDataBuf;

    // Allocate buffer for thumbnail output
    if (thumbwidth != 0) {
        if (!pkg.thumb.get()) {
            if (!pkg.main.get()) {
                LOGE("No source for thumb");
                return;
            }
            pkg.thumb = pkg.main;
        }

        if (mThumbOutBuf.get() && (mThumbOutBuf->width() != thumbwidth ||
                    mThumbOutBuf->height() != thumbheight))
            mThumbOutBuf.reset();

        if (!mThumbOutBuf.get()) {
            LOGI("Allocating thumb data buffer with %dx%d", thumbwidth, thumbheight);
            // Use thumbwidth as stride for the heap buffer
            mThumbOutBuf = MemoryUtils::allocateHeapBuffer(thumbwidth, thumbheight,
                    thumbwidth, pkg.thumb->v4l2Fmt(), mCameraId,
                    thumbwidth * thumbheight * 2);
        }
    }
    pkg.thumbOut = mThumbOutBuf;
}

/**
 * encodeSync
 *
 * Convert CameraBuffer to CommonBuffer
 * Call ImgEncoderCore to finish the job
 */
status_t ImgEncoder::encodeSync(EncodePackage& package, ExifMetaData& metaData)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;
    ImgEncoderCore::EncodePackage corePackage;
    // allocate needed camera buffers for output
    allocateOutputCameraBuffers(package, metaData);

    // do the conversion
    convertEncodePackage(package, corePackage);

    status = ImgEncoderCore::encodeSync(corePackage, metaData);
    if (status) {
        LOGE("Error %d happened in ImgEncoderCore", status);
        return status;
    }

    // On success, fetch the result
    if (corePackage.encodedDataSize) {
        package.encodedDataSize = corePackage.encodedDataSize;
    } else {
        LOGW("ImgEncoderCore out main jpeg size 0");
        package.encodedData = nullptr;
        package.encodedDataSize = 0;
    }

    if (corePackage.thumbSize) {
        package.thumbSize = corePackage.thumbSize;
    } else {
        LOGW("ImgEncoderCore out thumb size 0");
        package.thumbOut = nullptr;
        package.thumbSize = 0;
    }

    return status;
}

/**
 * jpegDone
 *
 * Async Jpeg encode request callback
 */
status_t ImgEncoder::jpegDone(ImgEncoderCore::EncodePackage& package,
            std::shared_ptr<ExifMetaData> metaData, status_t status)
{
    // find the CameraBuffer based package
    if (mEventFifo.empty()) {
        LOGE("JpegDone while event queue is empty");
        return OK;
    }

    AsyncEventData eventData = mEventFifo.front();
    mEventFifo.pop_front();

    if (package.encodedDataSize) {
        eventData.pkg.encodedDataSize = package.encodedDataSize;
    } else {
        LOGW("ImgEncoderCore out main jpeg size 0");
        eventData.pkg.encodedData = nullptr;
        eventData.pkg.encodedDataSize = 0;
    }

    if (package.encodedDataSize) {
        eventData.pkg.thumbSize = package.thumbSize;
    } else {
        LOGW("ImgEncoderCore out thumb size 0");
        eventData.pkg.thumbOut = nullptr;
        eventData.pkg.thumbSize = 0;
    }
    if (status)
        LOGE("Async Jpeg encode done with error:%d", status);

    eventData.callback->jpegDone(eventData.pkg, metaData, status);
    return OK;
}

} NAMESPACE_DECLARATION_END
