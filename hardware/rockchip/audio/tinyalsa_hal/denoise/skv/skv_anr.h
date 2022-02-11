
#ifndef SKV_ANR_H
#define SKV_ANR_H

/*--------------------------------------- include -----------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*rkaudio_anr_param_deinit)(void* param_);
/*----------------------------------------- º¯ÊýÉùÃ÷ ---------------------------------------*/
typedef void (*skv_anr_param_printf)(void* param_);
typedef void (*skv_anr_destory)(void* pstAnr);
typedef int (*skv_anr_process_time)(short* pfSigIn, short* pfSigOut, void* pstAnr);
typedef void* (*skv_anrstruct_bank_init)(int mSampleRate, int nb_mic, int* frame_size, void* param_);
#ifdef __cplusplus
}
#endif

#endif