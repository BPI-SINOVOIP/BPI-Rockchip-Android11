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

#ifndef _CAMERA3_HAL_IMG_ENCODER_H_
#define _CAMERA3_HAL_IMG_ENCODER_H_

#include <list>
#include "CameraBuffer.h"
#include "ImgEncoderCore.h"

NAMESPACE_DECLARATION {

/**
 * \class ImgEncoder
 * This class is an wrapper of ImgEncoderCore for fitting CameraBuffer
 * input and serving to the old generation IPU PSLs.
 */
class ImgEncoder : public ImgEncoderCore,
                   public ImgEncoderCore::IImgEncoderCoreCallback {
public:  /* types */
    struct EncodePackage {
        EncodePackage() :
                main(nullptr),
                thumb(nullptr),
                jpegOut(nullptr),
                jpegSize(0),
                encodedData(nullptr),
                encodedDataSize(0),
                thumbOut(nullptr),
                thumbSize(0),
                settings(nullptr),
                jpegDQTAddr(nullptr),
                padExif(false),
                encodeAll(true) {}

        std::shared_ptr<CameraBuffer> main;        // for input
        std::shared_ptr<CameraBuffer> thumb;       // for input, can be nullptr
        std::shared_ptr<CameraBuffer> jpegOut;     // for final JPEG output
        int              jpegSize;    // Jpeg output size
        std::shared_ptr<CameraBuffer> encodedData;    // encoder output for main image
        int              encodedDataSize;// main image encoded data size
        std::shared_ptr<CameraBuffer> thumbOut;    // for thumbnail output
        int              thumbSize;   // Thumb ouptut size
        const CameraMetadata    *settings;    // settings from request
        unsigned char    *jpegDQTAddr; // pointer to DQT marker inside jpeg, for in-place exif creation
        bool             padExif;     // Boolean to control if padding is preferred over copying during in-place exif creation
        bool             encodeAll;   // Boolean to control if both thumbnail and main image shall be encoded. False means just thumbnail.
    };

    class IImgEncoderCallback {
    public:
        virtual status_t jpegDone(EncodePackage & package, std::shared_ptr<ExifMetaData> metaData, status_t status) = 0;

    protected: /* No instantiation */
        IImgEncoderCallback() {}
        virtual ~IImgEncoderCallback() {}
    };

public: /* Methods */
    explicit ImgEncoder(int cameraid);
    virtual ~ImgEncoder();

    status_t encodeSync(EncodePackage & package, ExifMetaData& metaData);

    static void convertEncodePackage(EncodePackage& src, ImgEncoderCore::EncodePackage& dst);
private:  /* Methods */
    /**
     * Inherit from IImgEncoderCoreCallback for Async callback
     */
    status_t jpegDone(ImgEncoderCore::EncodePackage& package,
            std::shared_ptr<ExifMetaData> metaData, status_t status);

    /**
     * The conversion between CameraBuffer and CommonBuffer based EncodePackages
     */
    static std::shared_ptr<CommonBuffer> createCommonBuffer(std::shared_ptr<CameraBuffer> cBuffer);

    /**
     * For client that are using this old interface, The better way is to
     * allocate needed output CameraBuffers here and pass the pointer to
     * ImgEncoderCore to avoid extra memory allocation and memory copy
     */
    void allocateOutputCameraBuffers(EncodePackage &package, ExifMetaData& metaData);

private:  /* types */
    struct AsyncEventData {
        EncodePackage        pkg;
        IImgEncoderCallback *callback;
    };

private:  /* Members */
    int mCameraId;
    std::shared_ptr<CameraBuffer> mThumbOutBuf;
    std::shared_ptr<CameraBuffer> mJpegDataBuf;
    std::list<AsyncEventData> mEventFifo;
};
} NAMESPACE_DECLARATION_END

#endif  // _CAMERA3_HAL_IMG_ENCODER_H_
