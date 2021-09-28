/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : Sytem Wrapper Layer
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: dwl.h,v $
--  $Revision: 1.19 $
--  $Date: 2010/05/11 09:33:19 $
--
------------------------------------------------------------------------------*/
#ifndef __DWL_H__
#define __DWL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "vpu_type.h"
#include "decapicommon.h"
#include "vpu_mem.h"
#include "vpu.h"

#define NULL ((void *)0)
#define DWL_OK                      0
#define DWL_ERROR                  -1

#define DWL_HW_WAIT_OK              DWL_OK
#define DWL_HW_WAIT_ERROR           DWL_ERROR
#define DWL_HW_WAIT_TIMEOUT         1

#define DWL_CLIENT_TYPE_H264_DEC         1U
#define DWL_CLIENT_TYPE_MPEG4_DEC        2U
#define DWL_CLIENT_TYPE_JPEG_DEC         3U
#define DWL_CLIENT_TYPE_PP               4U
#define DWL_CLIENT_TYPE_VC1_DEC          5U
#define DWL_CLIENT_TYPE_MPEG2_DEC        6U
#define DWL_CLIENT_TYPE_AVS_DEC          9U /* TODO: fix */
#define DWL_CLIENT_TYPE_VP8_DEC          10U

    /* Linear memory area descriptor */
    typedef struct DWLLinearMem
    {
        RK_U32 *virtualAddress;
        RK_U32 busAddress;
        RK_U32 size;
    } DWLLinearMem_t;

    /* DWLInitParam is used to pass parameters when initializing the DWL */
    typedef struct DWLInitParam
    {
        RK_U32 clientType;
    } DWLInitParam_t;

    /* Hardware configuration description */

    typedef struct DWLHwConfig
    {
        RK_U32 maxDecPicWidth;  /* Maximum video decoding width supported  */
        RK_U32 maxPpOutPicWidth;   /* Maximum output width of Post-Processor */
        RK_U32 h264Support;     /* HW supports h.264 */
        RK_U32 jpegSupport;     /* HW supports JPEG */
        RK_U32 mpeg4Support;    /* HW supports MPEG-4 */
        RK_U32 customMpeg4Support; /* HW supports custom MPEG-4 features */
        RK_U32 vc1Support;      /* HW supports VC-1 Simple */
        RK_U32 mpeg2Support;    /* HW supports MPEG-2 */
        RK_U32 ppSupport;       /* HW supports post-processor */
        RK_U32 ppConfig;        /* HW post-processor functions bitmask */
        RK_U32 resv3Support;
        RK_U32 refBufSupport;   /* HW supports reference picture buffering */
        RK_U32 resv2Support;
        RK_U32 vp7Support;      /* HW supports VP7 */
        RK_U32 vp8Support;      /* HW supports VP8 */
        RK_U32 avsSupport;      /* HW supports AVS */
        RK_U32 jpegESupport;    /* HW supports JPEG extensions */
        RK_U32 resv0Support;
        RK_U32 mvcSupport;      /* HW supports H264 MVC extension */
    } DWLHwConfig_t;

	typedef struct DWLHwFuseStatus
    {
        RK_U32 h264SupportFuse;     /* HW supports h.264 */
        RK_U32 mpeg4SupportFuse;    /* HW supports MPEG-4 */
        RK_U32 mpeg2SupportFuse;    /* HW supports MPEG-2 */
        RK_U32 resv3SupportFuse;
		RK_U32 jpegSupportFuse;     /* HW supports JPEG */
        RK_U32 resv2SupportFuse;
        RK_U32 vp7SupportFuse;      /* HW supports VP7 */
        RK_U32 vp8SupportFuse;      /* HW supports VP8 */
        RK_U32 vc1SupportFuse;      /* HW supports VC-1 Simple */
		RK_U32 jpegProgSupportFuse; /* HW supports Progressive JPEG */
        RK_U32 ppSupportFuse;       /* HW supports post-processor */
        RK_U32 ppConfigFuse;        /* HW post-processor functions bitmask */
        RK_U32 maxDecPicWidthFuse;  /* Maximum video decoding width supported  */
        RK_U32 maxPpOutPicWidthFuse; /* Maximum output width of Post-Processor */
        RK_U32 refBufSupportFuse;   /* HW supports reference picture buffering */
		RK_U32 avsSupportFuse;      /* one of the AVS values defined above */
		RK_U32 resv0SupportFuse;
		RK_U32 mvcSupportFuse;
        RK_U32 customMpeg4SupportFuse; /* Fuse for custom MPEG-4 */

    } DWLHwFuseStatus_t;

/* HW ID retriving, static implementation */
    RK_U32 DWLReadAsicID(void);

/* HW configuration retrieving, static implementation */
    void DWLReadAsicConfig(DWLHwConfig_t * pHwCfg);

/* HW fuse retrieving, static implementation */
	void DWLReadAsicFuseStatus(DWLHwFuseStatus_t * pHwFuseSts);

/* DWL initilaization and release */
    const void *DWLInit(DWLInitParam_t * param);
    RK_S32 DWLRelease(const void *instance);

/* HW sharing */
    RK_S32 DWLReserveHw(const void *instance);
    void DWLReleaseHw(const void *instance);

/* Frame buffers memory */
    RK_S32 DWLMallocRefFrm(const void *instance, RK_U32 size, DWLLinearMem_t * info);
    void DWLFreeRefFrm(const void *instance, DWLLinearMem_t * info);

/* SW/HW shared memory */
    RK_S32 DWLMallocLinear(const void *instance, RK_U32 size, DWLLinearMem_t * info);
    void DWLFreeLinear(const void *instance, DWLLinearMem_t * info);

/* D-Cache coherence */
    void DWLDCacheRangeFlush(const void *instance, DWLLinearMem_t * info);  /* NOT in use */
    void DWLDCacheRangeRefresh(const void *instance, DWLLinearMem_t * info);    /* NOT in use */

/* Register access */
    void DWLWriteReg(const void *instance, RK_U32 offset, RK_U32 value);
    RK_U32 DWLReadReg(const void *instance, RK_U32 offset);

    void DWLWriteRegAll(const void *instance, const RK_U32 * table, RK_U32 size); /* NOT in use */
    void DWLReadRegAll(const void *instance, RK_U32 * table, RK_U32 size);    /* NOT in use */

/* HW starting/stopping */
    void DWLEnableHW(const void *instance, RK_U32 offset, RK_U32 value);
    void DWLDisableHW(const void *instance, RK_U32 offset, RK_U32 value);

/* HW synchronization */
    RK_S32 VPUWaitHwReady(int socket, RK_U32 timeout);

	RK_S32 DWLWaitHwReady(const void *instance, RK_U32 timeout);
/* SW/SW shared memory */
    void *DWLmalloc(RK_U32 n);
    void DWLfree(void *p);
    void *DWLcalloc(RK_U32 n, RK_U32 s);
    void *DWLmemcpy(void *d, const void *s, RK_U32 n);
    void *DWLmemset(void *d, RK_S32 c, RK_U32 n);
#ifdef __cplusplus
}
#endif

#endif                       /* __DWL_H__ */
