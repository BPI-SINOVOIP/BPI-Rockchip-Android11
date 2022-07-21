/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <cutils/log.h>
#include <speex/speex_preprocess.h>
#include <dlfcn.h>
#include "rkdenoise.h"
#include "skv/skv_anr.h"

//#define LOG_NDEBUG 0
#define LOG_TAG "RKDENOISE"

typedef struct SKVANRParam_ {
    float noiseFactor;
    int swU;
    float PsiMin;
    float PsiMax;
    float fGmin;
    int frameType;
} SKVANRParam;

typedef struct _SKV_APIS_ {
    int period;
    void* st_anr;
    SKVANRParam *param;
    rkaudio_anr_param_deinit anr_deinit;
    skv_anr_param_printf anr_printf;
    skv_anr_destory anr_destory;
    skv_anr_process_time anr_process;
    skv_anrstruct_bank_init anr_init;
} SKV_APIS;

typedef struct _DENOISE_STATE_ {
    int period;
    int rate;
    int ch;
    int flag;
    SpeexPreprocessState* mSpeexState;
    int mSpeexFrameSize;
    int16_t *mSpeexPcmIn;
    void *hskvlib;
    SKV_APIS skvapi;
} DENOISE_STATE;

SKVANRParam *_rkaudio_anr_param_init(int rate, int ch, int period)
{
    SKVANRParam *param = NULL;
    int frameType = -1;

    if (rate == 48000) {
        if (period == 480)
            frameType = 0;
        else if (period == 768)
            frameType = 1;
    } else if (rate == 44100) {
        if (period == 441)
            frameType = 0;
    } else if (rate == 32000) {
        if (period == 320)
            frameType = 0;
        else if (period == 512)
            frameType = 1;
    } else if (rate == 16000) {
        if (period == 160)
            frameType = 0;
        else if (period == 256)
            frameType = 1;
    } else if (rate == 8000) {
        if (period == 80)
            frameType = 0;
        else if (period == 128)
            frameType = 1;
    }
    if (frameType == -1)
        return NULL;
    param = (SKVANRParam*)malloc(sizeof(SKVANRParam));
    if (!param)
        return NULL;
    param->noiseFactor = 0.88;
    param->swU = 10;
    param->PsiMin = 0.05;
    param->PsiMax = 0.516;
    param->fGmin = 0.1;
    param->frameType = frameType; /* 0 stands for 10ms; 1 stands for 16ms */
    return param;
}

#define LOADE_AND_CHECK(h, api, name)\
    ALOGD("%s: loading api(%s)...", __FUNCTION__, name);\
    api = (typeof(api))dlsym(h, name);\
    if (!api) {\
        ALOGE("%s: load api(%s)fail:(%s)!!!", __FUNCTION__, name, dlerror());\
        goto load_exit;\
    }
int _skv_denoise_create(DENOISE_STATE *pDenoiseState, int rate, int ch, int period)
{
    SKV_APIS *pskvapi = &pDenoiseState->skvapi;
    void *hskvlib;
    int frame_size;
    const char *libname = "/vendor/lib/hw/libanr.so";

    hskvlib = dlopen(libname, RTLD_LAZY);
    if (hskvlib == NULL) {
        ALOGE("%s: loader libanr.so fail!!!", __FUNCTION__);
        goto fail_exit;
    }
    LOADE_AND_CHECK(hskvlib, pskvapi->anr_deinit, "rkaudio_anr_param_deinit")
    LOADE_AND_CHECK(hskvlib, pskvapi->anr_printf, "skv_anr_param_printf")
    LOADE_AND_CHECK(hskvlib, pskvapi->anr_destory, "skv_anr_destory")
    LOADE_AND_CHECK(hskvlib, pskvapi->anr_process, "skv_anr_process_time")
    LOADE_AND_CHECK(hskvlib, pskvapi->anr_init, "skv_anrstruct_bank_init")
    pskvapi->param = _rkaudio_anr_param_init(rate, ch, period);
    if (!pskvapi->param) {
        ALOGE("%s: para init error\n", __FUNCTION__);
        goto load_exit;
    }
    pskvapi->st_anr = pskvapi->anr_init(rate, ch, &frame_size, pskvapi->param);
    if (!pskvapi->st_anr) {
        ALOGE("%s: Failed to create audio preprocess handle\n", __FUNCTION__);
        goto load_exit;
    }
    pDenoiseState->hskvlib = hskvlib;
    pskvapi->period = frame_size;
    ALOGD("%s: skv denoise create okay period:%d", __FUNCTION__, pskvapi->period);
    return 0;
load_exit:
    if (hskvlib)
        dlclose(hskvlib);
    if (pskvapi->param)
        free(pskvapi->param);
    pskvapi->param = NULL;
    pDenoiseState->hskvlib = NULL;
fail_exit:
    return -1;
}
#undef LOADE_AND_CHECK

int _spx_denoise_create(DENOISE_STATE *pDenoiseState, int rate, int ch, int period)
{
    int denoise = 1;
    int noiseSuppress = -24;

    pDenoiseState->mSpeexFrameSize = period;
    pDenoiseState->mSpeexPcmIn = malloc(sizeof(int16_t) * pDenoiseState->mSpeexFrameSize);
    if(!pDenoiseState->mSpeexPcmIn) {
        ALOGE("speexPcmIn malloc failed");
        return -1;
    }
    pDenoiseState->mSpeexState = speex_preprocess_state_init(pDenoiseState->mSpeexFrameSize, pDenoiseState->rate);
    if(pDenoiseState->mSpeexState == NULL) {
        ALOGE("speex error");
        goto err_speex_malloc;
    }
    speex_preprocess_ctl(pDenoiseState->mSpeexState, SPEEX_PREPROCESS_SET_DENOISE, &denoise);
    speex_preprocess_ctl(pDenoiseState->mSpeexState, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noiseSuppress);
    return 0;
err_speex_malloc:
    if (pDenoiseState->mSpeexPcmIn)
        free(pDenoiseState->mSpeexPcmIn);
    pDenoiseState->mSpeexPcmIn = NULL;
    return -1;
}

hrkdeniose rkdenoise_create(int rate, int ch, int period, uint32_t flag)
{
    DENOISE_STATE *pDenoiseState = (DENOISE_STATE*)malloc(sizeof(DENOISE_STATE));

    ALOGD("%s: rate:%d ch:%d, flag:%x", __FUNCTION__, rate, ch, flag);
    if (!pDenoiseState) {
        ALOGE("pDenoiseState malloc failed");
        return NULL;
    }
    memset(pDenoiseState, 0, sizeof(*pDenoiseState));
    if ((flag&ALG_SKV) || (flag&ALG_AUTO))
        flag = ALG_SKV;
    pDenoiseState->flag  = flag;
    pDenoiseState->ch = ch;
    pDenoiseState->rate = rate;
    if (pDenoiseState->flag & ALG_SPX) {
        if (_spx_denoise_create(pDenoiseState, rate, ch, period))
            goto exit;
        pDenoiseState->period = pDenoiseState->mSpeexFrameSize;
    } else if (pDenoiseState->flag & ALG_SKV) {
        if (_skv_denoise_create(pDenoiseState, rate, ch, period))
            goto exit;
        pDenoiseState->period = pDenoiseState->skvapi.period;
    }
    return (hrkdeniose)pDenoiseState;
exit:
    return NULL;
}

int rkdenoise_get_period(hrkdeniose context)
{
    DENOISE_STATE *pDenoiseState = (DENOISE_STATE *)context;

    return pDenoiseState->period;
}

int _skv_denoise_process(DENOISE_STATE *pDenoiseState, void *bufferin, int bytes, void *bufferout)
{
    SKV_APIS *skvapi = &pDenoiseState->skvapi;
    int index = 0;
    int startPos = 0;
    spx_int16_t* data = (spx_int16_t*) bufferin;
    int ch = pDenoiseState->ch;
    int curFrameSize = bytes/(ch*sizeof(int16_t));
    long cid;
    int out_size;

    ALOGV("%s: ch:%d, framesize:%d", __FUNCTION__, ch, pDenoiseState->period);
    if(curFrameSize != skvapi->period) {
        ALOGW("%s:frame size mismatch skv FrameSize %d curFrameSize:%d( bytes:%d)",
              __FUNCTION__, skvapi->period, curFrameSize, bytes);
    }
    out_size = skvapi->anr_process(bufferin, bufferout, skvapi->st_anr);
    if (curFrameSize != out_size) {
        ALOGD("%s:_skv_denoise_process, in_size(%d) != out_size(%d)",
              __FUNCTION__, curFrameSize, out_size);
    }
    return 0;
}

int _spx_denoise_process(DENOISE_STATE *pDenoiseState, void *bufferin, int bytes, void *bufferout)
{
        int index = 0;
        int startPos = 0;
        spx_int16_t* data = (spx_int16_t*) bufferin;
        int ch = pDenoiseState->ch;
        int curFrameSize = bytes/(ch*sizeof(int16_t));
        long cid;

        ALOGV("%s: ch:%d, framesize:%d", __FUNCTION__, ch, pDenoiseState->period);
        if(curFrameSize != pDenoiseState->period) {
            ALOGW("%s:frame size mismatch speex FrameSize %d curFrameSize:%d( bytes:%d)",
                  __FUNCTION__, pDenoiseState->period, curFrameSize, bytes);
        }
        while(curFrameSize >= startPos + pDenoiseState->mSpeexFrameSize) {
            if( 2 == ch) {
                for(index = startPos; index < startPos +pDenoiseState->mSpeexFrameSize; index++ )
                    pDenoiseState->mSpeexPcmIn[index-startPos] = data[index*ch]/2 + data[index*ch+1]/2;
            } else {
                for(index = startPos; index< startPos +pDenoiseState->mSpeexFrameSize; index++ )
                    pDenoiseState->mSpeexPcmIn[index-startPos] = data[index*ch];
            }
            speex_preprocess_run(pDenoiseState->mSpeexState, pDenoiseState->mSpeexPcmIn);
#ifndef TARGET_RK2928
            for(cid = 0 ; cid < ch; cid++)
                for(index = startPos; index< startPos + pDenoiseState->mSpeexFrameSize ; index++ ) {
                    data[index*ch + cid] = pDenoiseState->mSpeexPcmIn[index-startPos];
                }
#else
            for(index = startPos; index< startPos + pDenoiseState->mSpeexFrameSize ; index++ ) {
                int tmp = (int)pDenoiseState->mSpeexPcmIn[index-startPos]+ pDenoiseState->mSpeexPcmIn[index-startPos]/2;
                data[index*ch+0] = tmp > 32767 ? 32767 : (tmp < -32768 ? -32768 : tmp);
            }
            for(cid = 1 ; cid < ch; cid++)
                for(index = startPos; index < startPos + pDenoiseState->mSpeexFrameSize ; index++ ) {
                    data[index*ch + cid] = data[index*ch+0];
                }
#endif
            startPos += pDenoiseState->mSpeexFrameSize;
        }
        return 0;
}

int rkdenoise_process(hrkdeniose context, void *bufferin, int bytes, void *bufferout)
{
    DENOISE_STATE *pDenoiseState = (DENOISE_STATE *)context;

    if (pDenoiseState == NULL) {
        ALOGE("%s: pDenoiseState NULL", __FUNCTION__);
        return -1;
    }
    if (pDenoiseState->flag & ALG_SPX) {
        return _spx_denoise_process(pDenoiseState, bufferin, bytes, bufferout);
    } else if (pDenoiseState->flag & ALG_SKV){
        return _skv_denoise_process(pDenoiseState, bufferin, bytes, bufferout);
    }
    return -1;
}

void rkdenoise_destroy(hrkdeniose context)
{
    DENOISE_STATE *pDenoiseState = (DENOISE_STATE *)context;

    ALOGD("%s: rkdenoise context destroy", __FUNCTION__);
    if(pDenoiseState) {
        if(pDenoiseState->mSpeexPcmIn)
            free(pDenoiseState->mSpeexPcmIn);
        if(pDenoiseState->hskvlib) {
            SKV_APIS *skvapi = &pDenoiseState->skvapi;
            if (skvapi->anr_destory && skvapi->st_anr)
                skvapi->anr_destory(skvapi->st_anr);
            if (skvapi->anr_deinit)
                skvapi->anr_deinit(skvapi->param);
            dlclose(pDenoiseState->hskvlib);
        }
        free(pDenoiseState);
    }
    return;
}
