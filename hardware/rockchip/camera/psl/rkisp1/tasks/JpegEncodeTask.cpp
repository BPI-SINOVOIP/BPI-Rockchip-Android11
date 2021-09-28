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

#define LOG_TAG "JpegEncode_Task"

#include "JpegEncodeTask.h"
#include "LogHelper.h"
#include "ProcUnitSettings.h"
#include "CameraStream.h"
#include "PlatformData.h"
#include "RKISP1CameraHw.h" // PartialResultEnum
#include "rkcamera_vendor_tags.h"

#define MAKERNOTEDATALEN 600
namespace android {
namespace camera2 {

JpegEncodeTask::JpegEncodeTask(int cameraId):
    mImgEncoder(nullptr),
    mJpegMaker(nullptr),
    mCameraId(cameraId)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    mMakernote.data = new char[MAKERNOTEDATALEN];
    if(mMakernote.data)
        memset(mMakernote.data, 0, MAKERNOTEDATALEN);
    mMakernote.size = 0;
}

JpegEncodeTask::~JpegEncodeTask()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    if (mJpegMaker != nullptr) {
        delete mJpegMaker;
        mJpegMaker = nullptr;
    }

    if(mMakernote.data)
        delete[] static_cast<char*>(mMakernote.data);
    mMakernote.size = 0;
    if (!mExifCacheStorage.empty())
        LOGE("EXIF cache should be empty at destruction - BUG?");
}

status_t
JpegEncodeTask::init()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;

#ifdef RK_HW_JPEG_ENCODE
    mImgEncoder = std::make_shared<ImgHWEncoder>(mCameraId);
#else
    mImgEncoder = std::make_shared<ImgEncoder>(mCameraId);
#endif
    status = mImgEncoder->init();
    if (status != NO_ERROR) {
        LOGE("Failed to init ImgEncoder!");
        mImgEncoder.reset();
        return NO_INIT;
    }

    mJpegMaker = new JpegMaker(mCameraId);

    status = mJpegMaker->init();
    if (status != NO_ERROR) {
        LOGE("Failed to init JpegMaker!");
        delete mJpegMaker;
        mJpegMaker = nullptr;
        mImgEncoder.reset();
        return NO_INIT;
    }

    return status;
}

status_t JpegEncodeTask::handleMessageSettings(ProcUnitSettings &procSettings)
{
    Camera3Request *req = procSettings.request;
    std::shared_ptr<CaptureUnitSettings> capSettings = procSettings.captureSettings;

    // EXIF data to be mapped to request ID
    ExifDataCache exifCache;
    CLEAR(exifCache);

    if (!req) {
        LOGE("JPEG settings, nullptr request!");
        return BAD_VALUE;
    }

    const CameraMetadata* settings = req->getSettings();
    uint8_t aeMode =  ANDROID_CONTROL_AE_MODE_ON;
    uint8_t controlMode =  ANDROID_CONTROL_MODE_AUTO;

    if (settings != NULL) {
        camera_metadata_ro_entry entry;
        entry = settings->find(ANDROID_CONTROL_MODE);
        if (entry.count == 1) {
            controlMode = entry.data.u8[0];
        }

        entry = settings->find(ANDROID_CONTROL_AE_MODE);
        if (entry.count == 1) {
            aeMode = entry.data.u8[0];
        }
    } else {
        LOGE("JPEG settings, no settings in request - BUG");
    }

    if (capSettings.get() == nullptr) {
        LOGE("JPEG settings, nullptr CapU settings");
        return BAD_VALUE;
    }

    int jpegBufCount = req->getBufferCountOfFormat(HAL_PIXEL_FORMAT_BLOB);
    if (jpegBufCount <= 0) {
        // No JPEG/blob buffers in request, no need to cache EXIF data
        return NO_ERROR;
    }

    // TODO: Search metadata from correct partial!
    // Currently only one, CONTROL_UNIT_PARTIAL_RESULT
    CameraMetadata *partRes = req->getAndWaitforFilledResults(CONTROL_UNIT_PARTIAL_RESULT);
    if (partRes == nullptr) {
        LOGE("No partial result for EXIF in request.");
        return BAD_VALUE;
    }

    // Read metadata result for any info useful for EXIF
    readExifInfoFromAndroidResult(*partRes, exifCache);

    exifCache.flashFired = capSettings->flashFired;

    // TODO:
    // CAM_AE_MODE_SHUTTER_PRIORITY,  // AIQ default?
    // CAM_AE_MODE_APERTURE_PRIORITY  // Only with SOC / other custom 3A control?
    if (controlMode == ANDROID_CONTROL_MODE_OFF ||
        aeMode == ANDROID_CONTROL_AE_MODE_OFF) {
        exifCache.aiqAeMode = CAM_AE_MODE_MANUAL;
    } else {
        // When android.control.aeMode:
        // ON, ON_AUTO_FLASH, ON_ALWAYS_FLASH, ON_AUTO_FLASH_REDEYE
        exifCache.aiqAeMode = CAM_AE_MODE_AUTO;
    }

    camera_metadata_entry entry;
    entry = partRes->find(RKCAMERA3_PRIVATEDATA_STILLCAP_ISP_PARAM);
    if (entry.count != 0) {
        MEMCPY_S(mMakernote.data, MAKERNOTEDATALEN , &entry.data.u8[0], entry.count);
        mMakernote.size = entry.count > (MAKERNOTEDATALEN) ? (MAKERNOTEDATALEN) : entry.count;
    }else{
        mMakernote.size = 0;
        LOGW("can't find isp param metadata!");
    }
    //MakernoteData mknTmp = capSettings->makernote;
    MakernoteData mknTmp = mMakernote;
    if (mknTmp.data != nullptr && mknTmp.size != 0) {
        exifCache.makernote = mknTmp;
    } else {
        if (mknTmp.data != nullptr) {
            // Size = 0 and non-null data is not valid for MKN.
            LOGW("Makernote data not nullptr, size %d. Should not happen.", mknTmp.size);
        }

        // Reset, just in case.
        exifCache.makernote.data = nullptr;
        exifCache.makernote.size = 0;
    }

    // Add ID-mapped cache item to vector
    mExifCacheStorage.insert(std::make_pair(req->getId(), exifCache));
    return NO_ERROR;
}

/**
 * Extracts the EXIF-usable pieces of information from Android result metadata
 *
 * \param[in] result The Android result metadata to extract information from
 * \param[out] exifCache The EXIF 'cache' object containing the EXIF info
 */
void JpegEncodeTask::readExifInfoFromAndroidResult(const CameraMetadata &result,
                                                   ExifDataCache &exifCache) const
{
    //# ANDROID_METADATA_Dynamic android.jpeg.orientation read_for_EXIF
    camera_metadata_ro_entry_t entry = result.find(ANDROID_JPEG_ORIENTATION);
    if (entry.count == 1) {
        exifCache.jpegSettings.orientation = *entry.data.i32;
    } else {
        LOGD("No ANDROID_JPEG_ORIENTATION in results for EXIF");
    }

    //# ANDROID_METADATA_Dynamic android.jpeg.quality read_for_EXIF
    entry = result.find(ANDROID_JPEG_QUALITY);
    if (entry.count == 1) {
        exifCache.jpegSettings.jpegQuality = *entry.data.u8;
    } else {
        LOGD("No ANDROID_JPEG_QUALITY in results for EXIF");
        exifCache.jpegSettings.jpegQuality = JPEG_QUALITY_DEFAULT;
    }

    //# ANDROID_METADATA_Dynamic android.jpeg.thumbnailQuality read_for_EXIF
    entry = result.find(ANDROID_JPEG_THUMBNAIL_QUALITY);
    if (entry.count == 1) {
        exifCache.jpegSettings.jpegThumbnailQuality = *entry.data.u8;
    } else {
        LOGD("No ANDROID_JPEG_THUMBNAIL_QUALITY in results for EXIF");
        exifCache.jpegSettings.jpegThumbnailQuality = THUMBNAIL_QUALITY_DEFAULT;
    }
    //# ANDROID_METADATA_Dynamic android.jpeg.thumbnailSize read_for_EXIF
    entry = result.find(ANDROID_JPEG_THUMBNAIL_SIZE);
    if (entry.count == 2) {
        exifCache.jpegSettings.thumbWidth = entry.data.i32[0];
        exifCache.jpegSettings.thumbHeight = entry.data.i32[1];
    } else {
        LOGD("No ANDROID_JPEG_THUMBNAIL_SIZE in results for EXIF");
    }

    //# ANDROID_METADATA_Dynamic android.sensor.exposureTime read_for_EXIF
    entry = result.find(ANDROID_SENSOR_EXPOSURE_TIME);
    if (entry.count == 1) {
        // EXIF exposure rational value is in seconds.
        // NOTE: the denominator in ExifMaker is usecs, Android data is nsecs.
        exifCache.exposureTimeSecs = *entry.data.i64 / 1e3;
    } else {
        LOGD("No ANDROID_SENSOR_EXPOSURE_TIME in results for EXIF");
    }

    //# ANDROID_METADATA_Dynamic android.sensor.sensitivity read_for_EXIF
    entry = result.find(ANDROID_SENSOR_SENSITIVITY);
    if (entry.count == 1) {
        exifCache.sensitivity = *entry.data.i32;
    } else {
        LOGD("No ANDROID_SENSOR_SENSITIVITY in results for EXIF");
    }

    //# ANDROID_METADATA_Dynamic android.control.awbMode read_for_EXIF
    entry = result.find(ANDROID_CONTROL_AWB_MODE);
    if (entry.count == 1) {
        AwbMode camAwb = convertAwbMode(*entry.data.i32);
        exifCache.lightSource = camAwb;
    } else {
        LOGD("No ANDROID_CONTROL_AWB_MODE in results for EXIF");
    }

    //# ANDROID_METADATA_Dynamic  android.control.aeMode read_for_EXIF
    entry = result.find(ANDROID_CONTROL_AE_MODE);
    if (entry.count == 1) {
        exifCache.aeMode = *entry.data.u8;
    } else {
        LOGD("No ANDROID_CONTROL_AE_MODE in results for EXIF");
    }

    //# ANDROID_METADATA_Dynamic android.flash.mode read_for_EXIF
    entry = result.find(ANDROID_FLASH_MODE);
    if (entry.count == 1) {
        exifCache.flashMode = *entry.data.u8;
    } else {
        LOGD("No ANDROID_FLASH_MODE in results for EXIF");
    }
}

status_t
JpegEncodeTask::handleMessageNewJpegInput(ITaskEventListener::PUTaskEvent &msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;

    LOGI("begin jpeg encoder");
    ImgEncoder::EncodePackage package;
    /* ImgHWEncoder::EncodePackage package; */

    package.jpegOut = msg.buffer;
    package.main = msg.jpegInputbuffer;
    package.thumb = nullptr;
    package.settings = msg.request->getSettings();

    ExifMetaData exifData;
    //CLEAR(exifData);

    ExifDataCache exifCache;
    CLEAR(exifCache);
    // NOTE: MKN fields now nullptr. Will bail later, if exifCache
    // data not found from vec.

    int reqId = msg.request->getId();
    std::map<int, ExifDataCache>::iterator it =
                                  mExifCacheStorage.find(reqId);
    if (it != mExifCacheStorage.end())
        exifCache = it->second;
    else
        LOGE("EXIF data for req ID %d not cached - BUG.", reqId);

    status = handleISPData(exifData);
    if (status != OK) {
        LOGE("Error setting ISP related EXIF data.");
        return status;
    }

    // Set 3A-related EXIF info
    status = handleExposureData(exifData, exifCache);
    if (status != OK) {
        LOGE("Error setting exposure EXIF data.");
        return status;
    }

    status = handleIa3ASetting(exifData, exifCache);
    if (status != OK) {
        LOGE("Error setting IA 3A EXIF data.");
        return status;
    }

    status = handleFlashData(exifData, exifCache);
    if (status != OK) {
        LOGE("Error setting flash EXIF data.");
        return status;
    }

    // GPS handled in JpegMaker::processGpsSettings()

    status = handleMakernote(exifData, exifCache);
    if (status != OK) {
        LOGE("Error setting Makernote EXIF data.");
        return status;
    }

    status = handleJpegSettings(exifData, exifCache);
    if (status != OK) {
        LOGE("Error setting JPEG info to EXIF data.");
        return status;
    }

    status = mJpegMaker->setupExifWithMetaData(package, exifData);
#ifndef RK_HW_JPEG_ENCODE
    // Do sw or HW encoding. Also create Thumb buffer if needed
    status = mImgEncoder->encodeSync(package, exifData);
    if (package.thumbOut == nullptr) {
        LOGE("%s: No thumb in EXIF", __FUNCTION__);
    }
    // Create a full JPEG image with exif data
    status = mJpegMaker->makeJpeg(package, package.jpegOut);
    if (status != NO_ERROR) {
        LOGE("%s: Make Jpeg Failed !", __FUNCTION__);
    }
#else
    exif_attribute_t exifAttributes;
    mJpegMaker->getExifAttrbutes(exifAttributes);
    ImgHWEncoder::EncodePackage pkg;
    pkg.jpegOut = msg.buffer;
    pkg.main = msg.jpegInputbuffer;
    pkg.exifMeta = &exifData;
    pkg.exifAttrs = &exifAttributes;
    status = mImgEncoder->encodeSync(pkg);
#endif

    if (!mExifCacheStorage.empty()) {
        // Done with the cached EXIF item for this request. Discard.
        mExifCacheStorage.erase(reqId);
    } else {
        LOGE("Attempt to remove item from empty EXIF cache. - BUG");
    }

    return status;
}

/**
 *
 * Converts an Android AWB mode into internal
 * Camera HAL Awb mode.
 *
 * \param androidAwb[in] Android specified AWB mode
 * \return AWB mode in HAL format
 */
AwbMode JpegEncodeTask::convertAwbMode(const uint8_t androidAwb) const
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    AwbMode cameraAwb = CAM_AWB_MODE_NOT_SET;

    cameraAwb =
        (androidAwb == ANDROID_CONTROL_AWB_MODE_INCANDESCENT) ?
                CAM_AWB_MODE_WARM_INCANDESCENT : \
        (androidAwb == ANDROID_CONTROL_AWB_MODE_FLUORESCENT) ?
                CAM_AWB_MODE_FLUORESCENT : \
        (androidAwb == ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT) ?
                CAM_AWB_MODE_WARM_FLUORESCENT : \
        (androidAwb == ANDROID_CONTROL_AWB_MODE_DAYLIGHT) ?
                CAM_AWB_MODE_DAYLIGHT : \
        (androidAwb == ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT) ?
                CAM_AWB_MODE_CLOUDY : \
        (androidAwb == ANDROID_CONTROL_AWB_MODE_TWILIGHT) ?
                CAM_AWB_MODE_SUNSET : \
        (androidAwb == ANDROID_CONTROL_AWB_MODE_SHADE) ?
                CAM_AWB_MODE_SHADOW : \
        (androidAwb == ANDROID_CONTROL_AWB_MODE_OFF) ?
                CAM_AWB_MODE_OFF : \
                CAM_AWB_MODE_AUTO;

    return cameraAwb;
}

/**
 * handleISPData
 *
 * handleISPData adds the focal length and default f-number to ExifMetaData via
 * ispData structure. The caller is responsible of freeing the allocated ispData
 * structure (ExifMetaData destructor does it)
 */
status_t
JpegEncodeTask::handleISPData(ExifMetaData& exifData) const
{
    // this gets freed when ExifMetaData is destructed
    ExifMetaData::makernote_info *ispData = new ExifMetaData::makernote_info();

    ispData->focal_length = EXIF_DEF_FOCAL_LEN_DEN * EXIF_DEF_FOCAL_LEN_NUM;

    CameraMetadata staticMeta;
    staticMeta = PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_entry focalLengths = staticMeta.find(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
    if (focalLengths.count >= 1) {
        uint32_t den = 100;
        uint32_t num = (uint32_t)(focalLengths.data.f[0] * den + 0.5);
        ispData->focal_length = num;
    }

    camera_metadata_entry apertures = staticMeta.find(ANDROID_LENS_INFO_AVAILABLE_APERTURES);
    if (apertures.count >= 1) {
        uint32_t den = 10;
        uint32_t num = (uint32_t)(apertures.data.f[0] * den + 0.5);
        ispData->f_number_curr = num << 16;
    } else {
        ispData->f_number_curr = EXIF_DEF_FNUMBER_NUM << 16;
    }
    ispData->f_number_curr += (EXIF_DEF_FNUMBER_DEN & 0xffff);
    exifData.mIspMkNote = ispData;

    return OK;
}

status_t JpegEncodeTask::handleExposureData(ExifMetaData& exifData,
                                            const ExifDataCache &exifCache) const
{
    SensorAeConfig aeConfig;
    aeConfig.evBias = 0;
    aeConfig.expTime = exifCache.exposureTimeSecs;
    aeConfig.aperture_num = 0;
    aeConfig.aperture_denum = 0;
    aeConfig.fn_num = 0;
    aeConfig.fn_denum = 0;
    aeConfig.aecApexTv = 0;
    aeConfig.aecApexSv = 0;
    aeConfig.aecApexAv = 0;
    aeConfig.digitalGain = 0;
    aeConfig.totalGain = 0;

    exifData.saveAeConfig(aeConfig);

    return OK;
}

status_t JpegEncodeTask::handleIa3ASetting(ExifMetaData& exifData,
                                           const ExifDataCache &exifCache) const
{
    exifData.mIa3ASetting.isoSpeed = exifCache.sensitivity;
    exifData.mIa3ASetting.focusDistance = exifCache.focusDistance;
    exifData.mIa3ASetting.aeMode = exifCache.aiqAeMode;
    exifData.mIa3ASetting.lightSource = exifCache.lightSource;

    return OK;
}

status_t JpegEncodeTask::handleFlashData(ExifMetaData& exifData,
                                         const ExifDataCache &exifCache) const
{
    exifData.mFlashFired = exifCache.flashFired;
    exifData.mV3AeMode = exifCache.aeMode;
    exifData.mFlashMode = exifCache.flashMode;

    return OK;
}

status_t JpegEncodeTask::handleMakernote(ExifMetaData& exifData,
                                         ExifDataCache &exifCache) const
{
    status_t status = OK;

    if (exifCache.makernote.data != nullptr && exifCache.makernote.size != 0) {
        // NOTE: saveIa3AMkNote() owns and takes a memcpy() of the MKN
        exifData.saveIa3AMkNote(exifCache.makernote);
#if 0
        delete[] static_cast<char*>(exifCache.makernote.data);
        exifCache.makernote.data = nullptr;
        exifCache.makernote.size = 0;
#endif
    } else if (exifCache.makernote.data == nullptr && exifCache.makernote.size == 0) {
      // do nothing
    } else {
        LOGE("Error writing MKN to ExifMetaData, ptr: %p size: %d.",
             exifCache.makernote.data,
             exifCache.makernote.size);
        status = UNKNOWN_ERROR;
    }

    return status;
}

status_t JpegEncodeTask::handleJpegSettings(ExifMetaData& exifData,
                                            JpegEncodeTask::ExifDataCache &exifCache) const
{
    exifData.mJpegSetting = exifCache.jpegSettings;

    return NO_ERROR;
}

} //  namespace camera2
} //  namespace android
