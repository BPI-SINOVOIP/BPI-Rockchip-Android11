/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#define LOG_TAG "IPSLConfParser"

#include "LogHelper.h"
#include "PlatformData.h"
#include "IPSLConfParser.h"
#include <string.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include "v4l2device.h"

NAMESPACE_DECLARATION {
/**
 * Table to map the name  of the sections in the xml file to the enum used
 * during parsing
 */
IPSLConfParser::Item IPSLConfParser::sSectionNames[NUMBER_OF_COMMON_SECTIONS] =
{
        {HAL_TUNING, "hal_tuning"}
};

void IPSLConfParser::handleCommonSection(int sectionId, int sensorIndex,
                                        const char *name, const char **atts)
{
    LOGI("@%s: sectionId: %d, sensor: %d", __FUNCTION__, sectionId, sensorIndex);
    UNUSED(sensorIndex);

    switch(sectionId) {
    case HAL_TUNING:
        // HAL tuning section is only once in XML, but the information needs
        // to be stored for all cameras
        for (unsigned int i = 0; i < mCaps.size(); i++) {
            parseHalTuningSection(mCaps[i], i, name, atts);
        }
        break;
    default:
        LOGE("@%s: Unknown section id %d - BUG?", __FUNCTION__, sectionId);
    }
}
/**
 * Parse the tags from hal_tuning section of the XML that are per camera
 * but that are common for all PSL's
 * Store the results in the CameraCapInfo
 * \param info [OUT]: class used to store the settings
 * \param sensorIndex [IN]: sensor index for per-camera settings
 * \param name [IN]: name of the XML tag to parse
 * \param atts [IN]: value of the XML tag
 */
void IPSLConfParser::parseHalTuningSection(CameraCapInfo* info, int sensorIndex,
                                          const char *name, const char **atts)
{
   LOGI("@%s: sensor: %d tag: %s", __FUNCTION__, sensorIndex, name);
}

int IPSLConfParser::commonFieldForName(const char * name)
{
    for (int i = 0; i < NUMBER_OF_COMMON_SECTIONS; i++)
        if (strncmp(name,sSectionNames[i].name, SECTION_NAME_MAX_LENGTH) == 0)
            return sSectionNames[i].id;

    return -1;
}

bool IPSLConfParser::isCommonSection(const char * name)
{
    return commonFieldForName(name) > 0;
}

bool IPSLConfParser::isCommonSection(int sectionId)
{
    return (sectionId >= COMMON_SECTION_BASE);
}

/*
 * Helper function for converting string to int for the
 * V4L2 pixel formats requested for media controller set-up.
 * pixel format accepted by the media ctl entities
 */
int IPSLConfParser::getPixelFormatAsValue(const char* format)
{
    /* for subdevs */
    if (!strcmp(format, "V4L2_MBUS_FMT_SBGGR12_1X12")) {
        return V4L2_MBUS_FMT_SBGGR12_1X12;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SGBRG12_1X12")) {
        return V4L2_MBUS_FMT_SGBRG12_1X12;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SGRBG12_1X12")) {
        return V4L2_MBUS_FMT_SGRBG12_1X12;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SRGGB12_1X12")) {
        return V4L2_MBUS_FMT_SRGGB12_1X12;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SBGGR10_1X10")) {
        return V4L2_MBUS_FMT_SBGGR10_1X10;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SGBRG10_1X10")) {
        return V4L2_MBUS_FMT_SGBRG10_1X10;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SGRBG10_1X10")) {
        return V4L2_MBUS_FMT_SGRBG10_1X10;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SRGGB10_1X10")) {
        return V4L2_MBUS_FMT_SRGGB10_1X10;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SBGGR8_1X8")) {
        return V4L2_MBUS_FMT_SBGGR8_1X8;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SGBRG8_1X8")) {
        return V4L2_MBUS_FMT_SGBRG8_1X8;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SGRBG8_1X8")) {
        return V4L2_MBUS_FMT_SGRBG8_1X8;
    } else if (!strcmp(format, "V4L2_MBUS_FMT_SRGGB8_1X8")) {
        return V4L2_MBUS_FMT_SRGGB8_1X8;
    /* for nodes */
#if defined V4L2_PIX_FMT_SGRBG12V32
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGRBG12V32")) {
        return V4L2_PIX_FMT_SGRBG12V32;
#endif
    } else if (!strcmp(format, "V4L2_PIX_FMT_SBGGR12")) {
        return V4L2_PIX_FMT_SBGGR12;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGBRG12")) {
        return V4L2_PIX_FMT_SGBRG12;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGRBG12")) {
        return V4L2_PIX_FMT_SGRBG12;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SRGGB12")) {
        return V4L2_PIX_FMT_SRGGB12;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SBGGR10")) {
        return V4L2_PIX_FMT_SBGGR10;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGBRG10")) {
        return V4L2_PIX_FMT_SGBRG10;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGRBG10")) {
        return V4L2_PIX_FMT_SGRBG10;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SRGGB10")) {
        return V4L2_PIX_FMT_SRGGB10;
#if defined V4L2_PIX_FMT_SBGGR10P
    } else if (!strcmp(format, "V4L2_PIX_FMT_SBGGR10P")) {
        return V4L2_PIX_FMT_SBGGR10P;
#endif
#if defined V4L2_PIX_FMT_SGBRG10P
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGBRG10P")) {
        return V4L2_PIX_FMT_SGBRG10P;
#endif
#if defined V4L2_PIX_FMT_SGRBG10P
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGRBG10P")) {
        return V4L2_PIX_FMT_SGRBG10P;
#endif
#if defined V4L2_PIX_FMT_SRGGB10P
    } else if (!strcmp(format, "V4L2_PIX_FMT_SRGGB10P")) {
        return V4L2_PIX_FMT_SRGGB10P;
#endif
    } else if (!strcmp(format, "V4L2_PIX_FMT_SBGGR8")) {
        return V4L2_PIX_FMT_SBGGR8;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGBRG8")) {
        return V4L2_PIX_FMT_SGBRG8;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SGRBG8")) {
        return V4L2_PIX_FMT_SGRBG8;
    } else if (!strcmp(format, "V4L2_PIX_FMT_SRGGB8")) {
        return V4L2_PIX_FMT_SRGGB8;
    /* for stream formats */
    } else if (!strcmp(format, "V4L2_PIX_FMT_NV12")) {
        return V4L2_PIX_FMT_NV12;
    } else if (!strcmp(format, "V4L2_PIX_FMT_JPEG")) {
        return V4L2_PIX_FMT_JPEG;
    } else if (!strcmp(format, "V4L2_PIX_FMT_YUV420")) {
        return V4L2_PIX_FMT_YUV420;
    } else if (!strcmp(format, "V4L2_PIX_FMT_NV21")) {
        return V4L2_PIX_FMT_NV21;
    } else if (!strcmp(format, "V4L2_PIX_FMT_YUV422P")) {
        return V4L2_PIX_FMT_YUV422P;
    } else if (!strcmp(format, "V4L2_PIX_FMT_YVU420")) {
        return V4L2_PIX_FMT_YVU420;
#if defined V4L2_PIX_FMT_YUYV420_V32
    } else if (!strcmp(format, "V4L2_PIX_FMT_YUYV420_V32")) {
        return V4L2_PIX_FMT_YUYV420_V32;
#endif
    } else if (!strcmp(format, "V4L2_PIX_FMT_YUYV")) {
        return V4L2_PIX_FMT_YUYV;
    } else if (!strcmp(format, "V4L2_PIX_FMT_RGB565")) {
        return V4L2_PIX_FMT_RGB565;
    } else if (!strcmp(format, "V4L2_PIX_FMT_RGB24")) {
        return V4L2_PIX_FMT_RGB24;
    } else if (!strcmp(format, "V4L2_PIX_FMT_BGR32")) {
        return V4L2_PIX_FMT_BGR32;
#if defined MEDIA_BUS_FMT_YUYV12_1X24
    } else if (!strcmp(format, "MEDIA_BUS_FMT_YUYV12_1X24")) {
        return MEDIA_BUS_FMT_YUYV12_1X24;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SBGGR12_1X12")) {
        return MEDIA_BUS_FMT_SBGGR12_1X12;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SGBRG12_1X12")) {
        return MEDIA_BUS_FMT_SGBRG12_1X12;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SGRBG12_1X12")) {
        return MEDIA_BUS_FMT_SGRBG12_1X12;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SRGGB12_1X12")) {
        return MEDIA_BUS_FMT_SRGGB12_1X12;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SBGGR10_1X10")) {
        return MEDIA_BUS_FMT_SBGGR10_1X10;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SGBRG10_1X10")) {
        return MEDIA_BUS_FMT_SGBRG10_1X10;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SGRBG10_1X10")) {
        return MEDIA_BUS_FMT_SGRBG10_1X10;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SRGGB10_1X10")) {
        return MEDIA_BUS_FMT_SRGGB10_1X10;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SBGGR8_1X8")) {
        return MEDIA_BUS_FMT_SBGGR8_1X8;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SGBRG8_1X8")) {
        return MEDIA_BUS_FMT_SGBRG8_1X8;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SGRBG8_1X8")) {
        return MEDIA_BUS_FMT_SGRBG8_1X8;
    } else if (!strcmp(format, "MEDIA_BUS_FMT_SRGGB8_1X8")) {
        return MEDIA_BUS_FMT_SRGGB8_1X8;
#endif
    } else {
        LOGE("%s, Unknown Pixel Format (%s)", __FUNCTION__, format);
        return -1;
    }
}

} NAMESPACE_DECLARATION_END
