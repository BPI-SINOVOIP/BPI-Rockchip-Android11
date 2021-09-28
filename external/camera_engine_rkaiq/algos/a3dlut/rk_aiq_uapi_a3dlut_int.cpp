#include "rk_aiq_uapi_a3dlut_int.h"
#include "a3dlut/rk_aiq_types_a3dlut_algo_prvt.h"
XCamReturn
rk_aiq_uapi_a3dlut_SetAttrib(RkAiqAlgoContext *ctx,
                             rk_aiq_lut3d_attrib_t attr,
                             bool need_sync)
{

    alut3d_context_t* alut3d_context = (alut3d_context_t*)ctx->a3dlut_para;
    alut3d_context->mNewAtt = attr;
    alut3d_context->updateAtt = true;

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_uapi_a3dlut_GetAttrib(const RkAiqAlgoContext *ctx,
                             rk_aiq_lut3d_attrib_t *attr)
{

    alut3d_context_t* alut3d_context = (alut3d_context_t*)ctx->a3dlut_para;

    memcpy(attr, &alut3d_context->mCurAtt, sizeof(rk_aiq_lut3d_attrib_t));

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_uapi_a3dlut_Query3dlutInfo(const RkAiqAlgoContext *ctx,
                                  rk_aiq_lut3d_querry_info_t *lut3d_querry_info )
{

    alut3d_context_t* alut3d_context = (alut3d_context_t*)ctx->a3dlut_para;
    lut3d_querry_info->alpha = alut3d_context->restinfo.alpha;
    strcpy(lut3d_querry_info->name, alut3d_context->restinfo.pLutProfile->name);
    memcpy(lut3d_querry_info->look_up_table_b, alut3d_context->lut3d_hw_conf.look_up_table_b,
           sizeof(lut3d_querry_info->look_up_table_b));
    memcpy(lut3d_querry_info->look_up_table_g, alut3d_context->lut3d_hw_conf.look_up_table_g,
           sizeof(lut3d_querry_info->look_up_table_g));
    memcpy(lut3d_querry_info->look_up_table_r, alut3d_context->lut3d_hw_conf.look_up_table_r,
           sizeof(lut3d_querry_info->look_up_table_r));
    lut3d_querry_info->lut3d_en = alut3d_context->lut3d_hw_conf.enable;

    return XCAM_RETURN_NO_ERROR;
}


