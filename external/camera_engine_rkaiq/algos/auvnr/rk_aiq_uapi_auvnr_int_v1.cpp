#include "auvnr/rk_aiq_uapi_auvnr_int_v1.h"
#include "auvnr/rk_aiq_types_auvnr_algo_prvt_v1.h"
#include "auvnr/rk_aiq_auvnr_algo_uvnr_v1.h"
#include "uvnr_xml2json_v1.h"



#if 1
#define UVNR_CHROMA_SF_STRENGTH_MAX_PERCENT (50.0)


XCamReturn
rk_aiq_uapi_auvnr_SetAttrib(RkAiqAlgoContext *ctx,
                            rk_aiq_uvnr_attrib_v1_t *attr,
                            bool need_sync)
{

    Auvnr_Context_V1_t* pCtx = (Auvnr_Context_V1_t*)ctx;

    pCtx->eMode = attr->eMode;
    pCtx->stAuto = attr->stAuto;
    pCtx->stManual = attr->stManual;

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_uapi_auvnr_GetAttrib(const RkAiqAlgoContext *ctx,
                            rk_aiq_uvnr_attrib_v1_t *attr)
{

    Auvnr_Context_V1_t* pCtx = (Auvnr_Context_V1_t*)ctx;

    attr->eMode = pCtx->eMode;
    memcpy(&attr->stAuto, &pCtx->stAuto, sizeof(Auvnr_Auto_Attr_V1_t));
    memcpy(&attr->stManual, &pCtx->stManual, sizeof(Auvnr_Manual_Attr_V1_t));

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
rk_aiq_uapi_auvnr_SetIQPara(RkAiqAlgoContext *ctx,
                            rk_aiq_uvnr_IQPara_v1_t *pPara,
                            bool need_sync)
{

    Auvnr_Context_V1_t* pCtx = (Auvnr_Context_V1_t*)ctx;

    CalibDb_UVNR_2_t calibdb_2;
    memset(&calibdb_2, 0x00, sizeof(calibdb_2));
    calibdb_2.mode_num = sizeof(pPara->stUvnrPara.mode_cell) / sizeof(CalibDb_UVNR_ModeCell_t);
    calibdb_2.mode_cell = (CalibDb_UVNR_ModeCell_t *)malloc(calibdb_2.mode_num * sizeof(CalibDb_UVNR_ModeCell_t));

    calibdb_2.enable = pPara->stUvnrPara.enable;
    memcpy(calibdb_2.version, pPara->stUvnrPara.version, sizeof(pPara->stUvnrPara.version));
    for(int i = 0; i < calibdb_2.mode_num; i++) {
        calibdb_2.mode_cell[i] = pPara->stUvnrPara.mode_cell[i];
    }
    pCtx->isIQParaUpdate = true;

#if(AUVNR_USE_JSON_FILE_V1)
    uvnrV1_calibdb_to_calibdbV2(&calibdb_2, &pCtx->uvnr_v1, 0);
#else
    pCtx->stUvnrCalib.enable = calibdb_2.enable;
    memcpy(pCtx->stUvnrCalib.version, calibdb_2.version, sizeof(pPara->stUvnrPara.version));
    for(int i = 0; i < calibdb_2.mode_num && pCtx->stUvnrCalib.mode_num; i++) {
        pCtx->stUvnrCalib.mode_cell[i] = calibdb_2.mode_cell[i];
    }
#endif

    free(calibdb_2.mode_cell);

    return XCAM_RETURN_NO_ERROR;
}


XCamReturn
rk_aiq_uapi_auvnr_GetIQPara(RkAiqAlgoContext *ctx,
                            rk_aiq_uvnr_IQPara_v1_t *pPara)
{

    Auvnr_Context_V1_t* pCtx = (Auvnr_Context_V1_t*)ctx;

    if(ctx != NULL && pPara != NULL) {
        CalibDb_UVNR_2_t calibdb_2;
        memset(&calibdb_2, 0x00, sizeof(calibdb_2));
        calibdb_2.mode_num = sizeof(pPara->stUvnrPara.mode_cell) / sizeof(CalibDb_UVNR_ModeCell_t);
        calibdb_2.mode_cell = (CalibDb_UVNR_ModeCell_t *)malloc(calibdb_2.mode_num * sizeof(CalibDb_UVNR_ModeCell_t));
#if(AUVNR_USE_JSON_FILE_V1)
        uvnrV1_calibdbV2_to_calibdb(&pCtx->uvnr_v1, &calibdb_2, 0);
#else
        calibdb_2.enable = pCtx->stUvnrCalib.enable;
        memcpy(calibdb_2.version, pCtx->stUvnrCalib.version, sizeof(pPara->stUvnrPara.version));
        for(int i = 0; i < calibdb_2.mode_num && i < pCtx->stUvnrCalib.mode_num; i++) {
            calibdb_2.mode_cell[i] = pCtx->stUvnrCalib.mode_cell[i];
        }
#endif

        memset(&pPara->stUvnrPara, 0x00, sizeof(CalibDb_UVNR_t));
        pPara->stUvnrPara.enable = calibdb_2.enable;
        memcpy(pPara->stUvnrPara.version, calibdb_2.version, sizeof(pPara->stUvnrPara.version));
        for(int i = 0; i < calibdb_2.mode_num && i < CALIBDB_MAX_MODE_NUM; i++) {
            pPara->stUvnrPara.mode_cell[i] = calibdb_2.mode_cell[i];
        }

        free(calibdb_2.mode_cell);
    }
    return XCAM_RETURN_NO_ERROR;
}


XCamReturn
rk_aiq_uapi_auvnr_SetChromaSFStrength(const RkAiqAlgoContext *ctx,
                                      float fPercent)
{
    Auvnr_Context_V1_t* pCtx = (Auvnr_Context_V1_t*)ctx;

    float fStrength = 1.0f;
    float fMax = UVNR_CHROMA_SF_STRENGTH_MAX_PERCENT;

    if(fPercent <= 0.5) {
        fStrength =  fPercent / 0.5;
    } else {
        fStrength = (fPercent - 0.5) * (fMax - 1) * 2 + 1;
    }

    pCtx->fChrom_SF_Strength = fStrength;

    return XCAM_RETURN_NO_ERROR;
}



XCamReturn
rk_aiq_uapi_auvnr_GetChromaSFStrength(const RkAiqAlgoContext *ctx,
                                      float *pPercent)
{
    Auvnr_Context_V1_t* pCtx = (Auvnr_Context_V1_t*)ctx;

    float fStrength = 1.0f;
    float fMax = UVNR_CHROMA_SF_STRENGTH_MAX_PERCENT;

    fStrength = pCtx->fChrom_SF_Strength;


    if(fStrength <= 1) {
        *pPercent = fStrength * 0.5;
    } else {
        *pPercent = (fStrength - 1) / ((fMax - 1) * 2) + 0.5;
    }


    return XCAM_RETURN_NO_ERROR;
}


XCamReturn
rk_aiq_uapi_auvnr_SetJsonPara(RkAiqAlgoContext *ctx,
                              rk_aiq_uvnr_JsonPara_v1_t *pPara,
                              bool need_sync)
{

    Auvnr_Context_V1_t* pCtx = (Auvnr_Context_V1_t*)ctx;

    uvnr_calibdbV2_assign_v1(&pCtx->uvnr_v1, &pPara->uvnr_v1);
    pCtx->isIQParaUpdate = true;

    return XCAM_RETURN_NO_ERROR;
}



XCamReturn
rk_aiq_uapi_auvnr_GetJsonPara(RkAiqAlgoContext *ctx,
                              rk_aiq_uvnr_JsonPara_v1_t *pPara)
{

    Auvnr_Context_V1_t* pCtx = (Auvnr_Context_V1_t*)ctx;

    uvnr_calibdbV2_assign_v1(&pPara->uvnr_v1, &pCtx->uvnr_v1);

    return XCAM_RETURN_NO_ERROR;
}


#endif

