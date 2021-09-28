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
 * @file        Rkvpu_OMX_Venc.h
 * @brief
 * @author      Csy (csy@rock-chips.com)

 * @version     1.0.0
 * @history
 *   2013.11.28 : Create
 */

#ifndef Rkvpu_OMX_VIDEO_ENCODER
#define Rkvpu_OMX_VIDEO_ENCODER

#include "OMX_Component.h"
#include "Rockchip_OMX_Def.h"
#include "Rockchip_OSAL_Queue.h"
#include "Rockchip_OMX_Baseport.h"
#include "Rockchip_OMX_Basecomponent.h"
#include "OMX_Video.h"
#include "OMX_VideoExt.h"
#include "vpu_api.h"

#define MAX_VIDEOENC_INPUTBUFFER_NUM           2
#define MAX_VIDEOENC_OUTPUTBUFFER_NUM          4

#define DEFAULT_ENC_FRAME_WIDTH                 1920
#define DEFAULT_ENC_FRAME_HEIGHT                1080
#define DEFAULT_ENC_FRAME_FRAMERATE             (25 << 16)
#define DEFAULT_ENC_FRAME_BITRATE               3000000

#define DEFAULT_VIDEOENC_INPUT_BUFFER_SIZE    (DEFAULT_ENC_FRAME_WIDTH * DEFAULT_ENC_FRAME_HEIGHT * 3)/2
#define DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE   (DEFAULT_ENC_FRAME_WIDTH * DEFAULT_ENC_FRAME_HEIGHT)


#define OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX   4
//#define WRITE_FILE

typedef struct _RKVPU_OMX_VIDEOENC_COMPONENT {
    OMX_HANDLETYPE hCodecHandle;

    OMX_BOOL bFirstFrame;

    OMX_VIDEO_PARAM_AVCTYPE AVCComponent[ALL_PORT_NUM];

    OMX_VIDEO_PARAM_HEVCTYPE HEVCComponent[ALL_PORT_NUM];

    /* Buffer Process */
    OMX_BOOL       bExitBufferProcessThread;
    OMX_HANDLETYPE hInputThread;
    OMX_HANDLETYPE hOutputThread;

    OMX_VIDEO_CODINGTYPE codecId;

    /* Shared Memory Handle */
    OMX_HANDLETYPE hSharedMemory;

    OMX_BOOL configChange;

    OMX_BOOL IntraRefreshVOP;

    OMX_VIDEO_CONTROLRATETYPE eControlRate[ALL_PORT_NUM];

    OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization;

    OMX_VIDEO_PARAM_INTRAREFRESHTYPE intraRefresh;

    OMX_VIDEO_PARAMS_EXTENDED  params_extend;
    OMX_BOOL bFirstInput;

    OMX_BOOL bFirstOutput;

    OMX_BOOL bStoreMetaData;

    OMX_BOOL bPrependSpsPpsToIdr;

    OMX_BOOL bRkWFD;

    VpuCodecContext_t *vpu_ctx;

    void *rga_ctx;

    OMX_U8 *bSpsPpsbuf;

    OMX_U32 bSpsPpsLen;

    OMX_BOOL bSpsPpsHeaderFlag;

    OMX_BOOL bEncSendEos;

    OMX_U32 bFrame_num;
    OMX_U32 bCurrent_width;

    OMX_U32 bCurrent_height;

    OMX_U32 bLast_config_frame;

    VPUMemLinear_t *enc_vpumem;

    OMX_HANDLETYPE  bScale_Mutex;
    OMX_HANDLETYPE  bRecofig_Mutex;
    OMX_S32 bPixel_format;
    OMX_BOOL bRgb2yuvFlag;

    void *rkapi_hdl;
    //for debug
    FILE *fp_enc_out;
    FILE *fp_enc_in;

    //add by xlm for use mpp or vpuapi
    OMX_BOOL bIsUseMpp;
    OMX_BOOL bIsNewVpu;
#ifdef AVS80
    OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS ConfigColorAspects;
    OMX_BOOL bIsCfgColorAsp;
    ISO_COLORASPECTS *colorAspects;
#endif
    OMX_S32 (*rkvpu_open_cxt)(VpuCodecContext_t **ctx);
    OMX_S32 (*rkvpu_close_cxt)(VpuCodecContext_t **ctx);

    OMX_ERRORTYPE (*Rkvpu_codec_srcInputProcess) (OMX_COMPONENTTYPE *pOMXComponent, ROCKCHIP_OMX_DATA *pInputData);

    OMX_ERRORTYPE (*Rkvpu_codec_srcOutputProcess) (OMX_COMPONENTTYPE *pOMXComponent, ROCKCHIP_OMX_DATA *pInputData);

    OMX_ERRORTYPE (*Rkvpu_codec_dstInputProcess) (OMX_COMPONENTTYPE *pOMXComponent, ROCKCHIP_OMX_DATA *pOutputData);

    OMX_ERRORTYPE (*Rkvpu_codec_dstOutputProcess) (OMX_COMPONENTTYPE *pOMXComponent, ROCKCHIP_OMX_DATA *pOutputData);

} RKVPU_OMX_VIDEOENC_COMPONENT;

#ifdef __cplusplus
extern "C" {
#endif

int calc_plane(int width, int height);
void UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent);
OMX_BOOL Rkvpu_Check_BufferProcess_State(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 nPortIndex);

OMX_ERRORTYPE Rkvpu_OMX_InputBufferProcess(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Rkvpu_OMX_OutputBufferProcess(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Rkvpu_Enc_ComponentInit(OMX_COMPONENTTYPE *pOMXComponent);
OMX_ERRORTYPE Rkvpu_Enc_Terminate(OMX_COMPONENTTYPE *pOMXComponent);
OMX_ERRORTYPE Rkvpu_Enc_GetEncParams(OMX_COMPONENTTYPE *pOMXComponent, EncParameter_t **encParams);


OMX_ERRORTYPE Rockchip_OMX_ComponentConstructor(OMX_HANDLETYPE hComponent, OMX_STRING componentName);
OMX_ERRORTYPE Rockchip_OMX_ComponentDeInit(OMX_HANDLETYPE hComponent);

#ifdef __cplusplus
}
#endif

#endif
