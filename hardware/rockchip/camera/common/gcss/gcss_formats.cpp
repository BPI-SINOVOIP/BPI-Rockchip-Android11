/*
 * Copyright (C) 2017 Intel Corporation
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


#define LOG_TAG "GCSSFormats"

#include "LogHelper.h"

#include "GCSSParser.h"
#include "gcss_formats.h"
#include <stdio.h>
#include <string.h>

using namespace GCSS;
using namespace std;
using namespace android::camera2;

namespace GCSSFormats {

css_err_t
parseFormats(const char* formatsXML, formats_vector_t &formatsV)
{
    IGraphConfig *formats;

    // create gcss parser
    GCSSParser *parser = new GCSSParser();
    parser->parseGCSSXmlFile(formatsXML, &formats);
    delete parser;

    if (formats == nullptr) {
        LOGE("Failed to parse formats");
        return css_err_internal;
    }

    formats = formats->getDescendant(GCSS_KEY_FORMATS);
    if (formats == nullptr) {
        LOGE("Couldn't get formats");
        return css_err_data;
    }

    uint32_t descCount;
    descCount = formats->getDescendantCount();

    for (uint32_t i = 0; i <= descCount; i++) {

        // Iterate through source nodes in the graph
        IGraphConfig *formatNode;
        formatNode = formats->iterateDescendantByIndex(GCSS_KEY_FORMAT,
                                                       i);
        if (formatNode == nullptr)
            continue;

        ia_format_t format;
        int32_t vectorized;
        int32_t packed;

        // populate formats struct
        formatNode->getValue(GCSS_KEY_BPP, format.bpp);
        formatNode->getValue(GCSS_KEY_NAME, format.name);
        formatNode->getValue(GCSS_KEY_TYPE, format.type);
        formatNode->getValue(GCSS_KEY_VECTORIZED, vectorized);
        formatNode->getValue(GCSS_KEY_PACKED, packed);

        // set bool values
        format.vectorized = (vectorized > 0 ? true : false);
        format.packed = (packed > 0 ? true : false);

        // Check if format has planes and add them
        uint32_t planeCount;
        planeCount = formatNode->getDescendantCount();
        for (uint32_t ii = 0; ii <= planeCount; ii++) {
            IGraphConfig *planeNode = formatNode->iterateDescendantByIndex(GCSS_KEY_PLANE,
                                                                           ii);
            if (planeNode == nullptr)
                continue;

            ia_format_plane_t plane;
            planeNode->getValue(GCSS_KEY_BPP, plane.bpp);
            planeNode->getValue(GCSS_KEY_NAME, plane.name);

            format.planes.push_back(plane);
        }
        formatsV.push_back(format);
    }
    delete(formats->getRoot());
    return css_err_none;
}

css_err_t getFormatById(const formats_vector_t &formatsV,
        uint32_t id, ia_format_t &format)
{
    for (size_t i = 0; i < formatsV.size(); i++) {
        if (formatsV[i].id == id) {
            format = formatsV[i];
            return css_err_none;
        }
    }
    LOGE("Could not find format with id %d", id);
    return css_err_data;
}

css_err_t getFormatByName(const formats_vector_t &formatsV,
        const std::string &name, ia_format_t &format)
{
    for (size_t i = 0; i < formatsV.size(); i++) {
        if (formatsV[i].name == name) {
            format = formatsV[i];
            return css_err_none;
        }
    }
    LOGE("Could not find format with name %s", name.c_str());
    return css_err_data;
}

css_err_t setFormatId(formats_vector_t &formatsV, const string &name, uint32_t id)
{
    for (size_t i = 0; i < formatsV.size(); i++) {
        if (formatsV[i].name == name) {
            formatsV[i].id = id;
            return css_err_none;
        }
    }
    LOGE("Failed to set format id for %s", name.c_str());
    return css_err_argument;
}
} // namespace
