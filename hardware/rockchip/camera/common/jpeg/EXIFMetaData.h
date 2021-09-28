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

#include "3ATypes.h"
#include "Camera3V4l2Format.h"
#include "UtilityMacros.h"
#include <utils/Errors.h>
#include <CameraMetadata.h>

#ifndef _EXIFMETADATA_H_
#define _EXIFMETADATA_H_

#define MAX_NUM_GPS_PROCESSING_METHOD 64

NAMESPACE_DECLARATION {
/**
 * \class ExifMetaData
 *
 */
class ExifMetaData
{
public:
    ExifMetaData();
    virtual ~ExifMetaData();
    status_t saveAeConfig(SensorAeConfig& config);
    status_t saveIa3AMkNote(const ia_binary_data& mkNote);

    // jpeg info
    struct JpegSetting{
        int jpegQuality;
        int jpegThumbnailQuality;
        int thumbWidth;
        int thumbHeight;
        int orientation;
    };
    // GPS info
    struct GpsSetting{
        double latitude;
        double longitude;
        double altitude;
        char gpsProcessingMethod[MAX_NUM_GPS_PROCESSING_METHOD];
        long gpsTimeStamp;
    };
    // exif info
    struct Ia3ASetting{
        AeMode aeMode;
        MeteringMode meteringMode;
        AwbMode lightSource;
        float brightness;
        int isoSpeed;
        unsigned short focusDistance;  // used to calculate subject distance
        char contrast;    /*!< Tag a408. valid values are 0:normal, 1:low and 2:high */
        char saturation;  /*!< Tag a409. valid values are 0:normal, 1:low and 2:high */
        char sharpness;   /*!< Tag a40a. valid values are 0:normal, 1:low and 2:high */
    };
    struct makernote_info{
        unsigned int focal_length;
        unsigned int f_number_curr;
        unsigned int f_number_range;
    };
    JpegSetting mJpegSetting;
    GpsSetting mGpsSetting;
    Ia3ASetting mIa3ASetting;
    SensorAeConfig *mAeConfig;                  /*!< defined in I3AControls.h */

typedef makernote_info MakernoteType;

    MakernoteType *mIspMkNote;
    status_t saveIspMkNote(MakernoteType& mkNote);

    ia_binary_data *mIa3AMkNote;                /*!< defined in ia_mkn_types.h */
    AwbMode mAwbMode;
    char *mSoftware;                            /*!< software string from HAL */
    bool mFlashFired;                           /*!< whether flash was fired */
    int8_t mV3AeMode;                           /*!< v3 ae mode (e.g. for flash) */
    int8_t mFlashMode;                          /*!< flash mode (e.g. TORCH,SINGLE,OFF) */
    int mZoomRatio;
};
} NAMESPACE_DECLARATION_END
#endif   // _EXIFMETADATA_H_

