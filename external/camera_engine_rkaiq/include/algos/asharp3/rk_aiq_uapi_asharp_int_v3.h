#ifndef _RK_AIQ_UAPI_ASHARP_INT_V3_H_
#define _RK_AIQ_UAPI_ASHARP_INT_V3_H_

#include "base/xcam_common.h"
#include "rk_aiq_algo_des.h"
#include "asharp3/rk_aiq_types_asharp_algo_int_v3.h"

// need_sync means the implementation should consider
// the thread synchronization
// if called by RkAiqAlscHandleInt, the sync has been done
// in framework. And if called by user app directly,
// sync should be done in inner. now we just need implement
// the case of need_sync == false; need_sync is for future usage.


XCamReturn
rk_aiq_uapi_asharpV3_SetAttrib(RkAiqAlgoContext *ctx,
                               rk_aiq_sharp_attrib_v3_t *attr,
                               bool need_sync);

XCamReturn
rk_aiq_uapi_asharpV3_GetAttrib(const RkAiqAlgoContext *ctx,
                               rk_aiq_sharp_attrib_v3_t *attr);

XCamReturn
rk_aiq_uapi_asharpV3_SetStrength(const RkAiqAlgoContext *ctx,
                                 float fPercent);

XCamReturn
rk_aiq_uapi_asharpV3_GetStrength(const RkAiqAlgoContext *ctx,
                                 float *pPercent);





#endif
