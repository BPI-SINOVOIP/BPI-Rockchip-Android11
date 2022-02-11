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

#define LOG_TAG "ImgHWEncoder"

#include "ImgHWEncoder.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "CameraMetadataHelper.h"
#include "PlatformData.h"
#include <cutils/properties.h>

namespace android {
namespace camera2 {
#define ALIGN(value, x)     ((value + (x-1)) & (~(x-1)))

ImgHWEncoder::ImgHWEncoder(int cameraid) :
    mCameraId(cameraid),
    mEncoder(new MpiJpegEncoder)
{
    memset(sMaker, 0, sizeof(sMaker));
    memset(sModel, 0, sizeof(sModel));
    LOGI("@%s enter", __FUNCTION__);
}

ImgHWEncoder::~ImgHWEncoder()
{
    LOGI("@%s enter", __FUNCTION__);
    deInit();
}

status_t ImgHWEncoder::init()
{
    status_t status = NO_ERROR;
    LOGI("@%s enter", __FUNCTION__);

    memset(&mExifInfo, 0, sizeof(RkExifInfo));
    memset(&mGpsInfo, 0, sizeof(RkGPSInfo));

    if (!mEncoder->prepareEncoder()) {
        LOGE("@%s %d: failed to setup encoder", __FUNCTION__, __LINE__);
        status = UNKNOWN_ERROR;
    }

    return status;
}

void ImgHWEncoder::deInit()
{
    LOGI("@%s enter", __FUNCTION__);

    if (mEncoder) {
        delete mEncoder;
        mEncoder = NULL;
    }
}

void ImgHWEncoder::fillRkExifInfo(RkExifInfo &exifInfo, exif_attribute_t* exifAttrs)
{
    char maker_value[PROPERTY_VALUE_MAX] = {0};
    char model_value[PROPERTY_VALUE_MAX] = {0};

    property_get("ro.product.manufacturer", maker_value, "rockchip");
    property_get("ro.product.model", model_value, "rockchip_mid");
    memcpy(sMaker, maker_value, strlen(maker_value));
    memcpy(sModel, model_value, strlen(model_value));
    exifInfo.maker = sMaker;
    exifInfo.makerchars = ALIGN(strlen(sMaker),4);  //gallery can't get the maker if maker value longer than 4byte
    exifInfo.modelstr = sModel;
    exifInfo.modelchars = ALIGN(strlen(sModel),4);  //gallery can't get the tag if the value longer than 4byte, need fix

    exifInfo.Orientation = exifAttrs->orientation;
    memcpy(exifInfo.DateTime, exifAttrs->date_time, 20);
    exifInfo.ExposureTime.num = exifAttrs->exposure_time.num;
    exifInfo.ExposureTime.denom = exifAttrs->exposure_time.den;
    exifInfo.ApertureFNumber.num = exifAttrs->fnumber.num;
    exifInfo.ApertureFNumber.denom = exifAttrs->fnumber.den;
    exifInfo.ISOSpeedRatings = exifAttrs->iso_speed_rating;
    exifInfo.CompressedBitsPerPixel.num = 0x4;
    exifInfo.CompressedBitsPerPixel.denom = 0x1;
    exifInfo.ShutterSpeedValue.num = exifAttrs->shutter_speed.num;
    exifInfo.ShutterSpeedValue.denom = exifAttrs->shutter_speed.den;
    exifInfo.ApertureValue.num = exifAttrs->aperture.num;
    exifInfo.ApertureValue.denom = exifAttrs->aperture.den;
    exifInfo.ExposureBiasValue.num = exifAttrs->exposure_bias.num;
    exifInfo.ExposureBiasValue.denom = exifAttrs->exposure_bias.den;
    exifInfo.MaxApertureValue.num = exifAttrs->max_aperture.num;
    exifInfo.MaxApertureValue.denom = exifAttrs->max_aperture.den;
    exifInfo.MeteringMode = exifAttrs->metering_mode;
    exifInfo.Flash = exifAttrs->flash;
    exifInfo.FocalLength.num = exifAttrs->focal_length.num;
    exifInfo.FocalLength.denom = exifAttrs->focal_length.den;
    exifInfo.FocalPlaneXResolution.num = exifAttrs->x_resolution.num;
    exifInfo.FocalPlaneXResolution.denom = exifAttrs->x_resolution.den;
    exifInfo.FocalPlaneYResolution.num = exifAttrs->y_resolution.num;
    exifInfo.FocalPlaneYResolution.denom = exifAttrs->y_resolution.den;
    exifInfo.SensingMethod = 2;//2 means 1 chip color area sensor
    exifInfo.FileSource = 3;//3 means the image source is digital still camera
    exifInfo.CustomRendered = exifAttrs->custom_rendered;
    exifInfo.ExposureMode = exifAttrs->exposure_mode;
    exifInfo.WhiteBalance = exifAttrs->white_balance;
    exifInfo.DigitalZoomRatio.num = exifAttrs->zoom_ratio.num;
    exifInfo.DigitalZoomRatio.denom = exifAttrs->zoom_ratio.den;
    exifInfo.SceneCaptureType = exifAttrs->scene_capture_type;
    exifInfo.makernote = NULL;
    exifInfo.makernotechars = 0;
    memcpy(exifInfo.subsec_time, exifAttrs->subsec_time, 8);
}

void ImgHWEncoder::fillGpsInfo(RkGPSInfo &gpsInfo, exif_attribute_t* exifAttrs)
{
    gpsInfo.GPSLatitudeRef[0] = exifAttrs->gps_latitude_ref[0];
    gpsInfo.GPSLatitudeRef[1] = exifAttrs->gps_latitude_ref[1];
    gpsInfo.GPSLatitude[0].num = exifAttrs->gps_latitude[0].num;
    gpsInfo.GPSLatitude[0].denom = exifAttrs->gps_latitude[0].den;
    gpsInfo.GPSLatitude[1].num = exifAttrs->gps_latitude[1].num;
    gpsInfo.GPSLatitude[1].denom = exifAttrs->gps_latitude[1].den;
    gpsInfo.GPSLatitude[2].num = exifAttrs->gps_latitude[2].num;
    gpsInfo.GPSLatitude[2].denom = exifAttrs->gps_latitude[2].den;
    gpsInfo.GPSLongitudeRef[0] = exifAttrs->gps_longitude_ref[0];
    gpsInfo.GPSLongitudeRef[1] = exifAttrs->gps_longitude_ref[1];
    gpsInfo.GPSLongitude[0].num = exifAttrs->gps_longitude[0].num;
    gpsInfo.GPSLongitude[0].denom = exifAttrs->gps_longitude[0].den;
    gpsInfo.GPSLongitude[1].num = exifAttrs->gps_longitude[1].num;
    gpsInfo.GPSLongitude[1].denom = exifAttrs->gps_longitude[1].den;
    gpsInfo.GPSLongitude[2].num = exifAttrs->gps_longitude[2].num;
    gpsInfo.GPSLongitude[2].denom = exifAttrs->gps_longitude[2].den;
    gpsInfo.GPSAltitudeRef = exifAttrs->gps_altitude_ref;
    gpsInfo.GPSAltitude.num = exifAttrs->gps_altitude.num;
    gpsInfo.GPSAltitude.denom = exifAttrs->gps_altitude.den;
    gpsInfo.GpsTimeStamp[0].num = exifAttrs->gps_timestamp[0].num;
    gpsInfo.GpsTimeStamp[0].denom = exifAttrs->gps_timestamp[0].den;
    gpsInfo.GpsTimeStamp[1].num = exifAttrs->gps_timestamp[1].num;
    gpsInfo.GpsTimeStamp[1].denom = exifAttrs->gps_timestamp[1].den;
    gpsInfo.GpsTimeStamp[2].num = exifAttrs->gps_timestamp[2].num;
    gpsInfo.GpsTimeStamp[2].denom = exifAttrs->gps_timestamp[2].den;
    memcpy(gpsInfo.GpsDateStamp, exifAttrs->gps_datestamp, 11);//"YYYY:MM:DD\0"
    gpsInfo.GPSProcessingMethod = (char *)exifAttrs->gps_processing_method;
    gpsInfo.GpsProcessingMethodchars = 100;//length of GpsProcessingMethod
}

bool
ImgHWEncoder::checkInputBuffer(CameraBuffer* buf) {
    // just for YUV420 format buffer
    int Ysize = ALIGN(buf->width(), 16) * ALIGN(buf->height(), 16);
    int UVsize = ALIGN(buf->width(), 16) * ALIGN(buf->height() / 2, 16);
    if(buf->size() >= Ysize + UVsize) {
        return true;
    } else {
        LOGE("@%s : Input buffer (%dx%d) size(%d) can't  meet the HwJpeg input condition",
             __FUNCTION__, buf->width(), buf->height(), buf->size());
        return false;
    }
}

/**
 * encodeSync
 *
 */
status_t ImgHWEncoder::encodeSync(EncodePackage & package)
{
    PERFORMANCE_ATRACE_CALL();
    status_t status = NO_ERROR;

    MpiJpegEncoder::EncInInfo encInInfo;
    MpiJpegEncoder::EncOutInfo encOutInfo;

    std::shared_ptr<CameraBuffer> srcBuf = package.main;
    std::shared_ptr<CameraBuffer> destBuf = package.jpegOut;
    ExifMetaData     *exifMeta = package.exifMeta;
    exif_attribute_t *exifAttrs = package.exifAttrs;

    int width = srcBuf->width();
    int height = srcBuf->height();
    int stride = srcBuf->stride();
    void* srcY = srcBuf->data();

    int  jpegw = width;
    int  jpegh = height;
    int outJPEGSize = destBuf->size();

    int quality = exifMeta->mJpegSetting.jpegQuality;
    int thumbquality = exifMeta->mJpegSetting.jpegThumbnailQuality;

    LOGI("@%s %d: in buffer fd:%d, vir_addr:%p, out buffer fd:%d, vir_addr:%p", __FUNCTION__, __LINE__,
         srcBuf->dmaBufFd(), srcBuf->data(),
         destBuf->dmaBufFd(), destBuf->data());

    // HwJpeg encode require buffer width and height align to 16 or large enough.
    if(!checkInputBuffer(srcBuf.get()))
        return UNKNOWN_ERROR;

    memset(&encInInfo, 0, sizeof(MpiJpegEncoder::EncInInfo));
    memset(&encOutInfo, 0, sizeof(MpiJpegEncoder::EncOutInfo));

    encInInfo.inputPhyAddr = srcBuf->dmaBufFd();
    encInInfo.inputVirAddr = (unsigned char *)srcBuf->data();
    encInInfo.width = jpegw;
    encInInfo.height = jpegh;
    encInInfo.format = MpiJpegEncoder::INPUT_FMT_YUV420SP;
    encInInfo.qLvl = 8;
    encInInfo.thumbQLvl = 8;
    // if not doThumb,please set doThumbNail,thumbW and thumbH to zero;
    if (exifMeta->mJpegSetting.thumbWidth && exifMeta->mJpegSetting.thumbHeight)
        encInInfo.doThumbNail = 1;
    else
        encInInfo.doThumbNail = 0;
    LOGD("@%s : exifAttrs->enableThumb = %d doThumbNail=%d", __FUNCTION__,
         exifAttrs->enableThumb, encInInfo.doThumbNail);
    encInInfo.thumbWidth = exifMeta->mJpegSetting.thumbWidth;
    encInInfo.thumbHeight = exifMeta->mJpegSetting.thumbHeight;
    // if thumbData is NULL, do scale, the type above can not be 420_P or
    // 422_UYVY MppJpegEncInInfo.thumbData = NULL; MppJpegEncInInfo.thumbDataLen
    // = 0; //don't care when thumbData is Null
    encInInfo.thumbQLvl = thumbquality / 10;
    if (encInInfo.thumbQLvl < 5)
        encInInfo.thumbQLvl = 5;
    if (encInInfo.thumbQLvl > 10)
        encInInfo.thumbQLvl = 10;

    fillRkExifInfo(mExifInfo, exifAttrs);
    mExifInfo.InputWidth = jpegw;
    mExifInfo.InputHeight = jpegh;

    encInInfo.exifInfo = &mExifInfo;
    if (exifAttrs->enableGps) {
        fillGpsInfo(mGpsInfo, exifAttrs);
        encInInfo.gpsInfo = &mGpsInfo;
    } else {
        encInInfo.gpsInfo = NULL;
    }

    encOutInfo.outputPhyAddr = destBuf->dmaBufFd();
    encOutInfo.outputVirAddr = (unsigned char *)destBuf->data();
    encOutInfo.outBufLen = outJPEGSize;

    LOGI("MppJpegEncInInfo thumbWidth:%d, thumbHeight:%d, thumbQLvl:%d, "
         "width:%d, height:%d, qLvl:%d",
         encInInfo.thumbWidth, encInInfo.thumbHeight,
         encInInfo.thumbQLvl, encInInfo.width,
         encInInfo.height, encInInfo.qLvl);

    if (mEncoder->encode(&encInInfo, &encOutInfo) == false ||
        encOutInfo.outBufLen <= 0) {
        LOGE("@%s %d: hw jpeg encode fail.", __FUNCTION__, __LINE__);
        return UNKNOWN_ERROR;
    }
    LOGI("@%s %d: actual jpeg size: %d, destBuf size: %d ", __FUNCTION__,
         __LINE__, encOutInfo.outBufLen, destBuf->size());

    // save jpeg size at the end of file, App will detect this header for the
    // jpeg actual size
    if (destBuf->size()) {
        char *pCur = (char*)(destBuf->data()) + destBuf->size()
            - sizeof(struct camera_jpeg_blob);
        struct camera_jpeg_blob *blob = new struct camera_jpeg_blob;
        blob->jpeg_blob_id = CAMERA_JPEG_BLOB_ID;
        blob->jpeg_size = encOutInfo.outBufLen;
        STDCOPY(pCur, (int8_t *)blob, sizeof(struct camera_jpeg_blob));
        delete blob;
    } else {
        LOGE("ERROR: JPEG_MAX_SIZE is 0 !");
    }

    return status;
}

}
}
