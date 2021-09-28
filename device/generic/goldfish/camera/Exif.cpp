/*
 * Copyright (C) 2016 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera_Exif"
#include <log/log.h>
#include <cutils/properties.h>

#include <inttypes.h>
#include <math.h>
#include <stdint.h>

#include "Exif.h"
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-tag.h>

#include <string>
#include <vector>

#include "fake-pipeline2/Sensor.h"

// For GPS timestamping we want to ensure we use a 64-bit time_t, 32-bit
// platforms have time64_t but 64-bit platforms do not.
#if defined(__LP64__)
#include <time.h>
using Timestamp = time_t;
#define TIMESTAMP_TO_TM(timestamp, tm) gmtime_r(timestamp, tm)
#else
#include <time64.h>
using Timestamp = time64_t;
#define TIMESTAMP_TO_TM(timestamp, tm) gmtime64_r(timestamp, tm)
#endif

namespace android {

// A prefix that is used for tags with the "undefined" format to indicate that
// the contents are ASCII encoded. See the user comment section of the EXIF spec
// for more details http://www.exif.org/Exif2-2.PDF
static const unsigned char kAsciiPrefix[] = {
    0x41, 0x53, 0x43, 0x49, 0x49, 0x00, 0x00, 0x00 // "ASCII\0\0\0"
};

// Remove an existing EXIF entry from |exifData| if it exists. This is useful
// when replacing existing data, it's easier to just remove the data and
// re-allocate it than to adjust the amount of allocated data.
static void removeExistingEntry(ExifData* exifData, ExifIfd ifd, int tag) {
    ExifEntry* entry = exif_content_get_entry(exifData->ifd[ifd],
                                              static_cast<ExifTag>(tag));
    if (entry) {
        exif_content_remove_entry(exifData->ifd[ifd], entry);
    }
}

static ExifEntry* allocateEntry(int tag,
                                ExifFormat format,
                                unsigned int numComponents) {
    ExifMem* mem = exif_mem_new_default();
    ExifEntry* entry = exif_entry_new_mem(mem);

    unsigned int size = numComponents * exif_format_get_size(format);
    entry->data = reinterpret_cast<unsigned char*>(exif_mem_alloc(mem, size));
    entry->size = size;
    entry->tag = static_cast<ExifTag>(tag);
    entry->components = numComponents;
    entry->format = format;

    exif_mem_unref(mem);
    return entry;
}

// Create an entry and place it in |exifData|, the entry is initialized with an
// array of floats from |values|
template<size_t N>
static bool createEntry(ExifData* exifData,
                        ExifIfd ifd,
                        int tag,
                        const float (&values)[N],
                        float denominator = 1000.0) {
    removeExistingEntry(exifData, ifd, tag);
    ExifByteOrder byteOrder = exif_data_get_byte_order(exifData);
    ExifEntry* entry = allocateEntry(tag, EXIF_FORMAT_RATIONAL, N);
    exif_content_add_entry(exifData->ifd[ifd], entry);
    unsigned int rationalSize = exif_format_get_size(EXIF_FORMAT_RATIONAL);
    for (size_t i = 0; i < N; ++i) {
        ExifRational rational = {
            static_cast<uint32_t>(values[i] * denominator),
            static_cast<uint32_t>(denominator)
        };

        exif_set_rational(&entry->data[i * rationalSize], byteOrder, rational);
    }

    // Unref entry after changing owner to the ExifData struct
    exif_entry_unref(entry);
    return true;
}

// Create an entry with a single float |value| in it and place it in |exifData|
static bool createEntry(ExifData* exifData,
                        ExifIfd ifd,
                        int tag,
                        const float value,
                        float denominator = 1000.0) {
    float values[1] = { value };
    // Recycling functions is good for the environment
    return createEntry(exifData, ifd, tag, values, denominator);
}

// Create an entry and place it in |exifData|, the entry contains the raw data
// pointed to by |data| of length |size|.
static bool createEntry(ExifData* exifData,
                        ExifIfd ifd,
                        int tag,
                        const unsigned char* data,
                        size_t size,
                        ExifFormat format = EXIF_FORMAT_UNDEFINED) {
    removeExistingEntry(exifData, ifd, tag);
    ExifEntry* entry = allocateEntry(tag, format, size);
    memcpy(entry->data, data, size);
    exif_content_add_entry(exifData->ifd[ifd], entry);
    // Unref entry after changing owner to the ExifData struct
    exif_entry_unref(entry);
    return true;
}

// Create an entry and place it in |exifData|, the entry is initialized with
// the string provided in |value|
static bool createEntry(ExifData* exifData,
                        ExifIfd ifd,
                        int tag,
                        const char* value) {
    unsigned int length = strlen(value) + 1;
    const unsigned char* data = reinterpret_cast<const unsigned char*>(value);
    return createEntry(exifData, ifd, tag, data, length, EXIF_FORMAT_ASCII);
}

// Create an entry and place it in |exifData|, the entry is initialized with a
// single byte in |value|
//static bool createEntry(ExifData* exifData,
//                        ExifIfd ifd,
//                        int tag,
//                        uint8_t value) {
//    return createEntry(exifData, ifd, tag, &value, 1, EXIF_FORMAT_BYTE);
//}

// Create an entry and place it in |exifData|, the entry is default initialized
// by the exif library based on |tag|
static bool createEntry(ExifData* exifData,
                        ExifIfd ifd,
                        int tag) {
    removeExistingEntry(exifData, ifd, tag);
    ExifEntry* entry = exif_entry_new();
    exif_content_add_entry(exifData->ifd[ifd], entry);
    exif_entry_initialize(entry, static_cast<ExifTag>(tag));
    // Unref entry after changing owner to the ExifData struct
    exif_entry_unref(entry);
    return true;
}

// Create an entry with a single EXIF LONG (32-bit value) and place it in
// |exifData|.
static bool createEntry(ExifData* exifData,
                        ExifIfd ifd,
                        int tag,
                        int value) {
    removeExistingEntry(exifData, ifd, tag);
    ExifByteOrder byteOrder = exif_data_get_byte_order(exifData);
    ExifEntry* entry = allocateEntry(tag, EXIF_FORMAT_LONG, 1);
    exif_content_add_entry(exifData->ifd[ifd], entry);
    exif_set_long(entry->data, byteOrder, value);

    // Unref entry after changing owner to the ExifData struct
    exif_entry_unref(entry);
    return true;
}

// Create an entry with a single EXIF SHORT (16-bit value) and place it in
// |exifData|.
static bool createEntry(ExifData* exifData,
                        ExifIfd ifd,
                        int tag,
                        uint16_t value) {
    removeExistingEntry(exifData, ifd, tag);
    ExifByteOrder byteOrder = exif_data_get_byte_order(exifData);
    ExifEntry* entry = allocateEntry(tag, EXIF_FORMAT_SHORT, 1);
    exif_content_add_entry(exifData->ifd[ifd], entry);
    exif_set_short(entry->data, byteOrder, value);

    // Unref entry after changing owner to the ExifData struct
    exif_entry_unref(entry);
    return true;
}

static bool getCameraParam(const CameraParameters& parameters,
                           const char* parameterKey,
                           const char** outValue) {
    const char* value = parameters.get(parameterKey);
    if (value) {
        *outValue = value;
        return true;
    }
    return false;
}

static bool getCameraParam(const CameraParameters& parameters,
                           const char* parameterKey,
                           float* outValue) {
    const char* value = parameters.get(parameterKey);
    if (value) {
        *outValue = parameters.getFloat(parameterKey);
        return true;
    }
    return false;
}

static bool getCameraParam(const CameraParameters& parameters,
                           const char* parameterKey,
                           int64_t* outValue) {
    const char* value = parameters.get(parameterKey);
    if (value) {
        char dummy = 0;
        // Attempt to scan an extra character and then make sure it was not
        // scanned by checking that the return value indicates only one item.
        // This way we fail on any trailing characters
        if (sscanf(value, "%" SCNd64 "%c", outValue, &dummy) == 1) {
            return true;
        }
    }
    return false;
}

// Convert a GPS coordinate represented as a decimal degree value to sexagesimal
// GPS coordinates comprised of <degrees> <minutes>' <seconds>"
static void convertGpsCoordinate(float degrees, float (*result)[3]) {
    float absDegrees = fabs(degrees);
    // First value is degrees without any decimal digits
    (*result)[0] = floor(absDegrees);

    // Subtract degrees so we only have the fraction left, then multiply by
    // 60 to get the minutes
    float minutes = (absDegrees - (*result)[0]) * 60.0f;
    (*result)[1] = floor(minutes);

    // Same thing for seconds but here we store seconds with the fraction
    float seconds = (minutes - (*result)[1]) * 60.0f;
    (*result)[2] = seconds;
}

// Convert a UNIX epoch timestamp to a timestamp comprised of three floats for
// hour, minute and second, and a date part that is represented as a string.
static bool convertTimestampToTimeAndDate(int64_t timestamp,
                                          float (*timeValues)[3],
                                          std::string* date) {
    Timestamp time = timestamp;
    struct tm utcTime;
    if (TIMESTAMP_TO_TM(&time, &utcTime) == nullptr) {
        ALOGE("Could not decompose timestamp into components");
        return false;
    }
    (*timeValues)[0] = utcTime.tm_hour;
    (*timeValues)[1] = utcTime.tm_min;
    (*timeValues)[2] = utcTime.tm_sec;

    char buffer[64] = {};
    if (strftime(buffer, sizeof(buffer), "%Y:%m:%d", &utcTime) == 0) {
        ALOGE("Could not construct date string from timestamp");
        return false;
    }
    *date = buffer;
    return true;
}

// Convert and store key values in CameraMetadata
static void convertToMetadata(const CameraParameters& src, CameraMetadata& dst) {
    int64_t longValue;
    float floatValue, floatGps[3];
    const char* stringValue;

    // Orientation
    if (getCameraParam(src,
                       CameraParameters::KEY_ROTATION,
                       &longValue)) {
        int32_t degrees = (int32_t)longValue;
        dst.update(ANDROID_JPEG_ORIENTATION, &degrees, 1);
    }
    // Focal length
    if (getCameraParam(src,
                       CameraParameters::KEY_FOCAL_LENGTH,
                       &floatValue)) {
        dst.update(ANDROID_LENS_FOCAL_LENGTH, &floatValue, 1);
    }
    // GPS latitude longitude and altitude
    if (getCameraParam(src,
                       CameraParameters::KEY_GPS_LATITUDE,
                       &floatGps[0]) &&
        getCameraParam(src,
                       CameraParameters::KEY_GPS_LONGITUDE,
                       &floatGps[1]) &&
        getCameraParam(src,
                       CameraParameters::KEY_GPS_ALTITUDE,
                       &floatGps[2])) {
        double gps[3];
        gps[0] = (double)floatGps[0];
        gps[1] = (double)floatGps[1];
        gps[2] = (double)floatGps[2];
        dst.update(ANDROID_JPEG_GPS_COORDINATES, gps, 3);
    }
    // GPS timestamp and datestamp
    if (getCameraParam(src,
                       CameraParameters::KEY_GPS_TIMESTAMP,
                       &longValue)) {
        dst.update(ANDROID_JPEG_GPS_TIMESTAMP, &longValue, 1);
    }
    // GPS processing method
    if (getCameraParam(src,
                       CameraParameters::KEY_GPS_PROCESSING_METHOD,
                       &stringValue)) {
        dst.update(ANDROID_JPEG_GPS_PROCESSING_METHOD, (unsigned char*)stringValue,
                   strlen(stringValue));
    }
}

// Create Exif data common for both HAL1 and HAL3
static ExifData* createExifDataCommon(const CameraMetadata& params, int width, int height) {
    ExifData* exifData = exif_data_new();

    exif_data_set_option(exifData, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);
    exif_data_set_data_type(exifData, EXIF_DATA_TYPE_COMPRESSED);
    exif_data_set_byte_order(exifData, EXIF_BYTE_ORDER_INTEL);

    // Create mandatory exif fields and set their default values
    exif_data_fix(exifData);

    float triplet[3];
    const char* stringValue;
    int32_t degrees;
    float focalLength;

    // Datetime, creating and initializing a datetime tag will automatically
    // set the current date and time in the tag so just do that.
    createEntry(exifData, EXIF_IFD_0, EXIF_TAG_DATE_TIME);

    // Make and model
    std::vector<char> prop(PROPERTY_VALUE_MAX);
    property_get("ro.product.manufacturer", &prop[0], "");
    createEntry(exifData, EXIF_IFD_0, EXIF_TAG_MAKE, &prop[0]);
    property_get("ro.product.model", &prop[0], "");
    createEntry(exifData, EXIF_IFD_0, EXIF_TAG_MODEL, &prop[0]);

    // Width and height
    if (width > 0 && height > 0) {
        createEntry(exifData, EXIF_IFD_EXIF,
                    EXIF_TAG_PIXEL_X_DIMENSION, width);
        createEntry(exifData, EXIF_IFD_EXIF,
                    EXIF_TAG_PIXEL_Y_DIMENSION, height);
    }

    camera_metadata_ro_entry_t entry;
    entry = params.find(ANDROID_LENS_FOCAL_LENGTH);
    focalLength = (entry.count > 0) ? entry.data.f[0] : 5.0f;
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH, focalLength);
    entry = params.find(ANDROID_JPEG_ORIENTATION);
    degrees = (entry.count > 0) ? entry.data.i32[0] : 0;
    ALOGV("degrees %d focalLength %f", degrees, focalLength);
    enum {
        EXIF_ROTATE_CAMERA_CW0 = 1,
        EXIF_ROTATE_CAMERA_CW90 = 6,
        EXIF_ROTATE_CAMERA_CW180 = 3,
        EXIF_ROTATE_CAMERA_CW270 = 8,
    };
    uint16_t exifOrien = 1;
    switch (degrees) {
        case 0:
            exifOrien = EXIF_ROTATE_CAMERA_CW0;
            break;
        case 90:
            exifOrien = EXIF_ROTATE_CAMERA_CW90;
            break;
        case 180:
            exifOrien = EXIF_ROTATE_CAMERA_CW180;
            break;
        case 270:
            exifOrien = EXIF_ROTATE_CAMERA_CW270;
            break;
    }
    createEntry(exifData, EXIF_IFD_0, EXIF_TAG_ORIENTATION, exifOrien);

    // GPS information
    entry = params.find(ANDROID_JPEG_GPS_COORDINATES);
    if (entry.count > 0) {
        ALOGV("Latitude %f Longitude %f Altitude %f", entry.data.d[0], entry.data.d[1], entry.data.d[2]);
        convertGpsCoordinate(entry.data.d[0], &triplet);
        createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_LATITUDE, triplet);

        const char* ref = entry.data.d[0] < 0.0f ? "S" : "N";
        createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_LATITUDE_REF, ref);

        // GPS longitude and reference, reference indicates sign, store unsigned
        convertGpsCoordinate(entry.data.d[1], &triplet);
        createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_LONGITUDE, triplet);

        ref = entry.data.d[1] < 0.0f ? "W" : "E";
        createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_LONGITUDE_REF, ref);

        createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_ALTITUDE,
                    static_cast<float>(fabs(entry.data.d[2])));
        int ref1;
        // 1 indicated below sea level, 0 indicates above sea level
        ref1 = entry.data.d[2] < 0.0f ? 1 : 0;
        createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_ALTITUDE_REF, ref1);
    }

    int64_t timestamp = 0;
    entry = params.find(ANDROID_JPEG_GPS_TIMESTAMP);
    if (entry.count > 0) {
        timestamp = entry.data.i64[0];
        std::string date;
        if (convertTimestampToTimeAndDate(timestamp, &triplet, &date)) {
            createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_TIME_STAMP,
                        triplet, 1.0f);
            createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_DATE_STAMP,
                        date.c_str());
        }
    }

    // GPS processing method
    entry = params.find(ANDROID_JPEG_GPS_PROCESSING_METHOD);
    if (entry.count > 0) {
        stringValue = (const char*)entry.data.u8;
        ALOGV("ANDROID_JPEG_GPS_PROCESSING_METHOD(len=%d) %s", entry.count, stringValue);
        std::vector<unsigned char> data;
        // Because this is a tag with an undefined format it has to be prefixed
        // with the encoding type. Insert an ASCII prefix first, then the
        // actual string. Undefined tags do not have to be null terminated.
        data.insert(data.end(),
                    std::begin(kAsciiPrefix),
                    std::end(kAsciiPrefix));
        data.insert(data.end(), stringValue, stringValue + entry.count);
        createEntry(exifData, EXIF_IFD_GPS, EXIF_TAG_GPS_PROCESSING_METHOD,
                    &data[0], data.size());
    }
    return exifData;
}

ExifData* createExifData(const CameraMetadata& params, int width, int height) {
    ExifData* exifData = createExifDataCommon(params, width, height);
    // Exposure Time
    camera_metadata_ro_entry entry;
    entry= params.find(ANDROID_SENSOR_EXPOSURE_TIME);
    int64_t exposureTimesNs =
        (entry.count > 0) ? entry.data.i64[0] : Sensor::kExposureTimeRange[0];
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_TIME,
                exposureTimesNs/1000000000.0f, 1000000000);
    // Aperture
    entry = params.find(ANDROID_LENS_APERTURE);
    float aperture = (entry.count > 0) ? entry.data.f[0] : 2.8;
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_FNUMBER, aperture);
    // Flash, 0 for off
    entry = params.find(ANDROID_FLASH_MODE);
    uint16_t flash = (entry.count > 0) ? entry.data.i32[0] : 0;
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_FLASH, flash);
    // White balance, 0 for auto, 1 for manual.
    entry = params.find(ANDROID_CONTROL_AWB_MODE);
    uint16_t awb = 1;
    if (entry.count > 0 && entry.data.i32[0] == ANDROID_CONTROL_AWB_MODE_AUTO) {
        awb = 0;
    }
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_WHITE_BALANCE, awb);
    // ISO
    entry = params.find(ANDROID_SENSOR_SENSITIVITY);
    int isoSpeedRating = (entry.count > 0) ?
        entry.data.i32[0] : Sensor::kSensitivityRange[0];
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_ISO_SPEED_RATINGS,
                (uint16_t)isoSpeedRating);
    // Date and time
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_DIGITIZED);
    // Sub second time
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME, "0");
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME_ORIGINAL, "0");
    createEntry(exifData, EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME_DIGITIZED, "0");

    return exifData;
}

ExifData* createExifData(const CameraParameters& params) {
    int width = -1, height = -1;
    CameraMetadata cameraMetadata;
    convertToMetadata(params, cameraMetadata);
    params.getPictureSize(&width, &height);
    ExifData* exifData =  createExifDataCommon(cameraMetadata, width, height);
    return exifData;
}

void freeExifData(ExifData* exifData) {
    exif_data_free(exifData);
}

}  // namespace android

