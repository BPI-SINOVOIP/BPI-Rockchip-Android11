/******************************************************************************
 *
 * Copyright 2019, Fuzhou Rockchip Electronics Co.Ltd. All rights reserved.
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of:
 * Fuzhou Rockchip Electronics Co.Ltd .
 *
 *
 *****************************************************************************/
/**
 * @file    TuingServer.cpp
 *
 *****************************************************************************/

#define LOG_TAG "TuningServer"

#include <memory>
#include <Utils.h>
#include <vector>
#include <mutex>
#include <array>
#include <dlfcn.h>
#include <utils/Errors.h>
#include <pthread.h>
#include <string>
#include <sys/prctl.h>
#include <string.h>
#include "LogHelper.h"
#include "TuningServer.h"
#include "ControlUnit.h"
#include "GraphConfig.h"
#include "FormatUtils.h"
//#define  DEBUG_ON
using GCSS::GraphConfigNode;
namespace android {
namespace camera2 {
namespace gcu = graphconfig::utils;
TuningServer::TuningServer():mMainTh(this),mCmdTh(this){
    mTuningMode = false;
    bExpCmdCap = false;
    bExpCmdSet = false;
    mStartCapture = false;
    uvcExpTime = 30 * 1e6;
    uvcSensitivity = 100;
    uvcAeMode = 0;

    mCapRawNum = 0;
    mCurGain = 0.0f;
    mCurTime = 0.0f;
    mSkipFrame = 0;

    mBlsGetOn = false;
    mBlsSetOn = false;
    mBlsEnable = false;
    mLscGetOn = false;
    mLscSetOn = false;
    mLscEnable = false;
    mAwbCcmGetOn = false;
    mAwbCcmSetOn = false;
    mCcmEnable = false;
    mAwbGetOn = false;
    mAwbSetOn = false;
    mAwbEnable = false;
    mAwbWpGetOn = false;
    mAwbWpSetOn = false;
    mAwbCurGetOn = false;
    mAwbCurSetOn = false;
    mAwbRefGainGetOn = false;
    mAwbRefGainSetOn = false;
    mGocGetOn = false;
    mGocSetOn = false;
    mGocEnable = false;
    mCprocGetOn = false;
    mCprocSetOn = false;
    mCprocEnable = false;
    mDpfGetOn = false;
    mDpfSetOn = false;
    mFltSetOn = false;
    mFltGetOn = false;
    mSensorInfoOn = false;
    mSysInfoOn = false;
    mExpSetOn = false;
    mCapReqOn = false;
    mRestartOn = false;
    mProtocolOn = false;
    moduleEnabled = NULL;
    mLibUvcApp = NULL;
}

TuningServer::~TuningServer(){
}

void TuningServer::init(ControlUnit *pCu, RKISP1CameraHw* pCh, int camId)
{
    char prop_adb[PROPERTY_VALUE_MAX];
    char prop_uvc[PROPERTY_VALUE_MAX];

    property_get("sys.camera.uvc", prop_uvc, "0");
    if(strcmp(prop_uvc, "1") == 0) {
        mLibUvcApp = dlopen("libuvcapp.so", RTLD_NOLOAD);
        if (mLibUvcApp == NULL){
            mLibUvcApp = dlopen("libuvcapp.so", RTLD_NOW);
        }

        if (mLibUvcApp == NULL)
        {
            LOGE("open libuvcapp fail");
            return;
        }

        mUvc_vpu_ops = (uvc_vpu_ops_t *)dlsym(mLibUvcApp, "uvc_vpu_ops");
        if(mUvc_vpu_ops == NULL){
            LOGE("%s(%d):get symbol fail,%s",__FUNCTION__,__LINE__,dlerror());
            return;
        }

        mUvc_proc_ops = (uvc_proc_ops_t *)dlsym(mLibUvcApp, "uvc_proc_ops");
        if(mUvc_proc_ops == NULL){
            LOGE("%s(%d):get symbol fail,%s",__FUNCTION__,__LINE__,dlerror());
            return;
        }

        uint32_t uvc_version = mUvc_proc_ops->uvc_getVersion();
        if (uvc_version != UVC_HAL_VERSION)
        {
            LOGE("\n\n\nversion(%#x.%#x.%#x) in uvcApp library is not same with tuningServer(%#x.%#x.%#x)!\n\n\n",
                (uvc_version>>16)&0xff, (uvc_version>>8)&0xff,(uvc_version)&0xff,
                (UVC_HAL_VERSION>>16)&0xff, (UVC_HAL_VERSION>>8)&0xff,(UVC_HAL_VERSION)&0xff);
            return;
        }

        property_get("sys.usb.config", prop_adb, "adb");
        if(strcmp(prop_adb, "uvc,adb")){
            property_set("sys.usb.config", "none");
            usleep(300000);
            property_set("sys.usb.config", "uvc,adb");
        }
        mCtrlUnit = pCu;
        mCamHw = pCh;
        mCamId = camId;
        mMainTh.run();
        mCmdTh.run();
        mTuningMode = true;
    }

}

void TuningServer::deinit()
{
    if(mTuningMode){
        mMainTh.exit();
        mCmdTh.exit();
        mTuningMode = false;
        dlclose(mLibUvcApp);
        mLibUvcApp = NULL;
        property_set("sys.usb.config", "none");
        usleep(300000);
        property_set("sys.usb.config", "adb");
    }
}

void TuningServer::startCaptureRaw(int w, int h)
{
    mStartCapture = true;
    if(mCamHw)
        mCamHw->sendTuningDumpCmd(w, h);
}

void TuningServer::stopCatureRaw()
{
    mStartCapture = false;
    if(mCamHw)
        mCamHw->sendTuningDumpCmd(0, 0);
}

bool TuningServer::isControledByTuningServer()
{
    if(bExpCmdCap||bExpCmdSet||mBlsSetOn||mLscSetOn||mAwbCcmSetOn||mAwbSetOn||mAwbWpSetOn||mAwbCurSetOn||
       mAwbRefGainSetOn||mGocSetOn||mCprocSetOn||mDpfSetOn||mFltSetOn||mExpSetOn||mCapReqOn||mRestartOn){
       return true;
    }else{
        return false;
    }
}

void TuningServer::set_cap_req(CameraMetadata &uvcCamMeta)
{
    uint8_t aeMode;
    if(mCapReqOn){
        mCapReqOn = false;
        bExpCmdCap = true;
        uvcExpTime = (int64_t)(((float)mPtrCapReq->exp_time_h + mPtrCapReq->exp_time_l / 256.0) * 1e6);
        uvcSensitivity = (int32_t)(((float)mPtrCapReq->exp_gain_h + mPtrCapReq->exp_gain_l / 256.0) * 100);

        if(mPtrCapReq->ae_mode == HAL_ISP_AE_MANUAL)
            uvcAeMode = ANDROID_CONTROL_AE_MODE_OFF;
        else
            uvcAeMode = ANDROID_CONTROL_AE_MODE_ON;
        mSkipFrame = 10;
        mCapRawNum = mPtrCapReq->cap_num;
        LOGD("CMD_SET_CAPS:%dx%d,%d,%lld",mPtrCapReq->cap_width, mPtrCapReq->cap_height,uvcSensitivity,uvcExpTime);
    }
    if(bExpCmdCap){
        if(uvcAeMode == HAL_ISP_AE_MANUAL)
            aeMode = ANDROID_CONTROL_AE_MODE_OFF;
        else
            aeMode = ANDROID_CONTROL_AE_MODE_ON;
        uvcCamMeta.update(ANDROID_SENSOR_SENSITIVITY, &uvcSensitivity, 1);
        uvcCamMeta.update(ANDROID_CONTROL_AE_MODE, &aeMode, 1);
        uvcCamMeta.update(ANDROID_SENSOR_EXPOSURE_TIME, &uvcExpTime, 1);
	    if(mSkipFrame > 0){
		   mSkipFrame--;
		}else if(mSkipFrame == 0) {
		   bExpCmdCap = false;
		   startCaptureRaw(mPtrCapReq->cap_width, mPtrCapReq->cap_height);
		}
    }
}

void TuningServer::get_exposure(CameraMetadata &uvcCamMeta)
{
    camera_metadata_entry entry;
    entry = uvcCamMeta.find(ANDROID_SENSOR_SENSITIVITY);
    if (!entry.count)
        return;
    float gain = (float)(entry.data.i32[0] / 100.0f);
    if (mCurGain != gain)
        mCurGain = gain;
    entry = uvcCamMeta.find(ANDROID_SENSOR_EXPOSURE_TIME);
    if (!entry.count)
        return;
    float time = (float)(entry.data.i64[0] / 1000000.0f);

    if (mCurTime != time)
        mCurTime = time;
    LOGD("%s: now gain&time:%f,%f",__FUNCTION__, mCurGain,mCurTime);
}

void TuningServer::set_exposure(CameraMetadata &uvcCamMeta)
{
    uint8_t aeMode;
    if(mExpSetOn){
        uvcExpTime = (int64_t)(((float)mPtrExp->exp_time_h + mPtrExp->exp_time_l / 256.0) * 1e6);
        uvcSensitivity = (int32_t)(((float)mPtrExp->exp_gain_h + mPtrExp->exp_gain_l / 256.0) * 100);
        uvcAeMode = mPtrExp->ae_mode;
        bExpCmdSet = true;
        mExpSetOn = false;
        if(mMsgType == CMD_TYPE_SYNC){
            mUvc_proc_ops->uvc_signal();
        }
    }
    if(bExpCmdSet){
        if(uvcAeMode == HAL_ISP_AE_MANUAL){
            LOGD("expgain=%d",uvcSensitivity);
            uvcCamMeta.update(ANDROID_SENSOR_SENSITIVITY, &uvcSensitivity, 1);
            aeMode = ANDROID_CONTROL_AE_MODE_OFF;
            uvcCamMeta.update(ANDROID_CONTROL_AE_MODE, &aeMode, 1);
            LOGD("exptime=%ld",uvcExpTime);
            uvcCamMeta.update(ANDROID_SENSOR_EXPOSURE_TIME, &uvcExpTime, 1);
            bExpCmdSet = true;
        }else{
            aeMode = ANDROID_CONTROL_AE_MODE_ON;
            uvcCamMeta.update(ANDROID_CONTROL_AE_MODE, &aeMode, 1);
            bExpCmdSet = false;
        }
    }
}

void TuningServer::get_bls(CameraMetadata &uvcCamMeta)
{
    #ifdef DEBUG_ON
    if (1) {
        struct HAL_ISP_bls_cfg_s temp;
        mPtrBls = &temp;
    #else
    if (mBlsGetOn) {
    #endif
        mBlsGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_BLS);
        if (!entry.count){
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            if(moduleEnabled)
               *moduleEnabled = *pchr;
            pchr++;
            mPtrBls->mode = (enum HAL_BLS_MODE)*pchr;
            pchr++;
            if (*pchr == 1){
                mPtrBls->win_cfg = HAL_BLS_WINCFG_WIN1;
                pchr++;
            }else if(*pchr == 2){
                mPtrBls->win_cfg = HAL_BLS_WINCFG_WIN2;
                pchr++;
            }else if(*pchr == 3){
                mPtrBls->win_cfg = HAL_BLS_WINCFG_WIN1_2;
                pchr++;
            }else{
                mPtrBls->win_cfg = HAL_BLS_WINCFG_OFF;
                pchr++;
            }
            memcpy(&mPtrBls->win1, pchr, 8);
            pchr += 8;
            memcpy(&mPtrBls->win2, pchr, 8);
            pchr += 8;
            mPtrBls->samples = *pchr;
            pchr++;
            memcpy(&mPtrBls->fixed_blue, pchr, 2);
            pchr += 2;
            memcpy(&mPtrBls->fixed_greenB, pchr, 2);
            pchr += 2;
            memcpy(&mPtrBls->fixed_greenR, pchr, 2);
            pchr += 2;
            memcpy(&mPtrBls->fixed_red, pchr, 2);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_bls(CameraMetadata &uvcCamMeta)
{
    uint8_t blc_param[30];
    uint8_t *pbuf;

    if(mBlsSetOn){
        mBlsSetOn = false;
        pbuf = blc_param;
        blc_param[0] = (uint8_t)mBlsEnable;//enbale?
        pbuf++;
        blc_param[1] = (uint8_t)mPtrBls->mode;
        pbuf++;
        blc_param[2] = (uint8_t)mPtrBls->win_cfg;
        pbuf++;

        memcpy(pbuf, &mPtrBls->win1.h_offs, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->win1.v_offs, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->win1.width, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->win1.height, 2);
        pbuf += 2;

        memcpy(pbuf, &mPtrBls->win2.h_offs, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->win2.v_offs, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->win2.width, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->win2.height, 2);
        pbuf += 2;

        *pbuf = mPtrBls->samples;
        pbuf++;
        memcpy(pbuf, &mPtrBls->fixed_red,2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->fixed_greenR,2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->fixed_greenB,2);
        pbuf += 2;
        memcpy(pbuf, &mPtrBls->fixed_blue,2);
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_BLS_SET, (uint8_t*)blc_param, sizeof(blc_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::get_lsc(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if (1){
        struct HAL_ISP_Lsc_Profile_s Lsc;
        struct HAL_ISP_Lsc_Query_s LscQuery;
        mPtrLsc = &Lsc;
        mPtrLscQuery = &LscQuery;
#else
    if (mLscGetOn){
#endif
        mLscGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_LSC_GET);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            if(moduleEnabled)
                *moduleEnabled = entry.data.u8[0];
            pchr++;
            memcpy(mPtrLscQuery->LscNameUp, pchr, sizeof(mPtrLscQuery->LscNameUp));
            pchr += sizeof(mPtrLscQuery->LscNameUp);
            memcpy(mPtrLscQuery->LscNameDn, pchr, sizeof(mPtrLscQuery->LscNameDn));
            pchr += sizeof(mPtrLscQuery->LscNameDn);
            memcpy(&mPtrLsc->LscSectors, pchr, sizeof(mPtrLsc->LscSectors));
            pchr += sizeof(mPtrLsc->LscSectors);
            memcpy(&mPtrLsc->LscNo, pchr, sizeof(mPtrLsc->LscNo));
            pchr += sizeof(mPtrLsc->LscNo);
            memcpy(&mPtrLsc->LscXo, pchr, sizeof(mPtrLsc->LscXo));
            pchr += sizeof(mPtrLsc->LscXo);
            memcpy(&mPtrLsc->LscYo, pchr, sizeof(mPtrLsc->LscYo));
            pchr += sizeof(mPtrLsc->LscYo);
            memcpy(&mPtrLsc->LscXSizeTbl, pchr, sizeof(mPtrLsc->LscXSizeTbl));
            pchr += sizeof(mPtrLsc->LscXSizeTbl);
            memcpy(&mPtrLsc->LscYSizeTbl, pchr, sizeof(mPtrLsc->LscYSizeTbl));
            pchr += sizeof(mPtrLsc->LscYSizeTbl);
            memcpy(&mPtrLsc->LscMatrix, pchr, sizeof(mPtrLsc->LscMatrix));
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }

}

void TuningServer::set_lsc(CameraMetadata &uvcCamMeta)
{
    uint8_t lsc_param[2380];
    uint8_t *pbuf;

    if (mLscSetOn) {
        mLscSetOn = false;
        pbuf = lsc_param;
        lsc_param[0] = (uint8_t)mLscEnable;
        pbuf++;
        memcpy(pbuf, mPtrLsc->LscName, sizeof(mPtrLsc->LscName));
        pbuf += sizeof(mPtrLsc->LscName);
        memcpy(pbuf, &mPtrLsc->LscSectors, sizeof(mPtrLsc->LscSectors));
        pbuf += sizeof(mPtrLsc->LscSectors);
        memcpy(pbuf, &mPtrLsc->LscNo, sizeof(mPtrLsc->LscNo));
        pbuf += sizeof(mPtrLsc->LscNo);
        memcpy(pbuf, &mPtrLsc->LscXo, sizeof(mPtrLsc->LscXo));
        pbuf += sizeof(mPtrLsc->LscXo);
        memcpy(pbuf, &mPtrLsc->LscYo, sizeof(mPtrLsc->LscYo));
        pbuf += sizeof(mPtrLsc->LscYo);
        memcpy(pbuf, mPtrLsc->LscXSizeTbl, sizeof(mPtrLsc->LscXSizeTbl));
        pbuf += sizeof(mPtrLsc->LscXSizeTbl);
        memcpy(pbuf, mPtrLsc->LscYSizeTbl, sizeof(mPtrLsc->LscYSizeTbl));
        pbuf += sizeof(mPtrLsc->LscYSizeTbl);
        memcpy(pbuf, mPtrLsc->LscMatrix, sizeof(mPtrLsc->LscMatrix));
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_LSC_SET, (uint8_t*)lsc_param, sizeof(lsc_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }

}

void TuningServer::get_ccm(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if(1){
    struct HAL_ISP_AWB_CCM_GET_s temp;
    mPtrAwbCcmGet = &temp;
#else
    if(mAwbCcmGetOn){
#endif
        mAwbCcmGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_CCM_GET);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;

            pchr = &entry.data.u8[0];
            if(moduleEnabled)
                *moduleEnabled = entry.data.u8[0];
            pchr++;
            memcpy(mPtrAwbCcmGet->name_up, pchr, 20);
            pchr += 20;
            memcpy(mPtrAwbCcmGet->name_dn, pchr, 20);
            pchr += 20;
            #if 0
            memcpy(&mPtrAwbCcmGet->coeff00,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->coeff01,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->coeff02,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->coeff10,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->coeff11,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->coeff12,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->coeff20,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->coeff21,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->coeff22,pchr,4);
            pchr += 4;
            #else
            memcpy(mPtrAwbCcmGet->coeff, pchr, sizeof(mPtrAwbCcmGet->coeff));
            pchr += sizeof(mPtrAwbCcmGet->coeff);
            #endif
            memcpy(&mPtrAwbCcmGet->ct_offset_r,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->ct_offset_g,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCcmGet->ct_offset_b,pchr,4);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_ccm(CameraMetadata &uvcCamMeta)
{
    uint8_t ccm_param[70];
     uint8_t *pbuf;

    if (mAwbCcmSetOn) {
        mAwbCcmSetOn = false;
        pbuf = ccm_param;
        ccm_param[0] = (uint8_t)mCcmEnable;
        pbuf++;
        memcpy(pbuf, mPtrAwbCcmSet->ill_name, sizeof(mPtrAwbCcmSet->ill_name));
        pbuf += sizeof(mPtrAwbCcmSet->ill_name);
        #if 0
        memcpy(pbuf, &mPtrAwbCcmSet->coeff00, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->coeff01, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->coeff02, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->coeff10, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->coeff11, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->coeff12, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->coeff20, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->coeff21, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->coeff22, 4);
        pbuf += 4;
        #else
        memcpy(pbuf, mPtrAwbCcmSet->coeff, sizeof(mPtrAwbCcmSet->coeff));
        pbuf += sizeof(mPtrAwbCcmSet->coeff);
        #endif
        memcpy(pbuf, &mPtrAwbCcmSet->ct_offset_r, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->ct_offset_g, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCcmSet->ct_offset_b, 4);
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_CCM_SET, (uint8_t*)ccm_param, sizeof(ccm_param));

        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }

}

void TuningServer::get_awb(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if(1) {
        struct HAL_ISP_AWB_s Awb;
        mPtrAwb = &Awb;
#else
    if(mAwbGetOn) {
#endif
        mAwbGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_AWB_GET);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            if(moduleEnabled)
                *moduleEnabled = entry.data.u8[0];
            pchr++;
            memcpy(&mPtrAwb->r_gain,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwb->gr_gain,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwb->gb_gain,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwb->b_gain,pchr,4);
            pchr += 4;
            mPtrAwb->lock_ill = *pchr;//locked
            pchr++;
            memcpy(mPtrAwb->ill_name, pchr, 20);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_awb(CameraMetadata &uvcCamMeta)
{
    uint8_t awb_param[40];
     uint8_t *pbuf;

    if (mAwbSetOn) {
        mAwbSetOn = false;
        pbuf = awb_param;
        awb_param[0] = (uint8_t)mAwbEnable;
        pbuf++;
        memcpy(pbuf, &mPtrAwb->r_gain, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwb->gr_gain, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwb->gb_gain, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwb->b_gain, 4);
        pbuf += 4;
        *pbuf = mPtrAwb->lock_ill;
        pbuf++;
        memcpy(pbuf, mPtrAwb->ill_name, sizeof(mPtrAwb->ill_name));
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_AWB_SET, (uint8_t*)awb_param, sizeof(awb_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }

}

void TuningServer::get_awb_wp(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if(1) {
        struct HAL_ISP_AWB_White_Point_Get_s AwbWpGet;
        mPtrAwbWpGet = &AwbWpGet;
#else
    if(mAwbWpGetOn) {
#endif
        mAwbWpGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_AWB_WP);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            memcpy(&mPtrAwbWpGet->win_h_offs,pchr,2);
            pchr += 2;
            memcpy(&mPtrAwbWpGet->win_v_offs,pchr,2);
            pchr += 2;
            memcpy(&mPtrAwbWpGet->win_width,pchr,2);
            pchr += 2;
            memcpy(&mPtrAwbWpGet->win_height,pchr,2);
            pchr += 2;
            mPtrAwbWpGet->awb_mode = *pchr++;
            memcpy(&mPtrAwbWpGet->cnt, pchr, 4);
            pchr += 4;
            mPtrAwbWpGet->mean_y = *pchr++;
            mPtrAwbWpGet->mean_cb = *pchr++;
            mPtrAwbWpGet->mean_cr = *pchr++;
            memcpy(&mPtrAwbWpGet->mean_r, pchr, 2);
            pchr += 2;
            memcpy(&mPtrAwbWpGet->mean_b, pchr, 2);
            pchr += 2;
            memcpy(&mPtrAwbWpGet->mean_g, pchr, 2);
            pchr += 2;
            mPtrAwbWpGet->RefCr = *pchr++;
            mPtrAwbWpGet->RefCb = *pchr++;
            mPtrAwbWpGet->MinY = *pchr++;
            mPtrAwbWpGet->MaxY = *pchr++;
            mPtrAwbWpGet->MinC = *pchr++;
            mPtrAwbWpGet->MaxCSum = *pchr++;
            memcpy(&mPtrAwbWpGet->RgProjection,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbWpGet->RegionSize,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbWpGet->Rg_clipped,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbWpGet->Rg_unclipped,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbWpGet->Bg_clipped,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbWpGet->Bg_unclipped,pchr,4);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_awb_wp(CameraMetadata &uvcCamMeta)
{
    uint8_t awb_wp_param[440];
     uint8_t *pbuf;

    if (mAwbWpSetOn) {
        mAwbWpSetOn = false;
        pbuf = awb_wp_param;
        memcpy(pbuf, &mPtrAwbWpSet->win_h_offs, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrAwbWpSet->win_v_offs, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrAwbWpSet->win_width, 2);
        pbuf += 2;
        memcpy(pbuf, &mPtrAwbWpSet->win_height, 2);
        pbuf += 2;
        *pbuf++ = mPtrAwbWpSet->awb_mode;
        #if 1//awb_v11
        memcpy(pbuf, mPtrAwbWpSet->afFade, sizeof(mPtrAwbWpSet->afFade));
        pbuf += sizeof(mPtrAwbWpSet->afFade);
        memcpy(pbuf, mPtrAwbWpSet->afmaxCSum_br, sizeof(mPtrAwbWpSet->afmaxCSum_br));
        pbuf += sizeof(mPtrAwbWpSet->afmaxCSum_br);
        memcpy(pbuf, mPtrAwbWpSet->afmaxCSum_sr, sizeof(mPtrAwbWpSet->afmaxCSum_sr));
        pbuf += sizeof(mPtrAwbWpSet->afmaxCSum_sr);
        memcpy(pbuf, mPtrAwbWpSet->afminC_br, sizeof(mPtrAwbWpSet->afminC_br));
        pbuf += sizeof(mPtrAwbWpSet->afminC_br);
        memcpy(pbuf, mPtrAwbWpSet->afminC_sr, sizeof(mPtrAwbWpSet->afminC_sr));
        pbuf += sizeof(mPtrAwbWpSet->afminC_sr);
        memcpy(pbuf, mPtrAwbWpSet->afMaxY_br, sizeof(mPtrAwbWpSet->afMaxY_br));
        pbuf += sizeof(mPtrAwbWpSet->afMaxY_br);
        memcpy(pbuf, mPtrAwbWpSet->afMaxY_sr, sizeof(mPtrAwbWpSet->afMaxY_sr));
        pbuf += sizeof(mPtrAwbWpSet->afMaxY_sr);
        memcpy(pbuf, mPtrAwbWpSet->afMinY_br, sizeof(mPtrAwbWpSet->afMinY_br));
        pbuf += sizeof(mPtrAwbWpSet->afMinY_br);
        memcpy(pbuf, mPtrAwbWpSet->afMinY_sr, sizeof(mPtrAwbWpSet->afMinY_sr));
        pbuf += sizeof(mPtrAwbWpSet->afMinY_sr);
        memcpy(pbuf, mPtrAwbWpSet->afRefCb, sizeof(mPtrAwbWpSet->afRefCb));
        pbuf += sizeof(mPtrAwbWpSet->afRefCb);
        memcpy(pbuf, mPtrAwbWpSet->afRefCr, sizeof(mPtrAwbWpSet->afRefCr));
        pbuf += sizeof(mPtrAwbWpSet->afRefCr);
        #else//awb_10
        memcpy(pbuf, mPtrAwbWpSet->afFade, sizeof(mPtrAwbWpSet->afFade));
        pbuf += sizeof(mPtrAwbWpSet->afFade);
        memcpy(pbuf, mPtrAwbWpSet->afCbMinRegionMax, sizeof(mPtrAwbWpSet->afCbMinRegionMax));
        pbuf += sizeof(mPtrAwbWpSet->afCbMinRegionMax);
        memcpy(pbuf, mPtrAwbWpSet->afCrMinRegionMax, sizeof(mPtrAwbWpSet->afCrMinRegionMax));
        pbuf += sizeof(mPtrAwbWpSet->afCrMinRegionMax);
        memcpy(pbuf, mPtrAwbWpSet->afMaxCSumRegionMax, sizeof(mPtrAwbWpSet->afMaxCSumRegionMax));
        pbuf += sizeof(mPtrAwbWpSet->afMaxCSumRegionMax);
        memcpy(pbuf, mPtrAwbWpSet->afCbMinRegionMin, sizeof(mPtrAwbWpSet->afCbMinRegionMin));
        pbuf += sizeof(mPtrAwbWpSet->afCbMinRegionMin);
        memcpy(pbuf, mPtrAwbWpSet->afCrMinRegionMin, sizeof(mPtrAwbWpSet->afCrMinRegionMin));
        pbuf += sizeof(mPtrAwbWpSet->afCrMinRegionMin);
        memcpy(pbuf, mPtrAwbWpSet->afMaxCSumRegionMin, sizeof(mPtrAwbWpSet->afMaxCSumRegionMin));
        pbuf += sizeof(mPtrAwbWpSet->afMaxCSumRegionMin);
        memcpy(pbuf, mPtrAwbWpSet->afMinCRegionMax, sizeof(mPtrAwbWpSet->afMinCRegionMax));
        pbuf += sizeof(mPtrAwbWpSet->afMinCRegionMax);
        memcpy(pbuf, mPtrAwbWpSet->afMinCRegionMin, sizeof(mPtrAwbWpSet->afMinCRegionMin));
        pbuf += sizeof(mPtrAwbWpSet->afMinCRegionMin);
        memcpy(pbuf, mPtrAwbWpSet->afMaxYRegionMax, sizeof(mPtrAwbWpSet->afMaxYRegionMax));
        pbuf += sizeof(mPtrAwbWpSet->afMaxYRegionMax);
        memcpy(pbuf, mPtrAwbWpSet->afMaxYRegionMin, sizeof(mPtrAwbWpSet->afMaxYRegionMin));
        pbuf += sizeof(mPtrAwbWpSet->afMaxYRegionMin);
        memcpy(pbuf, mPtrAwbWpSet->afMinYMaxGRegionMax, sizeof(mPtrAwbWpSet->afMinYMaxGRegionMax));
        pbuf += sizeof(mPtrAwbWpSet->afMinYMaxGRegionMax);
        memcpy(pbuf, mPtrAwbWpSet->afMinYMaxGRegionMin, sizeof(mPtrAwbWpSet->afMinYMaxGRegionMin));
        pbuf += sizeof(mPtrAwbWpSet->afMinYMaxGRegionMin);
        memcpy(pbuf, mPtrAwbWpSet->afRefCb, sizeof(mPtrAwbWpSet->afRefCb));
        pbuf += sizeof(mPtrAwbWpSet->afRefCb);
        memcpy(pbuf, mPtrAwbWpSet->afRefCr, sizeof(mPtrAwbWpSet->afRefCr));
        pbuf += sizeof(mPtrAwbWpSet->afRefCr);
        #endif
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjIndoorMin, sizeof(mPtrAwbWpSet->fRgProjIndoorMin));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjIndoorMin);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjOutdoorMin, sizeof(mPtrAwbWpSet->fRgProjOutdoorMin));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjOutdoorMin);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjMax, sizeof(mPtrAwbWpSet->fRgProjMax));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjMax);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjMaxSky, sizeof(mPtrAwbWpSet->fRgProjMaxSky));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjMaxSky);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjALimit, sizeof(mPtrAwbWpSet->fRgProjALimit));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjALimit);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjAWeight, sizeof(mPtrAwbWpSet->fRgProjAWeight));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjAWeight);
        memcpy(pbuf,& mPtrAwbWpSet->fRgProjYellowLimitEnable, sizeof(mPtrAwbWpSet->fRgProjYellowLimitEnable));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjYellowLimitEnable);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjYellowLimit, sizeof(mPtrAwbWpSet->fRgProjYellowLimit));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjYellowLimit);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjIllToCwfEnable, sizeof(mPtrAwbWpSet->fRgProjIllToCwfEnable));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjIllToCwfEnable);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjIllToCwf, sizeof(mPtrAwbWpSet->fRgProjIllToCwf));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjIllToCwf);
        memcpy(pbuf, &mPtrAwbWpSet->fRgProjIllToCwfWeight, sizeof(mPtrAwbWpSet->fRgProjIllToCwfWeight));
        pbuf += sizeof(mPtrAwbWpSet->fRgProjIllToCwfWeight);
        memcpy(pbuf, &mPtrAwbWpSet->fRegionSize, sizeof(mPtrAwbWpSet->fRegionSize));
        pbuf += sizeof(mPtrAwbWpSet->fRegionSize);
        memcpy(pbuf, &mPtrAwbWpSet->fRegionSizeInc, sizeof(mPtrAwbWpSet->fRegionSizeInc));
        pbuf += sizeof(mPtrAwbWpSet->fRegionSizeInc);
        memcpy(pbuf, &mPtrAwbWpSet->fRegionSizeDec, sizeof(mPtrAwbWpSet->fRegionSizeDec));
        pbuf += sizeof(mPtrAwbWpSet->fRegionSizeDec);
        memcpy(pbuf, &mPtrAwbWpSet->cnt, sizeof(mPtrAwbWpSet->cnt));
        pbuf += sizeof(mPtrAwbWpSet->cnt);
        *pbuf = mPtrAwbWpSet->mean_y;
        pbuf++;
        *pbuf = mPtrAwbWpSet->mean_cb;
        pbuf++;
        *pbuf = mPtrAwbWpSet->mean_cr;
        pbuf++;
        memcpy(pbuf, &mPtrAwbWpSet->mean_r, sizeof(mPtrAwbWpSet->mean_r));
        pbuf += sizeof(mPtrAwbWpSet->mean_r);
        memcpy(pbuf, &mPtrAwbWpSet->mean_b, sizeof(mPtrAwbWpSet->mean_b));
        pbuf += sizeof(mPtrAwbWpSet->mean_b);
        memcpy(pbuf, &mPtrAwbWpSet->mean_g, sizeof(mPtrAwbWpSet->mean_g));
        pbuf += sizeof(mPtrAwbWpSet->mean_g);
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_AWB_WP_SET, (uint8_t*)awb_wp_param, sizeof(awb_wp_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }

}

void TuningServer::get_awb_cur(CameraMetadata &uvcCamMeta)
{

#ifdef DEBUG_ON
    if (1){
        struct HAL_ISP_AWB_Curve_s awb_cur;
        mPtrAwbCur = &awb_cur;
#else
    if (mAwbCurGetOn){
#endif
        mAwbCurGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_AWB_CURV);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            memcpy(&mPtrAwbCur->f_N0_Rg,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCur->f_N0_Bg,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCur->f_d,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbCur->Kfactor,pchr,4);
            pchr += 4;
            memcpy(mPtrAwbCur->afRg1,pchr,sizeof(mPtrAwbCur->afRg1));
            pchr += sizeof(mPtrAwbCur->afRg1);
            memcpy(mPtrAwbCur->afMaxDist1,pchr,sizeof(mPtrAwbCur->afMaxDist1));
            pchr += sizeof(mPtrAwbCur->afMaxDist1);
            memcpy(mPtrAwbCur->afRg2,pchr,sizeof(mPtrAwbCur->afRg2));
            pchr += sizeof(mPtrAwbCur->afRg2);
            memcpy(mPtrAwbCur->afMaxDist2,pchr,sizeof(mPtrAwbCur->afMaxDist2));
            pchr += sizeof(mPtrAwbCur->afMaxDist2);
            memcpy(mPtrAwbCur->afGlobalFade1,pchr,sizeof(mPtrAwbCur->afGlobalFade1));
            pchr += sizeof(mPtrAwbCur->afGlobalFade1);
            memcpy(mPtrAwbCur->afGlobalGainDistance1,pchr,sizeof(mPtrAwbCur->afGlobalGainDistance1));
            pchr += sizeof(mPtrAwbCur->afGlobalGainDistance1);
            memcpy(mPtrAwbCur->afGlobalFade2,pchr,sizeof(mPtrAwbCur->afGlobalFade2));
            pchr += sizeof(mPtrAwbCur->afGlobalFade2);
            memcpy(mPtrAwbCur->afGlobalGainDistance2,pchr,sizeof(mPtrAwbCur->afGlobalGainDistance2));
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_awb_cur(CameraMetadata &uvcCamMeta)
{
    uint8_t awb_cur_param[530];
     uint8_t *pbuf;

    if (mAwbCurSetOn) {
        mAwbCurSetOn = false;
        pbuf = awb_cur_param;

        memcpy(pbuf, &mPtrAwbCur->f_N0_Rg, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCur->f_N0_Bg, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCur->f_d, 4);
        pbuf += 4;
        memcpy(pbuf, &mPtrAwbCur->Kfactor, 4);
        pbuf += 4;
        memcpy(pbuf, mPtrAwbCur->afRg1, sizeof(mPtrAwbCur->afRg1));
        pbuf += sizeof(mPtrAwbCur->afRg1);
        memcpy(pbuf, mPtrAwbCur->afMaxDist1, sizeof(mPtrAwbCur->afMaxDist1));
        pbuf += sizeof(mPtrAwbCur->afMaxDist1);
        memcpy(pbuf, mPtrAwbCur->afRg2, sizeof(mPtrAwbCur->afRg2));
        pbuf += sizeof(mPtrAwbCur->afRg2);
        memcpy(pbuf, mPtrAwbCur->afMaxDist2, sizeof(mPtrAwbCur->afMaxDist2));
        pbuf += sizeof(mPtrAwbCur->afMaxDist2);

        memcpy(pbuf, mPtrAwbCur->afGlobalFade1, sizeof(mPtrAwbCur->afGlobalFade1));
        pbuf += sizeof(mPtrAwbCur->afGlobalFade1);
        memcpy(pbuf, mPtrAwbCur->afGlobalGainDistance1, sizeof(mPtrAwbCur->afGlobalGainDistance1));
        pbuf += sizeof(mPtrAwbCur->afGlobalGainDistance1);
        memcpy(pbuf, mPtrAwbCur->afGlobalFade2, sizeof(mPtrAwbCur->afGlobalFade2));
        pbuf += sizeof(mPtrAwbCur->afGlobalFade2);
        memcpy(pbuf, mPtrAwbCur->afGlobalGainDistance2, sizeof(mPtrAwbCur->afGlobalGainDistance2));
        pbuf += sizeof(mPtrAwbCur->afGlobalGainDistance2);
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_AWB_CURV_SET, (uint8_t*)awb_cur_param, sizeof(awb_cur_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }

}

void TuningServer::get_awb_refgain(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if (1){
        struct HAL_ISP_AWB_RefGain_s refgain;
        mPtrAwbRefGain = &refgain;
#else
    if(mAwbRefGainGetOn){
#endif
        mAwbRefGainGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_AWB_REFGAIN);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            memcpy(mPtrAwbRefGain->ill_name,pchr,20);
            pchr += 20;
            memcpy(&mPtrAwbRefGain->refRGain,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbRefGain->refGrGain,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbRefGain->refGbGain,pchr,4);
            pchr += 4;
            memcpy(&mPtrAwbRefGain->refBGain,pchr,4);
            LOGV("refgain: %f,%f,%f,%f",mPtrAwbRefGain->refRGain,mPtrAwbRefGain->refGrGain,mPtrAwbRefGain->refGbGain,mPtrAwbRefGain->refBGain);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_awb_refgain(CameraMetadata &uvcCamMeta)
{
    uint8_t awb_refgain_param[37];
     uint8_t *pbuf;

    if (mAwbRefGainSetOn) {
        mAwbRefGainSetOn = false;
        pbuf = awb_refgain_param;
        memcpy(pbuf, mPtrAwbRefGain->ill_name, sizeof(mPtrAwbRefGain->ill_name));
        pbuf += sizeof(mPtrAwbRefGain->ill_name);
        memcpy(pbuf, &mPtrAwbRefGain->refRGain, sizeof(mPtrAwbRefGain->refRGain));
        pbuf += sizeof(mPtrAwbRefGain->refRGain);
        memcpy(pbuf, &mPtrAwbRefGain->refGrGain, sizeof(mPtrAwbRefGain->refGrGain));
        pbuf += sizeof(mPtrAwbRefGain->refGrGain);
        memcpy(pbuf, &mPtrAwbRefGain->refBGain, sizeof(mPtrAwbRefGain->refBGain));
        pbuf += sizeof(mPtrAwbRefGain->refBGain);
        memcpy(pbuf, &mPtrAwbRefGain->refGbGain, sizeof(mPtrAwbRefGain->refGbGain));
        pbuf += sizeof(mPtrAwbRefGain->refGbGain);
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_AWB_REFGAIN_SET, (uint8_t*)awb_refgain_param, sizeof(awb_refgain_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::get_goc(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if (1){
        struct HAL_ISP_GOC_s goc;
        mPtrGoc = &goc;
#else
    if (mGocGetOn){
#endif
        mGocGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_GOC_NORMAL);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            if(moduleEnabled)
                *moduleEnabled = *pchr;
            pchr++;
            memcpy(mPtrGoc->scene_name, pchr, 20);
            pchr += 20;
            mPtrGoc->wdr_status = (enum HAL_ISP_GOC_WDR_STATUS)*pchr;
            pchr++;
            mPtrGoc->cfg_mode = (enum HAL_ISP_GOC_CFG_MODE)*pchr;
            pchr++;
            memcpy(mPtrGoc->gamma_y, pchr, sizeof(mPtrGoc->gamma_y));
            LOGV("%s:%d [%s],%d",__FUNCTION__,*moduleEnabled,mPtrGoc->scene_name,mPtrGoc->gamma_y[10]);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_goc(CameraMetadata &uvcCamMeta)
{
    uint8_t goc_param[92];
     uint8_t *pbuf;

    if (mGocSetOn) {
        mGocSetOn = false;
        pbuf = goc_param;
        *pbuf++ = (uint8_t)mGocEnable;
        memcpy(pbuf, mPtrGoc->scene_name, sizeof(mPtrGoc->scene_name));
        pbuf += sizeof(mPtrGoc->scene_name);
        *pbuf++ = mPtrGoc->wdr_status;
        *pbuf++ = mPtrGoc->cfg_mode;
        memcpy(pbuf, mPtrGoc->gamma_y, sizeof(mPtrGoc->gamma_y));

        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_GOC_SET, (uint8_t*)goc_param, sizeof(goc_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::get_cproc(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if(1){
        struct HAL_ISP_CPROC_s cproc;
        mPtrCproc = &cproc;
#else
    if(mCprocGetOn){
#endif
        mCprocGetOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_CPROC_PREVIEW);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            if(moduleEnabled)
                *moduleEnabled = *pchr;
            pchr++;
            mPtrCproc->mode = (enum HAL_ISP_CPROC_MODE)(*pchr);
            pchr++;
            memcpy(&mPtrCproc->cproc_contrast, pchr, 4);
            pchr += 4;
            memcpy(&mPtrCproc->cproc_hue, pchr, 4);
            pchr += 4;
            memcpy(&mPtrCproc->cproc_saturation, pchr, 4);
            pchr += 4;
            mPtrCproc->cproc_brightness = *pchr;
            LOGV("%s: %d,%f,%f,%f,%d",__FUNCTION__,mPtrCproc->mode,mPtrCproc->cproc_contrast,
                 mPtrCproc->cproc_saturation,mPtrCproc->cproc_hue,mPtrCproc->cproc_brightness);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_cproc(CameraMetadata &uvcCamMeta)
{
    uint8_t cproc_param[16];
     uint8_t *pbuf;

    if (mCprocSetOn) {
        mCprocSetOn = false;
        pbuf = cproc_param;
        *pbuf = (uint8_t)mCprocEnable;
        pbuf++;
        *pbuf = mPtrCproc->mode;
        pbuf++;
        memcpy(pbuf, &mPtrCproc->cproc_contrast, sizeof(mPtrCproc->cproc_contrast));
        pbuf += sizeof(mPtrCproc->cproc_contrast);
        memcpy(pbuf, &mPtrCproc->cproc_hue, sizeof(mPtrCproc->cproc_hue));
        pbuf += sizeof(mPtrCproc->cproc_hue);
        memcpy(pbuf, &mPtrCproc->cproc_saturation, sizeof(mPtrCproc->cproc_saturation));
        pbuf += sizeof(mPtrCproc->cproc_saturation);
        *pbuf = mPtrCproc->cproc_brightness;
        pbuf++;
        LOGV("set_cproc:%f,%f,%f,%d",mPtrCproc->cproc_contrast,mPtrCproc->cproc_hue,mPtrCproc->cproc_saturation,mPtrCproc->cproc_brightness);
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_CPROC_SET, (uint8_t*)cproc_param, sizeof(cproc_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::get_dpf(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if(1) {
        struct HAL_ISP_ADPF_DPF_s dpf_get;
        mPtrDpf = &dpf_get;
#else
    if(mDpfGetOn){
#endif
        mDpfGetOn = false;
        camera_metadata_entry entry;

        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_DPF_GET);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            memcpy(mPtrDpf->dpf_name, pchr, sizeof(mPtrDpf->dpf_name));
            pchr += sizeof(mPtrDpf->dpf_name);
            mPtrDpf->dpf_enable = *pchr;
            pchr++;
            mPtrDpf->nll_segment = *pchr;
            pchr++;
            memcpy(mPtrDpf->nll_coeff, pchr, sizeof(mPtrDpf->nll_coeff));//3399 is not same
            pchr += sizeof(mPtrDpf->nll_coeff);
            memcpy(&mPtrDpf->sigma_green, pchr, sizeof(mPtrDpf->sigma_green));
            pchr += sizeof(mPtrDpf->sigma_green);
            memcpy(&mPtrDpf->sigma_redblue, pchr, sizeof(mPtrDpf->sigma_redblue));
            pchr += sizeof(mPtrDpf->sigma_redblue);
            memcpy(&mPtrDpf->gradient, pchr, sizeof(mPtrDpf->gradient));
            pchr += sizeof(mPtrDpf->gradient);
            memcpy(&mPtrDpf->offset, pchr, sizeof(mPtrDpf->offset));
            pchr += sizeof(mPtrDpf->offset);
            memcpy(&mPtrDpf->fRed, pchr, sizeof(mPtrDpf->fRed));
            pchr += sizeof(mPtrDpf->fRed);
            memcpy(&mPtrDpf->fGreenR, pchr, sizeof(mPtrDpf->fGreenR));
            pchr += sizeof(mPtrDpf->fGreenR);
            memcpy(&mPtrDpf->fGreenB, pchr, sizeof(mPtrDpf->fGreenB));
            pchr += sizeof(mPtrDpf->fGreenB);
            memcpy(&mPtrDpf->fBlue, pchr, sizeof(mPtrDpf->fBlue));

            LOGV("%s:[%s], %d,%f,%f",__FUNCTION__,mPtrDpf->dpf_name,mPtrDpf->nll_segment,mPtrDpf->gradient,mPtrDpf->fBlue);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_dpf(CameraMetadata &uvcCamMeta)
{
    uint8_t dpf_param[85];
     uint8_t *pbuf;

    if (mDpfSetOn) {
        mDpfSetOn = false;
        pbuf = dpf_param;
        memcpy(pbuf, mPtrDpf->dpf_name, sizeof(mPtrDpf->dpf_name));
        pbuf += sizeof(mPtrDpf->dpf_name);
        *pbuf = (uint8_t)mPtrDpf->dpf_enable;
        pbuf++;
        *pbuf = mPtrDpf->nll_segment;
        pbuf++;
        memcpy(pbuf, mPtrDpf->nll_coeff, sizeof(mPtrDpf->nll_coeff));
        pbuf += sizeof(mPtrDpf->nll_coeff);
        memcpy(pbuf, &mPtrDpf->sigma_green, sizeof(mPtrDpf->sigma_green));
        pbuf += sizeof(mPtrDpf->sigma_green);
        memcpy(pbuf, &mPtrDpf->sigma_redblue, sizeof(mPtrDpf->sigma_redblue));
        pbuf += sizeof(mPtrDpf->sigma_redblue);
        memcpy(pbuf, &mPtrDpf->gradient, sizeof(mPtrDpf->gradient));
        pbuf += sizeof(mPtrDpf->gradient);
        memcpy(pbuf, &mPtrDpf->offset, sizeof(mPtrDpf->offset));
        pbuf += sizeof(mPtrDpf->offset);
        memcpy(pbuf, &mPtrDpf->fRed, sizeof(mPtrDpf->fRed));
        pbuf += sizeof(mPtrDpf->fRed);
        memcpy(pbuf, &mPtrDpf->fGreenR, sizeof(mPtrDpf->fGreenR));
        pbuf += sizeof(mPtrDpf->fGreenR);
        memcpy(pbuf, &mPtrDpf->fGreenB, sizeof(mPtrDpf->fGreenB));
        pbuf += sizeof(mPtrDpf->fGreenB);
        memcpy(pbuf, &mPtrDpf->fBlue, sizeof(mPtrDpf->fBlue));
        pbuf += sizeof(mPtrDpf->fBlue);
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_DPF_SET, (uint8_t*)dpf_param, sizeof(dpf_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::get_flt(CameraMetadata &uvcCamMeta)
{
    struct Flt_level_conf_s{
        uint8_t grn_stage1[HAL_ISP_FLT_CURVE_NUM];
        uint8_t chr_h_mode[HAL_ISP_FLT_CURVE_NUM];
        uint8_t chr_v_mode[HAL_ISP_FLT_CURVE_NUM];
        uint32_t thresh_bl0[HAL_ISP_FLT_CURVE_NUM];
        uint32_t thresh_bl1[HAL_ISP_FLT_CURVE_NUM];
        uint32_t thresh_sh0[HAL_ISP_FLT_CURVE_NUM];
        uint32_t thresh_sh1[HAL_ISP_FLT_CURVE_NUM];
        uint32_t fac_sh1[HAL_ISP_FLT_CURVE_NUM];
        uint32_t fac_sh0[HAL_ISP_FLT_CURVE_NUM];
        uint32_t fac_mid[HAL_ISP_FLT_CURVE_NUM];
        uint32_t fac_bl0[HAL_ISP_FLT_CURVE_NUM];
        uint32_t fac_bl1[HAL_ISP_FLT_CURVE_NUM];
    }__attribute__((packed));
    struct Flt_level_conf_s flt_level_conf;
    uint8_t flt_level[HAL_ISP_FLT_CURVE_NUM];
#ifdef DEBUG_ON
    if(1) {
        struct HAL_ISP_FLT_Get_s flt_get;
        struct HAL_ISP_FLT_Get_ParamIn_s flt_get_param;
        flt_get_param.level = 1;
        flt_get_param.scene = 0;
        mPtrFltGet = &flt_get;
        mPtrFltGetParamIn = &flt_get_param;
#else
    if(mFltGetOn){
#endif
        mFltGetOn = false;
        camera_metadata_entry entry;
        if(mPtrFltGetParamIn->scene == 0)
        {
            entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_FLT_NORMAL);
        }else if(mPtrFltGetParamIn->scene == 1)
        {
            entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_FLT_NIGHT);
        }

        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            memcpy(mPtrFltGet->filter_name, pchr, sizeof(mPtrFltGet->filter_name));
            pchr += sizeof(mPtrFltGet->filter_name);
            mPtrFltGet->filter_enable = *pchr++;
            memcpy(&mPtrFltGet->denoise, pchr, sizeof(mPtrFltGet->denoise));
            pchr += sizeof(mPtrFltGet->denoise);
            memcpy(&mPtrFltGet->sharp, pchr, sizeof(mPtrFltGet->sharp));
            pchr += sizeof(mPtrFltGet->sharp);
            mPtrFltGet->level_conf_enable = *pchr++;
            memcpy(flt_level, pchr, sizeof(flt_level));
            pchr += sizeof(flt_level);
            memcpy(flt_level_conf.grn_stage1, pchr, sizeof(flt_level_conf.grn_stage1));
            pchr += sizeof(flt_level_conf.grn_stage1);
            memcpy(flt_level_conf.chr_h_mode, pchr, sizeof(flt_level_conf.chr_h_mode));
            pchr += sizeof(flt_level_conf.chr_h_mode);
            memcpy(flt_level_conf.chr_v_mode, pchr, sizeof(flt_level_conf.chr_v_mode));
            pchr += sizeof(flt_level_conf.chr_v_mode);
            memcpy(flt_level_conf.thresh_bl0, pchr, sizeof(flt_level_conf.thresh_bl0));
            pchr += sizeof(flt_level_conf.thresh_bl0);
            memcpy(flt_level_conf.thresh_bl1, pchr, sizeof(flt_level_conf.thresh_bl1));
            pchr += sizeof(flt_level_conf.thresh_bl1);
            memcpy(flt_level_conf.thresh_sh0, pchr, sizeof(flt_level_conf.thresh_sh0));
            pchr += sizeof(flt_level_conf.thresh_sh0);
            memcpy(flt_level_conf.thresh_sh1, pchr, sizeof(flt_level_conf.thresh_sh1));
            pchr += sizeof(flt_level_conf.thresh_sh1);
            memcpy(flt_level_conf.fac_sh1, pchr, sizeof(flt_level_conf.fac_sh1));
            pchr += sizeof(flt_level_conf.fac_sh1);
            memcpy(flt_level_conf.fac_sh0, pchr, sizeof(flt_level_conf.fac_sh0));
            pchr += sizeof(flt_level_conf.fac_sh0);
            memcpy(flt_level_conf.fac_mid, pchr, sizeof(flt_level_conf.fac_mid));
            pchr += sizeof(flt_level_conf.fac_mid);
            memcpy(flt_level_conf.fac_bl0, pchr, sizeof(flt_level_conf.fac_bl0));
            pchr += sizeof(flt_level_conf.fac_bl0);
            memcpy(flt_level_conf.fac_bl1, pchr, sizeof(flt_level_conf.fac_bl1));
            pchr += sizeof(flt_level_conf.fac_bl1);
            mPtrFltGet->is_level_exit = 0;
            for(int i=0; i<HAL_ISP_FLT_CURVE_NUM; i++)
            {
                if (flt_level[i] == mPtrFltGetParamIn->level){
                    mPtrFltGet->level_conf.grn_stage1 = flt_level_conf.grn_stage1[i];
                    mPtrFltGet->level_conf.chr_h_mode = flt_level_conf.chr_h_mode[i];
                    mPtrFltGet->level_conf.chr_v_mode = flt_level_conf.chr_v_mode[i];
                    mPtrFltGet->level_conf.thresh_bl0 = flt_level_conf.thresh_bl0[i];
                    mPtrFltGet->level_conf.thresh_bl1 = flt_level_conf.thresh_bl1[i];
                    mPtrFltGet->level_conf.thresh_sh0 = flt_level_conf.thresh_sh0[i];
                    mPtrFltGet->level_conf.thresh_sh1 = flt_level_conf.thresh_sh1[i];
                    mPtrFltGet->level_conf.fac_sh1 = flt_level_conf.fac_sh1[i];
                    mPtrFltGet->level_conf.fac_sh0 = flt_level_conf.fac_sh0[i];
                    mPtrFltGet->level_conf.fac_mid = flt_level_conf.fac_mid[i];
                    mPtrFltGet->level_conf.fac_bl0 = flt_level_conf.fac_bl0[i];
                    mPtrFltGet->level_conf.fac_bl1 = flt_level_conf.fac_bl1[i];
                    mPtrFltGet->is_level_exit = 1;
                    break;
                }
            }
            LOGV("flt_level:%d,%d,%d,%d,%d",flt_level[0],flt_level[1],flt_level[2],flt_level[3],flt_level[4]);
            LOGV("level=%d,%f,%f,%f,%f,%f",mPtrFltGetParamIn->level,mPtrFltGet->level_conf.fac_sh1,mPtrFltGet->level_conf.fac_sh0,
            mPtrFltGet->level_conf.fac_mid,mPtrFltGet->level_conf.fac_bl0,mPtrFltGet->level_conf.fac_bl1);

           if(mMsgType == CMD_TYPE_SYNC)
               mUvc_proc_ops->uvc_signal();
        }
    }
}

void TuningServer::set_flt(CameraMetadata &uvcCamMeta)
{
    uint8_t flt_param[84];
     uint8_t *pbuf;

    if (mFltSetOn) {
        mFltSetOn = false;
        pbuf = flt_param;
        memcpy(pbuf, mPtrFltSet->filter_name, sizeof(mPtrFltSet->filter_name));
        pbuf += sizeof(mPtrFltSet->filter_name);
        *pbuf++ = mPtrFltSet->scene_mode;
        *pbuf++ = mPtrFltSet->filter_enable;
        memcpy(pbuf, &mPtrFltSet->denoise, sizeof(mPtrFltSet->denoise));
        pbuf += sizeof(mPtrFltSet->denoise);
        memcpy(pbuf, &mPtrFltSet->sharp, sizeof(mPtrFltSet->sharp));
        pbuf += sizeof(mPtrFltSet->sharp);
        *pbuf++ = mPtrFltSet->level_conf_enable;
        *pbuf++ = mPtrFltSet->level;
        memcpy(pbuf, &mPtrFltSet->level_conf, sizeof(mPtrFltSet->level_conf));
        pbuf += sizeof(mPtrFltSet->level_conf);
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_FLT_SET, (uint8_t*)flt_param, sizeof(flt_param));
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::set_restart(CameraMetadata &uvcCamMeta)
{
    uint8_t param[40];

    if (mRestartOn) {
        mRestartOn = false;
        param[0] = mRestart->reboot;
        uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_ISP_RESTART, (uint8_t*)param, sizeof(param));
        bExpCmdSet = false;//disable exposure set
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::get_sensor_info(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if(1) {
        struct HAL_ISP_Sensor_Info_s sensor_info;
        mPtrSensorInfo = &sensor_info;
#else
    if(mSensorInfoOn){
#endif
        mSensorInfoOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_SENSOR_INFO);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
            return;
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            mPtrSensorInfo->mirror_info = *pchr;
            pchr++;
            memcpy(&mPtrSensorInfo->frame_length_lines, pchr, 2);
            pchr += 2;
            memcpy(&mPtrSensorInfo->line_length_pck, pchr, 2);
            pchr += 2;
            memcpy(&mPtrSensorInfo->vt_pix_clk_freq_hz, pchr, 4);
            pchr += 4;
            mPtrSensorInfo->binning = *pchr;
            pchr += 1;
            mPtrSensorInfo->black_white_mode = *pchr;
        }

        entry = uvcCamMeta.find(ANDROID_SENSOR_SENSITIVITY);
        if (!entry.count)
            return;
        LOGV("entry gain=%d",entry.data.i32[0]);
        float gain = (float)(entry.data.i32[0] / 100.0);
        mPtrSensorInfo->exp_gain_h = (uint)gain;
        mPtrSensorInfo->exp_gain_l = (gain - mPtrSensorInfo->exp_gain_h)*256.0;
        entry = uvcCamMeta.find(ANDROID_SENSOR_EXPOSURE_TIME);
        if (!entry.count)
            return;
        LOGV("entry time=%lld",entry.data.i64[0]);
        float time = (float)(entry.data.i64[0] / 1e6);//entry data is nsec,transfer to msec
        mPtrSensorInfo->exp_time_h = (uint)time;
        mPtrSensorInfo->exp_time_l = (time - mPtrSensorInfo->exp_time_h)*256.0;
        LOGV("vts:%d,hts:%d,pclk:%d \n gain:%d,time:%d",mPtrSensorInfo->frame_length_lines,mPtrSensorInfo->line_length_pck,
             mPtrSensorInfo->vt_pix_clk_freq_hz,mPtrSensorInfo->exp_gain_h,mPtrSensorInfo->exp_time_h);

        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::get_sys_info(CameraMetadata &uvcCamMeta)
{
#ifdef DEBUG_ON
    if(1){
        struct HAL_ISP_Sys_Info_s mSysInfo;
        mPtrSysInfo = &mSysInfo;
#else
    if(mSysInfoOn){
#endif
        mSysInfoOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_MODULE_INFO);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
            return;
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            memcpy(mPtrSysInfo->iq_name, pchr, sizeof(mPtrSysInfo->iq_name));
            pchr += 64;
            memcpy(mPtrSysInfo->sensor, pchr, sizeof(mPtrSysInfo->sensor));
            pchr += sizeof(mPtrSysInfo->sensor);
            memcpy(mPtrSysInfo->module, pchr, sizeof(mPtrSysInfo->module));
            pchr += sizeof(mPtrSysInfo->module);
            memcpy(mPtrSysInfo->lens, pchr, sizeof(mPtrSysInfo->lens));
            pchr += sizeof(mPtrSysInfo->lens);
            mPtrSysInfo->otp_flag = *pchr++;
            memcpy(&mPtrSysInfo->otp_r_value, pchr, 4);
            pchr += 4;
            memcpy(&mPtrSysInfo->otp_gr_value, pchr, 4);
            pchr += 4;
            memcpy(&mPtrSysInfo->otp_gb_value, pchr, 4);
            pchr += 4;
            memcpy(&mPtrSysInfo->otp_b_value, pchr, 4);
        }

        char sdkversion[PROPERTY_VALUE_MAX],*pstr1,*pstr2;
        char resolution[15];
        property_get("ro.board.platform",(char *)mPtrSysInfo->platform, "null");
        property_get("ro.rksdk.version",sdkversion,"null");
        pstr1 = strcasestr(sdkversion, "ANDROID");
        if (pstr1)
        {
            pstr2 = strtok(pstr1, "-");
            if(pstr2){
                strcat((char *)mPtrSysInfo->platform, "_");
                strcat((char *)mPtrSysInfo->platform, pstr2);
            }
        }else {
            LOGE("rksdk.version is not exits!");
        }
        entry = uvcCamMeta.find(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
        }else{
            LOGV("sysinfo gain: %d,%d",entry.data.i32[0],entry.data.i32[1]);
            float gain = (float)(entry.data.i32[1] / 100.0);
            mPtrSysInfo->max_exp_gain_h = (uint)gain;
            mPtrSysInfo->max_exp_gain_l = (gain - mPtrSysInfo->max_exp_gain_h)*256.0;
            LOGV("gain: %d,%d",mPtrSysInfo->max_exp_gain_h,mPtrSysInfo->max_exp_gain_l);
        }
        entry = uvcCamMeta.find(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
        }else{
            LOGV("sysinfo exp: %lld,%lld",entry.data.i64[0],entry.data.i64[1]);
            float time = (entry.data.i64[1]>entry.data.i64[0] ? entry.data.i64[1] : entry.data.i64[0]) / (1e6);//ms
            mPtrSysInfo->max_exp_time_h = (uint)time;
            mPtrSysInfo->max_exp_time_l = (time - mPtrSysInfo->max_exp_time_h)*256.0;
            LOGV("exp:%f %d,%d",time,mPtrSysInfo->max_exp_time_h,mPtrSysInfo->max_exp_time_l);
        }

        SensorFormat mAvailableSensorFormat;
        uint32_t format;
        PlatformData::getCameraHWInfo()->getAvailableSensorOutputFormats(mCamId, mAvailableSensorFormat);
        format = mAvailableSensorFormat.begin()->first;
        uint32_t index=0;
        std::vector<struct SensorFrameSize> &frameSize = mAvailableSensorFormat[format];

        for (auto it = frameSize.begin(); it != frameSize.end(); ++it) {
            LOGV("wxh:%dx%d",(*it).max_width,(*it).max_height);
            mPtrSysInfo->reso[index].width = (*it).max_width;
            mPtrSysInfo->reso[index].height = (*it).max_height;
            index++;
            if(index >= HAL_ISP_SENSOR_RESOLUTION_NUM)
                break;
        }
        mPtrSysInfo->reso_num = index;
        mPtrSysInfo->sensor_fmt = 0x2b;
        int code = 0, fmt = 0;
        char bayer[8];
        memset(bayer, 0, sizeof(bayer));
        PlatformData::getCameraHWInfo()->getSensorBayerPattern(mCamId, code);
        sscanf(gcu::pixelCode2String(code).c_str(),"%*[^_]_%*[^_]_%*[^_]_S%[^0-9]%d_%*s", bayer, &fmt);
        if (0 == strcmp(bayer, "BGGR")){
            mPtrSysInfo->bayer_pattern = 1;
        }else if(0 == strcmp(bayer, "GBRG")){
            mPtrSysInfo->bayer_pattern = 2;
        }else if(0 == strcmp(bayer, "GRBG")){
            mPtrSysInfo->bayer_pattern = 3;
        }else if(0 == strcmp(bayer, "RGGB")){
            mPtrSysInfo->bayer_pattern = 4;
        }
        if (fmt == 8)
            mPtrSysInfo->sensor_fmt = 0x2a;
        else if (fmt == 10)
            mPtrSysInfo->sensor_fmt = 0x2b;
        else if (fmt == 12)
            mPtrSysInfo->sensor_fmt = 0x2c;
        else
            mPtrSysInfo->sensor_fmt = 0x2b;

        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::get_protocol_info(CameraMetadata &uvcCamMeta)
{
    if(mProtocolOn){
        mProtocolOn = false;
        camera_metadata_entry entry;
        entry = uvcCamMeta.find(RKCAMERA3_PRIVATEDATA_ISP_PROTOCOL_INFO);
        if (!entry.count){
            LOGE("%s: entry.count = 0",__FUNCTION__);
            if(mMsgType == CMD_TYPE_SYNC)
                mUvc_proc_ops->uvc_signal();
            return;
        }else{
            unsigned char *pchr;
            pchr = &entry.data.u8[0];
            mPtrProtocol->major_ver = 0x01;
            mPtrProtocol->minor_ver = 0x01;
            memcpy(&mPtrProtocol->magicCode, pchr, sizeof(mPtrProtocol->magicCode));
        }
        if(mMsgType == CMD_TYPE_SYNC)
            mUvc_proc_ops->uvc_signal();
    }
}

void TuningServer::set_tuning_params(CameraMetadata &uvcCamMeta)
{
    set_exposure(uvcCamMeta);
    set_cap_req(uvcCamMeta);
    set_bls(uvcCamMeta);
    set_lsc(uvcCamMeta);
    set_ccm(uvcCamMeta);
    set_awb(uvcCamMeta);
    set_awb_wp(uvcCamMeta);
    set_awb_cur(uvcCamMeta);
    set_awb_refgain(uvcCamMeta);
    set_goc(uvcCamMeta);
    set_cproc(uvcCamMeta);
    set_dpf(uvcCamMeta);
    set_flt(uvcCamMeta);
    set_restart(uvcCamMeta);
    enable_tuning_flag(uvcCamMeta);
}

void TuningServer::get_tuning_params(CameraMetadata &uvcCamMeta)
{
    get_exposure(uvcCamMeta);
    get_bls(uvcCamMeta);
    get_lsc(uvcCamMeta);
    get_ccm(uvcCamMeta);
    get_awb(uvcCamMeta);
    get_awb_wp(uvcCamMeta);
    get_awb_cur(uvcCamMeta);
    get_awb_refgain(uvcCamMeta);
    get_goc(uvcCamMeta);
    get_cproc(uvcCamMeta);
    get_dpf(uvcCamMeta);
    get_flt(uvcCamMeta);
    get_sensor_info(uvcCamMeta);
    get_sys_info(uvcCamMeta);
    get_protocol_info(uvcCamMeta);
}

void TuningServer::enable_tuning_flag(CameraMetadata &uvcCamMeta)
{
    uint8_t enable = 1;
    uvcCamMeta.update(RKCAMERA3_PRIVATEDATA_TUNING_FLAG, (uint8_t*)&enable, sizeof(enable));
}

void* TuningMainThread::threadLoop(void* This)  {
    TuningMainThread * mMainTh = (TuningMainThread *)This;
    if (mMainTh && mMainTh->server->mUvc_proc_ops &&
        mMainTh->server->mUvc_proc_ops->uvc_main_proc){
        mMainTh->server->mUvc_proc_ops->uvc_main_proc();
    }
    return nullptr;
};

int TuningMainThread::exit(){
    /* set state false to end uvc process */
    server->mUvc_proc_ops->set_state(false);
    return requestExitAndWait();
}

void* TuningCmdThread::threadLoop(void* This)  {
    Message_cam msg;
    TuningCmdThread * commdTh = (TuningCmdThread *)This;

    if(commdTh == NULL)
        return nullptr;

    commdTh->mExit = false;
    while (commdTh->mExit == false)
    {
        memset(&msg,0x0, sizeof(msg));
        commdTh->server->mUvc_proc_ops->uvc_getMessage((void*)&msg);
        switch(msg.command)
        {
            case CMD_REBOOT:
            {
                commdTh->server->mRestart = (struct HAL_ISP_Reboot_Req_s *)msg.arg2;
                commdTh->server->mRestartOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_CAPS:
            {
                commdTh->server->mPtrCapReq = (struct HAL_ISP_Cap_Req_s *)msg.arg2;
                commdTh->server->mCapReqOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
            }
            break;
            case CMD_GET_CAPS:
            {
                break;
            }
            case CMD_GET_BLS:
            {
                commdTh->server->mPtrBls = (struct HAL_ISP_bls_cfg_s *)msg.arg2;
                commdTh->server->moduleEnabled = (bool *)msg.arg3;
                commdTh->server->mBlsGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_BLS:
            {
                commdTh->server->mBlsEnable = (bool)msg.arg2;
                commdTh->server->mPtrBls = (struct HAL_ISP_bls_cfg_s *)msg.arg3;
                commdTh->server->mBlsSetOn = true;

                break;
            }
            case CMD_GET_LSC:
            {
                commdTh->server->mPtrLsc = (struct HAL_ISP_Lsc_Profile_s *)msg.arg2;
                commdTh->server->mPtrLscQuery = (struct HAL_ISP_Lsc_Query_s *)msg.arg3;
                commdTh->server->moduleEnabled = (bool *)msg.arg4;
                commdTh->server->mLscGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_LSC:
            {
                commdTh->server->mLscEnable = (bool)msg.arg2;
                commdTh->server->mPtrLsc = (struct HAL_ISP_Lsc_Profile_s *)msg.arg3;
                commdTh->server->mLscSetOn = true;
                break;
            }
            case CMD_GET_CCM:
            {
                commdTh->server->mPtrAwbCcmGet = (struct HAL_ISP_AWB_CCM_GET_s *)msg.arg2;
                commdTh->server->moduleEnabled = (bool *)msg.arg3;
                commdTh->server->mAwbCcmGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_CCM:
            {
                commdTh->server->mCcmEnable = (bool)msg.arg2;
                commdTh->server->mPtrAwbCcmSet = (struct HAL_ISP_AWB_CCM_SET_s *)msg.arg3;
                commdTh->server->mAwbCcmSetOn = true;
                break;
            }
            case CMD_GET_AWB:
            {
                commdTh->server->mPtrAwb = (struct HAL_ISP_AWB_s *)msg.arg2;
                commdTh->server->moduleEnabled = (bool *)msg.arg3;
                commdTh->server->mAwbGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_AWB:
            {
                commdTh->server->mAwbEnable = (bool)msg.arg2;
                commdTh->server->mPtrAwb = (struct HAL_ISP_AWB_s *)msg.arg3;
                commdTh->server->mAwbSetOn = true;
                break;
            }
            case CMD_GET_AWB_CURV:
            {
                commdTh->server->mPtrAwbCur = (struct HAL_ISP_AWB_Curve_s *)msg.arg2;
                commdTh->server->mAwbCurGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_AWB_CURV:
            {
                commdTh->server->mPtrAwbCur = (struct HAL_ISP_AWB_Curve_s *)msg.arg2;
                commdTh->server->mAwbCurSetOn = true;
                break;
            }
            case CMD_GET_AWB_REFGAIN:
            {
                commdTh->server->mPtrAwbRefGain = (struct HAL_ISP_AWB_RefGain_s *)msg.arg2;
                commdTh->server->mAwbRefGainGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_AWB_REFGAIN:
            {
                commdTh->server->mPtrAwbRefGain = (struct HAL_ISP_AWB_RefGain_s *)msg.arg2;
                commdTh->server->mAwbRefGainSetOn = true;
                break;
            }
            case CMD_GET_AWB_WP:
            {
                commdTh->server->mPtrAwbWpGet = (struct HAL_ISP_AWB_White_Point_Get_s *)msg.arg2;
                commdTh->server->mAwbWpGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_AWB_WP:
            {
                commdTh->server->mPtrAwbWpSet = (struct HAL_ISP_AWB_White_Point_Set_s *)msg.arg2;
                commdTh->server->mAwbWpSetOn = true;
                break;
            }
            case CMD_GET_GOC:
            {
                commdTh->server->mPtrGoc = (struct HAL_ISP_GOC_s *)msg.arg2;
                commdTh->server->moduleEnabled = (bool *)msg.arg3;
                commdTh->server->mGocGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_GOC:
            {
                commdTh->server->mGocEnable = (bool)msg.arg2;
                commdTh->server->mPtrGoc = (struct HAL_ISP_GOC_s *)msg.arg3;
                commdTh->server->mGocSetOn = true;
                break;
            }
            case CMD_GET_CPROC:
            {
                commdTh->server->mPtrCproc = (struct HAL_ISP_CPROC_s *)msg.arg2;
                commdTh->server->moduleEnabled = (bool *)msg.arg3;
                commdTh->server->mCprocGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_CPROC:
            {
                commdTh->server->mCprocEnable = (bool)msg.arg2;
                commdTh->server->mPtrCproc = (struct HAL_ISP_CPROC_s *)msg.arg3;
                commdTh->server->mCprocSetOn = true;
                break;
            }
            case CMD_GET_DPF:
            {
                commdTh->server->mPtrDpf = (struct HAL_ISP_ADPF_DPF_s *)msg.arg2;
                commdTh->server->mDpfGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_DPF:
            {
                commdTh->server->mPtrDpf = (struct HAL_ISP_ADPF_DPF_s *)msg.arg2;
                commdTh->server->mDpfSetOn = true;
                break;
            }
            case CMD_GET_FLT:
            {
                commdTh->server->mPtrFltGet = (struct HAL_ISP_FLT_Get_s *)msg.arg2;
                commdTh->server->mPtrFltGetParamIn = (struct HAL_ISP_FLT_Get_ParamIn_s *)msg.arg3;
                commdTh->server->mFltGetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_FLT:
            {
                commdTh->server->mPtrFltSet = (struct HAL_ISP_FLT_Set_s *)msg.arg2;
                commdTh->server->mFltSetOn = true;
                break;
            }
            case CMD_GET_SYSINFO:
            {
                commdTh->server->mPtrSysInfo = (struct HAL_ISP_Sys_Info_s *)msg.arg2;
                commdTh->server->mSysInfoOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_GET_SENSOR_INFO:
            {
                commdTh->server->mPtrSensorInfo = (struct HAL_ISP_Sensor_Info_s *)msg.arg2;
                commdTh->server->mSensorInfoOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_GET_PROTOCOL_VER:
            {
                commdTh->server->mPtrProtocol = (struct HAL_ISP_Protocol_Ver_s *)msg.arg2;
                commdTh->server->mProtocolOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            case CMD_SET_EXPOSURE:
            {
                commdTh->server->mPtrExp= (struct HAL_ISP_Sensor_Exposure_s *)msg.arg2;
                commdTh->server->mExpSetOn = true;
                commdTh->server->mMsgType = (enum ISP_UVC_CMD_TYPE_e)msg.type;
                break;
            }
            default:
                break;
        }
    }

    return nullptr;
};

int TuningCmdThread::exit(){
    mExit = true;
    return requestExitAndWait();
}

}
}
