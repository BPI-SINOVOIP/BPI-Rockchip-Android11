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
 * @file   Rockchip_OMX_Def.h
 * @brief   Rockchip_OMX specific define
 * @author  csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *   2013.11.26 : Create
 */

#ifndef ROCKCHIP_OMX_DEF
#define ROCKCHIP_OMX_DEF

#include "OMX_Types.h"
#include "OMX_IVCommon.h"

#define VERSIONMAJOR_NUMBER                1
#define VERSIONMINOR_NUMBER                0
#define REVISION_NUMBER                    0
#define STEP_NUMBER                        0


#define MAX_OMX_COMPONENT_NUM              28
#define MAX_OMX_COMPONENT_ROLE_NUM         20
#define MAX_OMX_COMPONENT_NAME_SIZE        OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_ROLE_SIZE        OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_LIBNAME_SIZE     OMX_MAX_STRINGNAME_SIZE * 2
#define MAX_OMX_MIMETYPE_SIZE              OMX_MAX_STRINGNAME_SIZE

#define MAX_TIMESTAMP        40
#define MAX_FLAGS            40
#define MAX_BUFFER_REF       40

#define MAX_BUFFER_PLANE     1

#define ROCKCHIP_OMX_INSTALL_PATH "/system/lib/"

#define OMX_COLORSPACE_MASK     (0x00f00000)
#define OMX_DYNCRANGE_MASK      (0x0f000000)

/* note: must sync with gralloc */
typedef enum _ANB_PRIVATE_BUF_TYPE {
    ANB_PRIVATE_BUF_NONE    = 0,
    ANB_PRIVATE_BUF_VIRTUAL = 0x01,
    ANB_PRIVATE_BUF_BUTT,
} ANB_PRIVATE_BUF_TYPE;

typedef enum _ROCKCHIP_CODEC_TYPE {
    SW_CODEC,
    HW_VIDEO_DEC_CODEC,
    HW_VIDEO_ENC_CODEC,
} ROCKCHIP_CODEC_TYPE;

typedef struct _ROCKCHIP_OMX_PRIORITYMGMTTYPE {
    OMX_U32 nGroupPriority; /* the value 0 represents the highest priority */
    /* for a group of components                   */
    OMX_U32 nGroupID;
} ROCKCHIP_OMX_PRIORITYMGMTTYPE;

/*set when wfd*/
typedef struct _ROCKCHIP_OMX_WFD {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bEnable;
} ROCKCHIP_OMX_WFD;

typedef struct _OMX_VIDEO_PARAMS_EXTENDED {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 ui32Flags;
    OMX_BOOL bEnableScaling; // Resolution Scaling
    OMX_U16 ui16ScaledWidth;
    OMX_U16 ui16ScaledHeight;
    OMX_BOOL bEnableCropping; // Resolution Cropping
    OMX_U16 ui16CropLeft;//Number of columns to be cropped from lefthand-side edge
    OMX_U16 ui16CropRight;//Number of columns to be cropped from righthand-side edge
    OMX_U16 ui16CropTop;//Number of rows to be cropped from the top edge
    OMX_U16 ui16CropBottom;// Number of rows to be cropped from the bottom edge
} OMX_VIDEO_PARAMS_EXTENDED;

typedef enum _ROCKCHIP_OMX_ERRORTYPE {
    OMX_ErrorNoEOF              = (OMX_S32) 0x90000001,
    OMX_ErrorInputDataDecodeYet = (OMX_S32) 0x90000002,
    OMX_ErrorInputDataEncodeYet = (OMX_S32) 0x90000003,
    OMX_ErrorCodecInit          = (OMX_S32) 0x90000004,
    OMX_ErrorCodecDecode        = (OMX_S32) 0x90000005,
    OMX_ErrorCodecEncode        = (OMX_S32) 0x90000006,
    OMX_ErrorCodecFlush         = (OMX_S32) 0x90000007,
    OMX_ErrorOutputBufferUseYet = (OMX_S32) 0x90000008
} ROCKCHIP_OMX_ERRORTYPE;

typedef enum _ROCKCHIP_OMX_COMMANDTYPE {
    ROCKCHIP_OMX_CommandComponentDeInit = 0x7F000001,
    ROCKCHIP_OMX_CommandEmptyBuffer,
    ROCKCHIP_OMX_CommandFillBuffer,
    ROCKCHIP_OMX_CommandFakeBuffer
} ROCKCHIP_OMX_COMMANDTYPE;

typedef enum _ROCKCHIP_OMX_TRANS_STATETYPE {
    ROCKCHIP_OMX_TransStateInvalid,
    ROCKCHIP_OMX_TransStateLoadedToIdle,
    ROCKCHIP_OMX_TransStateIdleToExecuting,
    ROCKCHIP_OMX_TransStateExecutingToIdle,
    ROCKCHIP_OMX_TransStateIdleToLoaded,
    ROCKCHIP_OMX_TransStateMax = 0X7FFFFFFF
} ROCKCHIP_OMX_TRANS_STATETYPE;

typedef enum _ROCKCHIP_OMX_COLOR_FORMATTYPE {

    /* to copy a encoded data for drm component using gsc or fimc */
    OMX_SEC_COLOR_FormatEncodedData                 = OMX_COLOR_FormatYCbYCr,
    /* for Android SurfaceMediaSource*/
    OMX_COLOR_FormatYUV420Flexible                  = 0x7F420888
} ROCKCHIP_OMX_COLOR_FORMATTYPE;

typedef enum _ROCKCHIP_OMX_SUPPORTFORMAT_TYPE {
    supportFormat_0 = 0x00,
    supportFormat_1,
    supportFormat_2,
    supportFormat_3,
    supportFormat_4,
    supportFormat_5,
    supportFormat_6,
    supportFormat_7
} ROCKCHIP_OMX_SUPPORTFORMAT_TYPE;

typedef enum _ROCKCHIP_OMX_BUFFERPROCESS_TYPE {
    BUFFER_DEFAULT  = 0x00,
    BUFFER_COPY     = 0x01,
    BUFFER_SHARE    = 0x02,
    BUFFER_METADATA = 0x04,
    BUFFER_ANBSHARE = 0x08
} ROCKCHIP_OMX_BUFFERPROCESS_TYPE;

typedef struct _ROCKCHIP_OMX_VIDEO_PROFILELEVEL {
    OMX_S32  profile;
    OMX_S32  level;
} ROCKCHIP_OMX_VIDEO_PROFILELEVEL;

typedef struct _ROCKCHIP_OMX_VIDEO_THUMBNAILMODE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnable;
} ROCKCHIP_OMX_VIDEO_THUMBNAILMODE;

typedef enum _ROCKCHIP_VIDEO_CODINGTYPE_EXT {
    OMX_VIDEO_CodingFLV1 = 0x01000000,       /**< Sorenson H.263 */
    OMX_VIDEO_CodingDIVX3,                   /**< DIVX3 */
    OMX_VIDEO_CodingVP6,                      /**< VP6 */
    OMX_VIDEO_CodingVC1,                      /**< VP6 */
    OMX_VIDEO_OLD_CodingHEVC,
} ROCKCHIP_VIDEO_CODINGTYPE_EXT;

typedef enum {
    UNSUPPORT_PROFILE = -1,
    BASELINE_PROFILE = 66,
    MAIN_PROFILE = 77,
    HIGHT_PROFILE = 100,
} EncProfile;

typedef enum {
    HEVC_UNSUPPORT_PROFILE = -1,
    HEVC_MAIN_PROFILE = 0x1,
    HEVC_MAIN10_PROFILE = 0x2,
    HEVC_MAIN10HDR10_PROFILE = 0x1000,
} HEVCEncProfile;

typedef enum {
    UNSUPPORT_BITMODE          = -1,
    Video_RC_Mode_Disable      = 0,
    Video_RC_Mode_VBR          = 1,
    Video_RC_Mode_CBR          = 2,
} EncRCMode;

#if 0
typedef struct _ROCKCHIP_PARAM_ENABLE_NATIVEBUFFER {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnable;
} ROCKCHIP_PARAM_ENABLE_NATIVEBUFFER;

typedef struct _ROCKCHIP_PARAM_NATIVEBUFFER_USAGE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nUsage;
} ROCKCHIP_PARAM_NATIVEBUFFER_USAGE;

typedef struct _StoreMetaDataInBuffersParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bStoreMetaData;
} StoreMetaDataInBuffersParams;

typedef struct _PrependSPSPPSToIDRFramesParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bEnable;
} PrependSPSPPSToIDRFramesParams;

typedef enum {

    /*
     * kMetadataBufferTypeCameraSource is used to indicate that
     * the source of the metadata buffer is the camera component.
     */
    kMetadataBufferTypeCameraSource  = 0,

    /*
     * kMetadataBufferTypeGrallocSource is used to indicate that
     * the payload of the metadata buffers can be interpreted as
     * a buffer_handle_t.
     * So in this case,the metadata that the encoder receives
     * will have a byte stream that consists of two parts:
     * 1. First, there is an integer indicating that it is a GRAlloc
     * source (kMetadataBufferTypeGrallocSource)
     * 2. This is followed by the buffer_handle_t that is a handle to the
     * GRalloc buffer. The encoder needs to interpret this GRalloc handle
     * and encode the frames.
     * --------------------------------------------------------------
     * |  kMetadataBufferTypeGrallocSource | sizeof(buffer_handle_t) |
     * --------------------------------------------------------------
     */
    kMetadataBufferTypeGrallocSource = 1,

    // Add more here...

} MetadataBufferType;

#endif

typedef struct _RockchipVideoPlane {
    void          *addr;
    OMX_U32   allocSize;
    OMX_U32   dataSize;
    OMX_U32   offset;
    OMX_S32   fd;
    OMX_S32   type;
    OMX_U32   stride;
} RockchipVideoPlane;




#ifndef __OMX_EXPORTS
#define __OMX_EXPORTS
#define ROCKCHIP_EXPORT_REF __attribute__((visibility("default")))
#define ROCKCHIP_IMPORT_REF __attribute__((visibility("default")))
#endif

#endif
