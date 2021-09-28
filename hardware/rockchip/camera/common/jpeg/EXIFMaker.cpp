/*
 * Copyright (C) 2012-2017 Intel Corporation
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

#define LOG_TAG "EXIFMaker"

#include <limits.h> // LONG_MAX, LONG_MIN
#include <sstream>
#include <iomanip>
#include "EXIFMaker.h"
#include "LogHelper.h"

#include "PlatformData.h"
#include "Utils.h"
#include "common/CommonUtilMacros.h"

#define DEFAULT_ISO_SPEED 100
#define EPSILON 0.00001

NAMESPACE_DECLARATION {
EXIFMaker::EXIFMaker() :
    exifSize(-1)
    ,initialized(false)
{
    LOGI("@%s", __FUNCTION__);
    CLEAR(exifAttributes);
}

EXIFMaker::~EXIFMaker()
{
    LOGI("@%s", __FUNCTION__);
}

/**
 *  * Sets MakerNote field data.
 *   */
void EXIFMaker::setMakerNote(const ia_binary_data &aaaMkNoteData)
{
    LOGI("@%s: %d bytes", __FUNCTION__, aaaMkNoteData.size);

    if (aaaMkNoteData.data) {
        exifAttributes.makerNoteDataSize = aaaMkNoteData.size;
        exifAttributes.makerNoteData = (unsigned char*) aaaMkNoteData.data;
    }
}

uint32_t EXIFMaker::getMakerNoteDataSize() const
{
    LOGI("@%s", __FUNCTION__);
    // overhead: MAKERNOTE_ID + APP2 marker + length field
    return exifAttributes.makerNoteDataSize + SIZEOF_APP2_OVERHEAD;
}

/**
 * Sets picture meta data retrieved from atomisp kernel driver.
 *
 * @param ispData struct retrieved with ATOMISP_IOC_ISP_MAKERNOTE
 *                kernel ioctl
 */
void EXIFMaker::setDriverData(const ExifMetaData::MakernoteType &ispData)
{
    LOGI("@%s", __FUNCTION__);

    // f number
    if (ispData.f_number_curr > 0) {
        exifAttributes.fnumber.num = ispData.f_number_curr >> 16;
        exifAttributes.fnumber.den = ispData.f_number_curr & 0xffff;

        double f_number = (double)exifAttributes.fnumber.num /
                          (double)exifAttributes.fnumber.den;
        exifAttributes.max_aperture.num =
                          (uint32_t)(10000 * APEX_FNUM_TO_APERTURE(f_number));
        exifAttributes.max_aperture.den = 10000;
    } else {
        LOGW("Invalid fnumber %u from driver", ispData.f_number_curr);
    }

    LOGI("EXIF: fnumber=%u (num=%d, den=%d)", ispData.f_number_curr,
         exifAttributes.fnumber.num, exifAttributes.fnumber.den);

    // the actual focal length of the lens, in mm.
    // there is no API for lens position.
    if (ispData.focal_length > 0) {
        //the focal length unit (mm * 100) from CMC
        exifAttributes.focal_length.num = ispData.focal_length;
        exifAttributes.focal_length.den = 100;
    } else {
        LOGW("Invalid focal length %u from driver", ispData.focal_length);
    }

    LOGI("EXIF: focal length=%u (num=%d, den=%d)", ispData.focal_length,
         exifAttributes.focal_length.num, exifAttributes.focal_length.den);
}

/**
 * Fills EXIF data after a picture has been taken to
 * record the active sensor, 3A and ISP state to EXIF metadata.
 *
 * This function is intented to set EXIF tags belonging
 * to the EXIF "Per Picture Camera Setting" group.
 *
 * @arg params active Android HAL parameters
 */
void EXIFMaker::pictureTaken(ExifMetaData& exifmetadata)
{
    LOGI("@%s", __FUNCTION__);

    // brightness, -99.99 to 99.99. FFFFFFFF.H means unknown.
    float brightness;
    // TODO: The check for getAeManualBrightness of 3A should be moved
    //       to MetaData class, because the metadata collection happen
    //       at capture time
    brightness = exifmetadata.mIa3ASetting.brightness;
    exifAttributes.brightness.num = static_cast<int>(brightness * 100);
    exifAttributes.brightness.den = 100;
    LOGI("EXIF: brightness = %.2f", brightness);

    exifAttributes.contrast   = exifmetadata.mIa3ASetting.contrast;
    exifAttributes.saturation = exifmetadata.mIa3ASetting.saturation;
    exifAttributes.sharpness  = exifmetadata.mIa3ASetting.sharpness;
    LOGI("EXIF: contrast=%d, saturation=%d, sharpness=%d (0:normal 1:low 2:high)",
            exifAttributes.contrast,
            exifAttributes.saturation,
            exifAttributes.sharpness);

    // set the exposure program mode
    AeMode aeMode = exifmetadata.mIa3ASetting.aeMode;
    switch (aeMode) {
    case CAM_AE_MODE_MANUAL:
        exifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_MANUAL;
        LOGI("EXIF: Exposure Program = Manual");
        exifAttributes.exposure_mode = EXIF_EXPOSURE_MANUAL;
        LOGI("EXIF: Exposure Mode = Manual");
        break;
    case CAM_AE_MODE_SHUTTER_PRIORITY:
        exifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_SHUTTER_PRIORITY;
        LOGI("EXIF: Exposure Program = Shutter Priority");
        break;
    case CAM_AE_MODE_APERTURE_PRIORITY:
        exifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_APERTURE_PRIORITY;
        LOGI("EXIF: Exposure Program = Aperture Priority");
        break;
    case CAM_AE_MODE_AUTO:
    default:
        exifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_NORMAL;
        LOGI("EXIF: Exposure Program = Normal");
        exifAttributes.exposure_mode = EXIF_EXPOSURE_AUTO;
        LOGI("EXIF: Exposure Mode = Auto");
        break;
    }

    // indicates the ISO speed of the camera
    if (exifmetadata.mIa3ASetting.isoSpeed > 0) {
        exifAttributes.iso_speed_rating = exifmetadata.mIa3ASetting.isoSpeed;
    } else {
        LOGW("EXIF: Could not query ISO speed!");
        exifAttributes.iso_speed_rating = DEFAULT_ISO_SPEED;
    }
    LOGI("EXIF: ISO=%d", exifAttributes.iso_speed_rating);

    // the metering mode.
    MeteringMode meteringMode  = exifmetadata.mIa3ASetting.meteringMode;
    switch (meteringMode) {
    case CAM_AE_METERING_MODE_AUTO:
        exifAttributes.metering_mode = EXIF_METERING_AVERAGE;
        LOGI("EXIF: Metering Mode = Average");
        break;
    case CAM_AE_METERING_MODE_SPOT:
        exifAttributes.metering_mode = EXIF_METERING_SPOT;
        LOGI("EXIF: Metering Mode = Spot");
        break;
    case CAM_AE_METERING_MODE_CENTER:
        exifAttributes.metering_mode = EXIF_METERING_CENTER;
        LOGI("EXIF: Metering Mode = Center");
        break;
    case CAM_AE_METERING_MODE_CUSTOMIZED:
    default:
        exifAttributes.metering_mode = EXIF_METERING_OTHER;
        LOGI("EXIF: Metering Mode = Other");
        break;
    }

    // white balance mode. 0: auto; 1: manual
    AwbMode awbMode = exifmetadata.mAwbMode;
    switch (awbMode) {
    case CAM_AWB_MODE_AUTO:
    case CAM_AWB_MODE_NOT_SET:
        exifAttributes.white_balance = EXIF_WB_AUTO;
        LOGI("EXIF: Whitebalance = Auto");
        break;
    default:
        exifAttributes.white_balance = EXIF_WB_MANUAL;
        LOGI("EXIF: Whitebalance = Manual");
        break;
    }

    // light source type. Refer to EXIF V2.2
    // TBD. Now light source is only set to UNKNOWN, when WB is auto mode.
    if (CAM_AWB_MODE_AUTO == awbMode) {
        exifAttributes.light_source = EXIF_LIGHT_SOURCE_UNKNOWN;
    }
    else {
        AwbMode lightSource = exifmetadata.mIa3ASetting.lightSource;
        switch (lightSource) {
        case CAM_AWB_MODE_MANUAL_INPUT:
        case CAM_AWB_MODE_AUTO:
        case CAM_AWB_MODE_NOT_SET:
            exifAttributes.light_source  = EXIF_LIGHT_SOURCE_OTHER_LIGHT_SOURCE;
            break;
        case CAM_AWB_MODE_SUNSET:
            exifAttributes.light_source  = EXIF_LIGHT_SOURCE_TUNGSTEN;
            break;
        case CAM_AWB_MODE_DAYLIGHT:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_FINE_WEATHER;
            break;
        case CAM_AWB_MODE_CLOUDY:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_CLOUDY_WEATHER;
            break;
        case CAM_AWB_MODE_SHADOW:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_SHADE;
            break;
        case CAM_AWB_MODE_TUNGSTEN:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_TUNGSTEN;
            break;
        case CAM_AWB_MODE_FLUORESCENT:
        case CAM_AWB_MODE_WARM_FLUORESCENT:
            //Since EXIF V2.2 not defined the warm white fluorescent, using fluorescent TAG for it
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_FLUORESCENT;
            break;
        case CAM_AWB_MODE_WARM_INCANDESCENT:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_TUNGSTEN;
            break;
        default:
            exifAttributes.light_source = EXIF_LIGHT_SOURCE_OTHER_LIGHT_SOURCE;
            break;
        }
    }

    int rotation = exifmetadata.mJpegSetting.orientation;
    exifAttributes.orientation = EXIF_ORIENTATION_UP;
    if (0 == rotation)
        exifAttributes.orientation = EXIF_ORIENTATION_UP;
    else if (90 == rotation)
        exifAttributes.orientation = EXIF_ORIENTATION_90;
    else if (180 == rotation)
        exifAttributes.orientation = EXIF_ORIENTATION_180;
    else if (270 == rotation)
        exifAttributes.orientation = EXIF_ORIENTATION_270;

    exifAttributes.zoom_ratio.num = exifmetadata.mZoomRatio;
    exifAttributes.zoom_ratio.den = 100;
    // the unit of subjectDistance is meter, focus distance from 3A is mm.
    exifAttributes.subject_distance.num = exifmetadata.mIa3ASetting.focusDistance;
    exifAttributes.subject_distance.den = 1000;
    LOGI("subject_distance is %d", exifAttributes.subject_distance.num);
}

/**
 * Called when the the camera static configuration is known.
 *
 * @arg width: width of the main JPEG picture.
 * @arg height: height of the main JPEG picture.
 */
void EXIFMaker::initialize(int width, int height)
{
    /* We clear the exif attributes, so we won't be using some old values
     * from a previous EXIF generation.
     */
    clear();

    // Initialize the exifAttributes with specific values
    // time information
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (timeinfo) {
        strftime((char *)exifAttributes.date_time, sizeof(exifAttributes.date_time), "%Y:%m:%d %H:%M:%S", timeinfo);
        // fields: tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst, tm_gmtoff, tm_zone
    } else {
        LOGW("nullptr timeinfo from localtime(), using defaults...");
        struct tm tmpTime = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "UTC"};
        strftime((char *)exifAttributes.date_time, sizeof(exifAttributes.date_time), "%Y:%m:%d %H:%M:%S", &tmpTime);
    }

    // set default subsec time to 1000
    const char subsecTime[] = "1000";
    MEMCPY_S(exifAttributes.subsec_time, sizeof(exifAttributes.subsec_time), subsecTime, sizeof(subsecTime));

    // conponents configuration.
    // Default = 4 5 6 0(if RGB uncompressed), 1 2 3 0(other cases)
    // 0 = does not exist; 1 = Y; 2 = Cb; 3 = Cr; 4 = R; 5 = G; 6 = B; other = reserved
    exifAttributes.components_configuration[0] = 1;
    exifAttributes.components_configuration[1] = 2;
    exifAttributes.components_configuration[2] = 3;
    exifAttributes.components_configuration[3] = 0;

    // set default values for fnumber and focal length
    // (see EXIFMaker::setDriverData() how to override these)
    exifAttributes.fnumber.num = EXIF_DEF_FNUMBER_NUM;
    exifAttributes.fnumber.den = EXIF_DEF_FNUMBER_DEN;
    exifAttributes.focal_length.num = EXIF_DEF_FOCAL_LEN_NUM;
    exifAttributes.focal_length.den = EXIF_DEF_FOCAL_LEN_DEN;

    // TODO: should ISO be omitted if the value cannot be trusted?
    exifAttributes.iso_speed_rating = DEFAULT_ISO_SPEED;

    // max aperture. the smallest F number of the lens. unit is APEX value.
    // TBD, should get from driver
    exifAttributes.max_aperture.num = exifAttributes.aperture.num;
    exifAttributes.max_aperture.den = exifAttributes.aperture.den;

    // subject distance,    0 means distance unknown; (~0) means infinity.
    exifAttributes.subject_distance.num = EXIF_DEF_SUBJECT_DISTANCE_UNKNOWN;
    exifAttributes.subject_distance.den = 1;

    // light source, 0 means light source unknown
    exifAttributes.light_source = 0;
    // TODO: for awb mode


    // gain control, 0 = none;
    // 1 = low gain up; 2 = high gain up; 3 = low gain down; 4 = high gain down
    exifAttributes.gain_control = 0;

    // contrast, 0 = normal; 1 = soft; 2 = hard; other = reserved
    exifAttributes.contrast = EXIF_CONTRAST_NORMAL;

    // saturation, 0 = normal; 1 = Low saturation; 2 = High saturation; other = reserved
    exifAttributes.saturation = EXIF_SATURATION_NORMAL;

    // sharpness, 0 = normal; 1 = soft; 2 = hard; other = reserved
    exifAttributes.sharpness = EXIF_SHARPNESS_NORMAL;

    // the picture's width and height
    exifAttributes.width = width;
    exifAttributes.height = height;

    exifAttributes.orientation = 1;

    exifAttributes.custom_rendered = EXIF_DEF_CUSTOM_RENDERED;

    // metering mode, 0 = normal; 1 = soft; 2 = hard; other = reserved
    exifAttributes.metering_mode = EXIF_METERING_UNKNOWN;
    //initializeLocation(params);
    initialized = true;
}

void EXIFMaker::initializeLocation(ExifMetaData& metadata)
{
    LOGI("@%s", __FUNCTION__);
    // GIS information
    bool gpsEnabled = false;
    double latitude = metadata.mGpsSetting.latitude;
    double longitude = metadata.mGpsSetting.longitude;
    double altitude = metadata.mGpsSetting.altitude;
    long timestamp = metadata.mGpsSetting.gpsTimeStamp;
    char* pprocmethod = metadata.mGpsSetting.gpsProcessingMethod;

    // check whether the GIS Information is valid
    if(!(latitude >= -EPSILON && latitude <= EPSILON) ||
       !(longitude >= -EPSILON && longitude <= EPSILON) ||
       !(altitude >= -EPSILON && altitude <= EPSILON) ||
       (timestamp != 0) || (STRLEN_S(pprocmethod) != 0))
        gpsEnabled = true;

    exifAttributes.enableGps = 0;
    LOGI("EXIF: gpsEnabled: %d", gpsEnabled);

    // the version is given as 2.2.0.0, it is mandatory when GPSInfo tag is present
    if(gpsEnabled) {
        const unsigned char gpsversion[4] = {0x02, 0x02, 0x00, 0x00};
        MEMCPY_S(exifAttributes.gps_version_id,
                 sizeof(exifAttributes.gps_version_id),
                 gpsversion,
                 sizeof(gpsversion));
    } else {
        return;
    }

    // latitude, for example, 39.904214 degrees, N
    if(latitude > 0) {
        MEMCPY_S(exifAttributes.gps_latitude_ref,
                sizeof(exifAttributes.gps_latitude_ref),
                "N",
                sizeof(exifAttributes.gps_latitude_ref));
    } else {
        MEMCPY_S(exifAttributes.gps_latitude_ref,
                 sizeof(exifAttributes.gps_latitude_ref),
                 "S",
                 sizeof(exifAttributes.gps_latitude_ref));
    }
    latitude = fabs(latitude);
    exifAttributes.gps_latitude[0].num = (uint32_t)latitude;
    exifAttributes.gps_latitude[0].den = 1;
    exifAttributes.gps_latitude[1].num = (uint32_t)((latitude - exifAttributes.gps_latitude[0].num) * 60);
    exifAttributes.gps_latitude[1].den = 1;
    exifAttributes.gps_latitude[2].num = (uint32_t)(((latitude - exifAttributes.gps_latitude[0].num) * 60 - exifAttributes.gps_latitude[1].num) * 60 * 100);
    exifAttributes.gps_latitude[2].den = 100;
    exifAttributes.enableGps |= EXIF_GPS_LATITUDE;
    LOGI("EXIF: latitude, ref:%s, dd:%d, mm:%d, ss:%d",
         exifAttributes.gps_latitude_ref, exifAttributes.gps_latitude[0].num,
         exifAttributes.gps_latitude[1].num, exifAttributes.gps_latitude[2].num);

    // longitude, for example, 116.407413 degrees, E
    if(longitude > 0)
        MEMCPY_S(exifAttributes.gps_longitude_ref,
                 sizeof(exifAttributes.gps_longitude_ref),
                 "E",
                 sizeof(exifAttributes.gps_longitude_ref));
    else
        MEMCPY_S(exifAttributes.gps_longitude_ref,
                 sizeof(exifAttributes.gps_longitude_ref),
                 "W",
                 sizeof(exifAttributes.gps_longitude_ref));

    longitude = fabs(longitude);
    exifAttributes.gps_longitude[0].num = (uint32_t)longitude;
    exifAttributes.gps_longitude[0].den = 1;
    exifAttributes.gps_longitude[1].num = (uint32_t)((longitude - exifAttributes.gps_longitude[0].num) * 60);
    exifAttributes.gps_longitude[1].den = 1;
    exifAttributes.gps_longitude[2].num = (uint32_t)(((longitude - exifAttributes.gps_longitude[0].num) * 60 - exifAttributes.gps_longitude[1].num) * 60 * 100);
    exifAttributes.gps_longitude[2].den = 100;
    exifAttributes.enableGps |= EXIF_GPS_LONGITUDE;
    LOGI("EXIF: longitude, ref:%s, dd:%d, mm:%d, ss:%d",
         exifAttributes.gps_longitude_ref, exifAttributes.gps_longitude[0].num,
         exifAttributes.gps_longitude[1].num, exifAttributes.gps_longitude[2].num);

    // altitude
    // altitude, sea level or above sea level, set it to 0; below sea level, set it to 1
    exifAttributes.gps_altitude_ref = ((altitude > 0) ? 0 : 1);
    altitude = fabs(altitude);
    exifAttributes.gps_altitude.num = (uint32_t)altitude;
    exifAttributes.gps_altitude.den = 1;
    exifAttributes.enableGps |= EXIF_GPS_ALTITUDE;
    LOGI("EXIF: altitude, ref:%d, height:%d", exifAttributes.gps_altitude_ref, exifAttributes.gps_altitude.num);

    // timestamp
    if (timestamp >= LONG_MAX || timestamp <= LONG_MIN)
    {
        timestamp = 0;
        LOGW("invalid timestamp was provided, defaulting to 0 (i.e. 1970)");
    }
    struct tm time;
    gmtime_r(&timestamp, &time);
    time.tm_year += 1900;
    time.tm_mon += 1;
    exifAttributes.gps_timestamp[0].num = time.tm_hour;
    exifAttributes.gps_timestamp[0].den = 1;
    exifAttributes.gps_timestamp[1].num = time.tm_min;
    exifAttributes.gps_timestamp[1].den = 1;
    exifAttributes.gps_timestamp[2].num = time.tm_sec;
    exifAttributes.gps_timestamp[2].den = 1;
    exifAttributes.enableGps |= EXIF_GPS_TIMESTAMP;
    std::ostringstream stringStream;
    stringStream << std::setfill('0')
            << std::setw(4) << time.tm_year << ":"
            << std::setw(2) << time.tm_mon << ":"
            << std::setw(2) << time.tm_mday;

    std::string tmpString = stringStream.str();
    size_t destLen = sizeof(exifAttributes.gps_datestamp);
    size_t srcLen = tmpString.length();
    size_t nullPos = std::min(destLen - 1, srcLen);

    const char *tmp = tmpString.c_str();
    MEMCPY_S((char *)exifAttributes.gps_datestamp, destLen, tmp, srcLen);
    exifAttributes.gps_datestamp[nullPos] = '\0';

    LOGI("EXIF: timestamp, year:%d,mon:%d,day:%d,hour:%d,min:%d,sec:%d",
            time.tm_year, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);

    // processing method
    MEMCPY_S(exifAttributes.gps_processing_method,
             sizeof(exifAttributes.gps_processing_method),
             metadata.mGpsSetting.gpsProcessingMethod,
             sizeof(metadata.mGpsSetting.gpsProcessingMethod));
    exifAttributes.gps_processing_method[sizeof(exifAttributes.gps_processing_method) - 1] = 0;

    exifAttributes.enableGps |= EXIF_GPS_PROCMETHOD;
    LOGI("EXIF: GPS processing method:%s", exifAttributes.gps_processing_method);
}

void EXIFMaker::setSensorAeConfig(const SensorAeConfig& aeConfig)
{
    LOGI("@%s", __FUNCTION__);

    if (aeConfig.expTime > 0) {
        if (aeConfig.aecApexTv != 0) {
            int expTime = (int) (pow(2.0, -aeConfig.aecApexTv * (1.52587890625e-005)) * 10000);
            exifAttributes.exposure_time.num = expTime;
            exifAttributes.exposure_time.den = 10000;
            // APEX shutter speed
            exifAttributes.shutter_speed.num = aeConfig.aecApexTv;
            exifAttributes.shutter_speed.den = 65536;
        } else {
            exifAttributes.exposure_time.num = aeConfig.expTime;
            exifAttributes.exposure_time.den = 1000000;

            uint32_t tv = APEX_EXPOSURE_TO_SHUTTER(
                    (double)exifAttributes.exposure_time.num /exifAttributes.exposure_time.den);
            exifAttributes.shutter_speed.num = tv * 65536;
            exifAttributes.shutter_speed.den = 65536;
        }
    } else {
        exifAttributes.exposure_time.num = 0;
        exifAttributes.exposure_time.den = 1;
        exifAttributes.shutter_speed.num = 0;
        exifAttributes.shutter_speed.den = 1;
    }

    // APEX aperture value
    if (aeConfig.aecApexAv >= 65536) {
        exifAttributes.aperture.num = aeConfig.aecApexAv;
        exifAttributes.aperture.den = 65536;
        if (aeConfig.fn_num != 0 && aeConfig.fn_denum != 0) {
            exifAttributes.fnumber.num = aeConfig.fn_num;
            exifAttributes.fnumber.den = aeConfig.fn_denum;
        }
    } else {
        double f_number = (double)exifAttributes.fnumber.num / (double)exifAttributes.fnumber.den;
        exifAttributes.aperture.num = (uint32_t)(10000 * APEX_FNUM_TO_APERTURE(f_number));
        exifAttributes.aperture.den = 10000;
    }
    // exposure bias. unit is APEX value. -99.99 to 99.99
    if (aeConfig.evBias > EV_LOWER_BOUND && aeConfig.evBias < EV_UPPER_BOUND) {
        exifAttributes.exposure_bias.num = (int)(aeConfig.evBias * 100);
        exifAttributes.exposure_bias.den = 100;
        LOGI("EXIF: Ev = %.2f", aeConfig.evBias);
    } else {
        LOGW("EXIF: Invalid Ev!");
        exifAttributes.exposure_bias.num = 0;
        exifAttributes.exposure_bias.den = 100;
    }

    LOGI("EXIF: shutter speed=%u/%u", exifAttributes.shutter_speed.num,
         exifAttributes.shutter_speed.den);
    LOGI("EXIF: exposure time=%u/%u", exifAttributes.exposure_time.num,
         exifAttributes.exposure_time.den);
    LOGI("EXIF: aperture=%u/%u", exifAttributes.aperture.num,
         exifAttributes.aperture.den);
}

/*
 * more secure attribute copy routine.
 * \param dst pointer to dst buffer
 * \param dstSize dst buffer size
 * \param src pointer to src character buffer
 * \param srcLength src buffer length in characters, not including null byte
 */
void EXIFMaker::copyAttribute(uint8_t *dst,
                              size_t dstSize,
                              const char *src,
                              size_t srcLength)
{
    size_t dstMaxLength = dstSize - 1; // leave space for null
    MEMCPY_S(dst, dstMaxLength, src, srcLength); // copy chars (not null)
    // add null termination
    size_t len = MIN(dstMaxLength, srcLength);
    dst[len] = '\0';
}

void EXIFMaker::clear()
{
    LOGI("@%s", __FUNCTION__);
    // Reset all the attributes
    CLEAR(exifAttributes);
    // Initialize the common values
    exifAttributes.enableThumb = false;
    copyAttribute(exifAttributes.image_description,
                  sizeof(exifAttributes.image_description),
                  EXIF_DEF_IMAGE_DESCRIPTION,
                  STRLEN_S(EXIF_DEF_IMAGE_DESCRIPTION));

    copyAttribute(exifAttributes.maker,
                  sizeof(exifAttributes.maker),
                  PlatformData::manufacturerName(),
                  STRLEN_S(PlatformData::manufacturerName()));

    copyAttribute(exifAttributes.model,
                  sizeof(exifAttributes.model),
                  PlatformData::productName(),
                  STRLEN_S(PlatformData::productName()));

    copyAttribute(exifAttributes.software,
                  sizeof(exifAttributes.software),
                  EXIF_DEF_SOFTWARE,
                  STRLEN_S(EXIF_DEF_SOFTWARE));

    copyAttribute(exifAttributes.exif_version,
                  sizeof(exifAttributes.exif_version),
                  EXIF_DEF_EXIF_VERSION,
                  STRLEN_S(EXIF_DEF_EXIF_VERSION));

    copyAttribute(exifAttributes.flashpix_version,
                  sizeof(exifAttributes.flashpix_version),
                  EXIF_DEF_FLASHPIXVERSION,
                  STRLEN_S(EXIF_DEF_FLASHPIXVERSION));

    // initially, set default flash
    exifAttributes.flash = EXIF_DEF_FLASH;

    // normally it is sRGB, 1 means sRGB. FFFF.H means uncalibrated
    exifAttributes.color_space = EXIF_DEF_COLOR_SPACE;

    // the number of pixels per ResolutionUnit in the w or h direction
    // 72 means the image resolution is unknown
    exifAttributes.x_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    exifAttributes.x_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    exifAttributes.y_resolution.num = exifAttributes.x_resolution.num;
    exifAttributes.y_resolution.den = exifAttributes.x_resolution.den;
    // resolution unit, 2 means inch
    exifAttributes.resolution_unit = EXIF_DEF_RESOLUTION_UNIT;
    // when thumbnail uses JPEG compression, this tag 103H's value is set to 6
    exifAttributes.compression_scheme = EXIF_DEF_COMPRESSION;

    // the TIFF default is 1 (centered)
    exifAttributes.ycbcr_positioning = EXIF_DEF_YCBCR_POSITIONING;

    // Clear the Rockchip 3A Makernote information
    exifAttributes.makerNoteData = nullptr;
    exifAttributes.makerNoteDataSize = 0;

    initialized = false;
}

void EXIFMaker::enableFlash(bool enable, int8_t aeMode, int8_t flashMode)
{
    // bit 0: flash fired; bit 1 to 2: flash return; bit 3 to 4: flash mode;
    // bit 5: flash function; bit 6: red-eye mode;
    // enable: flashfired, when aeMode set to ON or OFF, flashMode or flashfired been used.
    LOGI("@%s", __FUNCTION__);
    exifAttributes.flash = EXIF_DEF_FLASH;
    if (enable) {
        exifAttributes.flash = EXIF_FLASH_ON;
        exifAttributes.light_source = EXIF_LIGHT_SOURCE_FLASH;
    }
    switch (aeMode) {
        case ANDROID_CONTROL_AE_MODE_ON:
        case ANDROID_CONTROL_AE_MODE_OFF:
            if (enable || flashMode == ANDROID_FLASH_MODE_TORCH || flashMode == ANDROID_FLASH_MODE_SINGLE) {
                exifAttributes.flash |= EXIF_FLASH_FORCED_ON;
            } else {
                exifAttributes.flash |= EXIF_FLASH_FORCED_OFF;
            }
            break;
        case ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH:
            exifAttributes.flash |= EXIF_FLASH_AUTO;
            break;
        case ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH:
            exifAttributes.flash |= EXIF_FLASH_FORCED_ON;
            break;
        default:
            break;
    }
}

void EXIFMaker::setThumbnail(unsigned char *data, size_t size, int width, int height)
{
    LOGI("@%s: data = %p, size = %zu", __FUNCTION__, data, size);
    exifAttributes.enableThumb = true;
    exifAttributes.widthThumb = width;
    exifAttributes.heightThumb = height;
    if (encoder.setThumbData(data, size) != EXIF_SUCCESS) {
        LOGE("Error in setting EXIF thumbnail");
    }
}

bool EXIFMaker::isThumbnailSet() const {
    LOGI("@%s", __FUNCTION__);
    return encoder.isThumbDataSet();
}

size_t EXIFMaker::makeExif(unsigned char **data)
{
    LOGI("@%s", __FUNCTION__);
    if (*data == nullptr) {
        LOGE("nullptr pointer passed for EXIF. Cannot generate EXIF!");
        return 0;
    }
    if (encoder.makeExif(*data, &exifAttributes, &exifSize) == EXIF_SUCCESS) {
        LOGI("Generated EXIF (@%p) of size: %zu", *data, exifSize);
        return exifSize;
    }
    return 0;
}

size_t EXIFMaker::makeExifInPlace(unsigned char *bufferStartAddr,
                                  unsigned char *dqtAddress,
                                  size_t jpegSize,
                                  bool usePadding)
{
    LOGI("@%s", __FUNCTION__);
    if (bufferStartAddr == nullptr || dqtAddress == nullptr) {
        LOGE("nullptr pointer passed for EXIF. Cannot generate EXIF!");
        return 0;
    }
    if (encoder.makeExifInPlace(bufferStartAddr, dqtAddress, &exifAttributes,
                                jpegSize, usePadding, exifSize) == EXIF_SUCCESS) {
        LOGI("Generated EXIF (@%p) of size: %zu", bufferStartAddr, exifSize);
        return exifSize;
    }
    return 0;
}

void EXIFMaker::setMaker(const char *data)
{
    LOGI("@%s: data = %s", __FUNCTION__, data);
    size_t srcLen = std::char_traits<char>::length(data);
    size_t srcSize = srcLen + 1; // +1 = null termination
    size_t dstSize(sizeof(exifAttributes.maker));
    MEMCPY_S(exifAttributes.maker, dstSize, data, srcSize);
    exifAttributes.maker[dstSize - 1] = '\0';
}

void EXIFMaker::setModel(const char *data)
{
    LOGI("@%s: data = %s", __FUNCTION__, data);
    size_t srcLen = std::char_traits<char>::length(data);
    size_t srcSize = srcLen + 1; // +1 = null termination
    size_t dstSize(sizeof(exifAttributes.model));
    MEMCPY_S(exifAttributes.model, dstSize, data, srcSize);
    exifAttributes.model[dstSize - 1] = '\0';
}

void EXIFMaker::setSoftware(const char *data)
{
    LOGI("@%s: data = %s", __FUNCTION__, data);
    size_t srcLen = std::char_traits<char>::length(data);
    size_t srcSize = srcLen + 1; // +1 = null termination
    size_t dstSize(sizeof(exifAttributes.software));
    MEMCPY_S(exifAttributes.software, dstSize, data, srcSize);
    exifAttributes.software[dstSize - 1] = '\0';
}

void EXIFMaker::getExifAttrbutes(exif_attribute_t &exifAttrs)
{
    MEMCPY_S(&exifAttrs, sizeof(exif_attribute_t), &exifAttributes, sizeof(exif_attribute_t));
}

} NAMESPACE_DECLARATION_END
