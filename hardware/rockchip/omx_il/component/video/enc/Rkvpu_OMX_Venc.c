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
 * @file        Rkvpu_OMX_Venc.c
 * @brief
 * @author      csy (csy@rock-chips.com)
 * @version     2.0.0
 * @history
 *   2013.11.28 : Create
 */
#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_venc"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <dlfcn.h>
#include <hardware/hardware.h>
#include "Rockchip_OMX_Macros.h"
#include "Rockchip_OSAL_Event.h"
#include "Rkvpu_OMX_Venc.h"
#include "Rkvpu_OMX_VencControl.h"
#include "Rockchip_OMX_Basecomponent.h"
#include "Rockchip_OSAL_Thread.h"
#include "Rockchip_OSAL_Semaphore.h"
#include "Rockchip_OSAL_Mutex.h"
#include "Rockchip_OSAL_ETC.h"
#include "Rockchip_OSAL_SharedMemory.h"
#include "Rockchip_OSAL_RGA_Process.h"
#include "Rockchip_OSAL_Env.h"
#include "OMX_IVCommon.h"
#include "Rockchip_OSAL_Log.h"
#include "Rockchip_OSAL_ColorUtils.h"

#include "hardware/rga.h"
#include "vpu_type.h"
#include "gralloc_priv_omx.h"
#include "omx_video_global.h"

#ifdef USE_ANB
#include "Rockchip_OSAL_Android.h"
#endif

/* Using for the encode rate statistic*/
#ifdef ENCODE_RATE_STATISTIC
#define STATISTIC_PER_TIME 5  // statistic once per 5s
struct timeval nowGetTime;
static OMX_U64 lastEncodeTime = 0;
static OMX_U64 currentEncodeTime = 0;
static OMX_U32 lastEncodeFrameCount = 0;
static OMX_U32 currentEncodeFrameCount = 0;
#endif

/**
This enumeration is for levels. The value follows the level_idc in sequence
parameter set rbsp. See Annex A.
@published All
*/
typedef enum AVCLevel {
    AVC_LEVEL_AUTO = 0,
    AVC_LEVEL1_B = 9,
    AVC_LEVEL1 = 10,
    AVC_LEVEL1_1 = 11,
    AVC_LEVEL1_2 = 12,
    AVC_LEVEL1_3 = 13,
    AVC_LEVEL2 = 20,
    AVC_LEVEL2_1 = 21,
    AVC_LEVEL2_2 = 22,
    AVC_LEVEL3 = 30,
    AVC_LEVEL3_1 = 31,
    AVC_LEVEL3_2 = 32,
    AVC_LEVEL4 = 40,
    AVC_LEVEL4_1 = 41,
    AVC_LEVEL4_2 = 42,
    AVC_LEVEL5 = 50,
    AVC_LEVEL5_1 = 51
} AVCLevel;

typedef enum HEVCLevel {
    HEVC_UNSUPPORT_LEVEL = -1,
    HEVC_LEVEL4_1 = 0,
    HEVC_LEVEL_MAX = 0x7FFFFFFF,
} HEVCLevel;

typedef struct {
    OMX_RK_VIDEO_CODINGTYPE codec_id;
    OMX_VIDEO_CODINGTYPE     omx_id;
} CodeMap;


static const CodeMap kCodeMap[] = {
    { OMX_RK_VIDEO_CodingAVC,   OMX_VIDEO_CodingAVC},
    { OMX_RK_VIDEO_CodingVP8,   OMX_VIDEO_CodingVP8},
    { OMX_RK_VIDEO_CodingHEVC,  OMX_VIDEO_CodingHEVC},
};

int calc_plane(int width, int height)
{
    int mbX, mbY;

    mbX = (width + 15) / 16;
    mbY = (height + 15) / 16;

    /* Alignment for interlaced processing */
    mbY = (mbY + 1) / 2 * 2;

    return (mbX * 16) * (mbY * 16);
}

void UpdateFrameSize(OMX_COMPONENTTYPE *pOMXComponent)
{
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT      *rockchipInputPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    ROCKCHIP_OMX_BASEPORT      *rockchipOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];

    if ((rockchipOutputPort->portDefinition.format.video.nFrameWidth !=
         rockchipInputPort->portDefinition.format.video.nFrameWidth) ||
        (rockchipOutputPort->portDefinition.format.video.nFrameHeight !=
         rockchipInputPort->portDefinition.format.video.nFrameHeight)) {
        OMX_U32 width = 0, height = 0;

        rockchipOutputPort->portDefinition.format.video.nFrameWidth =
            rockchipInputPort->portDefinition.format.video.nFrameWidth;
        rockchipOutputPort->portDefinition.format.video.nFrameHeight =
            rockchipInputPort->portDefinition.format.video.nFrameHeight;
        width = rockchipOutputPort->portDefinition.format.video.nStride =
                    rockchipInputPort->portDefinition.format.video.nStride;
        height = rockchipOutputPort->portDefinition.format.video.nSliceHeight =
                     rockchipInputPort->portDefinition.format.video.nSliceHeight;

        switch (rockchipOutputPort->portDefinition.format.video.eColorFormat) {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
            if (width && height)
                rockchipOutputPort->portDefinition.nBufferSize = (width * height * 3) / 2;
            break;
        default:
            if (width && height)
                rockchipOutputPort->portDefinition.nBufferSize = width * height * 2;
            break;
        }
    }

    return;
}

OMX_BOOL Rkvpu_Check_BufferProcess_State(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 nPortIndex)
{
    OMX_BOOL ret = OMX_FALSE;

    if ((pRockchipComponent->currentState == OMX_StateExecuting) &&
        (pRockchipComponent->pRockchipPort[nPortIndex].portState == OMX_StateIdle) &&
        (pRockchipComponent->transientState != ROCKCHIP_OMX_TransStateExecutingToIdle) &&
        (pRockchipComponent->transientState != ROCKCHIP_OMX_TransStateIdleToExecuting)) {
        ret = OMX_TRUE;
    } else {
        ret = OMX_FALSE;
    }

    return ret;
}

OMX_ERRORTYPE Rkvpu_ResetAllPortConfig(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT      *pRockchipComponent  = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT           *pRockchipInputPort  = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    ROCKCHIP_OMX_BASEPORT           *pRockchipOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];

    /* Input port */
    pRockchipInputPort->portDefinition.format.video.nFrameWidth = DEFAULT_ENC_FRAME_WIDTH;
    pRockchipInputPort->portDefinition.format.video.nFrameHeight = DEFAULT_ENC_FRAME_HEIGHT;
    pRockchipInputPort->portDefinition.format.video.nStride = 0; /*DEFAULT_ENC_FRAME_WIDTH;*/
    pRockchipInputPort->portDefinition.format.video.nSliceHeight = 0;
    pRockchipInputPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_INPUT_BUFFER_SIZE;
    pRockchipInputPort->portDefinition.format.video.pNativeRender = 0;
    pRockchipInputPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pRockchipInputPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pRockchipInputPort->portDefinition.bEnabled = OMX_TRUE;
    pRockchipInputPort->bufferProcessType = BUFFER_COPY;
    pRockchipInputPort->portWayType = WAY2_PORT;

    /* Output port */
    pRockchipOutputPort->portDefinition.format.video.nFrameWidth = DEFAULT_ENC_FRAME_WIDTH;
    pRockchipOutputPort->portDefinition.format.video.nFrameHeight = DEFAULT_ENC_FRAME_HEIGHT;
    pRockchipOutputPort->portDefinition.format.video.nStride = 0; /*DEFAULT_ENC_FRAME_WIDTH;*/
    pRockchipOutputPort->portDefinition.format.video.nSliceHeight = 0;
    pRockchipOutputPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE;
    pRockchipOutputPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    if (pRockchipOutputPort->portDefinition.format.video.cMIMEType != NULL) {
        Rockchip_OSAL_Memset(pRockchipOutputPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        Rockchip_OSAL_Strcpy(pRockchipOutputPort->portDefinition.format.video.cMIMEType, "raw/video");
    }
    pRockchipOutputPort->portDefinition.format.video.pNativeRender = 0;
    pRockchipOutputPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pRockchipOutputPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pRockchipOutputPort->portDefinition.nBufferCountActual = MAX_VIDEOENC_OUTPUTBUFFER_NUM;
    pRockchipOutputPort->portDefinition.nBufferCountMin = MAX_VIDEOENC_OUTPUTBUFFER_NUM;
    pRockchipOutputPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE;
    pRockchipOutputPort->portDefinition.bEnabled = OMX_TRUE;
    pRockchipOutputPort->bufferProcessType = BUFFER_COPY | BUFFER_ANBSHARE;
    pRockchipOutputPort->portWayType = WAY2_PORT;

    return ret;
}


void Rkvpu_Wait_ProcessPause(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 nPortIndex)
{
    ROCKCHIP_OMX_BASEPORT *rockchipOMXPort = NULL;

    FunctionIn();

    rockchipOMXPort = &pRockchipComponent->pRockchipPort[nPortIndex];

    if (((pRockchipComponent->currentState == OMX_StatePause) ||
         (pRockchipComponent->currentState == OMX_StateIdle) ||
         (pRockchipComponent->transientState == ROCKCHIP_OMX_TransStateLoadedToIdle) ||
         (pRockchipComponent->transientState == ROCKCHIP_OMX_TransStateExecutingToIdle)) &&
        (pRockchipComponent->transientState != ROCKCHIP_OMX_TransStateIdleToLoaded) &&
        (!CHECK_PORT_BEING_FLUSHED(rockchipOMXPort))) {
        Rockchip_OSAL_SignalWait(pRockchipComponent->pRockchipPort[nPortIndex].pauseEvent, DEF_MAX_WAIT_TIME);
        Rockchip_OSAL_SignalReset(pRockchipComponent->pRockchipPort[nPortIndex].pauseEvent);
    }

    FunctionOut();

    return;
}

OMX_ERRORTYPE Rkvpu_Enc_ReConfig(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 new_width, OMX_U32 new_height)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT      *pRockchipComponent  = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc    =  (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    OMX_RK_VIDEO_CODINGTYPE codecId = OMX_RK_VIDEO_CodingUnused;
    H264EncPictureType encType = VPU_H264ENC_YUV420_SEMIPLANAR;
    VpuCodecContext_t *p_vpu_ctx = NULL;
    EncParameter_t *EncParam;
    EncParameter_t preEncParam;
    Rockchip_OSAL_MutexLock(pVideoEnc->bRecofig_Mutex);
    if (pVideoEnc->vpu_ctx) {
        memset(&preEncParam, 0, sizeof(EncParameter_t));
        pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_GETCFG, &preEncParam);
        if (pVideoEnc->rkvpu_close_cxt) {
            pVideoEnc->rkvpu_close_cxt(&pVideoEnc->vpu_ctx);
        }
    }
    if (pVideoEnc->vpu_ctx == NULL) {
        if (pVideoEnc->rkvpu_open_cxt) {
            pVideoEnc->rkvpu_open_cxt(&p_vpu_ctx);
        }
    }
    p_vpu_ctx->width = new_width;
    p_vpu_ctx->height = new_height;
    p_vpu_ctx->codecType = CODEC_ENCODER;
    {
        OMX_U32 i = 0;
        for (i = 0; i < ARRAY_SIZE(kCodeMap); i++) {
            if (kCodeMap[i].omx_id == pVideoEnc->codecId) {
                codecId = kCodeMap[i].codec_id;
                break;
            }
        }
    }

    p_vpu_ctx->videoCoding = codecId;
    p_vpu_ctx->codecType = CODEC_ENCODER;
    p_vpu_ctx->private_data = malloc(sizeof(EncParameter_t));
    memcpy(p_vpu_ctx->private_data, &preEncParam, sizeof(EncParameter_t));
    EncParam = (EncParameter_t*)p_vpu_ctx->private_data;
    EncParam->height = new_height;
    EncParam->width = new_width;
    if (p_vpu_ctx) {
        if (p_vpu_ctx->init(p_vpu_ctx, NULL, 0)) {
            ret = OMX_ErrorInsufficientResources;
            Rockchip_OSAL_MutexUnlock(pVideoEnc->bRecofig_Mutex);
            goto EXIT;

        }
        Rockchip_OSAL_Memcpy(pVideoEnc->bSpsPpsbuf, p_vpu_ctx->extradata, p_vpu_ctx->extradata_size);
        pVideoEnc->bSpsPpsLen = p_vpu_ctx->extradata_size;
    }
    EncParam->rc_mode = 1;
    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, EncParam);
    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
    pVideoEnc->vpu_ctx = p_vpu_ctx;
    pVideoEnc->bPrependSpsPpsToIdr = OMX_TRUE;
    Rockchip_OSAL_MutexUnlock(pVideoEnc->bRecofig_Mutex);
EXIT:
    FunctionOut();
    return ret;
}

OMX_U32 Rkvpu_N12_Process(OMX_COMPONENTTYPE *pOMXComponent, RockchipVideoPlane *vplanes, OMX_U32 *aPhy_address)
{

    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT *pOutPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    RK_U32 new_width = 0, new_height = 0, len = 0;
    OMX_U32 Width_in = pOutPort->portDefinition.format.video.nFrameWidth;
    OMX_U32 Height_in =  pOutPort->portDefinition.format.video.nFrameHeight;
    OMX_U32 Width = pOutPort->portDefinition.format.video.nFrameWidth;
    OMX_U32 Height =  pOutPort->portDefinition.format.video.nFrameHeight;
    if (pVideoEnc->params_extend.bEnableScaling || pVideoEnc->params_extend.bEnableCropping) {
        Rockchip_OSAL_MutexLock(pVideoEnc->bScale_Mutex);
        if (pVideoEnc->params_extend.bEnableScaling) {
            new_width = pVideoEnc->params_extend.ui16ScaledWidth;
            new_height = pVideoEnc->params_extend.ui16ScaledHeight;
        } else if (pVideoEnc->params_extend.bEnableCropping) {
            new_width = Width_in - pVideoEnc->params_extend.ui16CropLeft - pVideoEnc->params_extend.ui16CropRight;
            new_height = Height_in - pVideoEnc->params_extend.ui16CropTop - pVideoEnc->params_extend.ui16CropBottom;
            omx_trace("CropLeft = %d CropRight = %d CropTop %d CropBottom %d",
                      pVideoEnc->params_extend.ui16CropLeft, pVideoEnc->params_extend.ui16CropRight,
                      pVideoEnc->params_extend.ui16CropTop, pVideoEnc->params_extend.ui16CropBottom);
        }
        omx_trace("new_width = %d new_height = %d orign width %d orign height %d",
                  new_width, new_height, Width_in, Height_in);
        if (new_width != pVideoEnc->bCurrent_width ||
            new_height != pVideoEnc->bCurrent_height) {
            pVideoEnc->bCurrent_width  =  new_width;
            pVideoEnc->bCurrent_height =  new_height;
            Rkvpu_Enc_ReConfig(pOMXComponent, new_width, new_height);
        }
        rga_nv12_crop_scale(vplanes, pVideoEnc->enc_vpumem, &pVideoEnc->params_extend, Width, Height, pVideoEnc->rga_ctx);
        *aPhy_address = pVideoEnc->enc_vpumem->phy_addr;
        len = new_width * new_height * 3 / 2;
        Rockchip_OSAL_MutexUnlock(pVideoEnc->bScale_Mutex);
    } else {
        Rockchip_OSAL_SharedMemory_getPhyAddress(pVideoEnc->hSharedMemory, vplanes->fd, aPhy_address);
        len = Width * Height * 3 / 2;
    }
    return len;
}
#ifdef USE_STOREMETADATA
OMX_ERRORTYPE Rkvpu_ProcessStoreMetaData(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_U32 *aPhy_address, OMX_U32 *len)
{

    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT *pInPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    ROCKCHIP_OMX_BASEPORT *pOutPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];

    OMX_U32 Width = pOutPort->portDefinition.format.video.nFrameWidth;
    OMX_U32 Height =  pOutPort->portDefinition.format.video.nFrameHeight;
    OMX_PTR pGrallocHandle;
    *len = 0;
    *aPhy_address = 0;
    if (!Rockchip_OSAL_GetInfoRkWfdMetaData(pVideoEnc->bRkWFD, pInputBuffer->pBuffer, &pGrallocHandle)) {
        if (!(pInPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatAndroidOpaque)) {
            omx_info("Error colorformat != OMX_COLOR_FormatAndroidOpaque");
        }
        gralloc_private_handle_t priv_hnd_wfd;
        Rockchip_OSAL_Memset(&priv_hnd_wfd, 0, sizeof(priv_hnd_wfd));
        Rockchip_get_gralloc_private(pGrallocHandle, &priv_hnd_wfd);
        if (VPUMemJudgeIommu() == 0) {
            Rockchip_OSAL_SharedMemory_getPhyAddress(pVideoEnc->hSharedMemory, priv_hnd_wfd.share_fd, aPhy_address);
        } else {
            *aPhy_address = priv_hnd_wfd.share_fd;
        }
        *len = Width * Height * 4;
        if (pVideoEnc->bPixel_format < 0) {
            pVideoEnc->bPixel_format = priv_hnd_wfd.format;
            if (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_RGBA_8888) {
                H264EncPictureType encType = VPU_H264ENC_BGR888;    // add by lance 2014.01.20
                pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            } else {
                H264EncPictureType encType = VPU_H264ENC_RGB888;    // add by lance 2014.01.20
                pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            }
        }
    } else {
        RockchipVideoPlane vplanes;
        OMX_COLOR_FORMATTYPE omx_format = 0;
        OMX_U32 res;

#ifdef AVS80
        if (pInputBuffer->nFilledLen != Rockchip_OSAL_GetVideoNativeMetaSize() && pInputBuffer->nFilledLen != Rockchip_OSAL_GetVideoGrallocMetaSize()) {
            omx_info("MetaData buffer is wrong size! "
                     "(got %lu bytes, expected 8 or 12)", pInputBuffer->nFilledLen);
            return OMX_ErrorBadParameter;
        }
#else
        if (pInputBuffer->nFilledLen != 8) {
            omx_info("MetaData buffer is wrong size! "
                     "(got %lu bytes, expected 8)", pInputBuffer->nFilledLen);
            return OMX_ErrorBadParameter;
        }
#endif

        if (Rockchip_OSAL_GetInfoFromMetaData(pInputBuffer->pBuffer, &pGrallocHandle)) {
            return OMX_ErrorBadParameter;
        }

        if (pVideoEnc->bPixel_format < 0) {
            int gpu_fd = -1;
            omx_format = Rockchip_OSAL_GetANBColorFormat(pGrallocHandle);
            pVideoEnc->bPixel_format = Rockchip_OSAL_OMX2HalPixelFormat(omx_format);//mali_gpu
            gpu_fd = open("/dev/pvrsrvkm", O_RDWR, 0);
            if (gpu_fd > 0) {
                pVideoEnc->bRgb2yuvFlag = OMX_TRUE;
                close(gpu_fd);
            } else {
                if (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_RGBA_8888) {
                    pVideoEnc->bRgb2yuvFlag = OMX_TRUE;
                }
            }
        }
        res = Rockchip_OSAL_getANBHandle(pGrallocHandle, &vplanes);
        if (res != 0) {
            omx_err("Unable to lock image buffer %p for access", pGrallocHandle);
            pGrallocHandle = NULL;
            return OMX_ErrorBadParameter;
        }

        if (pVideoEnc->bRgb2yuvFlag == OMX_TRUE) {
            VPUMemLinear_t tmp_vpumem;
            OMX_U32 new_width = 0;
            OMX_U32 new_height = 0;
            if (pVideoEnc->params_extend.bEnableScaling) {
                new_width = pVideoEnc->params_extend.ui16ScaledWidth;
                new_height = pVideoEnc->params_extend.ui16ScaledHeight;
                if (new_width != pVideoEnc->bCurrent_width ||
                    new_height != pVideoEnc->bCurrent_height) {
                    pVideoEnc->bCurrent_width  =  new_width;
                    pVideoEnc->bCurrent_height =  new_height;
                    Rkvpu_Enc_ReConfig(pOMXComponent, new_width, new_height);
                }
            } else {
                new_width = (Width + 15) & ~15;
                new_height = (Height + 7) & ~7;
            }
            memset(&tmp_vpumem, 0, sizeof(VPUMemLinear_t));
            rga_rgb2nv12(&vplanes, pVideoEnc->enc_vpumem, Width, Height, new_width, new_height, pVideoEnc->rga_ctx);
            VPUMemClean(pVideoEnc->enc_vpumem);
            *aPhy_address = pVideoEnc->enc_vpumem->phy_addr;
            *len = new_width * new_height * 3 / 2;
            if (pVideoEnc->fp_enc_in) {
                VPUMemInvalidate(pVideoEnc->enc_vpumem);
                fwrite(pVideoEnc->enc_vpumem->vir_addr, 1, (*len), pVideoEnc->fp_enc_in);
                fflush(pVideoEnc->fp_enc_in);
            }
        } else if (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_YCrCb_NV12) {
            *len = Rkvpu_N12_Process(pOMXComponent, &vplanes, aPhy_address);
        } else if (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
            H264EncPictureType encType = VPU_H264ENC_YUV420_SEMIPLANAR;
            pVideoEnc->vpu_ctx->control(pVideoEnc->vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            if (Width != vplanes.stride || (Height & 0xf)) {
                rga_nv12_copy(&vplanes, pVideoEnc->enc_vpumem, Width, Height, pVideoEnc->rga_ctx);
                *aPhy_address = pVideoEnc->enc_vpumem->phy_addr;
                if (pVideoEnc->fp_enc_in) {
                    fwrite(pVideoEnc->enc_vpumem->vir_addr, 1, Width * Height * 3 / 2, pVideoEnc->fp_enc_in);
                    fflush(pVideoEnc->fp_enc_in);
                }
            } else {
                Rockchip_OSAL_SharedMemory_getPhyAddress(pVideoEnc->hSharedMemory, vplanes.fd, aPhy_address);
            }

            omx_err("aPhy_address = 0x%08x", *aPhy_address);
            *len = Width * Height * 3 / 2;
        } else {
            rga_rgb_copy(&vplanes, pVideoEnc->enc_vpumem, Width, Height, pVideoEnc->rga_ctx);
            *aPhy_address = pVideoEnc->enc_vpumem->phy_addr;
            *len = Width * Height * 4;
        }

#if 0 // def WRITE_FILE
        VPUMemInvalidate(pVideoEnc->enc_vpumem);
        fwrite(pVideoEnc->enc_vpumem->vir_addr, 1, Width_in * Height_in * 4, pVideoEnc->fp_h264);
        fflush(pVideoEnc->fp_h264);
#endif
    }
    return OMX_ErrorNone;
}
#endif
OMX_BOOL Rkvpu_SendInputData(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_U32 ret = OMX_FALSE;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT      *rockchipInputPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    ROCKCHIP_OMX_DATABUFFER    *inputUseBuffer = &rockchipInputPort->way.port2WayDataBuffer.inputDataBuffer;
    VpuCodecContext_t *p_vpu_ctx = pVideoEnc->vpu_ctx;
    FunctionIn();
    OMX_PTR pGrallocHandle;
    OMX_COLOR_FORMATTYPE omx_format = 0;
    if (inputUseBuffer->dataValid == OMX_TRUE) {
        EncInputStream_t aInput;

        if (pVideoEnc->bFirstFrame) {
            EncParameter_t vpug;
            char pValue[128];
            if (rockchipInputPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatAndroidOpaque) {
                Rockchip_OSAL_GetInfoFromMetaData(inputUseBuffer->bufferHeader->pBuffer, &pGrallocHandle);
                if (pGrallocHandle == NULL) {
                    omx_err("pGrallocHandle is NULL set omx_format default");
                    omx_format = 0;
                } else {
                    omx_format = Rockchip_OSAL_GetANBColorFormat(pGrallocHandle);
                }
                if (Rockchip_OSAL_OMX2HalPixelFormat(omx_format)  == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
                    Rockchip_OSAL_OMX2HalPixelFormat(omx_format)  == HAL_PIXEL_FORMAT_YCrCb_NV12) {
                    H264EncPictureType encType = VPU_H264ENC_YUV420_SEMIPLANAR;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
                } else {
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)&vpug);
                    vpug.rc_mode = 1;

                    omx_trace("set vpu_enc %d", vpug.rc_mode);
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)&vpug);
                    omx_trace("VPU_API_ENC_SETFORMAT in");
                    H264EncPictureType encType = VPU_H264ENC_RGB888;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
                }
            } else if (rockchipInputPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatYUV420Planar) {
                H264EncPictureType encType = VPU_H264ENC_YUV420_PLANAR;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            }

            /*
             * Improve encode quality for CtsTestCases input.
             * - android.media.cts.DecodeEditEncodeTest#testVideoEditQCIF
             */
            if (Rockchip_OSAL_OMX2HalPixelFormat(omx_format) != HAL_PIXEL_FORMAT_RGBA_8888) {
                if (p_vpu_ctx->width <= 176 && p_vpu_ctx->height <= 144) {
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)&vpug);
                    vpug.rc_mode = 2;
                    vpug.qp = 20;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)&vpug);
                }
            }

            pVideoEnc->bFirstFrame = OMX_FALSE;
        }

        if ((inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            omx_trace("bSaveFlagEOS : OMX_TRUE");
            pRockchipComponent->bSaveFlagEOS = OMX_TRUE;
        }
        Rockchip_OSAL_Memset(&aInput, 0, sizeof(EncInputStream_t));

#ifdef USE_STOREMETADATA
        if (pVideoEnc->bStoreMetaData && !pRockchipComponent->bSaveFlagEOS) {
            OMX_U32 aPhy_address = 0, len = 0;

            ret = Rkvpu_ProcessStoreMetaData(pOMXComponent, inputUseBuffer->bufferHeader, &aPhy_address, &len);
            p_vpu_ctx = pVideoEnc->vpu_ctx; // may be reconfig in preprocess

            if (ret != OMX_ErrorNone) {
                omx_err("Rkvpu_ProcessStoreMetaData return %d ", ret);
                Rkvpu_InputBufferReturn(pOMXComponent, inputUseBuffer);
                pRockchipComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                             pRockchipComponent->callbackData, OMX_EventError,
                                                             OUTPUT_PORT_INDEX,
                                                             OMX_IndexParamPortDefinition, NULL);
                goto EXIT;
            }

            aInput.buf =  NULL;
            aInput.bufPhyAddr = aPhy_address;
            aInput.size = len;
            aInput.timeUs = inputUseBuffer->timeStamp;
        } else {
            OMX_BUFFERHEADERTYPE* pInputBuffer = inputUseBuffer->bufferHeader;
            if (pInputBuffer->nFilledLen == 4) {
                aInput.bufPhyAddr = *(int32_t*)((uint8_t*)pInputBuffer->pBuffer + pInputBuffer->nOffset);
                omx_trace("rk camera metadata 0x%x", aInput.bufPhyAddr);
                aInput.buf = NULL;
            } else {
                aInput.buf =  inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen;
                aInput.bufPhyAddr = 0x80000000;
                if (pVideoEnc->fp_enc_in) {
                    fwrite(aInput.buf, 1, inputUseBuffer->dataLen, pVideoEnc->fp_enc_in);
                    fflush(pVideoEnc->fp_enc_in);
                }
                // while bufPhyAddr < 0 && buf != NULL
                // assign bufPhyAddr 8000000 to match rk_vpuapi to copy data from aInput.buf
            }
            aInput.size = inputUseBuffer->dataLen;
            aInput.timeUs = inputUseBuffer->timeStamp;
        }
#else
        {
            OMX_BUFFERHEADERTYPE* pInputBuffer = inputUseBuffer->bufferHeader;
            if (pInputBuffer->nFilledLen == 4) {
                aInput.bufPhyAddr = *(int32_t*)((uint8_t*)pInputBuffer->pBuffer + pInputBuffer->nOffset);
                omx_trace("rk camera metadata 0x%x", aInput.bufPhyAddr);
                aInput.buf = NULL;
            } else {
                aInput.buf =  inputUseBuffer->bufferHeader->pBuffer + inputUseBuffer->usedDataLen;
                aInput.bufPhyAddr = 0x80000000;
                // while bufPhyAddr < 0 && buf != NULL
                // assign bufPhyAddr 8000000 to match rk_vpuapi to copy data from aInput.buf
            }
            aInput.size = inputUseBuffer->dataLen;
            aInput.timeUs = inputUseBuffer->timeStamp;
        }
#endif

        if (rockchipInputPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatAndroidOpaque) {
            if ((pVideoEnc->bRgb2yuvFlag == OMX_TRUE) || (pVideoEnc->bPixel_format == HAL_PIXEL_FORMAT_YCrCb_NV12)) {
                omx_trace("set as nv12 format");
                H264EncPictureType encType = VPU_H264ENC_YUV420_SEMIPLANAR;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETFORMAT, (void *)&encType);
            }
        }

        if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC || pVideoEnc->codecId == OMX_VIDEO_CodingHEVC) {
            if (rockchipInputPort->portDefinition.format.video.eColorFormat == OMX_COLOR_FormatAndroidOpaque) {
                if (pVideoEnc->bFrame_num < 60 && (pVideoEnc->bFrame_num % 5 == 0)) {
                    EncParameter_t vpug;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETIDRFRAME, NULL);
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, &vpug);
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, &vpug);
                }
                if (pVideoEnc->bFrame_num - pVideoEnc->bLast_config_frame == 60) {
                    EncParameter_t vpug;
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, &vpug);
                    p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, &vpug);
                    omx_trace("pVideoEnc->bFrame_num %d pVideoEnc->mLast_config_frame %d",
                              pVideoEnc->bFrame_num, pVideoEnc->bLast_config_frame);
                    pVideoEnc->bLast_config_frame = pVideoEnc->bFrame_num;

                }
            }
        }

        if ((inputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            omx_info("send eos");
            aInput.nFlags |= OMX_BUFFERFLAG_EOS;
        }

        p_vpu_ctx->encoder_sendframe(p_vpu_ctx, &aInput);

        pVideoEnc->bFrame_num++;
        Rkvpu_InputBufferReturn(pOMXComponent, inputUseBuffer);

        if (pRockchipComponent->checkTimeStamp.needSetStartTimeStamp == OMX_TRUE) {
            pRockchipComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_TRUE;
            pRockchipComponent->checkTimeStamp.startTimeStamp = inputUseBuffer->timeStamp;
            pRockchipComponent->checkTimeStamp.nStartFlags = inputUseBuffer->nFlags;
            pRockchipComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
            omx_trace("first frame timestamp after seeking %lld us (%.2f secs)",
                      inputUseBuffer->timeStamp, inputUseBuffer->timeStamp / 1E6);
        }
        ret = OMX_TRUE;
    }

EXIT:
    FunctionOut();
    return ret;
}

OMX_BOOL Rkvpu_Post_OutputStream(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_BOOL                   ret = OMX_FALSE;
    ROCKCHIP_OMX_BASECOMPONENT  *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT         *pOutputPort        = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    ROCKCHIP_OMX_DATABUFFER    *outputUseBuffer = &pOutputPort->way.port2WayDataBuffer.outputDataBuffer;
    VpuCodecContext_t           *p_vpu_ctx = pVideoEnc->vpu_ctx;

    FunctionIn();


    if ((p_vpu_ctx == NULL) || (pVideoEnc->bEncSendEos == OMX_TRUE)) {
        goto EXIT;
    }
    if (outputUseBuffer->dataValid == OMX_TRUE) {
        EncoderOut_t pOutput;
        OMX_U8 *aOut_buf = outputUseBuffer->bufferHeader->pBuffer;
        Rockchip_OSAL_Memset(&pOutput, 0, sizeof(EncoderOut_t));
        if ((OMX_FALSE == pVideoEnc->bSpsPpsHeaderFlag)) {
            if (pVideoEnc->bSpsPpsLen > 0) {
                Rockchip_OSAL_Memcpy(aOut_buf, pVideoEnc->bSpsPpsbuf, pVideoEnc->bSpsPpsLen);
                outputUseBuffer->remainDataLen = pVideoEnc->bSpsPpsLen;
                outputUseBuffer->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
                omx_info("set bSpsPpsLen %d", pVideoEnc->bSpsPpsLen);
                pVideoEnc->bSpsPpsHeaderFlag = OMX_TRUE;
                ret = OMX_TRUE;
                if (pVideoEnc->fp_enc_out != NULL) {
                    fwrite(aOut_buf, 1, pVideoEnc->bSpsPpsLen, pVideoEnc->fp_enc_out);
                    fflush(pVideoEnc->fp_enc_out);
                }

                Rkvpu_OutputBufferReturn(pOMXComponent, outputUseBuffer);
                goto EXIT;
            }
        }

        omx_trace("encoder_getstream in ");
        if (p_vpu_ctx->encoder_getstream(p_vpu_ctx, &pOutput) < 0) {
            outputUseBuffer->dataLen = 0;
            outputUseBuffer->remainDataLen = 0;
            outputUseBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
            outputUseBuffer->timeStamp = 0;
            ret = OMX_TRUE;
            omx_info("OMX_BUFFERFLAG_EOS");
            Rkvpu_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            pVideoEnc->bEncSendEos = OMX_TRUE;
            goto EXIT;
        }
        if ((pOutput.size > 0) && (!CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
#ifdef ENCODE_RATE_STATISTIC
            gettimeofday(&nowGetTime, NULL);
            currentEncodeTime = nowGetTime.tv_sec * 1000000 + nowGetTime.tv_usec;
            if (lastEncodeTime != 0) {
                ++currentEncodeFrameCount;
                if (currentEncodeTime - lastEncodeTime >= (STATISTIC_PER_TIME * 1000000)) {
                    omx_err("Statistic Encode Rate %d", ((currentEncodeFrameCount - lastEncodeFrameCount) / STATISTIC_PER_TIME));
                    lastEncodeTime = currentEncodeTime;
                    lastEncodeFrameCount = currentEncodeFrameCount;
                }
            } else
                lastEncodeTime = currentEncodeTime;
#endif
            if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
                if (pVideoEnc->bPrependSpsPpsToIdr && pOutput.keyFrame) {
                    omx_info("IDR outputUseBuffer->remainDataLen  %d spslen %d size %d", outputUseBuffer->remainDataLen
                             , pVideoEnc->bSpsPpsLen, outputUseBuffer->allocSize);
                    memcpy(aOut_buf, pVideoEnc->bSpsPpsbuf, pVideoEnc->bSpsPpsLen);
                    memcpy(aOut_buf + pVideoEnc->bSpsPpsLen, "\x00\x00\x00\x01", 4);
                    Rockchip_OSAL_Memcpy(aOut_buf + pVideoEnc->bSpsPpsLen + 4, pOutput.data, pOutput.size);
                    outputUseBuffer->remainDataLen = pVideoEnc->bSpsPpsLen + pOutput.size + 4;
                    outputUseBuffer->usedDataLen += pVideoEnc->bSpsPpsLen;
                    outputUseBuffer->usedDataLen += 4;
                    outputUseBuffer->usedDataLen += pOutput.size;
                    omx_info("IDR outputUseBuffer->remainDataLen 1 %d spslen %d size %d", outputUseBuffer->remainDataLen
                             , pVideoEnc->bSpsPpsLen, outputUseBuffer->allocSize);
                } else {
                    memcpy(aOut_buf, "\x00\x00\x00\x01", 4);
                    Rockchip_OSAL_Memcpy(aOut_buf + 4, pOutput.data, pOutput.size);
                    outputUseBuffer->remainDataLen = pOutput.size + 4;
                    outputUseBuffer->usedDataLen += 4;
                    outputUseBuffer->usedDataLen += pOutput.size;
                }
            } else {
                Rockchip_OSAL_Memcpy(aOut_buf, pOutput.data, pOutput.size);
                outputUseBuffer->remainDataLen = pOutput.size;
                outputUseBuffer->usedDataLen = pOutput.size;
            }
            if (pVideoEnc->fp_enc_out != NULL) {
                fwrite(aOut_buf, 1, outputUseBuffer->remainDataLen , pVideoEnc->fp_enc_out);
                fflush(pVideoEnc->fp_enc_out);
            }

            outputUseBuffer->timeStamp = pOutput.timeUs;
            if (pOutput.keyFrame) {
                outputUseBuffer->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
            }
            if (pOutput.data) {
                free(pOutput.data);
                pOutput.data = NULL;
            }
            if ((outputUseBuffer->remainDataLen > 0) ||
                ((outputUseBuffer->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) ||
                (CHECK_PORT_BEING_FLUSHED(pOutputPort))) {
                omx_trace("Rkvpu_OutputBufferReturn");
                Rkvpu_OutputBufferReturn(pOMXComponent, outputUseBuffer);
            }
            ret = OMX_TRUE;
        } else if (CHECK_PORT_BEING_FLUSHED(pOutputPort)) {
            if (pOutput.data) {
                free(pOutput.data);
                pOutput.data = NULL;
            }
            outputUseBuffer->dataLen = 0;
            outputUseBuffer->remainDataLen = 0;
            outputUseBuffer->nFlags = 0;
            outputUseBuffer->timeStamp = 0;
            ret = OMX_TRUE;
            Rkvpu_OutputBufferReturn(pOMXComponent, outputUseBuffer);
        } else {
            //omx_err("output buffer is smaller than decoded data size Out Length");
            // pRockchipComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
            //                                        pRockchipComponent->callbackData,
            //                                         OMX_EventError, OMX_ErrorUndefined, 0, NULL);
            ret = OMX_FALSE;
        }
    } else {
        ret = OMX_FALSE;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_InputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT      *rockchipInputPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    ROCKCHIP_OMX_BASEPORT      *rockchipOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    ROCKCHIP_OMX_DATABUFFER    *srcInputUseBuffer = &rockchipInputPort->way.port2WayDataBuffer.inputDataBuffer;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        Rockchip_OSAL_SleepMillisec(0);
        Rkvpu_Wait_ProcessPause(pRockchipComponent, INPUT_PORT_INDEX);
        omx_trace("Rkvpu_Check_BufferProcess_State in");
        while ((Rkvpu_Check_BufferProcess_State(pRockchipComponent, INPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {


            if ((CHECK_PORT_BEING_FLUSHED(rockchipInputPort)) ||
                (((ROCKCHIP_OMX_EXCEPTION_STATE)rockchipOutputPort->exceptionFlag != GENERAL_STATE) && ((ROCKCHIP_OMX_ERRORTYPE)ret == OMX_ErrorInputDataDecodeYet)))
                break;

            if (rockchipInputPort->portState != OMX_StateIdle)
                break;

            Rockchip_OSAL_MutexLock(srcInputUseBuffer->bufferMutex);
            if ((ROCKCHIP_OMX_ERRORTYPE)ret != OMX_ErrorInputDataDecodeYet) {
                if ((srcInputUseBuffer->dataValid != OMX_TRUE) &&
                    (!CHECK_PORT_BEING_FLUSHED(rockchipInputPort))) {

                    ret = Rkvpu_InputBufferGetQueue(pRockchipComponent);
                    if (ret != OMX_ErrorNone) {
                        Rockchip_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                        break;
                    }
                }

                if (srcInputUseBuffer->dataValid == OMX_TRUE) {
                    if (Rkvpu_SendInputData(hComponent) != OMX_TRUE) {
                        Rockchip_OSAL_SleepMillisec(5);
                    }
                }
                if (CHECK_PORT_BEING_FLUSHED(rockchipInputPort)) {
                    Rockchip_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
                    break;
                }
            }
            Rockchip_OSAL_MutexUnlock(srcInputUseBuffer->bufferMutex);
            if ((ROCKCHIP_OMX_ERRORTYPE)ret == OMX_ErrorCodecInit)
                pVideoEnc->bExitBufferProcessThread = OMX_TRUE;
        }
    }

    goto EXIT;
EXIT:

    FunctionOut();

    return ret;
}


OMX_ERRORTYPE Rkvpu_OMX_OutputBufferProcess(OMX_HANDLETYPE hComponent)
{
    OMX_U32          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT      *rockchipOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    ROCKCHIP_OMX_DATABUFFER    *dstOutputUseBuffer = &rockchipOutputPort->way.port2WayDataBuffer.outputDataBuffer;

    FunctionIn();

    while (!pVideoEnc->bExitBufferProcessThread) {
        Rockchip_OSAL_SleepMillisec(0);
        Rkvpu_Wait_ProcessPause(pRockchipComponent, OUTPUT_PORT_INDEX);

        while ((Rkvpu_Check_BufferProcess_State(pRockchipComponent, OUTPUT_PORT_INDEX)) &&
               (!pVideoEnc->bExitBufferProcessThread)) {

            if (CHECK_PORT_BEING_FLUSHED(rockchipOutputPort))
                break;

            Rockchip_OSAL_MutexLock(dstOutputUseBuffer->bufferMutex);
            if ((dstOutputUseBuffer->dataValid != OMX_TRUE) &&
                (!CHECK_PORT_BEING_FLUSHED(rockchipOutputPort))) {

                omx_trace("Rkvpu_OutputBufferGetQueue in");
                ret = Rkvpu_OutputBufferGetQueue(pRockchipComponent);
                omx_trace("Rkvpu_OutputBufferGetQueue out");
                if (ret != OMX_ErrorNone) {
                    Rockchip_OSAL_MutexUnlock(dstOutputUseBuffer->bufferMutex);
                    break;
                }
            }

            if (dstOutputUseBuffer->dataValid == OMX_TRUE) {
                Rockchip_OSAL_MutexLock(pVideoEnc->bRecofig_Mutex);
                ret = Rkvpu_Post_OutputStream(pOMXComponent);
                Rockchip_OSAL_MutexUnlock(pVideoEnc->bRecofig_Mutex);
                if ( (OMX_BOOL)ret != OMX_TRUE) {
                    Rockchip_OSAL_SleepMillisec(5);
                }
            }
            Rockchip_OSAL_MutexUnlock(dstOutputUseBuffer->bufferMutex);
        }
    }

    goto EXIT;
EXIT:

    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Rkvpu_OMX_InputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    Rkvpu_OMX_InputBufferProcess(pOMXComponent);

    Rockchip_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Rkvpu_OMX_OutputProcessThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    Rkvpu_OMX_OutputBufferProcess(pOMXComponent);

    Rockchip_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_BufferProcess_Create( OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

    FunctionIn();

    pVideoEnc->bExitBufferProcessThread = OMX_FALSE;

    ret = Rockchip_OSAL_ThreadCreate(&pVideoEnc->hOutputThread,
                                     Rkvpu_OMX_OutputProcessThread,
                                     pOMXComponent,
                                     "omx_enc_output");

    if (ret == OMX_ErrorNone)
        ret = Rockchip_OSAL_ThreadCreate(&pVideoEnc->hInputThread,
                                         Rkvpu_OMX_InputProcessThread,
                                         pOMXComponent,
                                         "omx_enc_input");

    goto EXIT;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_BufferProcess_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    OMX_S32                countValue = 0;

    FunctionIn();

    pVideoEnc->bExitBufferProcessThread = OMX_TRUE;

    Rockchip_OSAL_Get_SemaphoreCount(pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        Rockchip_OSAL_SemaphorePost(pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].bufferSemID);

    Rockchip_OSAL_SignalSet(pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].pauseEvent);
    Rockchip_OSAL_ThreadTerminate(pVideoEnc->hInputThread);
    pVideoEnc->hInputThread = NULL;

    Rockchip_OSAL_Get_SemaphoreCount(pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX].bufferSemID, &countValue);
    if (countValue == 0)
        Rockchip_OSAL_SemaphorePost(pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX].bufferSemID);


    Rockchip_OSAL_SignalSet(pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].pauseEvent);

    Rockchip_OSAL_SignalSet(pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX].pauseEvent);
    Rockchip_OSAL_ThreadTerminate(pVideoEnc->hOutputThread);
    pVideoEnc->hOutputThread = NULL;

    pRockchipComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
    pRockchipComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;

    goto EXIT;
EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE ConvertOmxAvcLevelToAvcSpecLevel(
    int32_t omxLevel, AVCLevel *pvLevel)
{
    omx_err("ConvertOmxAvcLevelToAvcSpecLevel: %d", omxLevel);
    AVCLevel level = AVC_LEVEL5_1;
    switch (omxLevel) {
    case OMX_VIDEO_AVCLevel1:
    case OMX_VIDEO_AVCLevel1b:
        level = AVC_LEVEL1;
        break;
    case OMX_VIDEO_AVCLevel11:
        level = AVC_LEVEL1_1;
        break;
    case OMX_VIDEO_AVCLevel12:
        level = AVC_LEVEL1_2;
        break;
    case OMX_VIDEO_AVCLevel13:
        level = AVC_LEVEL1_3;
        break;
    case OMX_VIDEO_AVCLevel2:
        level = AVC_LEVEL2;
        break;
    case OMX_VIDEO_AVCLevel21:
        level = AVC_LEVEL2_1;
        break;
    case OMX_VIDEO_AVCLevel22:
        level = AVC_LEVEL2_2;
        break;
    case OMX_VIDEO_AVCLevel3:
        level = AVC_LEVEL3;
        break;
    case OMX_VIDEO_AVCLevel31:
        level = AVC_LEVEL3_1;
        break;
    case OMX_VIDEO_AVCLevel32:
        level = AVC_LEVEL3_2;
        break;
    case OMX_VIDEO_AVCLevel4:
        level = AVC_LEVEL4;
        break;
    case OMX_VIDEO_AVCLevel41:
        level = AVC_LEVEL4_1;
        break;
    case OMX_VIDEO_AVCLevel42:
        level = AVC_LEVEL4_2;
        break;
    case OMX_VIDEO_AVCLevel5:
        level = AVC_LEVEL5;
        break;
    case OMX_VIDEO_AVCLevel51:
        level = AVC_LEVEL5_1;
        break;
    default:
        omx_err("Unknown omx level: %d", omxLevel);
        return -1;
    }
    *pvLevel = level;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE ConvertOmxHevcProfile2HalHevcProfile(
    OMX_VIDEO_HEVCPROFILETYPE omxHevcProfile, HEVCEncProfile *halHevcProfile)
{
    HEVCEncProfile hevcProfile = HEVC_MAIN_PROFILE;
    switch (omxHevcProfile) {
    case OMX_VIDEO_HEVCProfileMain:
        hevcProfile = HEVC_MAIN_PROFILE;
        break;
    case OMX_VIDEO_HEVCProfileMain10:
        hevcProfile = HEVC_MAIN10_PROFILE;
        break;
    case OMX_VIDEO_HEVCProfileMain10HDR10:
        hevcProfile = HEVC_MAIN10HDR10_PROFILE;
        break;
    default:
        omx_err("Unknown omx profile: %d, forced to convert HEVC_MAIN_PROFILE",
                omxHevcProfile);
        break;
    }
    *halHevcProfile = hevcProfile;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE ConvertOmxHevcLevel2HalHevcLevel(
    OMX_VIDEO_HEVCLEVELTYPE omxHevcLevel, HEVCLevel *halHevcLevel)
{
    HEVCLevel hevcLevel = HEVC_LEVEL4_1;
    switch (omxHevcLevel) {
    case OMX_VIDEO_HEVCMainTierLevel41:
        hevcLevel = HEVC_LEVEL4_1;
        break;
    default:
        omx_err("Unknown omx level: %d, forced to convert HEVC_LEVEL4_1",
                omxHevcLevel);
        break;
    }
    *halHevcLevel = hevcLevel;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_open_vpuenc_context(RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc)
{
    pVideoEnc->rkapi_hdl = dlopen("libvpu.so", RTLD_LAZY | RTLD_GLOBAL);
    if (pVideoEnc->rkapi_hdl == NULL) {
        return OMX_ErrorHardware;
    }
    pVideoEnc->rkvpu_open_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoEnc->rkapi_hdl, "vpu_open_context");
    if (pVideoEnc->rkvpu_open_cxt == NULL) {
        dlclose(pVideoEnc->rkapi_hdl);
        pVideoEnc->rkapi_hdl = NULL;
        omx_trace("used old version lib");
        pVideoEnc->rkapi_hdl = dlopen("librk_vpuapi.so", RTLD_LAZY | RTLD_GLOBAL);
        if (pVideoEnc->rkapi_hdl == NULL) {
            omx_err("dll open fail librk_vpuapi.so");
            return OMX_ErrorHardware;
        }
        pVideoEnc->rkvpu_open_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoEnc->rkapi_hdl, "vpu_open_context");
        if (pVideoEnc->rkvpu_open_cxt == NULL) {
            omx_err("dlsym vpu_open_context fail");
            dlclose( pVideoEnc->rkapi_hdl);
            return OMX_ErrorHardware;
        }
        pVideoEnc->bIsNewVpu = OMX_FALSE;
    } else {
        pVideoEnc->bIsNewVpu = OMX_TRUE;
    }
    pVideoEnc->rkvpu_close_cxt = (OMX_S32 (*)(VpuCodecContext_t **ctx))dlsym(pVideoEnc->rkapi_hdl, "vpu_close_context");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE Rkvpu_Enc_DebugSwitchfromPropget(
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    RKVPU_OMX_VIDEOENC_COMPONENT  *pVideoEnc         = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    OMX_U32                        nValue = 0;

    if (!Rockchip_OSAL_GetEnvU32("vendor.omx.venc.debug", &omx_venc_debug, 0)
        && omx_venc_debug > 0) {
        omx_info("open video encoder debug, value: 0x%x", omx_venc_debug);
    }

    if (omx_venc_debug & VDEC_DBG_RECORD_OUT) {
        omx_info("Start recording stream to /data/video/enc_out.bin");
        if (pVideoEnc->fp_enc_out != NULL) {
            fclose(pVideoEnc->fp_enc_out);
        }
        pVideoEnc->fp_enc_out = fopen("data/video/enc_out.bin", "wb");
    }

    if (omx_venc_debug & VDEC_DBG_RECORD_IN) {
        omx_info("Start recording stream to /data/video/enc_in.bin");
        if (pVideoEnc->fp_enc_in != NULL) {
            fclose(pVideoEnc->fp_enc_in);
        }
        pVideoEnc->fp_enc_in = fopen("data/video/enc_in.bin", "wb");
    }

    return ret;
}


OMX_ERRORTYPE Rkvpu_Enc_ComponentInit(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT      *pRockchipComponent  = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc    =  (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    OMX_RK_VIDEO_CODINGTYPE codecId = OMX_RK_VIDEO_CodingUnused;
    ROCKCHIP_OMX_BASEPORT           *pRockchipInputPort  = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    VpuCodecContext_t               *p_vpu_ctx           = NULL;
    EncParameter_t *EncParam = NULL;
    RK_U32 new_width = 0, new_height = 0;

    if (omx_open_vpuenc_context(pVideoEnc) != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    if (pRockchipComponent->rkversion != NULL) {
        omx_err("omx decoder info : %s", pRockchipComponent->rkversion);
    }
    if (pVideoEnc->bIsNewVpu == OMX_TRUE) {
        p_vpu_ctx = (VpuCodecContext_t *)Rockchip_OSAL_Malloc(sizeof(VpuCodecContext_t));
    }

    if (pVideoEnc->rkvpu_open_cxt && pVideoEnc->bIsNewVpu == OMX_FALSE) {
        omx_err("open vpu context FALSE");
        pVideoEnc->rkvpu_open_cxt(&p_vpu_ctx);
    }

    int i = 0;
    for (i = 0; i < (int)ARRAY_SIZE(kCodeMap); i++) {
        if (kCodeMap[i].omx_id == pVideoEnc->codecId) {
            codecId = kCodeMap[i].codec_id;
            break;
        }
    }
    if (pVideoEnc->bIsNewVpu == OMX_TRUE) {
        memset(p_vpu_ctx, 0, sizeof(VpuCodecContext_t));
    }
    pVideoEnc->bCurrent_height = pRockchipInputPort->portDefinition.format.video.nFrameHeight;
    pVideoEnc->bCurrent_width = pRockchipInputPort->portDefinition.format.video.nFrameWidth;
    if (pVideoEnc->params_extend.bEnableScaling || pVideoEnc->params_extend.bEnableCropping) {
        if (pVideoEnc->params_extend.bEnableScaling) {
            new_width = pVideoEnc->params_extend.ui16ScaledWidth;
            new_height = pVideoEnc->params_extend.ui16ScaledHeight;
        } else if (pVideoEnc->params_extend.bEnableCropping) {
            new_width =  p_vpu_ctx->width - pVideoEnc->params_extend.ui16CropLeft - pVideoEnc->params_extend.ui16CropRight;
            new_height = p_vpu_ctx->height - pVideoEnc->params_extend.ui16CropTop - pVideoEnc->params_extend.ui16CropBottom;
            omx_trace("CropLeft = %d CropRight = %d CropTop %d CropBottom %d",
                      pVideoEnc->params_extend.ui16CropLeft, pVideoEnc->params_extend.ui16CropRight,
                      pVideoEnc->params_extend.ui16CropTop, pVideoEnc->params_extend.ui16CropBottom);
        }
        if (new_width != pVideoEnc->bCurrent_width ||
            new_height != pVideoEnc->bCurrent_height) {
            pVideoEnc->bCurrent_width  =  new_width;
            pVideoEnc->bCurrent_height =  new_height;
        }
    }

    p_vpu_ctx->codecType = CODEC_ENCODER;
    p_vpu_ctx->videoCoding = codecId;
    p_vpu_ctx->width =  pVideoEnc->bCurrent_width;
    p_vpu_ctx->height = pVideoEnc->bCurrent_height;
    if (pVideoEnc->rkvpu_open_cxt && pVideoEnc->bIsNewVpu == OMX_TRUE) {
        omx_err("open vpu context new");
        pVideoEnc->rkvpu_open_cxt(&p_vpu_ctx);
    }
    if (p_vpu_ctx == NULL) {
        omx_err("open vpu context fail!");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if (p_vpu_ctx->extra_cfg.reserved[0] == 1) {
        omx_info("use vpuapi.");
        pVideoEnc->bIsUseMpp = OMX_FALSE;
    } else {
        omx_info("use mpp.");
        pVideoEnc->bIsUseMpp = OMX_TRUE;
    }
    p_vpu_ctx->private_data = malloc(sizeof(EncParameter_t));
    memset(p_vpu_ctx->private_data, 0, sizeof(EncParameter_t));
    EncParam = (EncParameter_t*)p_vpu_ctx->private_data;
    Rkvpu_Enc_GetEncParams(pOMXComponent, &EncParam);

#ifdef ENCODE_RATE_STATISTIC
    lastEncodeTime = 0;
    currentEncodeTime = 0;
    lastEncodeFrameCount = 0;
    currentEncodeFrameCount = 0;
#endif

    if (p_vpu_ctx) {
        if (p_vpu_ctx->init(p_vpu_ctx, NULL, 0)) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;

        }
        omx_trace("eControlRate %d ", pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]);
        if (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] == OMX_Video_ControlRateConstant) {
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)EncParam);
            EncParam->rc_mode = 1;
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)EncParam);
        }
        if (pVideoEnc->bIsCfgColorAsp) {
            OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *colorAspectsParams = &pVideoEnc->ConfigColorAspects;
            OMX_COLORASPECTS * colorAspect = &(colorAspectsParams->sAspects);
            convertCodecAspectsToIsoColorAspects(colorAspect, pVideoEnc->colorAspects);
            if (p_vpu_ctx !=  NULL) {
                EncParameter_t vpug;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)&vpug);
                vpug.reserved[0] = 1;
                vpug.reserved[1] = (int)pVideoEnc->colorAspects;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)&vpug);
            }
        }
        pVideoEnc->bFrame_num = 0;
        pVideoEnc->bLast_config_frame = 0;
        pVideoEnc->bSpsPpsHeaderFlag = OMX_FALSE;
        pVideoEnc->bSpsPpsbuf = NULL;

        if (p_vpu_ctx->extradata == NULL) {
            omx_err("init get extradata fail!");
            pVideoEnc->bSpsPpsbuf = NULL;
            pVideoEnc->bSpsPpsLen = 0;
            goto EXIT;
        } else {
            if ((p_vpu_ctx->extradata != NULL) && p_vpu_ctx->extradata_size > 0 && p_vpu_ctx->extradata_size <= 2048) {
                pVideoEnc->bSpsPpsbuf = (OMX_U8 *)Rockchip_OSAL_Malloc(2048);
                Rockchip_OSAL_Memcpy(pVideoEnc->bSpsPpsbuf, p_vpu_ctx->extradata, p_vpu_ctx->extradata_size);
                pVideoEnc->bSpsPpsLen = p_vpu_ctx->extradata_size;
            } else {
                omx_err("p_vpu_ctx->extradata = %p,p_vpu_ctx->extradata_size = %d", p_vpu_ctx->extradata, p_vpu_ctx->extradata_size);
            }
        }
    }
    pVideoEnc->bEncSendEos = OMX_FALSE;
    pVideoEnc->enc_vpumem = NULL;
    pVideoEnc->enc_vpumem = (VPUMemLinear_t*)Rockchip_OSAL_Malloc(sizeof( VPUMemLinear_t));
    ret = VPUMallocLinear(pVideoEnc->enc_vpumem, ((EncParam->width + 15) & 0xfff0)
                          * EncParam->height * 4);
    if (ret) {

        omx_err("err  %dtemp->phy_addr %x mWidth %d mHeight %d", ret, pVideoEnc->enc_vpumem->phy_addr,
                EncParam->width, EncParam->height);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }


    if (rga_dev_open(&pVideoEnc->rga_ctx)  < 0) {
        omx_err("open rga device fail!");
    }

    pVideoEnc->bRgb2yuvFlag = OMX_FALSE;
    pVideoEnc->bPixel_format = -1;
#ifdef AVS80
    pVideoEnc->ConfigColorAspects.sAspects.mRange = RangeUnspecified;
    pVideoEnc->ConfigColorAspects.sAspects.mPrimaries = PrimariesUnspecified;
    pVideoEnc->ConfigColorAspects.sAspects.mMatrixCoeffs = MatrixUnspecified;
    pVideoEnc->ConfigColorAspects.sAspects.mTransfer = TransferUnspecified;
#endif
    Rkvpu_Enc_DebugSwitchfromPropget(pRockchipComponent);

    pVideoEnc->vpu_ctx = p_vpu_ctx;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_Enc_GetEncParams(OMX_COMPONENTTYPE *pOMXComponent, EncParameter_t **encParams)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT      *pRockchipComponent  = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT    *pVideoEnc         = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT           *pRockchipInputPort  = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    ROCKCHIP_OMX_BASEPORT           *pRockchipOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    FunctionIn();

    (*encParams)->height = pVideoEnc->bCurrent_height;
    (*encParams)->width = pVideoEnc->bCurrent_width;
    (*encParams)->bitRate = pRockchipOutputPort->portDefinition.format.video.nBitrate;
    (*encParams)->framerate = (pRockchipInputPort->portDefinition.format.video.xFramerate) >> 16;

    if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
        (*encParams)->enableCabac   = 0;
        (*encParams)->cabacInitIdc  = 0;
        (*encParams)->intraPicRate  = pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames;
        switch (pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].eProfile) {
        case OMX_VIDEO_AVCProfileBaseline:
            (*encParams)->profileIdc = BASELINE_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileMain:
            (*encParams)->profileIdc   = MAIN_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileHigh:
            (*encParams)->profileIdc   = HIGHT_PROFILE;
            break;
        default:
            (*encParams)->profileIdc   = BASELINE_PROFILE;
            break;
        }
        switch (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]) {
        case OMX_Video_ControlRateDisable:
            (*encParams)->rc_mode = Video_RC_Mode_Disable;
            break;
        case OMX_Video_ControlRateVariable:
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            break;
        case OMX_Video_ControlRateConstant:
            (*encParams)->rc_mode = Video_RC_Mode_CBR;
            break;
        default:
            omx_err("unknown rate control mode = %d, forced to VBR mode",
                    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]);
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            break;
        }
        switch (pRockchipInputPort->portDefinition.format.video.eColorFormat) {
        case OMX_COLOR_FormatAndroidOpaque: {
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            (*encParams)->format = VPU_H264ENC_RGB888;
        }
        break;
        case OMX_COLOR_FormatYUV420Planar: {
            (*encParams)->format = VPU_H264ENC_YUV420_PLANAR;
        }
        break;
        case OMX_COLOR_FormatYUV420SemiPlanar: {
            (*encParams)->format = VPU_H264ENC_YUV420_SEMIPLANAR;
        }
        break;
        default:
            omx_err("inputPort colorformat is not support format = %d",
                    pRockchipInputPort->portDefinition.format.video.eColorFormat);
            break;
        }
        void *levelTmp = &((*encParams)->levelIdc);
        AVCLevel *encLevel = (AVCLevel *)levelTmp;
        ConvertOmxAvcLevelToAvcSpecLevel((int32_t)pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].eLevel, encLevel);
    } else if (pVideoEnc->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC) {
        (*encParams)->enableCabac   = 0;
        (*encParams)->cabacInitIdc  = 0;
        (*encParams)->intraPicRate  = pVideoEnc->HEVCComponent[OUTPUT_PORT_INDEX].nKeyFrameInterval;
        void *profileTmp = &((*encParams)->profileIdc);
        HEVCEncProfile *encProfile = (HEVCEncProfile *)profileTmp;
        void *levelTmp = &((*encParams)->levelIdc);
        HEVCLevel *encLevel = (HEVCLevel *)levelTmp;
        ConvertOmxHevcProfile2HalHevcProfile(pVideoEnc->HEVCComponent[OUTPUT_PORT_INDEX].eProfile,
                                             encProfile);
        ConvertOmxHevcLevel2HalHevcLevel(pVideoEnc->HEVCComponent[OUTPUT_PORT_INDEX].eLevel,
                                         encLevel);
        switch (pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]) {
        case OMX_Video_ControlRateDisable:
            (*encParams)->rc_mode = Video_RC_Mode_Disable;
            break;
        case OMX_Video_ControlRateVariable:
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            break;
        case OMX_Video_ControlRateConstant:
            (*encParams)->rc_mode = Video_RC_Mode_CBR;
            break;
        default:
            omx_err("unknown rate control mode = %d, forced to VBR mode",
                    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX]);
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            break;
        }
        switch (pRockchipInputPort->portDefinition.format.video.eColorFormat) {
        case OMX_COLOR_FormatAndroidOpaque: {
            (*encParams)->rc_mode = Video_RC_Mode_VBR;
            (*encParams)->format = VPU_H264ENC_RGB888;
        }
        break;
        case OMX_COLOR_FormatYUV420Planar: {
            (*encParams)->format = VPU_H264ENC_YUV420_PLANAR;
        }
        break;
        case OMX_COLOR_FormatYUV420SemiPlanar: {
            (*encParams)->format = VPU_H264ENC_YUV420_SEMIPLANAR;
        }
        break;
        default:
            omx_err("inputPort colorformat is not support format = %d",
                    pRockchipInputPort->portDefinition.format.video.eColorFormat);
            break;
        }
    }

    omx_info("encode params init settings:\n"
             "width = %d\n"
             "height = %d\n"
             "bitRate = %d\n"
             "framerate = %d\n"
             "format = %d\n"
             "enableCabac = %d,\n"
             "cabacInitIdc = %d,\n"
             "intraPicRate = %d,\n"
             "profileIdc = %d,\n"
             "levelIdc = %d,\n"
             "rc_mode = %d,\n",
             (int)(*encParams)->width,
             (int)(*encParams)->height,
             (int)(*encParams)->bitRate,
             (int)(*encParams)->framerate,
             (int)(*encParams)->format,
             (int)(*encParams)->enableCabac,
             (int)(*encParams)->cabacInitIdc,
             (int)(*encParams)->intraPicRate,
             (int)(*encParams)->profileIdc,
             (int)(*encParams)->levelIdc,
             (int)(*encParams)->rc_mode);

    FunctionOut();
    return ret;
}

OMX_ERRORTYPE Rkvpu_Enc_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT      *pRockchipComponent  = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEOENC_COMPONENT    *pVideoEnc         = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

    FunctionIn();
    if (pVideoEnc->vpu_ctx) {
        if (pVideoEnc->rkvpu_close_cxt) {
            pVideoEnc->rkvpu_close_cxt(&pVideoEnc->vpu_ctx);
        }
        pVideoEnc->vpu_ctx = NULL;
        if (pVideoEnc->rkapi_hdl) {
            dlclose( pVideoEnc->rkapi_hdl);
            pVideoEnc->rkapi_hdl = NULL;
        }
    }

    if (pVideoEnc->bSpsPpsbuf) {
        Rockchip_OSAL_Free(pVideoEnc->bSpsPpsbuf);
        pVideoEnc->bSpsPpsbuf = NULL;
    }

    if (pVideoEnc->enc_vpumem) {
        VPUFreeLinear(pVideoEnc->enc_vpumem);
        Rockchip_OSAL_Free(pVideoEnc->enc_vpumem);
        pVideoEnc->enc_vpumem = NULL;
    }

    if (pVideoEnc->rga_ctx != NULL) {
        rga_dev_close(pVideoEnc->rga_ctx);
        pVideoEnc->rga_ctx = NULL;
    }

    pVideoEnc->bEncSendEos = OMX_FALSE;

    Rkvpu_ResetAllPortConfig(pOMXComponent);

    goto EXIT;
EXIT:
    FunctionOut();

    return ret;
}


OMX_ERRORTYPE Rockchip_OMX_ComponentConstructor(OMX_HANDLETYPE hComponent, OMX_STRING componentName)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;

    FunctionIn();

    if ((hComponent == NULL) || (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        omx_err("OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        omx_err("OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    ret = Rockchip_OMX_BaseComponent_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        omx_err("OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    ret = Rockchip_OMX_Port_Constructor(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Rockchip_OMX_BaseComponent_Destructor(pOMXComponent);
        omx_err("OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }

    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pVideoEnc = Rockchip_OSAL_Malloc(sizeof(RKVPU_OMX_VIDEOENC_COMPONENT));
    if (pVideoEnc == NULL) {
        Rockchip_OMX_BaseComponent_Destructor(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    Rockchip_OSAL_Memset(pVideoEnc, 0, sizeof(RKVPU_OMX_VIDEOENC_COMPONENT));

    pVideoEnc->hSharedMemory = Rockchip_OSAL_SharedMemory_Open();
    if ( pVideoEnc->hSharedMemory == NULL) {
        omx_err("Rockchip_OSAL_SharedMemory_Open open fail");
    }
    pRockchipComponent->componentName = (OMX_STRING)Rockchip_OSAL_Malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pRockchipComponent->componentName == NULL) {
        Rockchip_OMX_ComponentDeInit(hComponent);
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    /* Set componentVersion */
    pRockchipComponent->componentVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pRockchipComponent->componentVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pRockchipComponent->componentVersion.s.nRevision     = REVISION_NUMBER;
    pRockchipComponent->componentVersion.s.nStep         = STEP_NUMBER;
    /* Set specVersion */
    pRockchipComponent->specVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pRockchipComponent->specVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pRockchipComponent->specVersion.s.nRevision     = REVISION_NUMBER;
    pRockchipComponent->specVersion.s.nStep         = STEP_NUMBER;
    Rockchip_OSAL_Memset(pRockchipComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);
    pRockchipComponent->hComponentHandle = (OMX_HANDLETYPE)pVideoEnc;

    pRockchipComponent->bSaveFlagEOS = OMX_FALSE;
    pRockchipComponent->bBehaviorEOS = OMX_FALSE;
    pRockchipComponent->bMultiThreadProcess = OMX_TRUE;
    pRockchipComponent->codecType = HW_VIDEO_ENC_CODEC;

    pVideoEnc->bFirstFrame = OMX_TRUE;
    pVideoEnc->bFirstInput = OMX_TRUE;
    pVideoEnc->bFirstOutput = OMX_TRUE;
    pVideoEnc->configChange = OMX_FALSE;
    pVideoEnc->bStoreMetaData = OMX_FALSE;
    pVideoEnc->bPrependSpsPpsToIdr = OMX_FALSE;
    pVideoEnc->bRkWFD = OMX_FALSE;
    pVideoEnc->quantization.nQpI = 4; // I frame quantization parameter
    pVideoEnc->quantization.nQpP = 5; // P frame quantization parameter
    pVideoEnc->quantization.nQpB = 5; // B frame quantization parameter
    //add by xlm for use mpp or vpuapi
    pVideoEnc->bIsUseMpp = OMX_FALSE;
    pVideoEnc->bIsNewVpu = OMX_TRUE;
    pVideoEnc->bIsCfgColorAsp = OMX_FALSE;
    pVideoEnc->colorAspects = Rockchip_OSAL_Malloc(sizeof(OMX_COLORASPECTS));

    Rockchip_OSAL_MutexCreate(&pVideoEnc->bScale_Mutex);
    Rockchip_OSAL_MutexCreate(&pVideoEnc->bRecofig_Mutex);
    /* Input port */
    pRockchipPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    pRockchipPort->portDefinition.nBufferCountActual = MAX_VIDEOENC_INPUTBUFFER_NUM;
    pRockchipPort->portDefinition.nBufferCountMin = MAX_VIDEOENC_INPUTBUFFER_NUM;
    pRockchipPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_INPUT_BUFFER_SIZE;
    pRockchipPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pRockchipPort->portDefinition.format.video.nFrameWidth = DEFAULT_ENC_FRAME_WIDTH;
    pRockchipPort->portDefinition.format.video.nFrameHeight = DEFAULT_ENC_FRAME_HEIGHT;
    pRockchipPort->portDefinition.format.video.xFramerate = DEFAULT_ENC_FRAME_FRAMERATE;
    pRockchipPort->portDefinition.format.video.nBitrate = DEFAULT_ENC_FRAME_BITRATE;
    pRockchipPort->portDefinition.format.video.nStride = 0; /*DEFAULT_ENC_FRAME_WIDTH;*/
    pRockchipPort->portDefinition.format.video.nSliceHeight = 0;
    pRockchipPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_INPUT_BUFFER_SIZE;
    pRockchipPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pRockchipPort->portDefinition.format.video.cMIMEType =  Rockchip_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    Rockchip_OSAL_Strcpy(pRockchipPort->portDefinition.format.video.cMIMEType, "raw/video");
    pRockchipPort->portDefinition.format.video.pNativeRender = 0;
    pRockchipPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pRockchipPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    pRockchipPort->portDefinition.bEnabled = OMX_TRUE;
    pRockchipPort->portWayType = WAY2_PORT;
    pVideoEnc->eControlRate[INPUT_PORT_INDEX] = OMX_Video_ControlRateDisable;
    pRockchipPort->bStoreMetaData = OMX_FALSE;

    /* Output port */
    pRockchipPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    pRockchipPort->portDefinition.nBufferCountActual = MAX_VIDEOENC_OUTPUTBUFFER_NUM;
    pRockchipPort->portDefinition.nBufferCountMin = MAX_VIDEOENC_OUTPUTBUFFER_NUM;
    pRockchipPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE;
    pRockchipPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pRockchipPort->portDefinition.format.video.nFrameWidth = DEFAULT_ENC_FRAME_WIDTH;
    pRockchipPort->portDefinition.format.video.nFrameHeight = DEFAULT_ENC_FRAME_HEIGHT;
    pRockchipPort->portDefinition.format.video.xFramerate = DEFAULT_ENC_FRAME_FRAMERATE;
    pRockchipPort->portDefinition.format.video.nBitrate = DEFAULT_ENC_FRAME_BITRATE;
    pRockchipPort->portDefinition.format.video.nStride = 0; /*DEFAULT_ENC_FRAME_WIDTH;*/
    pRockchipPort->portDefinition.format.video.nSliceHeight = 0;
    pRockchipPort->portDefinition.nBufferSize = DEFAULT_VIDEOENC_OUTPUT_BUFFER_SIZE;
    pRockchipPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;

    pRockchipPort->portDefinition.format.video.cMIMEType = Rockchip_OSAL_Malloc(MAX_OMX_MIMETYPE_SIZE);
    Rockchip_OSAL_Memset(pRockchipPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    pRockchipPort->portDefinition.format.video.pNativeRender = 0;
    pRockchipPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pRockchipPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pRockchipPort->portDefinition.bEnabled = OMX_TRUE;
    pRockchipPort->portWayType = WAY2_PORT;
    pRockchipPort->portDefinition.eDomain = OMX_PortDomainVideo;
    pVideoEnc->eControlRate[OUTPUT_PORT_INDEX] = OMX_Video_ControlRateDisable;


    pOMXComponent->UseBuffer              = &Rkvpu_OMX_UseBuffer;
    pOMXComponent->AllocateBuffer         = &Rkvpu_OMX_AllocateBuffer;
    pOMXComponent->FreeBuffer             = &Rkvpu_OMX_FreeBuffer;
    pOMXComponent->ComponentTunnelRequest = &Rkvpu_OMX_ComponentTunnelRequest;
    pOMXComponent->GetParameter           = &Rkvpu_OMX_GetParameter;
    pOMXComponent->SetParameter           = &Rkvpu_OMX_SetParameter;
    pOMXComponent->GetConfig              = &Rkvpu_OMX_GetConfig;
    pOMXComponent->SetConfig              = &Rkvpu_OMX_SetConfig;
    pOMXComponent->GetExtensionIndex      = &Rkvpu_OMX_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum      = &Rkvpu_OMX_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit        = &Rockchip_OMX_ComponentDeInit;

    pRockchipComponent->rockchip_codec_componentInit      = &Rkvpu_Enc_ComponentInit;
    pRockchipComponent->rockchip_codec_componentTerminate = &Rkvpu_Enc_Terminate;

    pRockchipComponent->rockchip_AllocateTunnelBuffer = &Rkvpu_OMX_AllocateTunnelBuffer;
    pRockchipComponent->rockchip_FreeTunnelBuffer     = &Rkvpu_OMX_FreeTunnelBuffer;
    pRockchipComponent->rockchip_BufferProcessCreate    = &Rkvpu_OMX_BufferProcess_Create;
    pRockchipComponent->rockchip_BufferProcessTerminate = &Rkvpu_OMX_BufferProcess_Terminate;
    pRockchipComponent->rockchip_BufferFlush          = &Rkvpu_OMX_BufferFlush;

    if (!strcmp(componentName, RK_OMX_COMPONENT_H264_ENC)) {
        int i = 0;
        Rockchip_OSAL_Memset(pRockchipPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        Rockchip_OSAL_Strcpy(pRockchipPort->portDefinition.format.video.cMIMEType, "video/avc");
        for (i = 0; i < ALL_PORT_NUM; i++) {
            INIT_SET_SIZE_VERSION(&pVideoEnc->AVCComponent[i], OMX_VIDEO_PARAM_AVCTYPE);
            pVideoEnc->AVCComponent[i].nPortIndex = i;
            pVideoEnc->AVCComponent[i].eProfile   = OMX_VIDEO_AVCProfileBaseline;
            pVideoEnc->AVCComponent[i].eLevel     = OMX_VIDEO_AVCLevel31;
            pVideoEnc->AVCComponent[i].nPFrames = 20;
        }
        pVideoEnc->codecId = OMX_VIDEO_CodingAVC;
        pRockchipPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    } else if (!strcmp(componentName, RK_OMX_COMPONENT_VP8_ENC)) {
        Rockchip_OSAL_Memset(pRockchipPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        Rockchip_OSAL_Strcpy(pRockchipPort->portDefinition.format.video.cMIMEType, "video/x-vnd.on2.vp8");
        pVideoEnc->codecId = OMX_VIDEO_CodingVP8;
        pRockchipPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
    } else if (!strcmp(componentName, RK_OMX_COMPONENT_HEVC_ENC)) {
        int i = 0;
        Rockchip_OSAL_Memset(pRockchipPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
        Rockchip_OSAL_Strcpy(pRockchipPort->portDefinition.format.video.cMIMEType, "video/hevc");
        for (i = 0; i < ALL_PORT_NUM; i++) {
            INIT_SET_SIZE_VERSION(&pVideoEnc->HEVCComponent[i], OMX_VIDEO_PARAM_HEVCTYPE);
            pVideoEnc->HEVCComponent[i].nPortIndex = i;
            pVideoEnc->HEVCComponent[i].eProfile   = OMX_VIDEO_HEVCProfileMain;
            pVideoEnc->HEVCComponent[i].eLevel     = OMX_VIDEO_HEVCMainTierLevel41;
            pVideoEnc->HEVCComponent[i].nKeyFrameInterval = 20;
        }
        pVideoEnc->codecId = OMX_VIDEO_CodingHEVC;
        pRockchipPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
    } else {
        // IL client specified an invalid component name
        omx_err("VPU Component Invalid Component Name\n");
        ret =  OMX_ErrorInvalidComponentName;
        goto EXIT;
    }
    pRockchipComponent->currentState = OMX_StateLoaded;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_ComponentDeInit(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    int                    i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    if (pVideoEnc->fp_enc_out != NULL) {
        fclose(pVideoEnc->fp_enc_out);
    }
    if (pVideoEnc->colorAspects != NULL) {
        Rockchip_OSAL_Free(pVideoEnc->colorAspects);
    }
    Rockchip_OSAL_MutexTerminate(pVideoEnc->bScale_Mutex);
    Rockchip_OSAL_MutexTerminate(pVideoEnc->bRecofig_Mutex);
    if (pVideoEnc->hSharedMemory != NULL) {
        Rockchip_OSAL_SharedMemory_Close(pVideoEnc->hSharedMemory, OMX_FALSE);
        pVideoEnc->hSharedMemory = NULL;
    }

    Rockchip_OSAL_Free(pVideoEnc);
    pRockchipComponent->hComponentHandle = pVideoEnc = NULL;

    pRockchipPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    if (pRockchipPort->processData.extInfo != NULL) {
        Rockchip_OSAL_Free(pRockchipPort->processData.extInfo);
        pRockchipPort->processData.extInfo = NULL;
    }

    for (i = 0; i < ALL_PORT_NUM; i++) {
        pRockchipPort = &pRockchipComponent->pRockchipPort[i];
        Rockchip_OSAL_Free(pRockchipPort->portDefinition.format.video.cMIMEType);
        if (pRockchipPort->portDefinition.format.video.cMIMEType != NULL)
            pRockchipPort->portDefinition.format.video.cMIMEType = NULL;
    }

    ret = Rockchip_OMX_Port_Destructor(pOMXComponent);

    ret = Rockchip_OMX_BaseComponent_Destructor(hComponent);

EXIT:
    FunctionOut();

    return ret;
}
