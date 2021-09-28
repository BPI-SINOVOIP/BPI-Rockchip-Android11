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

#ifndef _CAMERA3_HAL_IMG_ENCODER_CORE_H_
#define _CAMERA3_HAL_IMG_ENCODER_CORE_H_

#include <memory>
#include <mutex>
#include "CommonBuffer.h"
#include "EXIFMaker.h"
USING_METADATA_NAMESPACE;
NAMESPACE_DECLARATION {
/**
 * \class ImgEncoderCore
 * This class does JPEG encoding of the main and the thumb buffers provided in
 * the EncodePackage. This class selects between hardware and software encoding
 * and provides the output in the jpeg and thumbout buffers in the EncodePackage.
 * JFIF output is produced by the JPEGMaker class
 *
 */
class ImgEncoderCore {
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

        std::shared_ptr<CommonBuffer> main;        // for input
        std::shared_ptr<CommonBuffer> thumb;       // for input, can be nullptr
        std::shared_ptr<CommonBuffer> jpegOut;     // for final JPEG output
        int                           jpegSize;    // Jpeg output size
        std::shared_ptr<CommonBuffer> encodedData;    // encoder output for main image
        int                           encodedDataSize;// main image encoded data size
        std::shared_ptr<CommonBuffer> thumbOut;    // for thumbnail output
        int                           thumbSize;   // Thumb ouptut size
        const CameraMetadata         *settings;    // settings from request
        unsigned char                *jpegDQTAddr; // pointer to DQT marker inside jpeg, for in-place exif creation
        bool                          padExif;     // Boolean to control if padding is preferred over copying during in-place exif creation
        bool                          encodeAll;   // Boolean to control if both thumbnail and main image shall be encoded. False means just thumbnail.
    };

    class IImgEncoderCoreCallback {
    public:
        virtual status_t jpegDone(EncodePackage & package, std::shared_ptr<ExifMetaData> metaData,
                                  status_t status) = 0;

    protected: /* No instantiation */
        IImgEncoderCoreCallback() {}
        virtual ~IImgEncoderCoreCallback() {}
    };

private: /* Internal types */
    class AsyncEncodeData {
    public:
        AsyncEncodeData(ImgEncoderCore::EncodePackage& p,
                        std::shared_ptr<ExifMetaData> m,
                        ImgEncoderCore::IImgEncoderCoreCallback* c) :
            mPackage(new EncodePackage(p)), mMetaData(m), mCallback(c) {}
        ~AsyncEncodeData() { delete mPackage; }

    public: /* public members, so that thread loop can access */
        ImgEncoderCore::EncodePackage* mPackage;
        std::shared_ptr<ExifMetaData> mMetaData;
        ImgEncoderCore::IImgEncoderCoreCallback* mCallback;

    private:
        // prevent copy constructor and assignment operator
        AsyncEncodeData(const AsyncEncodeData& other);
        AsyncEncodeData& operator=(const AsyncEncodeData& other);

    };

public: /* Methods */
    ImgEncoderCore();
    virtual ~ImgEncoderCore();

    status_t init();
    void deInit(void);
    status_t encodeSync(EncodePackage & package, ExifMetaData& metaData);

private:  /* Methods */
    status_t allocateBufferAndDownScale(EncodePackage & pkg);
    void thumbBufferDownScale(EncodePackage & pkg);
    void mainBufferDownScale(EncodePackage & pkg);
    int doSwEncode(std::shared_ptr<CommonBuffer> srcBuf,
                   int quality,
                   std::shared_ptr<CommonBuffer> destBuf,
                   unsigned int destOffset = 0);
    status_t getJpegSettings(EncodePackage & pkg, ExifMetaData& metaData);

private:  /* Members */

    std::shared_ptr<CommonBuffer> mThumbOutBuf;
    std::shared_ptr<CommonBuffer> mJpegDataBuf;
    std::shared_ptr<CommonBuffer> mMainScaled;
    std::shared_ptr<CommonBuffer> mThumbScaled;

    ExifMetaData::JpegSetting *mJpegSetting;

    std::mutex mEncodeLock; /* protect JPEG encoding progress */

    // arc::JpegCompressor needs YU12 format
    // and the ISP doesn't output YU12 directly.
    // so a temporary intermediate buffer is needed.
    std::unique_ptr<char[]> mInternalYU12;
    unsigned int mInternalYU12Size;
};

} NAMESPACE_DECLARATION_END
#endif  // _CAMERA3_HAL_IMG_ENCODER_CORE_H_
