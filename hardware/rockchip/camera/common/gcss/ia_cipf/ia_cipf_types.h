/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#ifndef _IA_CIPF_TYPES_H_
#define _IA_CIPF_TYPES_H_

#include <ia_tools/ia_list.h>
#include <ia_tools/css_types.h>

/** \file ia_cipf_types.h
 *
 * Definitions for datatypes used with public CIPF interfaces
 *
 * \ingroup ia_cipf
 *
 * Includes opaque CIPF objects and types for data management
 */

/**
 * \ingroup ia_cipf
 */
typedef uint32_t ia_uid;

/**
 * \ingroup ia_cipf
 */
#define ia_fourcc(a, b, c, d) ((uint32_t)(d) | ((uint32_t)(c) << 8) \
                              | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

#endif /* _IA_CIPF_TYPES_H_ */
