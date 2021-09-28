#ifndef _RK_AIQ_UAPI_AYNR_INT_V2_H_
#define _RK_AIQ_UAPI_AYNR_INT_V2_H_

#include "base/xcam_common.h"
#include "rk_aiq_algo_des.h"
#include "aynr2/rk_aiq_types_aynr_algo_int_v2.h"

// need_sync means the implementation should consider
// the thread synchronization
// if called by RkAiqAlscHandleInt, the sync has been done
// in framework. And if called by user app directly,
// sync should be done in inner. now we just need implement
// the case of need_sync == false; need_sync is for future usage.


XCamReturn
rk_aiq_uapi_aynrV2_SetAttrib(RkAiqAlgoContext *ctx,
                             rk_aiq_ynr_attrib_v2_t *attr,
                             bool need_sync);

XCamReturn
rk_aiq_uapi_aynrV2_GetAttrib(const RkAiqAlgoContext *ctx,
                             rk_aiq_ynr_attrib_v2_t *attr);

XCamReturn
rk_aiq_uapi_aynrV2_SetLumaSFStrength(const RkAiqAlgoContext *ctx,
                                     float fPercent);

XCamReturn
rk_aiq_uapi_aynrV2_GetLumaSFStrength(const RkAiqAlgoContext *ctx,
                                     float *pPercent);





#endif
