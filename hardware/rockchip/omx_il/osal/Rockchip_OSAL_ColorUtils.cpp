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
 * @file        Rockchip_OSAL_ColorUtils.c
 * @brief
 * @author      xhr(xhr@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2018.8.15 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "vpu.h"

#include "Rockchip_OSAL_ColorUtils.h"
#include "Rockchip_OSAL_Log.h"
#include "Rockchip_OMX_Macros.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    OMX_U32 mColorAspects;
    OMX_U32 mCodecAspects;
} MapAspects;

static MapAspects sIsoPrimaries[] = {
    { 1, PrimariesBT709_5 },
    { 2, PrimariesUnspecified },
    { 4, PrimariesBT470_6M },
    { 5, PrimariesBT601_6_625 },
    { 6, PrimariesBT601_6_525 /* main */},
    { 7, PrimariesBT601_6_525 },
    // -- ITU T.832 201201 ends here
    { 8, PrimariesGenericFilm },
    { 9, PrimariesBT2020 },
    { 10, PrimariesOther /* XYZ */ },
};

static MapAspects sIsoTransfers[] = {
    { 1, TransferSMPTE170M /* main */},
    { 2, TransferUnspecified },
    { 4, TransferGamma22 },
    { 5, TransferGamma28 },
    { 6, TransferSMPTE170M },
    { 7, TransferSMPTE240M },
    { 8, TransferLinear },
    { 9, TransferOther /* log 100:1 */ },
    { 10, TransferOther /* log 316:1 */ },
    { 11, TransferXvYCC },
    { 12, TransferBT1361 },
    { 13, TransferSRGB },
    // -- ITU T.832 201201 ends here
    { 14, TransferSMPTE170M },
    { 15, TransferSMPTE170M },
    { 16, TransferST2084 },
    { 17, TransferST428 },
    { 18, TransferHLG },
};

static MapAspects sIsoMatrixCoeffs[] = {
    { 0, MatrixOther },
    { 1, MatrixBT709_5 },
    { 2, MatrixUnspecified },
    { 4, MatrixBT470_6M },
    { 6, MatrixBT601_6 /* main */ },
    { 5, MatrixBT601_6 },
    { 7, MatrixSMPTE240M },
    { 8, MatrixOther /* YCgCo */ },
    // -- ITU T.832 201201 ends here
    { 9, MatrixBT2020 },
    { 10, MatrixBT2020Constant },
};

OMX_BOOL findCodecAspects(OMX_U32 colorAspects, OMX_U32 *codecAspects, MapAspects *mapAspects, OMX_U32 size)
{
    OMX_BOOL ret = OMX_FALSE;
    OMX_U32 i    = 0;

    for (i = 0; i < size; i++) {
        if (colorAspects == mapAspects[i].mColorAspects) {
            *codecAspects = mapAspects[i].mCodecAspects;
            ret = OMX_TRUE;
            break;
        }
    }

    return ret;
}

OMX_BOOL findColorAspects(OMX_U32 codecAspects, OMX_U32 *colorAspects, MapAspects *mapAspects, OMX_U32 size)
{
    OMX_BOOL ret = OMX_FALSE;
    OMX_U32 i    = 0;

    for (i = 0; i < size; i++) {
        if (codecAspects == mapAspects[i].mCodecAspects) {
            *colorAspects = mapAspects[i].mColorAspects;
            ret = OMX_TRUE;
            break;
        }
    }

    return ret;
}

// static
void convertIsoColorAspectsToCodecAspects(OMX_U32 primaries,
                                          OMX_U32 transfer,
                                          OMX_U32 coeffs,
                                          OMX_U32 fullRange,
                                          OMX_COLORASPECTS * aspects)
{
    if (!findCodecAspects(primaries, (OMX_U32 *)&aspects->mPrimaries, sIsoPrimaries, ARRAY_SIZE(sIsoPrimaries))) {
        aspects->mPrimaries = PrimariesUnspecified;
    }
    if (!findCodecAspects(transfer, (OMX_U32 *)&aspects->mTransfer, sIsoTransfers, ARRAY_SIZE(sIsoTransfers))) {
        aspects->mTransfer = TransferUnspecified;
    }
    if (!findCodecAspects(coeffs, (OMX_U32 *)&aspects->mMatrixCoeffs, sIsoMatrixCoeffs, ARRAY_SIZE(sIsoMatrixCoeffs))) {
        aspects->mMatrixCoeffs = MatrixUnspecified;
    }
    (aspects->mRange) = fullRange ? RangeFull : RangeLimited;
}

// static
void convertCodecAspectsToIsoColorAspects(OMX_COLORASPECTS *codecAspect, ISO_COLORASPECTS *colorAspect)
{
    if (!findColorAspects((OMX_U32)codecAspect->mPrimaries, &colorAspect->mPrimaries, sIsoPrimaries, ARRAY_SIZE(sIsoPrimaries))) {
        colorAspect->mPrimaries = 2;
    }
    if (!findColorAspects((OMX_U32)codecAspect->mTransfer, &colorAspect->mTransfer, sIsoTransfers, ARRAY_SIZE(sIsoTransfers))) {
        colorAspect->mTransfer = 2;
    }
    if (!findColorAspects((OMX_U32)codecAspect->mMatrixCoeffs, &colorAspect->mMatrixCoeffs, sIsoMatrixCoeffs, ARRAY_SIZE(sIsoMatrixCoeffs))) {
        colorAspect->mMatrixCoeffs = 2;
    }
    (colorAspect->mRange) = codecAspect->mRange == RangeFull ? 2 : 0;
}

OMX_BOOL colorAspectsDiffer(const OMX_COLORASPECTS *a, const OMX_COLORASPECTS *b)
{
    if (a->mRange != b->mRange
        || a->mPrimaries != b->mPrimaries
        || a->mTransfer != b->mTransfer
        || a->mMatrixCoeffs != b->mMatrixCoeffs) {
        return OMX_TRUE;
    }
    return OMX_FALSE;
}

OMX_U32 handleColorAspectsChange(const OMX_COLORASPECTS *a/*mDefaultColorAspects*/,
                                 const OMX_COLORASPECTS *b/*mBitstreamColorAspects*/,
                                 OMX_COLORASPECTS *c/*mFinalColorAspects*/,
                                 OMX_U32 perference)
{
    if (perference == kPreferBitstream) {
        updateFinalColorAspects(a, b, c);
    } else if (perference == kPreferContainer) {
        updateFinalColorAspects(b, a, c);
    } else {
        return OMX_ErrorUnsupportedSetting;
    }
    return OMX_TRUE;
}

void updateFinalColorAspects(const OMX_COLORASPECTS *otherAspects,
                             const OMX_COLORASPECTS *preferredAspects,
                             OMX_COLORASPECTS *mFinalColorAspects)
{
    OMX_COLORASPECTS newAspects;
    OMX_COLORASPECTS *pnewAspects = &newAspects;
    newAspects.mRange        = preferredAspects->mRange != RangeUnspecified ?
                               preferredAspects->mRange : otherAspects->mRange;
    newAspects.mPrimaries    = preferredAspects->mPrimaries != PrimariesUnspecified ?
                               preferredAspects->mPrimaries : otherAspects->mPrimaries;
    newAspects.mTransfer     = preferredAspects->mTransfer != TransferUnspecified ?
                               preferredAspects->mTransfer : otherAspects->mTransfer;
    newAspects.mMatrixCoeffs = preferredAspects->mMatrixCoeffs != MatrixUnspecified ?
                               preferredAspects->mMatrixCoeffs : otherAspects->mMatrixCoeffs;

    // Check to see if need update mFinalColorAspects.
    if (colorAspectsDiffer(mFinalColorAspects, pnewAspects)) {
        omx_info("updateFinalColorAspects");
        mFinalColorAspects->mRange = newAspects.mRange;
        mFinalColorAspects->mPrimaries = newAspects.mPrimaries;
        mFinalColorAspects->mTransfer = newAspects.mTransfer;
        mFinalColorAspects->mMatrixCoeffs = newAspects.mMatrixCoeffs;
    }
}
#ifdef __cplusplus
}
#endif
