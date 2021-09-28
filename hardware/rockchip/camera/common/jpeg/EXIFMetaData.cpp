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

#define LOG_TAG "EXIFMetaData"
#include "LogHelper.h"
#include "EXIFMetaData.h"

#define DEFAULT_ISO_SPEED 100

NAMESPACE_DECLARATION {
ExifMetaData::ExifMetaData():
    mAeConfig(nullptr)
    , mIspMkNote(nullptr)
    , mIa3AMkNote(nullptr)
    , mAwbMode(CAM_AWB_MODE_NOT_SET)
    , mSoftware(nullptr)
    , mFlashFired(false)
    , mV3AeMode(BAD_VALUE)
    , mFlashMode(BAD_VALUE)
    , mZoomRatio(1)
{
    LOGI("@%s", __FUNCTION__);
    mJpegSetting.jpegQuality = 90;
    mJpegSetting.jpegThumbnailQuality = 90;
    mJpegSetting.orientation = 0;
    mJpegSetting.thumbWidth = 320;
    mJpegSetting.thumbHeight = 240;
    mGpsSetting.latitude = 0.0;
    mGpsSetting.longitude = 0.0;
    mGpsSetting.altitude = 0.0;
    CLEAR(mGpsSetting.gpsProcessingMethod);
    mGpsSetting.gpsTimeStamp = 0;
    CLEAR(mIa3ASetting);
    mIa3ASetting.isoSpeed = DEFAULT_ISO_SPEED;
}

/**
 * Used for clone SensorAeConfig structure from its caller
 */
status_t ExifMetaData::saveAeConfig(SensorAeConfig& config)
{
    if (mAeConfig == nullptr) {
        mAeConfig = new SensorAeConfig();
    }
    *mAeConfig = config;

    return NO_ERROR;
}

/**
 * Used for clone Makernote structure from its caller
 */

status_t ExifMetaData::saveIspMkNote(MakernoteType& mkNote)
{
    if (mIspMkNote == nullptr) {
        mIspMkNote = new MakernoteType();
    }
    *mIspMkNote = mkNote;

    return NO_ERROR;
}



/**
 * Used for clone and deep copy ia_binary_data structure from its caller
 */
status_t ExifMetaData::saveIa3AMkNote(const ia_binary_data& mkNote)
{
    unsigned int newSize = mkNote.size;
    bool needAllocMem = false;

    if (mIa3AMkNote == nullptr) {
        mIa3AMkNote = new ia_binary_data();
        needAllocMem = true;
    } else if (mIa3AMkNote->size != newSize) {
        // If size not equal, release the old data and alloc new one.
        char *tmp = reinterpret_cast<char*> (mIa3AMkNote->data);
        delete[] tmp;
        needAllocMem = true;
    }

    if (needAllocMem) {
        mIa3AMkNote->data = new char[newSize];
    }

    mIa3AMkNote->size = newSize;
    MEMCPY_S(mIa3AMkNote->data, mIa3AMkNote->size, mkNote.data, newSize);

    return NO_ERROR;
}

ExifMetaData::~ExifMetaData()
{
    if (mIa3AMkNote) {
        if (mIa3AMkNote->data) {
            char *tmp = reinterpret_cast<char*> (mIa3AMkNote->data);
            delete[] tmp;
            mIa3AMkNote->data = nullptr;
        }
        delete mIa3AMkNote;
        mIa3AMkNote = nullptr;
    }

    if (mIspMkNote) {
        delete mIspMkNote;
        mIspMkNote = nullptr;
    }

    if (mAeConfig) {
        delete mAeConfig;
        mAeConfig = nullptr;
    }

}
} NAMESPACE_DECLARATION_END
