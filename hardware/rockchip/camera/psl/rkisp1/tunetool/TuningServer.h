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
 * @file    TuingServer.h
 *
 *****************************************************************************/
#ifndef CAMERA3_HAL_TUNINGSERVER_H_
#define CAMERA3_HAL_TUNINGSERVER_H_
#include <pthread.h>
#include <string>
#include <string.h>
#include <cutils/properties.h>
#include "Metadata.h"
#include "MediaEntity.h"
#include "rkcamera_vendor_tags.h"
#include "UvcMsgQ.h"
#include "uvc_hal_types.h"

namespace android {
namespace camera2 {

class TuningServer;
class ControlUnit;
class RKISP1CameraHw;

class TuningMainThread {
public:
    TuningMainThread(TuningServer *p)
    {
        mThreadId=0;
        server = p;
    }
    virtual ~TuningMainThread(){};
    int run(){
        int ret = pthread_create(&mThreadId, NULL, threadLoop, this);
        return (ret == 0) ? 0 : -1;
    }
    int exit();
    int requestExitAndWait(){
        return (pthread_join(mThreadId, nullptr) == 0) ? 0 : -1;
    }
private:
    TuningMainThread& operator=(const TuningMainThread& other);
    static void* threadLoop(void* This);
    TuningServer *server;
    pthread_t mThreadId;
};

class TuningCmdThread {
public:
    TuningCmdThread(TuningServer *p)
    {
        mThreadId=0;
        server = p;
    }
    virtual ~TuningCmdThread(){};

    int run(){
        int ret = pthread_create(&mThreadId, NULL, threadLoop, this);
        return (ret == 0) ? 0 : -1;
    }
    int exit();
    int requestExitAndWait(){
        mExit = true;
        return (pthread_join(mThreadId, nullptr) == 0) ? 0 : -1;
    }
private:
    TuningCmdThread& operator=(const TuningCmdThread& other);
    static void* threadLoop(void* This)  ;
    TuningServer *server;
    pthread_t mThreadId;
    bool mExit;
};

class TuningServer  {
private:
    TuningServer();
    RKISP1CameraHw *mCamHw;
    ControlUnit *mCtrlUnit;
    int mCamId;
    void *mLibUvcApp;
    bool mTuningMode;
public:
    friend class TuningCmdThread;
    ~TuningServer();
    static TuningServer *GetInstance()
    {
        static TuningServer instance;
        return &instance;
    }
    uvc_vpu_ops_t *get_vpu_ops()
    {
        return mUvc_vpu_ops;
    }
    uvc_proc_ops_t *get_proc_ops()
    {
        return mUvc_proc_ops;
    }
    bool isTuningMode()
    {
        return mTuningMode;
    }
    void init(ControlUnit *pCu, RKISP1CameraHw* pCh, int camId);
    void deinit();

//cmd implemet
    void set_restart(CameraMetadata &uvcCamMeta);
    void get_exposure(CameraMetadata &uvcCamMeta);
    void set_exposure(CameraMetadata &uvcCamMeta);
    void get_bls(CameraMetadata &uvcCamMeta);
    void set_bls(CameraMetadata &uvcCamMeta);
    void get_lsc(CameraMetadata &uvcCamMeta);
    void set_lsc(CameraMetadata &uvcCamMeta);
    void get_ccm(CameraMetadata &uvcCamMeta);
    void set_ccm(CameraMetadata &uvcCamMeta);
    void get_awb(CameraMetadata &uvcCamMeta);
    void set_awb(CameraMetadata &uvcCamMeta);
    void get_awb_wp(CameraMetadata &uvcCamMeta);
    void set_awb_wp(CameraMetadata &uvcCamMeta);
    void get_awb_cur(CameraMetadata &uvcCamMeta);
    void set_awb_cur(CameraMetadata &uvcCamMeta);
    void get_awb_refgain(CameraMetadata &uvcCamMeta);
    void set_awb_refgain(CameraMetadata &uvcCamMeta);
    void get_goc(CameraMetadata &uvcCamMeta);
    void set_goc(CameraMetadata &uvcCamMeta);
    void get_cproc(CameraMetadata &uvcCamMeta);
    void set_cproc(CameraMetadata &uvcCamMeta);
    void get_dpf(CameraMetadata &uvcCamMeta);
    void set_dpf(CameraMetadata &uvcCamMeta);
    void get_flt(CameraMetadata &uvcCamMeta);
    void set_flt(CameraMetadata &uvcCamMeta);
    void get_sys_info(CameraMetadata &uvcCamMeta);
    void get_sensor_info(CameraMetadata &uvcCamMeta);
    void get_protocol_info(CameraMetadata &uvcCamMeta);
    void set_cap_req(CameraMetadata &uvcCamMeta);
    void set_tuning_params(CameraMetadata &uvcCamMeta);
    void get_tuning_params(CameraMetadata &uvcCamMeta);
    void enable_tuning_flag(CameraMetadata &uvcCamMeta);
    bool isControledByTuningServer();
    void startCaptureRaw(int w, int h);
    void stopCatureRaw();
    TuningMainThread mMainTh;
    TuningCmdThread mCmdTh;
    uvc_proc_ops_t *mUvc_proc_ops;
    uvc_vpu_ops_t  *mUvc_vpu_ops;
    //cap raw data
    int64_t uvcExpTime;//ms
    int32_t uvcSensitivity;
    uint8_t uvcAeMode;
    bool bExpCmdCap;
    bool bExpCmdSet;
    bool mStartCapture;
    int mCapRawNum;
    float mCurGain;
    float mCurTime;//ms
    int mSkipFrame;
    enum ISP_UVC_CMD_TYPE_e   mMsgType;
private:
    bool *moduleEnabled;
    struct HAL_ISP_Cap_Req_s *mPtrCapReq;
    struct HAL_ISP_bls_cfg_s *mPtrBls;
    struct HAL_ISP_Lsc_Profile_s *mPtrLsc;
    struct HAL_ISP_Lsc_Query_s *mPtrLscQuery;
    struct HAL_ISP_AWB_CCM_GET_s *mPtrAwbCcmGet;
    struct HAL_ISP_AWB_CCM_SET_s *mPtrAwbCcmSet;
    struct HAL_ISP_AWB_s         *mPtrAwb;
    struct HAL_ISP_AWB_White_Point_Get_s *mPtrAwbWpGet;
    struct HAL_ISP_AWB_White_Point_Set_s *mPtrAwbWpSet;
    struct HAL_ISP_AWB_Curve_s *mPtrAwbCur;
    struct HAL_ISP_AWB_RefGain_s *mPtrAwbRefGain;
    struct HAL_ISP_GOC_s *mPtrGoc;
    struct HAL_ISP_CPROC_s  *mPtrCproc;
    struct HAL_ISP_ADPF_DPF_s *mPtrDpf;
    struct HAL_ISP_FLT_Set_s *mPtrFltSet;
    struct HAL_ISP_FLT_Get_s *mPtrFltGet;
    struct HAL_ISP_FLT_Get_ParamIn_s *mPtrFltGetParamIn;
    struct HAL_ISP_Sensor_Info_s *mPtrSensorInfo;
    struct HAL_ISP_Sys_Info_s *mPtrSysInfo;
    struct HAL_ISP_Sensor_Exposure_s *mPtrExp;
    struct HAL_ISP_Reboot_Req_s *mRestart;
    struct HAL_ISP_Protocol_Ver_s *mPtrProtocol;
    bool mBlsGetOn;
    bool mBlsSetOn;
    bool mBlsEnable;
    bool mLscGetOn;
    bool mLscSetOn;
    bool mLscEnable;
    bool mAwbCcmGetOn;
    bool mAwbCcmSetOn;
    bool mCcmEnable;
    bool mAwbGetOn;
    bool mAwbSetOn;
    bool mAwbEnable;
    bool mAwbWpGetOn;
    bool mAwbWpSetOn;
    bool mAwbCurGetOn;
    bool mAwbCurSetOn;
    bool mAwbRefGainGetOn;
    bool mAwbRefGainSetOn;
    bool mGocGetOn;
    bool mGocSetOn;
    bool mGocEnable;
    bool mCprocGetOn;
    bool mCprocSetOn;
    bool mCprocEnable;
    bool mDpfGetOn;
    bool mDpfSetOn;
    bool mFltSetOn;
    bool mFltGetOn;
    bool mSensorInfoOn;
    bool mSysInfoOn;
    bool mExpSetOn;
    bool mCapReqOn;
    bool mRestartOn;
    bool mProtocolOn;
};
} /* namespace camera2 */
} /* namespace android */

#endif

