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


#ifndef __GCSS_FORMATS_H__
#define __GCSS_FORMATS_H__

#include <ia_tools/css_types.h>
#include <ia_cipf/ia_cipf_types.h>
#include <map>
#include <vector>
#include <string>

namespace GCSSFormats {

struct ia_format_plane_t {
    std::string name;
    int32_t bpp;

    ia_format_plane_t() :
        name(""),
        bpp(0) {};
};
typedef std::vector<ia_format_plane_t> planes_vector_t;

/**
 * \todo default values
 * Container for common format data. Last member is reserved for user to
 * add own identifier for the format.
 */
struct ia_format_t {
    std::string      name;        /**< format name                            */
    std::string      type;        /**< format type (BAYER, YUV)               */
    int32_t          bpp;         /**< bits per pixel                         */
    bool             vectorized;  /**< vectorized yes =1, no =0               */
    bool             packed;      /**< packed yes =1, no =0                   */
    planes_vector_t  planes;      /**< vector to contain planes if available */
    uint32_t         id;          /**< Reserved for user defined id. This is to
                                       associate os defined formats with the
                                       common, xos formats */

    ia_format_t() :
        name(""),
        type(""),
        bpp(0),
        vectorized(false),
        packed(false),
        id(0) {};
};

typedef std::vector<ia_format_t> formats_vector_t;

/**
 * Parses given formats xml. Results can be retrieved with getFormats()
 *
 * \param[in] formatsXML, path to the xml file
 * \param[out] formatsV vector of formats
 * \return css_err_none at success
 */
css_err_t parseFormats(const char *formatsXML, formats_vector_t &formatsV);

/**
 * Get format by id
 * \param[in] formatsV formats vector
 * \param[in] id
 * \param[out] format
 * \return css_err_none at success
 */
css_err_t getFormatById(const formats_vector_t &formatsV,
                        uint32_t id,
                        ia_format_t &format);

/**
 * Get format by name
 * \param[in] formatsV formats vector
 * \param[in] name
 * \param[out] format
 * \return css_err_none at success
 */
css_err_t getFormatByName(const formats_vector_t &formatsV,
                          const std::string &name,
                          ia_format_t &format);

/**
 * set user defined id for format
 *
 * Searches vector of formats by name and sets id. The id is used to associate
 * os defined formats with the formats provided by xos.
 *
 * \param[in] formatsV, Vector of formats
 * \param[in] name,     Name of the format to which to set the id
 * \param[in] id,       the user defined id
 */
css_err_t setFormatId(formats_vector_t &formatsV,
                      const std::string &name,
                      uint32_t id);

} // namespace
#endif
