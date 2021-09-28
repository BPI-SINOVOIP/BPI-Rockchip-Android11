/*
 * Copyright (C) 2014-2017 Intel Corporation.
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

#ifndef CAMERA3_HAL_JPEGENCODETASK_H_
#define CAMERA3_HAL_JPEGENCODETASK_H_

#include "ExecuteTaskBase.h"
#include "ImgEncoder.h"
#include "JpegMaker.h"
#include "RKISP1Common.h"
#include "ImgHWEncoder.h"

namespace android {
namespace camera2 {
/**
 * \class JpegEncodeTask
 * Does the jpeg encoding of YUV input buffers.
 * This class listens for completed JPEG buffers from RawtoYUVTask. The JPEG
 * encode task shall execute in its own thread to ensure parallelism.
 * The JpegEncode task shall create its own instance of StreamOutputTask to
 * return the completed JPEG buffer to the framework.
 *
 */

class JpegEncodeTask {
public:
    explicit JpegEncodeTask(int cameraId);
    virtual ~JpegEncodeTask();

    virtual status_t init();

    virtual status_t handleMessageNewJpegInput(ITaskEventListener::PUTaskEvent &msg);
    virtual status_t handleMessageSettings(ProcUnitSettings &procSettings);

private:
    // Forward declare
    struct ExifDataCache;

    AwbMode convertAwbMode(const uint8_t googleAwb) const;

    void readExifInfoFromAndroidResult(const CameraMetadata &result,
                                       ExifDataCache &exifCache) const;
    status_t handleISPData(ExifMetaData& exifData) const;
    status_t handleExposureData(ExifMetaData& exifData, const ExifDataCache &exifCache) const;
    status_t handleIa3ASetting(ExifMetaData& exifData, const ExifDataCache &exifCache) const;
    status_t handleFlashData(ExifMetaData& exifData, const ExifDataCache &exifCache) const;
    status_t handleMakernote(ExifMetaData& exifData, ExifDataCache &exifCache) const;
    status_t handleJpegSettings(ExifMetaData& exifData, ExifDataCache &exifCache) const;

// Types
private:
    /**
     * Strucuture for 'caching' EXIF-related
     * information from Android metadata.
     */
    struct ExifDataCache {
        // Cached from Android result metadata
        int64_t exposureTimeSecs;
        int32_t sensitivity;        // ISO value
        uint8_t aeMode;
        uint8_t flashMode;
        AwbMode lightSource;
        // Cached from CaptureUnitSettings
        bool flashFired;
        // Cached from AIQ library data. Data types reflect the ones used in AIQ
        AeMode aiqAeMode;
        unsigned short focusDistance;
        MakernoteData makernote;
        // JPEG info. Used from EXIF for encoding params in some cases.
        ExifMetaData::JpegSetting jpegSettings;
    };

private:
#ifdef RK_HW_JPEG_ENCODE
    std::shared_ptr<ImgHWEncoder> mImgEncoder;
#else
    std::shared_ptr<ImgEncoder> mImgEncoder;
#endif
    JpegMaker      *mJpegMaker;
    int mCameraId;
    std::map<int, ExifDataCache> mExifCacheStorage; // key: Req ID
    MakernoteData mMakernote;
};

}  // namespace camera2
}  // namespace android

#endif  // CAMERA3_HAL_JPEGENCODETASK_H_
