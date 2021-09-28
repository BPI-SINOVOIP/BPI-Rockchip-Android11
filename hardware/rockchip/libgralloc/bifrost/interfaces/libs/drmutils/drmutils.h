/*
 * Copyright (C) 2020 Arm Limited.
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef GRALLOC_LIBS_DRMUTILS_H
#define GRALLOC_LIBS_DRMUTILS_H

#include "mali_fourcc.h"
#include "mali_gralloc_buffer.h"

/**
 * @brief Obtain the FOURCC corresponding to the given Gralloc internal format.
 *
 * @param hnd Private handle where the format information is stored.
 *
 * @return The DRM FOURCC format or DRM_FORMAT_INVALID in case of errors.
 */
uint32_t drm_fourcc_from_handle(const private_handle_t *hnd);

/**
 * @brief Extract the part of the DRM modifier stored inside the given internal format and private handle.
 *
 * @param hnd Private handle where part of the modifier information is stored.
 * @param internal_format The internal format, where part of the modifier information is stored.
 *
 * @return The information extracted from the argument, in the form of a DRM modifier.
 */
uint64_t drm_modifier_from_handle(const private_handle_t *hnd);

#endif
