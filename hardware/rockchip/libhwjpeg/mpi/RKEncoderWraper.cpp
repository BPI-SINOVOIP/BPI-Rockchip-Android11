/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 *
 * author: kevin.chen@rock-chips.com
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "RKEncoderWraper"
#include <utils/Log.h>

#include <stdlib.h>
#include <string.h>
#include "RKEncoderWraper.h"

void parse_exif_info(RkExifInfo *exifInfo, ExifData *edata)
{
    RkGPSInfo *gpsInfo = exifInfo->gpsInfo;
    ExifContent *ifd;
    // Max rational size(GPS Info) - 3
    ExifRational rat[3];
    char data[8];
    char imgDes[5] = "2020";

    edata->order = EXIF_BYTE_ORDER_INTEL;

    /* 1. Save IFD0 information start */
    ifd = &edata->ifd[EXIF_IFD_0];
    ifd->entry_count = 0;

    /* --- 1). Image Description */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_IMAGE_DESCRIPTION, EXIF_FORMAT_ASCII,
                     0x05, imgDes);
    /* --- 2). Maker */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_MAKE, EXIF_FORMAT_ASCII,
                     exifInfo->makerchars, exifInfo->maker);
    /* --- 3). Model */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_MODEL, EXIF_FORMAT_ASCII,
                     exifInfo->modelchars, exifInfo->modelstr);
    /* --- 4). Orientation */
    exif_setup_long_entry(&ifd->entries[ifd->entry_count++],
                          EXIF_TAG_ORIENTATION, EXIF_FORMAT_SHORT,
                          0x01, edata->order, exifInfo->Orientation);
    /* --- 5). XResolution */
    rat[0].numerator = 72;
    rat[0].denominator = 1; // 72 / 1 inch
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_X_RESOLUTION, EXIF_FORMAT_RATIONAL,
                              0x01, edata->order, rat);
    /* --- 6). YResolution */
    rat[0].numerator = 72;
    rat[0].denominator = 1; // 72 / 1 inch
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_Y_RESOLUTION, EXIF_FORMAT_RATIONAL,
                              0x01, edata->order, rat);
    /* --- 7). ResolutionUnit */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_RESOLUTION_UNIT, EXIF_FORMAT_SHORT,
                           0x01, edata->order, 0x02); // inch
    /* --- 8). DateTime */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_DATE_TIME, EXIF_FORMAT_ASCII,
                     0x14, exifInfo->DateTime);
    /* --- 9). EXIF SUB_IFD start*/
    ifd = &edata->ifd[EXIF_IFD_EXIF];
    ifd->entry_count = 0;
    /* --- --- --- 9.1). ExposureTime */
    rat[0].numerator = exifInfo->ExposureTime.num;
    rat[0].denominator = exifInfo->ExposureTime.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_EXPOSURE_TIME, EXIF_FORMAT_RATIONAL,
                              0x01, edata->order, rat);
    /* --- --- --- 9.2). FNumber */
    rat[0].numerator = exifInfo->ApertureFNumber.num;
    rat[0].denominator = exifInfo->ApertureFNumber.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_FNUMBER, EXIF_FORMAT_RATIONAL,
                              0x01, edata->order, rat);
    /* --- --- --- 9.3). ISO Speed Ratings */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_ISO_SPEED_RATINGS, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->ISOSpeedRatings);
    /* --- --- --- 9.4). Exif Version */
    exif_setup_long_entry(&ifd->entries[ifd->entry_count++],
                          EXIF_TAG_EXIF_VERSION, EXIF_FORMAT_UNDEFINED,
                          0x04, edata->order, 0x30323230);
    /* --- --- --- 9.5). DataTime Original */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_DATE_TIME_ORIGINAL, EXIF_FORMAT_ASCII,
                     0x14, exifInfo->DateTime);
    /* --- --- --- 9.6). DataTime Digitized */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_DATE_TIME_DIGITIZED, EXIF_FORMAT_ASCII,
                     0x14, exifInfo->DateTime);
    /* --- --- --- 9.7). ComponentsConfiguration */
    exif_setup_long_entry(&ifd->entries[ifd->entry_count++],
                          EXIF_TAG_COMPONENTS_CONFIGURATION, EXIF_FORMAT_UNDEFINED,
                          0x04, edata->order, 0x00030201); // YCbCr-format
    /* --- --- --- 9.8). CompressedBitsPerPixel */
    rat[0].numerator = exifInfo->CompressedBitsPerPixel.num;
    rat[0].denominator = exifInfo->CompressedBitsPerPixel.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_COMPRESSED_BITS_PER_PIXEL,
                              EXIF_FORMAT_RATIONAL, 0x01, edata->order, rat);
    /* --- --- --- 9.9). ShutterSpeedValue */
    rat[0].numerator = exifInfo->ShutterSpeedValue.num;
    rat[0].denominator = exifInfo->ShutterSpeedValue.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_SHUTTER_SPEED_VALUE,
                              EXIF_FORMAT_RATIONAL, 0x01, edata->order, rat);
    /* --- --- --- 9.10). ApertureValue */
    rat[0].numerator = exifInfo->ApertureValue.num;
    rat[0].denominator = exifInfo->ApertureValue.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_APERTURE_VALUE,
                              EXIF_FORMAT_RATIONAL, 0x01, edata->order, rat);
    /* --- --- --- 9.11). ExposureBiasValue */
    rat[0].numerator = exifInfo->ExposureBiasValue.num;
    rat[0].denominator = exifInfo->ExposureBiasValue.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_EXPOSURE_BIAS_VALUE,
                              EXIF_FORMAT_RATIONAL, 0x01, edata->order, rat);
    /* --- --- --- 9.12). MaxApertureValue */
    rat[0].numerator = exifInfo->MaxApertureValue.num;
    rat[0].denominator = exifInfo->MaxApertureValue.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_MAX_APERTURE_VALUE,
                              EXIF_FORMAT_RATIONAL, 0x01, edata->order, rat);
    /* --- --- --- 9.13). MeteringMode */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_METERING_MODE, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->MeteringMode);
    /* --- --- --- 9.14). Flash */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_FLASH, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->Flash);
    /* --- --- --- 9.15). FocalLength */
    rat[0].numerator = exifInfo->FocalLength.num;
    rat[0].denominator = exifInfo->FocalLength.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_FOCAL_LENGTH, EXIF_FORMAT_RATIONAL,
                              0x01, edata->order, rat);
    /* --- --- --- 9.16). MakerNote */
    if (exifInfo->makernote) {
        exif_setup_entry(&ifd->entries[ifd->entry_count++],
                         EXIF_TAG_MAKER_NOTE, EXIF_FORMAT_UNDEFINED,
                         exifInfo->makernotechars, exifInfo->makernote);
    }
    /* --- --- --- 9.17). SubsecTime */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_SUB_SEC_TIME, EXIF_FORMAT_ASCII,
                     0x08, exifInfo->subsec_time);
    /* --- --- --- 9.18). SubsecTime Original */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_SUB_SEC_TIME_ORIGINAL, EXIF_FORMAT_ASCII,
                     0x08, exifInfo->subsec_time);
    /* --- --- --- 9.19). SubsecTime Digitized */
    exif_setup_entry(&ifd->entries[ifd->entry_count++],
                     EXIF_TAG_SUB_SEC_TIME_DIGITIZED, EXIF_FORMAT_ASCII,
                     0x08, exifInfo->subsec_time);
    /* --- --- --- 9.20). ColorSpace */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_COLOR_SPACE, EXIF_FORMAT_SHORT,
                           0x01, edata->order, 0x01); // sRGB
    /* --- --- --- 9.21). Exif Image Width */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_PIXEL_X_DIMENSION, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->InputWidth);
    /* --- --- --- 9.22). Exif Image Height */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_PIXEL_Y_DIMENSION, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->InputHeight);
    /* --- --- --- 9.23). FocalPlaneXResolution */
    rat[0].numerator = exifInfo->FocalPlaneXResolution.num;
    rat[0].denominator = exifInfo->FocalPlaneXResolution.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_FOCAL_PLANE_X_RESOLUTION,
                              EXIF_FORMAT_RATIONAL, 0x01, edata->order, rat);
    /* --- --- --- 9.24). FocalPlaneYResolution */
    rat[0].numerator = exifInfo->FocalPlaneYResolution.num;
    rat[0].denominator = exifInfo->FocalPlaneYResolution.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_FOCAL_PLANE_Y_RESOLUTION,
                              EXIF_FORMAT_RATIONAL, 0x01, edata->order, rat);
    /* --- --- --- 9.25). FocalPlaneResolutionUnit */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_FOCAL_PLANE_RESOLUTION_UNIT, EXIF_FORMAT_SHORT,
                           0x01, edata->order, 0x02); // inch
    /* --- --- --- 9.26). SensingMethod */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_SENSING_METHOD, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->SensingMethod);
    /* --- --- --- 9.27). FileSource */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_FILE_SOURCE, EXIF_FORMAT_UNDEFINED,
                           0x01, edata->order, exifInfo->FileSource);
    /* --- --- --- 9.28). CustomRendered */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_CUSTOM_RENDERED, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->CustomRendered);
    /* --- --- --- 9.29). ExposureMode */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_EXPOSURE_MODE, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->ExposureMode);
    /* --- --- --- 9.30). WhiteBalance */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_WHITE_BALANCE, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->WhiteBalance);
    /* --- --- --- 9.31). DigitalZoomRatio */
    rat[0].numerator = exifInfo->DigitalZoomRatio.num;
    rat[0].denominator = exifInfo->DigitalZoomRatio.denom;
    exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_DIGITAL_ZOOM_RATIO, EXIF_FORMAT_RATIONAL,
                              0x01, edata->order, rat);
    /* --- --- --- 9.32). SceneCaptureType */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_SCENE_CAPTURE_TYPE, EXIF_FORMAT_SHORT,
                           0x01, edata->order, exifInfo->SceneCaptureType);
    /* --- 10). GPS_INFO start*/
    ifd = &edata->ifd[EXIF_IFD_GPS];
    ifd->entry_count = 0;
    if (gpsInfo != NULL) {
        /* --- --- 10.1). GPSVersionID */
        exif_setup_long_entry(&ifd->entries[ifd->entry_count++],
                              EXIF_TAG_GPS_VERSION_ID, EXIF_FORMAT_BYTE,
                              0x04, edata->order, 0x0202);
        /* --- --- 10.2). GPSLatitudeRef */
        exif_setup_entry(&ifd->entries[ifd->entry_count++],
                         EXIF_TAG_GPS_LATITUDE_REF, EXIF_FORMAT_ASCII,
                         0x02, gpsInfo->GPSLatitudeRef);
        /* --- --- 10.3). GPSLatitude */
        rat[0].numerator = gpsInfo->GPSLatitude[0].num;
        rat[0].denominator = gpsInfo->GPSLatitude[0].denom;
        rat[1].numerator = gpsInfo->GPSLatitude[1].num;
        rat[1].denominator = gpsInfo->GPSLatitude[1].denom;
        rat[2].numerator = gpsInfo->GPSLatitude[2].num;
        rat[2].denominator = gpsInfo->GPSLatitude[2].denom;
        exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                                  EXIF_TAG_GPS_LATITUDE, EXIF_FORMAT_RATIONAL,
                                  0x03, edata->order, rat);
        /* --- --- 10.4). GPSLongitudeRef */
        exif_setup_entry(&ifd->entries[ifd->entry_count++],
                         EXIF_TAG_GPS_LONGITUDE_REF, EXIF_FORMAT_ASCII,
                         0x02, gpsInfo->GPSLongitudeRef);
        /* --- --- 10.5). GPSLongitude */
        rat[0].numerator = gpsInfo->GPSLongitude[0].num;
        rat[0].denominator = gpsInfo->GPSLongitude[0].denom;
        rat[1].numerator = gpsInfo->GPSLongitude[1].num;
        rat[1].denominator = gpsInfo->GPSLongitude[1].denom;
        rat[2].numerator = gpsInfo->GPSLongitude[2].num;
        rat[2].denominator = gpsInfo->GPSLongitude[2].denom;
        exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                                  EXIF_TAG_GPS_LONGITUDE, EXIF_FORMAT_RATIONAL,
                                  0x03, edata->order, rat);
        /* --- --- 10.6). GPSAltitudeRef */
        data[0] = gpsInfo->GPSAltitudeRef;
        exif_setup_entry(&ifd->entries[ifd->entry_count++],
                         EXIF_TAG_GPS_ALTITUDE_REF, EXIF_FORMAT_BYTE,
                         0x01, data);
        /* --- --- 10.7). GPSAltitude */
        rat[0].numerator = gpsInfo->GPSAltitude.num;
        rat[0].denominator = gpsInfo->GPSAltitude.denom;
        exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                                  EXIF_TAG_GPS_ALTITUDE, EXIF_FORMAT_RATIONAL,
                                  0x01, edata->order, rat);
        /* --- --- 10.8). GpsTimeStamp */
        rat[0].numerator = gpsInfo->GpsTimeStamp[0].num;
        rat[0].denominator = gpsInfo->GpsTimeStamp[0].denom;
        rat[1].numerator = gpsInfo->GpsTimeStamp[1].num;
        rat[1].denominator = gpsInfo->GpsTimeStamp[1].denom;
        rat[2].numerator = gpsInfo->GpsTimeStamp[2].num;
        rat[2].denominator = gpsInfo->GpsTimeStamp[2].denom;
        exif_setup_rational_entry(&ifd->entries[ifd->entry_count++],
                                  EXIF_TAG_GPS_TIME_STAMP, EXIF_FORMAT_RATIONAL,
                                  0x03, edata->order, rat);
        /* --- --- 10.9). GPSProcessingMethod */
        exif_setup_entry(&ifd->entries[ifd->entry_count++],
                         EXIF_TAG_GPS_PROCESSING_METHOD, EXIF_FORMAT_UNDEFINED,
                         gpsInfo->GpsProcessingMethodchars,
                         gpsInfo->GPSProcessingMethod);
        /* --- --- 10.10). GPSDateStamp */
        exif_setup_entry(&ifd->entries[ifd->entry_count++],
                         EXIF_TAG_GPS_DATE_STAMP, EXIF_FORMAT_ASCII,
                         11, gpsInfo->GpsDateStamp);
    }

    /* 2. Save IFD1 information start */
    ifd = &edata->ifd[EXIF_IFD_1];
    ifd->entry_count = 0;

    /* --- 1). Compression */
    exif_setup_short_entry(&ifd->entries[ifd->entry_count++],
                           EXIF_TAG_COMPRESSION, EXIF_FORMAT_SHORT,
                           0x01, edata->order, 0x06);
}

void release_exif_data(ExifData *edata)
{
    ExifContent *ifd;
    int i, j;

    for (i = 0; i < EXIF_IFD_COUNT; i++) {
        ifd = &edata->ifd[i];
        for (j = 0; j < ifd->entry_count; j++) {
            exif_release_entry(&ifd->entries[j]);
        }
    }
}

bool generate_app1_header(RkHeaderData *data, unsigned char **buf, int *len)
{
    bool ret;
    ExifData eData;
    unsigned char* exif_buf;
    int exif_len;

    unsigned char App1Header[] = { 0xff, 0xd8, 0xff, 0xe1 };

    if (!buf || !len) {
        ALOGE("found invalid input param.");
        return false;
    }

    ALOGV("generate APP1 header start");

    memset(&eData, 0, sizeof(ExifData));
    eData.thumb_data = data->thumb_data;
    eData.thumb_size = data->thumb_size;

    /*
     * Parse from RkExifInfo -> ExifData.
     * -param[in]  - eInfo
     * -param[out] - eData
     */
    parse_exif_info(data->exifInfo, &eData);

    ret = exif_general_build(&eData, &exif_buf, &exif_len);
    if (!ret) {
        ALOGE("failed to genaral exif from builder.");
        goto EXIT;
    }

    // Allocate enough memory for app1 header
    // (SOI + App1Marker + App1Length + exif_info)
    *buf = (unsigned char*)malloc(exif_len + 6);
    if (!*buf)  {
        ALOGE("failed to alloc app1 header buf.");
        ret = false;
        goto EXIT;
    }

    // 0xff 0xd8 0xdd 0xe1
    memcpy(*buf, App1Header, 4);
    // length of app1 exif part
    exif_set_short(*buf + 4, EXIF_BYTE_ORDER_MOTOROLA, exif_len + 2);

    /* add exif info */
    memcpy(*buf + 6, exif_buf, exif_len);

    *len = exif_len + 6;
    ret = true;

    ALOGV("generate APP1 header get len - %d",  *len);

EXIT:
    release_exif_data(&eData);

    if (exif_buf)
        free(exif_buf);

    return ret;
}
