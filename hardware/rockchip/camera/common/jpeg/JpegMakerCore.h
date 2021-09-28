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

#ifndef _CAMERA3_HAL_JPEG_MAKER_CORE_H_
#define _CAMERA3_HAL_JPEG_MAKER_CORE_H_

#include "EXIFMaker.h"
#include "EXIFMetaData.h"
#include "ImgEncoderCore.h"
USING_METADATA_NAMESPACE;
NAMESPACE_DECLARATION {
// Android requires that camera HAL to include the final size of the compressed
// image inside the output stream buffer for the BLOB format. And The JPEG blob
// ID field must be set to CAMERA3_JPEG_BLOB_ID (0x00FF).
// The struct definition and enum value can be found in hardware\camera3.h.
struct camera_jpeg_blob {
    uint16_t jpeg_blob_id;
    uint32_t jpeg_size;
};

enum {
    CAMERA_JPEG_BLOB_ID = 0x00FF
};

/**
 * \class JpegMakerCore
 * Does the EXIF header creation and appending to the provided jpeg buffer
 *
 */
class JpegMakerCore {
public: /* Methods */
    explicit JpegMakerCore(int cameraid);
    virtual ~JpegMakerCore();
    status_t init();
    status_t setupExifWithMetaData(ImgEncoderCore::EncodePackage & package,
                                   ExifMetaData& metaData);
    status_t makeJpeg(ImgEncoderCore::EncodePackage & package);
    void getExifAttrbutes(exif_attribute_t& exifAttributes);

private:  /* Methods */
    // prevent copy constructor and assignment operator
    JpegMakerCore(const JpegMakerCore& other);
    JpegMakerCore& operator=(const JpegMakerCore& other);

    status_t processExifSettings(const CameraMetadata  *settings, ExifMetaData& metaData);
    status_t processJpegSettings(ImgEncoderCore::EncodePackage & package, ExifMetaData& metaData);
    status_t processGpsSettings(const CameraMetadata &settings, ExifMetaData& metaData);
    status_t processAwbSettings(const CameraMetadata &settings, ExifMetaData& metaData);
    status_t processScalerCropSettings(const CameraMetadata &settings, ExifMetaData& metaData);
    status_t processEvCompensationSettings(const CameraMetadata &settings, ExifMetaData& metaData);
    int32_t  getJpegBufferSize(void);

private:  /* Members */
    EXIFMaker* mExifMaker;
    int        mCameraId;
};

} NAMESPACE_DECLARATION_END
#endif  // _CAMERA3_HAL_JPEG_MAKER_CORE_H_
