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

#ifndef _CAMERA3_HAL_IMG_HWENCODER_H_
#define _CAMERA3_HAL_IMG_HWENCODER_H_

#include <memory>
#include <mutex>
#include "CameraBuffer.h"
#include "CommonBuffer.h"
#include "EXIFMetaData.h"
#include "Exif.h"
#include "JpegMakerCore.h"
#if PLATFORM_SDK_API_VERSION >= 30
#include "RkExifInfo.h"
#include "MpiJpegEncoder.h"
#else
#include "release/encode_release/hw_jpegenc.h"
#endif
#ifdef CLIP
#undef CLIP
#endif

namespace android {
namespace camera2 {

/**
 * \class ImgHWEncoder
 */
class ImgHWEncoder {
public:
    struct EncodePackage {
        EncodePackage() :
            main(nullptr),
            jpegOut(nullptr),
            exifMeta(nullptr),
            exifAttrs(nullptr) {}

        std::shared_ptr<CameraBuffer> main;        // for input
        std::shared_ptr<CameraBuffer> jpegOut;     // for final JPEG output
        ExifMetaData     *exifMeta;
        exif_attribute_t *exifAttrs;
    };
public: /* Methods */
    ImgHWEncoder(int cameraid);
    virtual ~ImgHWEncoder();
    status_t init();
    void deInit(void);

    status_t encodeSync(EncodePackage & package);

private:  /* Methods */

    bool checkInputBuffer(CameraBuffer* buf);
    void fillRkExifInfo(RkExifInfo &exifInfo, exif_attribute_t* exifAttrs);
    void fillGpsInfo(RkGPSInfo &gpsInfo, exif_attribute_t* exifAttrs);

private:  /* Members */
    char sMaker[256];
    char sModel[256];
    int mCameraId;

#if PLATFORM_SDK_API_VERSION >= 30
    MpiJpegEncoder *mEncoder;
#else
    vpu_display_mem_pool *mPool;
#endif
    RkExifInfo mExifInfo;
    RkGPSInfo mGpsInfo;
};
} // namespace camera2
} // namespace android
#endif  // _CAMERA3_HAL_IMG_ENCODER_H_
