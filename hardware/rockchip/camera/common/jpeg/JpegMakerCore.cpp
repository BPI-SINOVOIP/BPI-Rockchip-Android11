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

#define LOG_TAG "JpegMakerCore"

#include "LogHelper.h"
#include "JpegMakerCore.h"
#include "PlatformData.h"
#include "CameraMetadataHelper.h"
#include <cutils/properties.h>

NAMESPACE_DECLARATION {
static const unsigned char JPEG_MARKER_SOI[2] = {0xFF, 0xD8};  // JPEG StartOfImage marker

JpegMakerCore::JpegMakerCore(int cameraid) :
    mExifMaker(nullptr)
    ,mCameraId(cameraid)
{
    LOGI("@%s", __FUNCTION__);
}

JpegMakerCore::~JpegMakerCore()
{
    LOGI("@%s", __FUNCTION__);

    if (mExifMaker != nullptr) {
        delete mExifMaker;
        mExifMaker = nullptr;
    }
}

status_t JpegMakerCore::init(void)
{
    LOGI("@%s", __FUNCTION__);

    if (mExifMaker == nullptr) {
        mExifMaker = new EXIFMaker();
    }

    return NO_ERROR;
}

status_t JpegMakerCore::setupExifWithMetaData(ImgEncoderCore::EncodePackage & package,
                                              ExifMetaData& metaData)
{
    LOGI("@%s", __FUNCTION__);

    status_t status = NO_ERROR;

    status = processJpegSettings(package, metaData);

    status = processExifSettings(package.settings, metaData);
    if (status != NO_ERROR) {
        LOGE("@%s: Process settngs for Exif! %d", __FUNCTION__, status);
        return status;
    }

    mExifMaker->initialize(package.main->width(), package.main->height());
    mExifMaker->pictureTaken(metaData);
    if (metaData.mIspMkNote)
        mExifMaker->setDriverData(*metaData.mIspMkNote);
    if (metaData.mIa3AMkNote)
        mExifMaker->setMakerNote(*metaData.mIa3AMkNote);
    if (metaData.mAeConfig)
        mExifMaker->setSensorAeConfig(*metaData.mAeConfig);

    mExifMaker->enableFlash(metaData.mFlashFired, metaData.mV3AeMode, metaData.mFlashMode);

    mExifMaker->initializeLocation(metaData);

    if (metaData.mSoftware)
        mExifMaker->setSoftware(metaData.mSoftware);

    char property_value[PROPERTY_VALUE_MAX] = {0};

    property_get("ro.product.manufacturer", property_value, "rockchip");
    mExifMaker->setMaker(property_value);

    property_get("ro.product.model", property_value, "rockchip_mid");
    mExifMaker->setModel(property_value);

    return status;
}

void JpegMakerCore::getExifAttrbutes(exif_attribute_t& exifAttributes)
{
    return mExifMaker->getExifAttrbutes(exifAttributes);
}

/**
 * makeJpeg
 *
 * Create and prefix the exif header to the encoded jpeg data.
 * Skip the SOI marker got from the JPEG encoding. Append the camera3_jpeg_blob
 * at the end of the buffer.
 *
 * \param package [IN] EncodePackage from the caller with encoded main and thumb
 *  buffers , Jpeg settings, and encoded sizes
 * \param dest [IN] The final output buffer to client
 *
 */
status_t JpegMakerCore::makeJpeg(ImgEncoderCore::EncodePackage & package)
{
    LOGI("@%s", __FUNCTION__);
    unsigned int exifSize = 0;
    unsigned char* currentPtr = (unsigned char*)(package.jpegOut->data());

    // encodedDataSize <0 means main image encode failed
    if (package.encodedDataSize <= 0) {
        LOGE("ERROR: main image encode failed");
        return BAD_VALUE;
    }
    // Copy the SOI marker
    unsigned int jSOISize = sizeof(JPEG_MARKER_SOI);
    MEMCPY_S(currentPtr, package.jpegOut->size(), JPEG_MARKER_SOI, jSOISize);
    currentPtr += jSOISize;

    if (package.thumbOut.get()) {
        mExifMaker->setThumbnail((unsigned char *)package.thumbOut->data(), package.thumbSize,
                                 package.thumbOut->width(), package.thumbOut->height());
        exifSize = mExifMaker->makeExif(&currentPtr);
    } else {
        // No thumb is not critical, we can continue with main picture image
        exifSize = mExifMaker->makeExif(&currentPtr);
        LOGW("Exif created without thumbnail stream!");
    }
    currentPtr += exifSize;

    int finalSize = exifSize + package.encodedDataSize;
    int32_t jpegBufferSize = package.jpegOut->size();
    if (jpegBufferSize < finalSize) {
        LOGE("ERROR: alloc jpeg output size is not enough");
        return BAD_VALUE;
    }
    // Since the jpeg got from libmix JPEG encoder start with SOI marker
    // and EXIF also have the SOI marker so need to remove SOI marker fom
    // the start of the jpeg data
    STDCOPY(currentPtr,
            reinterpret_cast<char *>(package.encodedData->data())+jSOISize,
            package.encodedDataSize-jSOISize);

    // save jpeg size at the end of file
    if (jpegBufferSize) {
        LOGI("actual jpeg size=%d, jpegbuffer size=%d", finalSize, jpegBufferSize);
        currentPtr = (unsigned char*)(package.jpegOut->data()) + jpegBufferSize
                - sizeof(struct camera_jpeg_blob);
        struct camera_jpeg_blob *blob = new struct camera_jpeg_blob;
        blob->jpeg_blob_id = CAMERA_JPEG_BLOB_ID;
        blob->jpeg_size = finalSize;
        STDCOPY(currentPtr, (int8_t *)blob, sizeof(struct camera_jpeg_blob));
        delete blob;
    } else {
        LOGE("ERROR: JPEG_MAX_SIZE is 0 !");
    }

    return NO_ERROR;
}

status_t JpegMakerCore::processExifSettings(const CameraMetadata  *settings,
                                            ExifMetaData& metaData)
{
    LOGI("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;

    status = processAwbSettings(*settings, metaData);
    status |= processGpsSettings(*settings, metaData);
    status |= processScalerCropSettings(*settings, metaData);
    status |= processEvCompensationSettings(*settings, metaData);

    return status;
}

/**
 * processJpegSettings
 *
 * Store JPEG settings to the exif metadata
 *
 * \param settings [IN] settings metadata of the request
 *
 */
status_t JpegMakerCore::processJpegSettings(ImgEncoderCore::EncodePackage & package,
                                            ExifMetaData& metaData)
{
    LOGI("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;

    const CameraMetadata *settings = package.settings;

    //# METADATA_Control jpeg.quality done
    unsigned int tag = ANDROID_JPEG_QUALITY;
    camera_metadata_ro_entry entry = settings->find(tag);
    if (entry.count == 1) {
        uint8_t value = entry.data.u8[0];
        metaData.mJpegSetting.jpegQuality = value;
    }

    //# METADATA_Control jpeg.thumbnailQuality done
    tag = ANDROID_JPEG_THUMBNAIL_QUALITY;
    entry = settings->find(tag);
    if (entry.count == 1) {
        uint8_t value = entry.data.u8[0];
        metaData.mJpegSetting.jpegThumbnailQuality = value;
    }

    //# METADATA_Control jpeg.thumbnailSize done
    tag = ANDROID_JPEG_THUMBNAIL_SIZE;
    entry = settings->find(tag);
    if (entry.count == 2) {
        metaData.mJpegSetting.thumbWidth = entry.data.i32[0];
        metaData.mJpegSetting.thumbHeight = entry.data.i32[1];
    }

    //# METADATA_Control jpeg.orientation done
    tag = ANDROID_JPEG_ORIENTATION;
    entry = settings->find(tag);
    if (entry.count == 1) {
       metaData.mJpegSetting.orientation = entry.data.i32[0];
    }

    LOGI("jpegQuality=%d,thumbQuality=%d,thumbW=%d,thumbH=%d,orientation=%d",
         metaData.mJpegSetting.jpegQuality,
         metaData.mJpegSetting.jpegThumbnailQuality,
         metaData.mJpegSetting.thumbWidth,
         metaData.mJpegSetting.thumbHeight,
         metaData.mJpegSetting.orientation);

    return status;
}

/**
 * This function will get GPS metadata from request setting
 *
 * \param[in] settings The Anroid metadata to process GPS settings from
 * \param[out] metadata The EXIF data where the GPS setting are written to
 */
status_t JpegMakerCore::processGpsSettings(const CameraMetadata &settings,
                                           ExifMetaData& metadata)
{
    LOGI("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;
    unsigned int len = 0;
    // get GPS
    //# METADATA_Control jpeg.gpsCoordinates done
    unsigned int tag = ANDROID_JPEG_GPS_COORDINATES;
    camera_metadata_ro_entry entry = settings.find(tag);
    if (entry.count == 3) {
        metadata.mGpsSetting.latitude = entry.data.d[0];
        metadata.mGpsSetting.longitude = entry.data.d[1];
        metadata.mGpsSetting.altitude = entry.data.d[2];
    }
    LOGI("GPS COORDINATES(%f, %f, %f)", metadata.mGpsSetting.latitude,
         metadata.mGpsSetting.longitude, metadata.mGpsSetting.altitude);

    //# METADATA_Control jpeg.gpsProcessingMethod done
    tag = ANDROID_JPEG_GPS_PROCESSING_METHOD;
    entry = settings.find(tag);
    if (entry.count > 0) {
        MEMCPY_S(metadata.mGpsSetting.gpsProcessingMethod,
                 sizeof(metadata.mGpsSetting.gpsProcessingMethod),
                 entry.data.u8,
                 entry.count);
        len = MIN(entry.count, MAX_NUM_GPS_PROCESSING_METHOD - 1);
        metadata.mGpsSetting.gpsProcessingMethod[len] = '\0';
    }

    //# METADATA_Control jpeg.gpsTimestamp done
    tag = ANDROID_JPEG_GPS_TIMESTAMP;
        entry = settings.find(tag);
    if (entry.count == 1) {
        metadata.mGpsSetting.gpsTimeStamp = entry.data.i64[0];
    }
    return status;
}

status_t JpegMakerCore::processAwbSettings(const CameraMetadata &settings,
                                           ExifMetaData& metaData)
{
    LOGI("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;

    // get awb mode
    unsigned int tag = ANDROID_CONTROL_AWB_MODE;
    camera_metadata_ro_entry entry = settings.find(tag);
    if (entry.count == 1) {
        uint8_t value = entry.data.u8[0];
        AwbMode newValue = \
            (value == ANDROID_CONTROL_AWB_MODE_INCANDESCENT) ?
                    CAM_AWB_MODE_WARM_INCANDESCENT : \
            (value == ANDROID_CONTROL_AWB_MODE_FLUORESCENT) ?
                    CAM_AWB_MODE_FLUORESCENT : \
            (value == ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT) ?
                    CAM_AWB_MODE_WARM_FLUORESCENT : \
            (value == ANDROID_CONTROL_AWB_MODE_DAYLIGHT) ?
                    CAM_AWB_MODE_DAYLIGHT : \
            (value == ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT) ?
                    CAM_AWB_MODE_CLOUDY : \
            (value == ANDROID_CONTROL_AWB_MODE_TWILIGHT) ?
                    CAM_AWB_MODE_SUNSET : \
            (value == ANDROID_CONTROL_AWB_MODE_SHADE) ?
                    CAM_AWB_MODE_SHADOW : \
            CAM_AWB_MODE_AUTO;

        metaData.mAwbMode = newValue;
    }
    LOGI("awb mode=%d", metaData.mAwbMode);
    return status;
}

status_t JpegMakerCore::processScalerCropSettings(const CameraMetadata &settings,
                                                  ExifMetaData& metaData)
{
    LOGI("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;
    CameraMetadata staticMeta;
    const int32_t sensorActiveArrayCount = 4;
    const uint32_t scalerCropCount = 4;
    int count = 0;

    staticMeta = PlatformData::getStaticMetadata(mCameraId);
    const int32_t *rangePtr = (int32_t *)MetadataHelper::getMetadataValues(
        staticMeta, ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, TYPE_INT32, &count);

    camera_metadata_ro_entry entry = settings.find(ANDROID_SCALER_CROP_REGION);
    if (entry.count == scalerCropCount && count == sensorActiveArrayCount && rangePtr) {
        if (entry.data.i32[2] != 0 && entry.data.i32[3] != 0
            && rangePtr[2] != 0 && rangePtr[3] != 0) {
            metaData.mZoomRatio = (rangePtr[2] * 100)/ entry.data.i32[2];

            LOGI("scaler width %d height %d, sensor active array width %d height : %d",
                entry.data.i32[2], entry.data.i32[3], rangePtr[2], rangePtr[3]);
        }
    }

    return status;
}

status_t JpegMakerCore::processEvCompensationSettings(const CameraMetadata &settings,
                                                      ExifMetaData& metaData)
{
    LOGI("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;
    float stepEV = 1 / 3.0f;
    int32_t evCompensation = 0;

    camera_metadata_ro_entry entry =
            settings.find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
    if (entry.count != 1)
        return status;

    evCompensation = entry.data.i32[0];
    stepEV = PlatformData::getStepEv(mCameraId);
    // Fill the evBias
    if (metaData.mAeConfig != nullptr)
        metaData.mAeConfig->evBias = evCompensation * stepEV;

    return status;
}

} NAMESPACE_DECLARATION_END
