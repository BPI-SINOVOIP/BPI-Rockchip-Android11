/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "CameraExifUtils"
#define ATRACE_TAG ATRACE_TAG_CAMERA
//#define LOG_NDEBUG 0

#include "ExifUtils.h"

#include <inttypes.h>
#include <log/log.h>
#include <math.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "../EmulatedSensor.h"

extern "C" {
#include <libexif/exif-data.h>
}

namespace std {

template <>
struct default_delete<ExifEntry> {
  inline void operator()(ExifEntry* entry) const {
    exif_entry_unref(entry);
  }
};

}  // namespace std

namespace android {

class ExifUtilsImpl : public ExifUtils {
 public:
  ExifUtilsImpl(SensorCharacteristics sensor_chars);

  virtual ~ExifUtilsImpl();

  // Initialize() can be called multiple times. The setting of Exif tags will be
  // cleared.
  virtual bool Initialize();

  // set all known fields from a metadata structure
  virtual bool SetFromMetadata(const HalCameraMetadata& metadata,
                               size_t image_width, size_t image_height);

  // sets the len aperture.
  // Returns false if memory allocation fails.
  virtual bool SetAperture(float aperture);

  // sets the color space.
  // Returns false if memory allocation fails.
  virtual bool SetColorSpace(uint16_t color_space);

  // sets the date and time of image last modified. It takes local time. The
  // name of the tag is DateTime in IFD0.
  // Returns false if memory allocation fails.
  virtual bool SetDateTime(const struct tm& t);

  // sets the digital zoom ratio. If the numerator is 0, it means digital zoom
  // was not used.
  // Returns false if memory allocation fails.
  virtual bool SetDigitalZoomRatio(uint32_t crop_width, uint32_t crop_height,
                                   uint32_t sensor_width,
                                   uint32_t sensor_height);

  // Sets the exposure bias.
  // Returns false if memory allocation fails.
  virtual bool SetExposureBias(int32_t ev, uint32_t ev_step_numerator,
                               uint32_t ev_step_denominator);

  // sets the exposure mode set when the image was shot.
  // Returns false if memory allocation fails.
  virtual bool SetExposureMode(uint8_t exposure_mode);

  // sets the exposure time, given in seconds.
  // Returns false if memory allocation fails.
  virtual bool SetExposureTime(float exposure_time);

  // sets the status of flash.
  // Returns false if memory allocation fails.
  virtual bool SetFlash(uint8_t flash_available, uint8_t flash_state,
                        uint8_t ae_mode);

  // sets the F number.
  // Returns false if memory allocation fails.
  virtual bool SetFNumber(float f_number);

  // sets the focal length of lens used to take the image in millimeters.
  // Returns false if memory allocation fails.
  virtual bool SetFocalLength(float focal_length);

  // sets the focal length of lens for 35mm film used to take the image in
  // millimeters. Returns false if memory allocation fails.
  virtual bool SetFocalLengthIn35mmFilm(float focal_length, float sensor_size_x,
                                        float sensor_size_y);

  // sets make & model
  virtual bool SetMake(const std::string& make);
  virtual bool SetModel(const std::string& model);

  // sets the altitude in meters.
  // Returns false if memory allocation fails.
  virtual bool SetGpsAltitude(double altitude);

  // sets the latitude with degrees minutes seconds format.
  // Returns false if memory allocation fails.
  virtual bool SetGpsLatitude(double latitude);

  // sets the longitude with degrees minutes seconds format.
  // Returns false if memory allocation fails.
  virtual bool SetGpsLongitude(double longitude);

  // sets GPS processing method.
  // Returns false if memory allocation fails.
  virtual bool SetGpsProcessingMethod(const std::string& method);

  // sets GPS date stamp and time stamp (atomic clock). It takes UTC time.
  // Returns false if memory allocation fails.
  virtual bool SetGpsTimestamp(const struct tm& t);

  // sets the length (number of rows) of main image.
  // Returns false if memory allocation fails.
  virtual bool SetImageHeight(uint32_t length);

  // sets the width (number of columes) of main image.
  // Returns false if memory allocation fails.
  virtual bool SetImageWidth(uint32_t width);

  // sets the ISO speed.
  // Returns false if memory allocation fails.
  virtual bool SetIsoSpeedRating(uint16_t iso_speed_ratings);

  // sets the smallest F number of the lens.
  // Returns false if memory allocation fails.
  virtual bool SetMaxAperture(float aperture);

  // sets image orientation.
  // Returns false if memory allocation fails.
  virtual bool SetOrientation(uint16_t degrees);

  // sets image orientation.
  // Returns false if memory allocation fails.
  virtual bool SetOrientationValue(ExifOrientation orientation_value);

  // sets the shutter speed.
  // Returns false if memory allocation fails.
  virtual bool SetShutterSpeed(float exposure_time);

  // sets the distance to the subject, given in meters.
  // Returns false if memory allocation fails.
  virtual bool SetSubjectDistance(float diopters);

  // sets the fractions of seconds for the <DateTime> tag.
  // Returns false if memory allocation fails.
  virtual bool SetSubsecTime(const std::string& subsec_time);

  // sets the white balance mode set when the image was shot.
  // Returns false if memory allocation fails.
  virtual bool SetWhiteBalance(uint8_t white_balance);

  // Generates APP1 segment.
  // Returns false if generating APP1 segment fails.
  virtual bool GenerateApp1(unsigned char* thumbnail_buffer, uint32_t size);

  // Gets buffer of APP1 segment. This method must be called only after calling
  // GenerateAPP1().
  virtual const uint8_t* GetApp1Buffer();

  // Gets length of APP1 segment. This method must be called only after calling
  // GenerateAPP1().
  virtual unsigned int GetApp1Length();

 protected:
  // sets the version of this standard supported.
  // Returns false if memory allocation fails.
  virtual bool SetExifVersion(const std::string& exif_version);

  // Resets the pointers and memories.
  virtual void Reset();

  // Adds a variable length tag to |exif_data_|. It will remove the original one
  // if the tag exists.
  // Returns the entry of the tag. The reference count of returned ExifEntry is
  // two.
  virtual std::unique_ptr<ExifEntry> AddVariableLengthEntry(ExifIfd ifd,
                                                            ExifTag tag,
                                                            ExifFormat format,
                                                            uint64_t components,
                                                            unsigned int size);

  // Adds a entry of |tag| in |exif_data_|. It won't remove the original one if
  // the tag exists.
  // Returns the entry of the tag. It adds one reference count to returned
  // ExifEntry.
  virtual std::unique_ptr<ExifEntry> AddEntry(ExifIfd ifd, ExifTag tag);

  // Helpe functions to add exif data with different types.
  virtual bool SetShort(ExifIfd ifd, ExifTag tag, uint16_t value,
                        const std::string& msg);

  virtual bool SetLong(ExifIfd ifd, ExifTag tag, uint32_t value,
                       const std::string& msg);

  virtual bool SetRational(ExifIfd ifd, ExifTag tag, uint32_t numerator,
                           uint32_t denominator, const std::string& msg);

  virtual bool SetSRational(ExifIfd ifd, ExifTag tag, int32_t numerator,
                            int32_t denominator, const std::string& msg);

  virtual bool SetString(ExifIfd ifd, ExifTag tag, ExifFormat format,
                         const std::string& buffer, const std::string& msg);

  float ConvertToApex(float val) {
    return 2.0f * log2f(val);
  }

  // Destroys the buffer of APP1 segment if exists.
  virtual void DestroyApp1();

  // The Exif data (APP1). Owned by this class.
  ExifData* exif_data_;
  // The raw data of APP1 segment. It's allocated by ExifMem in |exif_data_| but
  // owned by this class.
  uint8_t* app1_buffer_;
  // The length of |app1_buffer_|.
  unsigned int app1_length_;

  // How precise the float-to-rational conversion for EXIF tags would be.
  const static int kRationalPrecision = 10000;

  SensorCharacteristics sensor_chars_;
};

#define SET_SHORT(ifd, tag, value)                              \
  do {                                                          \
    if (SetShort(ifd, tag, value, #tag) == false) return false; \
  } while (0);

#define SET_LONG(ifd, tag, value)                              \
  do {                                                         \
    if (SetLong(ifd, tag, value, #tag) == false) return false; \
  } while (0);

#define SET_RATIONAL(ifd, tag, numerator, denominator)                \
  do {                                                                \
    if (SetRational(ifd, tag, numerator, denominator, #tag) == false) \
      return false;                                                   \
  } while (0);

#define SET_SRATIONAL(ifd, tag, numerator, denominator)                \
  do {                                                                 \
    if (SetSRational(ifd, tag, numerator, denominator, #tag) == false) \
      return false;                                                    \
  } while (0);

#define SET_STRING(ifd, tag, format, buffer)                              \
  do {                                                                    \
    if (SetString(ifd, tag, format, buffer, #tag) == false) return false; \
  } while (0);

// This comes from the Exif Version 2.2 standard table 6.
const char gExifAsciiPrefix[] = {0x41, 0x53, 0x43, 0x49, 0x49, 0x0, 0x0, 0x0};

static void SetLatitudeOrLongitudeData(unsigned char* data, double num) {
  // Take the integer part of |num|.
  ExifLong degrees = static_cast<ExifLong>(num);
  ExifLong minutes = static_cast<ExifLong>(60 * (num - degrees));
  ExifLong microseconds =
      static_cast<ExifLong>(3600000000u * (num - degrees - minutes / 60.0));
  exif_set_rational(data, EXIF_BYTE_ORDER_INTEL, {degrees, 1});
  exif_set_rational(data + sizeof(ExifRational), EXIF_BYTE_ORDER_INTEL,
                    {minutes, 1});
  exif_set_rational(data + 2 * sizeof(ExifRational), EXIF_BYTE_ORDER_INTEL,
                    {microseconds, 1000000});
}

ExifUtils* ExifUtils::Create(SensorCharacteristics sensor_chars) {
  return new ExifUtilsImpl(sensor_chars);
}

ExifUtils::~ExifUtils() {
}

ExifUtilsImpl::ExifUtilsImpl(SensorCharacteristics sensor_chars)
    : exif_data_(nullptr),
      app1_buffer_(nullptr),
      app1_length_(0),
      sensor_chars_(sensor_chars) {
}

ExifUtilsImpl::~ExifUtilsImpl() {
  Reset();
}

bool ExifUtilsImpl::Initialize() {
  Reset();
  exif_data_ = exif_data_new();
  if (exif_data_ == nullptr) {
    ALOGE("%s: allocate memory for exif_data_ failed", __FUNCTION__);
    return false;
  }
  // set the image options.
  exif_data_set_option(exif_data_, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);
  exif_data_set_data_type(exif_data_, EXIF_DATA_TYPE_COMPRESSED);
  exif_data_set_byte_order(exif_data_, EXIF_BYTE_ORDER_INTEL);

  // set exif version to 2.2.
  if (!SetExifVersion("0220")) {
    return false;
  }

  return true;
}

bool ExifUtilsImpl::SetAperture(float aperture) {
  float apex_value = ConvertToApex(aperture);
  SET_RATIONAL(
      EXIF_IFD_EXIF, EXIF_TAG_APERTURE_VALUE,
      static_cast<uint32_t>(std::round(apex_value * kRationalPrecision)),
      kRationalPrecision);
  return true;
}

bool ExifUtilsImpl::SetColorSpace(uint16_t color_space) {
  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_COLOR_SPACE, color_space);
  return true;
}

bool ExifUtilsImpl::SetDateTime(const struct tm& t) {
  // The length is 20 bytes including NULL for termination in Exif standard.
  char str[20];
  int result = snprintf(str, sizeof(str), "%04i:%02i:%02i %02i:%02i:%02i",
                        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
                        t.tm_min, t.tm_sec);
  if (result != sizeof(str) - 1) {
    ALOGW("%s: Input time is invalid", __FUNCTION__);
    return false;
  }
  std::string buffer(str);
  SET_STRING(EXIF_IFD_0, EXIF_TAG_DATE_TIME, EXIF_FORMAT_ASCII, buffer);
  SET_STRING(EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_ORIGINAL, EXIF_FORMAT_ASCII,
             buffer);
  SET_STRING(EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_DIGITIZED, EXIF_FORMAT_ASCII,
             buffer);
  return true;
}

bool ExifUtilsImpl::SetDigitalZoomRatio(uint32_t crop_width,
                                        uint32_t crop_height,
                                        uint32_t sensor_width,
                                        uint32_t sensor_height) {
  float zoom_ratio_x = (crop_width == 0) ? 1.0 : 1.0 * sensor_width / crop_width;
  float zoom_ratio_y =
      (crop_height == 0) ? 1.0 : 1.0 * sensor_height / crop_height;
  float zoom_ratio = std::max(zoom_ratio_x, zoom_ratio_y);
  const static float no_zoom_threshold = 1.02f;

  if (zoom_ratio <= no_zoom_threshold) {
    SET_RATIONAL(EXIF_IFD_EXIF, EXIF_TAG_DIGITAL_ZOOM_RATIO, 0, 1);
  } else {
    SET_RATIONAL(
        EXIF_IFD_EXIF, EXIF_TAG_DIGITAL_ZOOM_RATIO,
        static_cast<uint32_t>(std::round(zoom_ratio * kRationalPrecision)),
        kRationalPrecision);
  }
  return true;
}

bool ExifUtilsImpl::SetExposureMode(uint8_t exposure_mode) {
  uint16_t mode = (exposure_mode == ANDROID_CONTROL_AE_MODE_OFF) ? 1 : 0;
  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_MODE, mode);
  return true;
}

bool ExifUtilsImpl::SetExposureTime(float exposure_time) {
  SET_RATIONAL(
      EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_TIME,
      static_cast<uint32_t>(std::round(exposure_time * kRationalPrecision)),
      kRationalPrecision);
  return true;
}

bool ExifUtilsImpl::SetFlash(uint8_t flash_available, uint8_t flash_state,
                             uint8_t ae_mode) {
  // EXIF_TAG_FLASH bits layout per EXIF standard:
  // Bit 0:    0 - did not fire
  //           1 - fired
  // Bit 1-2:  status of return light
  // Bit 3-4:  0 - unknown
  //           1 - compulsory flash firing
  //           2 - compulsory flash suppression
  //           3 - auto mode
  // Bit 5:    0 - flash function present
  //           1 - no flash function
  // Bit 6:    0 - no red-eye reduction mode or unknown
  //           1 - red-eye reduction supported
  uint16_t flash = 0x20;

  if (flash_available == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
    flash = 0x00;

    if (flash_state == ANDROID_FLASH_STATE_FIRED) {
      flash |= 0x1;
    }
    if (ae_mode == ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE) {
      flash |= 0x40;
    }

    uint16_t flash_mode = 0;
    switch (ae_mode) {
      case ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH:
      case ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE:
        flash_mode = 3;  // AUTO
        break;
      case ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH:
      case ANDROID_CONTROL_AE_MODE_ON_EXTERNAL_FLASH:
        flash_mode = 1;  // ON
        break;
      case ANDROID_CONTROL_AE_MODE_OFF:
      case ANDROID_CONTROL_AE_MODE_ON:
        flash_mode = 2;  // OFF
        break;
      default:
        flash_mode = 0;  // UNKNOWN
        break;
    }
    flash |= (flash_mode << 3);
  }
  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_FLASH, flash);
  return true;
}

bool ExifUtilsImpl::SetFNumber(float f_number) {
  SET_RATIONAL(EXIF_IFD_EXIF, EXIF_TAG_FNUMBER,
               static_cast<uint32_t>(std::round(f_number * kRationalPrecision)),
               kRationalPrecision);
  return true;
}

bool ExifUtilsImpl::SetFocalLength(float focal_length) {
  uint32_t numerator =
      static_cast<uint32_t>(std::round(focal_length * kRationalPrecision));
  SET_RATIONAL(EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH, numerator,
               kRationalPrecision);
  return true;
}

bool ExifUtilsImpl::SetFocalLengthIn35mmFilm(float focal_length,
                                             float sensor_size_x,
                                             float sensor_size_y) {
  static const float film_diagonal = 43.27;  // diagonal of 35mm film
  static const float min_sensor_diagonal = 0.01;
  float sensor_diagonal =
      std::sqrt(sensor_size_x * sensor_size_x + sensor_size_y * sensor_size_y);
  sensor_diagonal = std::max(sensor_diagonal, min_sensor_diagonal);
  float focal_length35mm_film =
      std::round(focal_length * film_diagonal / sensor_diagonal);
  focal_length35mm_film = std::min(1.0f * 65535, focal_length35mm_film);

  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM,
            static_cast<uint16_t>(focal_length35mm_film));
  return true;
}

bool ExifUtilsImpl::SetGpsAltitude(double altitude) {
  ExifTag ref_tag = static_cast<ExifTag>(EXIF_TAG_GPS_ALTITUDE_REF);
  std::unique_ptr<ExifEntry> ref_entry =
      AddVariableLengthEntry(EXIF_IFD_GPS, ref_tag, EXIF_FORMAT_BYTE, 1, 1);
  if (!ref_entry) {
    ALOGE("%s: Adding GPSAltitudeRef exif entry failed", __FUNCTION__);
    return false;
  }
  if (altitude >= 0) {
    *ref_entry->data = 0;
  } else {
    *ref_entry->data = 1;
    altitude *= -1;
  }

  ExifTag tag = static_cast<ExifTag>(EXIF_TAG_GPS_ALTITUDE);
  std::unique_ptr<ExifEntry> entry = AddVariableLengthEntry(
      EXIF_IFD_GPS, tag, EXIF_FORMAT_RATIONAL, 1, sizeof(ExifRational));
  if (!entry) {
    exif_content_remove_entry(exif_data_->ifd[EXIF_IFD_GPS], ref_entry.get());
    ALOGE("%s: Adding GPSAltitude exif entry failed", __FUNCTION__);
    return false;
  }
  exif_set_rational(entry->data, EXIF_BYTE_ORDER_INTEL,
                    {static_cast<ExifLong>(altitude * 1000), 1000});

  return true;
}

bool ExifUtilsImpl::SetGpsLatitude(double latitude) {
  const ExifTag ref_tag = static_cast<ExifTag>(EXIF_TAG_GPS_LATITUDE_REF);
  std::unique_ptr<ExifEntry> ref_entry =
      AddVariableLengthEntry(EXIF_IFD_GPS, ref_tag, EXIF_FORMAT_ASCII, 2, 2);
  if (!ref_entry) {
    ALOGE("%s: Adding GPSLatitudeRef exif entry failed", __FUNCTION__);
    return false;
  }
  if (latitude >= 0) {
    memcpy(ref_entry->data, "N", sizeof("N"));
  } else {
    memcpy(ref_entry->data, "S", sizeof("S"));
    latitude *= -1;
  }

  const ExifTag tag = static_cast<ExifTag>(EXIF_TAG_GPS_LATITUDE);
  std::unique_ptr<ExifEntry> entry = AddVariableLengthEntry(
      EXIF_IFD_GPS, tag, EXIF_FORMAT_RATIONAL, 3, 3 * sizeof(ExifRational));
  if (!entry) {
    exif_content_remove_entry(exif_data_->ifd[EXIF_IFD_GPS], ref_entry.get());
    ALOGE("%s: Adding GPSLatitude exif entry failed", __FUNCTION__);
    return false;
  }
  SetLatitudeOrLongitudeData(entry->data, latitude);

  return true;
}

bool ExifUtilsImpl::SetGpsLongitude(double longitude) {
  ExifTag ref_tag = static_cast<ExifTag>(EXIF_TAG_GPS_LONGITUDE_REF);
  std::unique_ptr<ExifEntry> ref_entry =
      AddVariableLengthEntry(EXIF_IFD_GPS, ref_tag, EXIF_FORMAT_ASCII, 2, 2);
  if (!ref_entry) {
    ALOGE("%s: Adding GPSLongitudeRef exif entry failed", __FUNCTION__);
    return false;
  }
  if (longitude >= 0) {
    memcpy(ref_entry->data, "E", sizeof("E"));
  } else {
    memcpy(ref_entry->data, "W", sizeof("W"));
    longitude *= -1;
  }

  ExifTag tag = static_cast<ExifTag>(EXIF_TAG_GPS_LONGITUDE);
  std::unique_ptr<ExifEntry> entry = AddVariableLengthEntry(
      EXIF_IFD_GPS, tag, EXIF_FORMAT_RATIONAL, 3, 3 * sizeof(ExifRational));
  if (!entry) {
    exif_content_remove_entry(exif_data_->ifd[EXIF_IFD_GPS], ref_entry.get());
    ALOGE("%s: Adding GPSLongitude exif entry failed", __FUNCTION__);
    return false;
  }
  SetLatitudeOrLongitudeData(entry->data, longitude);

  return true;
}

bool ExifUtilsImpl::SetGpsProcessingMethod(const std::string& method) {
  std::string buffer =
      std::string(gExifAsciiPrefix, sizeof(gExifAsciiPrefix)) + method;
  SET_STRING(EXIF_IFD_GPS, static_cast<ExifTag>(EXIF_TAG_GPS_PROCESSING_METHOD),
             EXIF_FORMAT_UNDEFINED, buffer);
  return true;
}

bool ExifUtilsImpl::SetGpsTimestamp(const struct tm& t) {
  const ExifTag date_tag = static_cast<ExifTag>(EXIF_TAG_GPS_DATE_STAMP);
  const size_t kGpsDateStampSize = 11;
  std::unique_ptr<ExifEntry> entry =
      AddVariableLengthEntry(EXIF_IFD_GPS, date_tag, EXIF_FORMAT_ASCII,
                             kGpsDateStampSize, kGpsDateStampSize);
  if (!entry) {
    ALOGE("%s: Adding GPSDateStamp exif entry failed", __FUNCTION__);
    return false;
  }
  int result =
      snprintf(reinterpret_cast<char*>(entry->data), kGpsDateStampSize,
               "%04i:%02i:%02i", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  if (result != kGpsDateStampSize - 1) {
    ALOGW("%s: Input time is invalid", __FUNCTION__);
    return false;
  }

  const ExifTag time_tag = static_cast<ExifTag>(EXIF_TAG_GPS_TIME_STAMP);
  entry = AddVariableLengthEntry(EXIF_IFD_GPS, time_tag, EXIF_FORMAT_RATIONAL,
                                 3, 3 * sizeof(ExifRational));
  if (!entry) {
    ALOGE("%s: Adding GPSTimeStamp exif entry failed", __FUNCTION__);
    return false;
  }
  exif_set_rational(entry->data, EXIF_BYTE_ORDER_INTEL,
                    {static_cast<ExifLong>(t.tm_hour), 1});
  exif_set_rational(entry->data + sizeof(ExifRational), EXIF_BYTE_ORDER_INTEL,
                    {static_cast<ExifLong>(t.tm_min), 1});
  exif_set_rational(entry->data + 2 * sizeof(ExifRational),
                    EXIF_BYTE_ORDER_INTEL, {static_cast<ExifLong>(t.tm_sec), 1});

  return true;
}

bool ExifUtilsImpl::SetImageHeight(uint32_t length) {
  SET_SHORT(EXIF_IFD_0, EXIF_TAG_IMAGE_LENGTH, length);
  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_PIXEL_Y_DIMENSION, length);
  return true;
}

bool ExifUtilsImpl::SetImageWidth(uint32_t width) {
  SET_SHORT(EXIF_IFD_0, EXIF_TAG_IMAGE_WIDTH, width);
  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_PIXEL_X_DIMENSION, width);
  return true;
}

bool ExifUtilsImpl::SetIsoSpeedRating(uint16_t iso_speed_ratings) {
  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_ISO_SPEED_RATINGS, iso_speed_ratings);
  return true;
}

bool ExifUtilsImpl::SetMaxAperture(float aperture) {
  float maxAperture = ConvertToApex(aperture);
  SET_RATIONAL(
      EXIF_IFD_EXIF, EXIF_TAG_MAX_APERTURE_VALUE,
      static_cast<uint32_t>(std::round(maxAperture * kRationalPrecision)),
      kRationalPrecision);
  return true;
}

bool ExifUtilsImpl::SetExposureBias(int32_t ev, uint32_t ev_step_numerator,
                                    uint32_t ev_step_denominator) {
  SET_RATIONAL(EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_BIAS_VALUE,
               ev * ev_step_numerator, ev_step_denominator);
  return true;
}

bool ExifUtilsImpl::SetOrientation(uint16_t degrees) {
  ExifOrientation value = ExifOrientation::ORIENTATION_0_DEGREES;
  switch (degrees) {
    case 90:
      value = ExifOrientation::ORIENTATION_90_DEGREES;
      break;
    case 180:
      value = ExifOrientation::ORIENTATION_180_DEGREES;
      break;
    case 270:
      value = ExifOrientation::ORIENTATION_270_DEGREES;
      break;
    default:
      break;
  }
  return SetOrientationValue(value);
}

bool ExifUtilsImpl::SetOrientationValue(ExifOrientation orientation_value) {
  SET_SHORT(EXIF_IFD_0, EXIF_TAG_ORIENTATION, orientation_value);
  return true;
}

bool ExifUtilsImpl::SetShutterSpeed(float exposure_time) {
  float shutter_speed = -log2f(exposure_time);
  SET_SRATIONAL(EXIF_IFD_EXIF, EXIF_TAG_SHUTTER_SPEED_VALUE,
                static_cast<uint32_t>(shutter_speed * kRationalPrecision),
                kRationalPrecision);
  return true;
}

bool ExifUtilsImpl::SetSubjectDistance(float diopters) {
  const static float kInfinityDiopters = 1.0e-6;
  uint32_t numerator, denominator;
  uint16_t distance_range;
  if (diopters > kInfinityDiopters) {
    float focus_distance = 1.0f / diopters;
    numerator =
        static_cast<uint32_t>(std::round(focus_distance * kRationalPrecision));
    denominator = kRationalPrecision;

    if (focus_distance < 1.0f) {
      distance_range = 1;  // Macro
    } else if (focus_distance < 3.0f) {
      distance_range = 2;  // Close
    } else {
      distance_range = 3;  // Distant
    }
  } else {
    numerator = 0xFFFFFFFF;
    denominator = 1;
    distance_range = 3;  // Distant
  }
  SET_RATIONAL(EXIF_IFD_EXIF, EXIF_TAG_SUBJECT_DISTANCE, numerator, denominator);
  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_SUBJECT_DISTANCE_RANGE, distance_range);
  return true;
}

bool ExifUtilsImpl::SetSubsecTime(const std::string& subsec_time) {
  SET_STRING(EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME, EXIF_FORMAT_ASCII,
             subsec_time);
  SET_STRING(EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME_ORIGINAL, EXIF_FORMAT_ASCII,
             subsec_time);
  SET_STRING(EXIF_IFD_EXIF, EXIF_TAG_SUB_SEC_TIME_DIGITIZED, EXIF_FORMAT_ASCII,
             subsec_time);
  return true;
}

bool ExifUtilsImpl::SetWhiteBalance(uint8_t white_balance) {
  uint16_t whiteBalance =
      (white_balance == ANDROID_CONTROL_AWB_MODE_AUTO) ? 0 : 1;
  SET_SHORT(EXIF_IFD_EXIF, EXIF_TAG_WHITE_BALANCE, whiteBalance);
  return true;
}

bool ExifUtilsImpl::GenerateApp1(unsigned char* thumbnail_buffer,
                                 uint32_t size) {
  DestroyApp1();
  exif_data_->data = thumbnail_buffer;
  exif_data_->size = size;
  // Save the result into |app1_buffer_|.
  exif_data_save_data(exif_data_, &app1_buffer_, &app1_length_);
  if (!app1_length_) {
    ALOGE("%s: Allocate memory for app1_buffer_ failed", __FUNCTION__);
    return false;
  }
  /*
   * The JPEG segment size is 16 bits in spec. The size of APP1 segment should
   * be smaller than 65533 because there are two bytes for segment size field.
   */
  if (app1_length_ > 65533) {
    DestroyApp1();
    ALOGE("%s: The size of APP1 segment is too large", __FUNCTION__);
    return false;
  }
  return true;
}

const uint8_t* ExifUtilsImpl::GetApp1Buffer() {
  return app1_buffer_;
}

unsigned int ExifUtilsImpl::GetApp1Length() {
  return app1_length_;
}

bool ExifUtilsImpl::SetExifVersion(const std::string& exif_version) {
  SET_STRING(EXIF_IFD_EXIF, EXIF_TAG_EXIF_VERSION, EXIF_FORMAT_UNDEFINED,
             exif_version);
  return true;
}

bool ExifUtilsImpl::SetMake(const std::string& make) {
  SET_STRING(EXIF_IFD_0, EXIF_TAG_MAKE, EXIF_FORMAT_UNDEFINED, make);
  return true;
}

bool ExifUtilsImpl::SetModel(const std::string& model) {
  SET_STRING(EXIF_IFD_0, EXIF_TAG_MODEL, EXIF_FORMAT_UNDEFINED, model);
  return true;
}

void ExifUtilsImpl::Reset() {
  DestroyApp1();
  if (exif_data_) {
    /*
     * Since we decided to ignore the original APP1, we are sure that there is
     * no thumbnail allocated by libexif. |exif_data_->data| is actually
     * allocated by JpegCompressor. sets |exif_data_->data| to nullptr to
     * prevent exif_data_unref() destroy it incorrectly.
     */
    exif_data_->data = nullptr;
    exif_data_->size = 0;
    exif_data_unref(exif_data_);
    exif_data_ = nullptr;
  }
}

std::unique_ptr<ExifEntry> ExifUtilsImpl::AddVariableLengthEntry(
    ExifIfd ifd, ExifTag tag, ExifFormat format, uint64_t components,
    unsigned int size) {
  // Remove old entry if exists.
  exif_content_remove_entry(exif_data_->ifd[ifd],
                            exif_content_get_entry(exif_data_->ifd[ifd], tag));
  ExifMem* mem = exif_mem_new_default();
  if (!mem) {
    ALOGE("%s: Allocate memory for exif entry failed", __FUNCTION__);
    return nullptr;
  }
  std::unique_ptr<ExifEntry> entry(exif_entry_new_mem(mem));
  if (!entry) {
    ALOGE("%s: Allocate memory for exif entry failed", __FUNCTION__);
    exif_mem_unref(mem);
    return nullptr;
  }
  void* tmp_buffer = exif_mem_alloc(mem, size);
  if (!tmp_buffer) {
    ALOGE("%s: Allocate memory for exif entry failed", __FUNCTION__);
    exif_mem_unref(mem);
    return nullptr;
  }

  entry->data = static_cast<unsigned char*>(tmp_buffer);
  entry->tag = tag;
  entry->format = format;
  entry->components = components;
  entry->size = size;

  exif_content_add_entry(exif_data_->ifd[ifd], entry.get());
  exif_mem_unref(mem);

  return entry;
}

std::unique_ptr<ExifEntry> ExifUtilsImpl::AddEntry(ExifIfd ifd, ExifTag tag) {
  std::unique_ptr<ExifEntry> entry(
      exif_content_get_entry(exif_data_->ifd[ifd], tag));
  if (entry) {
    // exif_content_get_entry() won't ref the entry, so we ref here.
    exif_entry_ref(entry.get());
    return entry;
  }
  entry.reset(exif_entry_new());
  if (!entry) {
    ALOGE("%s: Allocate memory for exif entry failed", __FUNCTION__);
    return nullptr;
  }
  entry->tag = tag;
  exif_content_add_entry(exif_data_->ifd[ifd], entry.get());
  exif_entry_initialize(entry.get(), tag);
  return entry;
}

bool ExifUtilsImpl::SetShort(ExifIfd ifd, ExifTag tag, uint16_t value,
                             const std::string& msg) {
  std::unique_ptr<ExifEntry> entry = AddEntry(ifd, tag);
  if (!entry) {
    ALOGE("%s: Adding '%s' entry failed", __FUNCTION__, msg.c_str());
    return false;
  }
  exif_set_short(entry->data, EXIF_BYTE_ORDER_INTEL, value);
  return true;
}

bool ExifUtilsImpl::SetLong(ExifIfd ifd, ExifTag tag, uint32_t value,
                            const std::string& msg) {
  std::unique_ptr<ExifEntry> entry = AddEntry(ifd, tag);
  if (!entry) {
    ALOGE("%s: Adding '%s' entry failed", __FUNCTION__, msg.c_str());
    return false;
  }
  exif_set_long(entry->data, EXIF_BYTE_ORDER_INTEL, value);
  return true;
}

bool ExifUtilsImpl::SetRational(ExifIfd ifd, ExifTag tag, uint32_t numerator,
                                uint32_t denominator, const std::string& msg) {
  std::unique_ptr<ExifEntry> entry = AddEntry(ifd, tag);
  if (!entry) {
    ALOGE("%s: Adding '%s' entry failed", __FUNCTION__, msg.c_str());
    return false;
  }
  exif_set_rational(entry->data, EXIF_BYTE_ORDER_INTEL,
                    {numerator, denominator});
  return true;
}

bool ExifUtilsImpl::SetSRational(ExifIfd ifd, ExifTag tag, int32_t numerator,
                                 int32_t denominator, const std::string& msg) {
  std::unique_ptr<ExifEntry> entry = AddEntry(ifd, tag);
  if (!entry) {
    ALOGE("%s: Adding '%s' entry failed", __FUNCTION__, msg.c_str());
    return false;
  }
  exif_set_srational(entry->data, EXIF_BYTE_ORDER_INTEL,
                     {numerator, denominator});
  return true;
}

bool ExifUtilsImpl::SetString(ExifIfd ifd, ExifTag tag, ExifFormat format,
                              const std::string& buffer,
                              const std::string& msg) {
  size_t entry_size = buffer.length();
  // Since the exif format is undefined, NULL termination is not necessary.
  if (format == EXIF_FORMAT_ASCII) {
    entry_size++;
  }
  std::unique_ptr<ExifEntry> entry =
      AddVariableLengthEntry(ifd, tag, format, entry_size, entry_size);
  if (!entry) {
    ALOGE("%s: Adding '%s' entry failed", __FUNCTION__, msg.c_str());
    return false;
  }
  memcpy(entry->data, buffer.c_str(), entry_size);
  return true;
}

void ExifUtilsImpl::DestroyApp1() {
  /*
   * Since there is no API to access ExifMem in ExifData->priv, we use free
   * here, which is the default free function in libexif. See
   * exif_data_save_data() for detail.
   */
  free(app1_buffer_);
  app1_buffer_ = nullptr;
  app1_length_ = 0;
}

bool ExifUtilsImpl::SetFromMetadata(const HalCameraMetadata& metadata,
                                    size_t image_width, size_t image_height) {
  if (!SetImageWidth(image_width) || !SetImageHeight(image_height)) {
    ALOGE("%s: setting image resolution failed.", __FUNCTION__);
    return false;
  }

  struct timespec tp;
  struct tm time_info;
  bool time_available = clock_gettime(CLOCK_REALTIME, &tp) != -1;
  localtime_r(&tp.tv_sec, &time_info);
  if (!SetDateTime(time_info)) {
    ALOGE("%s: setting data time failed.", __FUNCTION__);
    return false;
  }

  float focal_length;
  camera_metadata_ro_entry entry;
  auto ret = metadata.Get(ANDROID_LENS_FOCAL_LENGTH, &entry);
  if (ret == OK) {
    focal_length = entry.data.f[0];

    if (!SetFocalLength(focal_length)) {
      ALOGE("%s: setting focal length failed.", __FUNCTION__);
      return false;
    }

    if (!SetFocalLengthIn35mmFilm(focal_length, sensor_chars_.physical_size[0],
                                  sensor_chars_.physical_size[1])) {
      ALOGE("%s: setting focal length in 35mm failed.", __FUNCTION__);
      return false;
    }
  } else {
    ALOGV("%s: Cannot find focal length in metadata.", __FUNCTION__);
  }

  ret = metadata.Get(ANDROID_SCALER_CROP_REGION, &entry);
  if (ret == OK) {
    if (!SetDigitalZoomRatio(entry.data.i32[2], entry.data.i32[3],
                             sensor_chars_.width, sensor_chars_.height)) {
      ALOGE("%s: setting digital zoom ratio failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_JPEG_GPS_COORDINATES, &entry);
  if (ret == OK) {
    if (entry.count < 3) {
      ALOGE("%s: Gps coordinates in metadata is not complete.", __FUNCTION__);
      return false;
    }
    if (!SetGpsLatitude(entry.data.d[0])) {
      ALOGE("%s: setting gps latitude failed.", __FUNCTION__);
      return false;
    }
    if (!SetGpsLongitude(entry.data.d[1])) {
      ALOGE("%s: setting gps longitude failed.", __FUNCTION__);
      return false;
    }
    if (!SetGpsAltitude(entry.data.d[2])) {
      ALOGE("%s: setting gps altitude failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_JPEG_GPS_PROCESSING_METHOD, &entry);
  if (ret == OK) {
    std::string method_str(reinterpret_cast<const char*>(entry.data.u8));
    if (!SetGpsProcessingMethod(method_str)) {
      ALOGE("%s: setting gps processing method failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_JPEG_GPS_TIMESTAMP, &entry);
  if (time_available && (ret == OK)) {
    time_t timestamp = static_cast<time_t>(entry.data.i64[0]);
    if (gmtime_r(&timestamp, &time_info)) {
      if (!SetGpsTimestamp(time_info)) {
        ALOGE("%s: setting gps timestamp failed.", __FUNCTION__);
        return false;
      }
    } else {
      ALOGE("%s: Time transformation failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_JPEG_ORIENTATION, &entry);
  if (ret == OK) {
    if (!SetOrientation(entry.data.i32[0])) {
      ALOGE("%s: setting orientation failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_SENSOR_EXPOSURE_TIME, &entry);
  if (ret == OK) {
    float exposure_time = 1.0f * entry.data.i64[0] / 1e9;
    if (!SetExposureTime(exposure_time)) {
      ALOGE("%s: setting exposure time failed.", __FUNCTION__);
      return false;
    }

    if (!SetShutterSpeed(exposure_time)) {
      ALOGE("%s: setting shutter speed failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_LENS_FOCUS_DISTANCE, &entry);
  if (ret == OK) {
    if (!SetSubjectDistance(entry.data.f[0])) {
      ALOGE("%s: setting subject distance failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_SENSOR_SENSITIVITY, &entry);
  if (ret == OK) {
    int32_t iso = entry.data.i32[0];
    camera_metadata_ro_entry post_raw_sens_entry;
    metadata.Get(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST,
                 &post_raw_sens_entry);
    if (post_raw_sens_entry.count > 0) {
      iso = iso * post_raw_sens_entry.data.i32[0] / 100;
    }

    if (!SetIsoSpeedRating(static_cast<uint16_t>(iso))) {
      ALOGE("%s: setting iso rating failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_LENS_APERTURE, &entry);
  if (ret == OK) {
    if (!SetFNumber(entry.data.f[0])) {
      ALOGE("%s: setting F number failed.", __FUNCTION__);
      return false;
    }
    if (!SetAperture(entry.data.f[0])) {
      ALOGE("%s: setting aperture failed.", __FUNCTION__);
      return false;
    }
  }

  static const uint16_t kSRGBColorSpace = 1;
  if (!SetColorSpace(kSRGBColorSpace)) {
    ALOGE("%s: setting color space failed.", __FUNCTION__);
    return false;
  }

  camera_metadata_ro_entry flash_state_entry;
  metadata.Get(ANDROID_FLASH_STATE, &flash_state_entry);
  camera_metadata_ro_entry ae_mode_entry;
  metadata.Get(ANDROID_CONTROL_AE_MODE, &ae_mode_entry);
  uint8_t flash_state = flash_state_entry.count > 0
                            ? flash_state_entry.data.u8[0]
                            : ANDROID_FLASH_STATE_UNAVAILABLE;
  uint8_t ae_mode = ae_mode_entry.count > 0 ? ae_mode_entry.data.u8[0]
                                            : ANDROID_CONTROL_AE_MODE_OFF;

  if (!SetFlash(sensor_chars_.is_flash_supported, flash_state, ae_mode)) {
    ALOGE("%s: setting flash failed.", __FUNCTION__);
    return false;
  }

  ret = metadata.Get(ANDROID_CONTROL_AWB_MODE, &entry);
  if (ret == OK) {
    if (!SetWhiteBalance(entry.data.u8[0])) {
      ALOGE("%s: setting white balance failed.", __FUNCTION__);
      return false;
    }
  }

  ret = metadata.Get(ANDROID_CONTROL_AE_MODE, &entry);
  if (ret == OK) {
    if (!SetExposureMode(entry.data.u8[0])) {
      ALOGE("%s: setting exposure mode failed.", __FUNCTION__);
      return false;
    }
  }
  if (time_available) {
    char str[4];
    if (snprintf(str, sizeof(str), "%03ld", tp.tv_nsec / 1000000) < 0) {
      ALOGE("%s: Subsec is invalid: %ld", __FUNCTION__, tp.tv_nsec);
      return false;
    }
    if (!SetSubsecTime(std::string(str))) {
      ALOGE("%s: setting subsec time failed.", __FUNCTION__);
      return false;
    }
  }

  return true;
}

}  // namespace android
