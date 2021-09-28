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

#ifndef __RK_ENCODER_WRAPER_H__
#define __RK_ENCODER_WRAPER_H__

#include "ExifBuilder.h"

typedef struct {
    uint32_t num;
    uint32_t denom;
} rat_t;

typedef struct {
    /* GPS IFD */
    /*'N\0' 'S\0 */
    char GPSLatitudeRef[2];
    rat_t GPSLatitude[3];
    /* 'E\0' 'W\0' */
    char GPSLongitudeRef[2];
    rat_t GPSLongitude[3];
    char GPSAltitudeRef;
    rat_t GPSAltitude;
    rat_t GpsTimeStamp[3];
    /* "YYYY:MM:DD\0" */
    char GpsDateStamp[11];

    /* [101] */
    char *GPSProcessingMethod;
    /* length of GpsProcessingMethod */
    int GpsProcessingMethodchars;
} RkGPSInfo;

typedef struct {
    /* 1. IFD0 */
    /* manufacturer of digicam */
    char *maker;
    /* length of maker, equal to (strlen(maker) + 1) */
    int makerchars;
    /* model number of digicam */
    char *modelstr;
    /* length of modelstr, equal to (strlen(modelstr) + 1) */
    int modelchars;
    int Orientation;
    /* 20 chars-> yyyy:MM:dd0x20hh:mm:ss'\0' */
    char DateTime[20];

    /* 2. Exif SubIFD */
    /* ExposureTime - 1 / 400 = 0.0025s */
    rat_t ExposureTime;
    /* actual f-number */
    rat_t ApertureFNumber;
    /* CCD sensitivity equivalent to Ag-Hr film speedrate */
    int ISOSpeedRatings;
    rat_t CompressedBitsPerPixel;
    rat_t ShutterSpeedValue;
    rat_t ApertureValue;
    rat_t ExposureBiasValue;
    rat_t MaxApertureValue;
    int MeteringMode;
    int Flash;
    rat_t FocalLength;
    rat_t FocalPlaneXResolution;
    rat_t FocalPlaneYResolution;
    int SensingMethod;
    int FileSource;
    int CustomRendered;
    int ExposureMode;
    int WhiteBalance;
    rat_t DigitalZoomRatio;
    int SceneCaptureType;
    /* the internal data of maker */
    char *makernote;
    /* length of makernote, equal to (strlen(makernote) + 1) */
    int makernotechars;
    char subsec_time[8];

    int InputWidth;
    int InputHeight;

    RkGPSInfo *gpsInfo;
} RkExifInfo;

typedef struct {
    /* Pointer to thumbnail image, or NULL if not available */
    unsigned char *thumb_data;
    /* Number of bytes in thumbnail image at thumbData */
    int thumb_size;

    unsigned char *header_buf;
    int header_len;

    RkExifInfo *exifInfo;
} RkHeaderData;

/*
 * Parse from RkExifInfo -> ExifData.
 *
 * param[in] exifInfo - populated exif information outside
 * param[out] edata   - EXIF data for type #ExifBuilder
 */
void parse_exif_info(RkExifInfo *exifInfo, ExifData *edata);

/*
 * Release memery allocated at #exif_info_parser
 */
void release_exif_data(ExifData *edata);

/*
 * Generate JPEG exif app1 header.
 *
 * param[in] data     - RkHeaderData
 * param[out] buf     - pointer to buffer pointer containing app1 header data.
 * param[out] len     - pointer to hold the buf number bytes
 */
bool generate_app1_header(RkHeaderData *data, unsigned char **buf, int *len);

#endif  // __RK_ENCODER_WRAPER_H__
