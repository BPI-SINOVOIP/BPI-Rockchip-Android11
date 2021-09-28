/*
 *
 * Copyright 2013 Rockchip Electronics Co. LTD
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

/*
 * @file        Rockchip_OSAL_ColorUtils.h
 * @brief
 * @author      xhr(xhr@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2018.8.15 : Create
 */
#ifndef ROCKCHIP_OSAL_COLORUTILS
#define ROCKCHIP_OSAL_COLORUTILS

#include "OMX_Types.h"
#include "OMX_Video.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kNotSupported,
    kPreferBitstream,
    kPreferContainer,
};

OMX_BOOL colorAspectsDiffer(const OMX_COLORASPECTS *a, const OMX_COLORASPECTS *b);

void     convertIsoColorAspectsToCodecAspects(OMX_U32 primaries,
                                              OMX_U32 transfer,
                                              OMX_U32 coeffs,
                                              OMX_U32 fullRange,
                                              OMX_COLORASPECTS *aspects);

void    convertCodecAspectsToIsoColorAspects(OMX_COLORASPECTS *codecAspect, ISO_COLORASPECTS *colorAspect);

OMX_U32  handleColorAspectsChange(const OMX_COLORASPECTS *a/*mDefaultColorAspects*/,
                                  const OMX_COLORASPECTS *b/*mBitstreamColorAspects*/,
                                  OMX_COLORASPECTS *c/*mFinalColorAspects*/,
                                  OMX_U32 perference);

void    updateFinalColorAspects(const OMX_COLORASPECTS *otherAspects,
                                const OMX_COLORASPECTS *preferredAspects,
                                OMX_COLORASPECTS *mFinalColorAspects);

#ifdef __cplusplus
}
#endif

#endif
