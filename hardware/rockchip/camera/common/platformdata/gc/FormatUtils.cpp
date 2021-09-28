/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "FormatUtils"


#include <stdint.h>
#include <math.h>
#include <linux/v4l2-mediabus.h>
#include <string>
#include <sstream>
#include "FormatUtils.h"
#include "LogHelper.h"
#include "UtilityMacros.h"
#include "Camera3V4l2Format.h"

NAMESPACE_DECLARATION {
using std::string;

/**
 * Utilities to query information about V4L2 types in graph config
 */

namespace graphconfig {
namespace utils {

enum FormatType {
    FORMAT_RAW,
    FORMAT_RAW_VEC,
    FORMAT_YUV,
    FORMAT_YUV_VEC,
    FORMAT_RGB,
    FORMAT_MBUS_BAYER,
    FORMAT_MBUS_YUV,
    FORMAT_JPEG
};

struct FormatInfo {
    int32_t pixelCode;  // OS specific pixel code, in this case V4L2 or Media bus
    int32_t commonPixelCode;  // Common pixel code used by CIPF and GCSS in settings
    string fullName;
    string shortName;
    int32_t bpp;
    FormatType type;
};
/**
 * gFormatMapping
 *
 * Table for mapping OS agnostic formats defined in CIPF and OS specific ones
 * (in this case V4L2, or media bus).
 * The table also helps provide textual representation and bits per pixel.
 * CIPF does not define most of the formats, only the ones it needs, that is why
 * most of the entries have 0 on the common pixel format.
 * Conversely there are some new formats introduce by CIPF that do not have
 * V4L2 representation.
 */
static const FormatInfo gFormatMapping[] = {
    { V4L2_PIX_FMT_SBGGR8, 0, "V4L2_PIX_FMT_SBGGR8", "BGGR8", 8, FORMAT_RAW },
    { V4L2_PIX_FMT_SGBRG8, 0, "V4L2_PIX_FMT_SGBRG8", "GBRG8", 8, FORMAT_RAW },
    { V4L2_PIX_FMT_SGRBG8, 0, "V4L2_PIX_FMT_SGRBG8", "GRBG8", 8, FORMAT_RAW },
    { V4L2_PIX_FMT_SRGGB8, 0, "V4L2_PIX_FMT_SRGGB8", "RGGB8", 8, FORMAT_RAW },

    { V4L2_PIX_FMT_SBGGR12, get_fourcc('B', 'G', '1', '2'), "V4L2_PIX_FMT_SBGGR12", "BGGR12", 16, FORMAT_RAW },
    { V4L2_PIX_FMT_SGBRG12, get_fourcc('G', 'B', '1', '2'), "V4L2_PIX_FMT_SGBRG12", "GBRG12", 16, FORMAT_RAW },
    { V4L2_PIX_FMT_SGRBG12, get_fourcc('G', 'R', '1', '2'), "V4L2_PIX_FMT_SGRBG12", "GRBG12", 16, FORMAT_RAW },
    { V4L2_PIX_FMT_SRGGB12, get_fourcc('R', 'G', '1', '2'), "V4L2_PIX_FMT_SRGGB12", "RGGB12", 16, FORMAT_RAW },

    { V4L2_PIX_FMT_SBGGR10P, 0, "V4L2_PIX_FMT_SBGGR10P", "BGGR10P", 10, FORMAT_RAW },
    { V4L2_PIX_FMT_SGBRG10P, 0, "V4L2_PIX_FMT_SGBRG10P", "GBRG10P", 10, FORMAT_RAW },
    { V4L2_PIX_FMT_SGRBG10P, 0, "V4L2_PIX_FMT_SGRBG10P", "GRBG10P", 10, FORMAT_RAW },
    { V4L2_PIX_FMT_SRGGB10P, 0, "V4L2_PIX_FMT_SRGGB10P", "RGGB10P", 10, FORMAT_RAW },

    { V4L2_PIX_FMT_SBGGR10, get_fourcc('B', 'G', '1', '0'), "V4L2_PIX_FMT_SBGGR10", "BGGR10", 10, FORMAT_RAW },
    { V4L2_PIX_FMT_SGBRG10, get_fourcc('G', 'B', '1', '0'), "V4L2_PIX_FMT_SGBRG10", "GBRG10", 10, FORMAT_RAW },
    { V4L2_PIX_FMT_SGRBG10, get_fourcc('G', 'R', '1', '0'), "V4L2_PIX_FMT_SGRBG10", "GRBG10", 10, FORMAT_RAW },
    { V4L2_PIX_FMT_SRGGB10, get_fourcc('R', 'G', '1', '0'), "V4L2_PIX_FMT_SRGGB10", "RGGB10", 10, FORMAT_RAW },

    { V4L2_PIX_FMT_NV16, get_fourcc('N', 'V', '1', '6'), "V4L2_PIX_FMT_NV16", "NV16", 16, FORMAT_YUV },
    { V4L2_PIX_FMT_NV12, get_fourcc('N', 'V', '1', '2'), "V4L2_PIX_FMT_NV12", "NV12", 12, FORMAT_YUV },
    { V4L2_PIX_FMT_YUYV, get_fourcc('Y', 'U', 'Y', 'V'), "V4L2_PIX_FMT_YUYV", "YUYV", 16, FORMAT_YUV },

    { V4L2_PIX_FMT_YUV420, 0, "V4L2_PIX_FMT_YUV420", "YUV420", 12, FORMAT_YUV },
    { V4L2_PIX_FMT_YVU420, 0, "V4L2_PIX_FMT_YVU420", "YVU420", 12, FORMAT_YUV },
    { V4L2_PIX_FMT_YUV422P, 0, "V4L2_PIX_FMT_YUV422P", "YUV422P", 16, FORMAT_YUV },

    { V4L2_PIX_FMT_BGR24, 0, "V4L2_PIX_FMT_BGR24", "BGR24", 24, FORMAT_RGB },
    { V4L2_PIX_FMT_XBGR32, 0, "V4L2_PIX_FMT_XBGR32", "XBGR32", 32, FORMAT_RGB },
    { V4L2_PIX_FMT_XRGB32, 0, "V4L2_PIX_FMT_XRGB32", "XRGB32", 32, FORMAT_RGB },
    { V4L2_PIX_FMT_RGB565, 0, "V4L2_PIX_FMT_RGB565", "RGB565", 16, FORMAT_RGB },

    { V4L2_PIX_FMT_JPEG, 0, "V4L2_PIX_FMT_JPEG", "JPEG", 0, FORMAT_JPEG },

    { V4L2_MBUS_FMT_SBGGR10_1X10, get_fourcc('B', 'G', '1', '0'), "V4L2_MBUS_FMT_SBGGR10_1X10", "SBGGR10_1X10", 10, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SGBRG10_1X10, get_fourcc('G', 'B', '1', '0'), "V4L2_MBUS_FMT_SGBRG10_1X10", "SGBRG10_1X10", 10, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SGRBG10_1X10, get_fourcc('G', 'R', '1', '0'), "V4L2_MBUS_FMT_SGRBG10_1X10", "SGRBG10_1X10", 10, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SRGGB10_1X10, get_fourcc('R', 'G', '1', '0'), "V4L2_MBUS_FMT_SRGGB10_1X10", "SRGGB10_1X10", 10, FORMAT_MBUS_BAYER },

    { V4L2_MBUS_FMT_SBGGR12_1X12, get_fourcc('B', 'G', '1', '2'), "V4L2_MBUS_FMT_SBGGR12_1X12", "SBGGR12_1X12", 12, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SGBRG12_1X12, get_fourcc('G', 'B', '1', '2'), "V4L2_MBUS_FMT_SGBRG12_1X12", "SGBRG12_1X12", 12, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SGRBG12_1X12, get_fourcc('G', 'R', '1', '2'), "V4L2_MBUS_FMT_SGRBG12_1X12", "SGRBG12_1X12", 12, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SRGGB12_1X12, get_fourcc('R', 'G', '1', '2'), "V4L2_MBUS_FMT_SRGGB12_1X12", "SRGGB12_1X12", 12, FORMAT_MBUS_BAYER },

    { V4L2_MBUS_FMT_SBGGR8_1X8, 0, "V4L2_MBUS_FMT_SBGGR8_1X8", "SBGGR8_1X8", 8, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SGBRG8_1X8, 0, "V4L2_MBUS_FMT_SGBRG8_1X8", "SGBRG8_1X8", 8, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SGRBG8_1X8, 0, "V4L2_MBUS_FMT_SGRBG8_1X8", "SGRBG8_1X8", 8, FORMAT_MBUS_BAYER },
    { V4L2_MBUS_FMT_SRGGB8_1X8, 0, "V4L2_MBUS_FMT_SRGGB8_1X8", "SRGGB8_1X8", 8, FORMAT_MBUS_BAYER },

    { V4L2_MBUS_FMT_YUYV8_1X16, get_fourcc('N', 'V', '6', '1'), "V4L2_MBUS_FMT_YUYV8_1X16", "YUYV8_1X16", 16, FORMAT_MBUS_YUV },
    { V4L2_MBUS_FMT_YVYU8_1X16, get_fourcc('N', 'V', '1', '6'), "V4L2_MBUS_FMT_YVYU8_1X16", "YVYU8_1X16", 16, FORMAT_MBUS_YUV },
    { V4L2_MBUS_FMT_YUYV8_2X8, get_fourcc('Y', 'U', 'Y', 'V'), "V4L2_MBUS_FMT_YUYV8_2X8", "YUYV8_2X8", 16, FORMAT_MBUS_YUV },
    { V4L2_MBUS_FMT_YVYU8_2X8, get_fourcc('Y', 'V', 'Y', 'U'), "V4L2_MBUS_FMT_YVYU8_2X8", "YVYU8_2X8", 16, FORMAT_MBUS_YUV },
    { V4L2_MBUS_FMT_UYVY8_2X8, get_fourcc('U', 'Y', 'V', 'Y'), "V4L2_MBUS_FMT_UYVY8_2X8", "UYVY8_2X8", 16, FORMAT_MBUS_YUV },
};

bool isRawFormat(int32_t format) {
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].pixelCode == format) {
            FormatType type = gFormatMapping[i].type;
            return (type  == FORMAT_RAW || type == FORMAT_MBUS_BAYER) ? true : false;
        }
    }
    LOGW("@%s:Invalid Format: 0x%x, %s", __FUNCTION__, format, v4l2Fmt2Str(format));
    return false;
}

const string pixelCode2String(int32_t code)
{
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].pixelCode == code) {
            return gFormatMapping[i].fullName;
        }
    }
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].commonPixelCode == code) {
            return gFormatMapping[i].fullName;
        }
    }

    LOGE("Invalid Pixel Format: 0x%x, %s", code, v4l2Fmt2Str(code));
    return "INVALID FORMAT";
}

int32_t pixelCode2fourcc(int32_t code)
{
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].pixelCode == code) {
            return gFormatMapping[i].commonPixelCode;
        }
    }

    LOGE("@%s :Invalid Pixel Format: 0x%x, %s ", __FUNCTION__, code, v4l2Fmt2Str(code));
    return -1;
}

/**
 * Calculate bytes per line(bpl) based on fourcc format.
 *
 * \param[in] format 4CC code in OS specific format
 * \return bpl bytes per line
 */
int32_t getBpl(int32_t format, int32_t width)
{
    int32_t bpl = 0;
    switch (format) {
        default:
            bpl = width;
            LOGW("bpl defaulting to width");
            break;
    }
    return bpl;
}

/**
 *  Retrieve the bits per pixel  from the OS specific pixel code.
 *  This is ususally used for buffer allocation calculations
 *
 *  \param [in] format 4CC code in OS specific format
 *  \return bits per pixel
 */
int32_t getBpp(int32_t format)
{
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].pixelCode == format) {
            return gFormatMapping[i].bpp;
        }
    }

    LOGE("There is no bpp supplied for format %s",
            pixelCode2String(format).c_str());
    return -1;
}

/**
 *  Retrieve the bits per pixel from the common pixel code format (CIPF)
 *  This is usually used for buffer allocation calculations
 *
 *  \param [in] format 4CC code in Common format
 *  \return bits per pixel
 */
int32_t getBppFromCommon(int32_t format)
{
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].commonPixelCode == format) {
            return gFormatMapping[i].bpp;
        }
    }

    LOGE("There is no bpp supplied for format %s",
            pixelCode2String(format).c_str());
    return -1;
}

/**
 *  For a given bayer order and bpp combination it searches the table to find
 *  a valid MBUS format.
 *  The search is done on the shortname.
 *
 *  \param [in] bayerOrder String with bayer order. Ex: RGGB
 *  \param [in] bpp Bits per pixe
 *  \return pixel code of the MBUS format that matches that bayer order and
 *          bpp, or -1 if not found.
 *
 */

int32_t getMBusFormat(const std::string& bayerOrder, const int32_t bpp)
{
    std::ostringstream stringStream;
    stringStream << bpp;
    string bppAsStr = stringStream.str();

    string targetFormat = bayerOrder + bppAsStr;
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].type == FORMAT_MBUS_BAYER) {
            if (gFormatMapping[i].shortName.find(targetFormat) != string::npos)
                return gFormatMapping[i].pixelCode;
        } else if (gFormatMapping[i].type == FORMAT_MBUS_YUV) {
            if (gFormatMapping[i].shortName.find(bppAsStr) != string::npos)
                return gFormatMapping[i].pixelCode;
        }
    }

    LOGE("Failed to find any MBUS format with format %s",targetFormat.c_str());
    return -1;
}

int32_t getMBusFormat(int32_t commonPixelFormat)
{
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].type == FORMAT_MBUS_BAYER ||
            gFormatMapping[i].type == FORMAT_MBUS_YUV ) {
            if (gFormatMapping[i].commonPixelCode == commonPixelFormat)
                return gFormatMapping[i].pixelCode;
        }
    }

    LOGE("Failed to find any MBUS format with format %s",
            pixelCode2String(commonPixelFormat).c_str());
    return -1;
}

int32_t getV4L2Format(const int32_t commonPixelFormat)
{
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].type == FORMAT_MBUS_BAYER ||
            gFormatMapping[i].type == FORMAT_MBUS_YUV )
            continue;

        if (gFormatMapping[i].commonPixelCode == commonPixelFormat)
            return gFormatMapping[i].pixelCode;
    }

    LOGE("Failed to find any V4L2 format with format %s",
            pixelCode2String(commonPixelFormat).c_str());
    return -1;
}

int32_t getV4L2Format(const std::string& formatName)
{
    for (size_t i = 0; i < ARRAY_SIZE(gFormatMapping); i++) {
        if (gFormatMapping[i].fullName.compare(formatName) == 0)
            return gFormatMapping[i].pixelCode;
    }

    LOGE("Failed to find any V4L2 format with format %s",
            formatName.c_str());
    return -1;
}

} // namespace utils
} // namespace graphconfig
} NAMESPACE_DECLARATION_END
