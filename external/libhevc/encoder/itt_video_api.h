/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/
/*!
******************************************************************************
* \file itt_video_api.h
*
* \brief
*    This file contains the necessary structure and enumeration definitions
*    needed for the Application Program Interface(API)
*
* \date
*    18 09 2010
*
* \author
*    Ittiam
*
******************************************************************************
*/
#ifndef _ITT_VIDEO_API_H_
#define _ITT_VIDEO_API_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/** @brief IV_API_CALL_STATUS_T: This is only to return the FAIL/PASS status to the
 *         application for the current API call
 */

typedef enum
{
    IV_FAIL = 0xFFFFFFFF,
    IV_SUCCESS = 0
} IV_API_CALL_STATUS_T;

typedef enum
{
    ARCH_NA = 0x7FFFFFFF,
    ARCH_ARM_NONEON = 0x0,
    ARCH_ARM_V8_NEON,
    ARCH_ARM_A9Q,
    ARCH_ARM_A7,
    ARCH_ARM_A5,
    ARCH_ARM_NEONINTR,
    ARCH_X86_GENERIC,
    ARCH_X86_SSSE3,
    ARCH_X86_SSE4,
    ARCH_X86_AVX,
    ARCH_X86_AVX2
} IV_ARCH_T;

/** @brief IV_MEM_TYPE_T: This Enumeration defines the type of memory (Internal/Ext
 *         -ernal) along with the cacheable/non-cacheable attributes
 *        Additional memtypes added ( Normal, Numa_Node0, Numa_node1)
 */

typedef enum
{
    IV_NA_MEM_TYPE = 0xFFFFFFFF,
    IV_INTERNAL_CACHEABLE_PERSISTENT_MEM = 0x1,
    IV_INTERNAL_CACHEABLE_SCRATCH_MEM = 0x2,
    IV_EXTERNAL_CACHEABLE_PERSISTENT_MEM = 0x3,
    IV_EXTERNAL_CACHEABLE_SCRATCH_MEM = 0x4,
    IV_INTERNAL_NONCACHEABLE_PERSISTENT_MEM = 0x5,
    IV_INTERNAL_NONCACHEABLE_SCRATCH_MEM = 0x6,
    IV_EXTERNAL_NONCACHEABLE_PERSISTENT_MEM = 0x7,
    IV_EXTERNAL_NONCACHEABLE_SCRATCH_MEM = 0x8,

    IV_EXT_CACHEABLE_NORMAL_MEM = 0x9,
    IV_EXT_CACHEABLE_NUMA_NODE0_MEM = 0xA,
    IV_EXT_CACHEABLE_NUMA_NODE1_MEM = 0xB,

} IV_MEM_TYPE_T;

/** @brief IV_COLOR_FORMAT_T: This enumeration lists all the color formats which
 *         finds usage in video/image codecs
 */

typedef enum
{
    IV_CHROMA_NA = 0xFFFFFFFF,
    IV_YUV_420P = 0x1,
    IV_YUV_422P = 0x2,
    IV_420_UV_INTL = 0x3,
    IV_YUV_422IBE = 0x4,
    IV_YUV_422ILE = 0x5,
    IV_YUV_444P = 0x6,
    IV_YUV_411P = 0x7,
    IV_GRAY = 0x8,
    IV_RGB_565 = 0x9,
    IV_RGB_24 = 0xa,
    IV_YUV_420SP_UV = 0xb,
    IV_YUV_420SP_VU = 0xc,
    IV_YUV_422SP_UV = 0xd,
    IV_YUV_422SP_VU = 0xe

} IV_COLOR_FORMAT_T;

/** @brief IV_PICTURE_CODING_TYPE_T: VOP/Frame coding type Enumeration    */

typedef enum
{
    IV_NA_FRAME = 0xFFFFFFFF,
    IV_I_FRAME = 0x0,
    IV_P_FRAME = 0x1,
    IV_B_FRAME = 0x2,
    IV_IDR_FRAME = 0x3,
    IV_II_FRAME = 0x4,
    IV_IP_FRAME = 0x5,
    IV_IB_FRAME = 0x6,
    IV_PI_FRAME = 0x7,
    IV_PP_FRAME = 0x8,
    IV_PB_FRAME = 0x9,
    IV_BI_FRAME = 0xa,
    IV_BP_FRAME = 0xb,
    IV_BB_FRAME = 0xc,
    IV_MBAFF_I_FRAME = 0xd,
    IV_MBAFF_P_FRAME = 0xe,
    IV_MBAFF_B_FRAME = 0xf,
    IV_MBAFF_IDR_FRAME = 0x10,
    IV_NOT_CODED_FRAME = 0x11,
    IV_FRAMETYPE_DEFAULT = IV_I_FRAME
} IV_PICTURE_CODING_TYPE_T;

/* @brief IV_FLD_TYPE_T: field type Enumeration */

typedef enum
{
    IV_NA_FLD = 0xFFFFFFFF,
    IV_TOP_FLD = 0x0,
    IV_BOT_FLD = 0x1,
    IV_FLD_TYPE_DEFAULT = IV_TOP_FLD
} IV_FLD_TYPE_T;

/* @brief IV_CONTENT_TYPE_T: Video content type */

typedef enum
{
    IV_CONTENTTYPE_NA = -1,
    IV_PROGRESSIVE = 0x0,
    IV_INTERLACED = 0x1,
    IV_PROGRESSIVE_FRAME = 0x2,
    IV_INTERLACED_FRAME = 0x3,
    IV_INTERLACED_TOPFIELD = 0x4,
    IV_INTERLACED_BOTTOMFIELD = 0x5,
    IV_CONTENTTYPE_DEFAULT = IV_PROGRESSIVE,
} IV_CONTENT_TYPE_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/* @brief iv_mem_rec_t: This structure defines the memory record holder which will
 * be used by the modules to communicate its memory requirements to the
 * memory manager through appropriate API functions
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Pointer to the memory allocated by the memory manager */
    void *pv_base;

    /** size of the memory to be allocated */
    WORD32 i4_mem_size;

    /** Alignment of the memory pointer */
    WORD32 i4_mem_alignment;

    /** Nature of the memory to be allocated */
    IV_MEM_TYPE_T e_mem_type;

} iv_mem_rec_t;

/* @brief iv_input_bufs_req_t: This structure contains the parameters
 * related to input (data and control) buffer requirements of the codec.
 * Application can call the memory query API to get these requirements
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Minimum sets of input buffers required for the codec */
    WORD32 i4_min_num_yuv_bufs;

    /** YUV format of the input */
    WORD32 i4_yuv_format;

    /** Minimum Size in bytes of Luma input buffer */
    WORD32 i4_min_size_y_buf;

    /** Minimum Size in bytes of CB-CR input buffer .
     * if input format is Semiplanar then size will include
     * both Cb and Cr requirements
     */
    WORD32 i4_min_size_uv_buf;

    /** Minimum sets of Synchoronus command buffers
     *  required for the codec
     */
    WORD32 i4_min_num_synch_ctrl_bufs;

    /** Minimum size of the Synchoronus command buffer */
    WORD32 i4_min_size_synch_ctrl_bufs;

    /** Minimum sets of Asynchoronus command buffers
     *  required for the codec
     */
    WORD32 i4_min_num_asynch_ctrl_bufs;

    /** Minimum size of the Asynchoronus command buffer */
    WORD32 i4_min_size_asynch_ctrl_bufs;

} iv_input_bufs_req_t;

/* @brief iv_output_bufs_req_t: This structure contains the parameters
 * related to output (data and control) buffer requirements for a
 * given target resolution of the codec
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Minimum sets of output buffers required for the codec */
    WORD32 i4_min_num_out_bufs;

    /** Minimum Size in bytes of bitstream buffer */
    WORD32 i4_min_size_bitstream_buf;

} iv_output_bufs_req_t;

/* @brief iv_recon_bufs_req_t: This structure contains the parameters
 * related to recon buffer requirements for a
 * given target resolution of the codec
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Minimum sets of recon buffers required for the codec */
    WORD32 i4_min_num_recon_bufs;

    /** Minimum Size in bytes of Luma input buffer */
    WORD32 i4_min_size_y_buf;

    /** Minimum Size in bytes of CB-CR input buffer .
     * if input format is Semiplanar then size will include
     * both Cb and Cr requirements
     */
    WORD32 i4_min_size_uv_buf;

} iv_recon_bufs_req_t;

/* @brief iv_input_data_ctrl_buffs_desc_t: This structure contains the parameters
 * related to input (data and sync control) buffers
 * application should allocate these buffers and pass to the codec
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Number of sets of input buffers allocated by application */
    WORD32 i4_num_yuv_bufs;

    /** Size in bytes of each Luma input buffers passed */
    WORD32 i4_size_y_buf;

    /** Pointer to array of input Luma buffer pointers  */
    void **ppv_y_buf;

    /** Size in bytes of each CB-CR input buffer passed.
     * if input format is Semiplanar then size should include
     * both Cb and Cr requirements
     */
    WORD32 i4_size_uv_buf;

    /** Pointer to array of input Chroma Cb buffer pointers  */
    void **ppv_u_buf;

    /** Pointer to array of input Chroma Cr buffer pointers
      * Applicalbe if input format is planar
      */
    void **ppv_v_buf;

    /** Number of sets of sync control buffers allocated by application */
    WORD32 i4_num_synch_ctrl_bufs;

    /** Size of the each Synchoronus command buffer passed*/
    WORD32 i4_size_synch_ctrl_bufs;

    /** Pointer to array of input sync command buffer pointers  */
    void **ppv_synch_ctrl_bufs;

} iv_input_data_ctrl_buffs_desc_t;

/* @brief iv_input_asynch_ctrl_buffs_desc_t: This structure contains the parameters
 * related to input async control buffers
 * application should allocate these buffers and pass to the codec
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Number of sets of async control buffers allocated by application */
    WORD32 i4_num_asynch_ctrl_bufs;

    /** Size of each Asynchoronus command buffer */
    WORD32 i4_size_asynch_ctrl_bufs;

    /** Pointer to array of async command buffer pointers  */
    void **ppv_asynch_ctrl_bufs;

} iv_input_asynch_ctrl_buffs_desc_t;

/* @brief iv_output_data_buffs_desc_t: This structure contains the parameters
 * related to output  data buffers for a given resolution layer
 * application should allocate these buffers and pass to the codec
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Number of sets of output buffers allocated by application */
    WORD32 i4_num_bitstream_bufs;

    /** Size in bytes of each bitstream buffer passed */
    WORD32 i4_size_bitstream_buf;

    /** Pointer to array of output buffer pointers  */
    void **ppv_bitstream_bufs;

} iv_output_data_buffs_desc_t;

/* @brief iv_output_status_buffs_desc_t: This structure contains the parameters
 * related to output control acknowledgement buffers
 * application should allocate these buffers and pass to the codec
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Number of sets of async control ack buffers allocated by application */
    WORD32 i4_num_asynch_status_bufs;

    /** Size of each Asynchoronus command acknowledge buffer passed */
    WORD32 i4_size_asynch_status_bufs;

    /** Pointer to array of async command ack buffer pointers  */
    void **ppv_asynch_status_bufs;

} iv_output_status_buffs_desc_t;

/* @brief iv_recon_data_buffs_desc_t: This structure contains the parameters
 * related to recon data buffers
 * application should allocate these buffers and pass to the codec
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /** Number of sets of recon buffers allocated by application */
    WORD32 i4_num_recon_bufs;

    /** Size in bytes of each Luma recon buffers passed */
    WORD32 i4_size_y_buf;

    /** Pointer to array of recon Luma buffer pointers  */
    void **ppv_y_buf;

    /** Size in bytes of each CB-CR recon buffer passed.
     * if input format is Semiplanar then size should include
     * both Cb and Cr requirements
     */
    WORD32 i4_size_uv_buf;

    /** Pointer to array of recon Chroma Cb buffer pointers  */
    void **ppv_u_buf;

    /** Pointer to array of recon Chroma Cr buffer pointers
      * Applicalbe if input format is planar
      */
    void **ppv_v_buf;

} iv_recon_data_buffs_desc_t;

/* @brief IV_YUV_BUF_T: This structure defines attributes
 *        for the input yuv buffer
 */
typedef struct
{
    /** i4_size of the structure */
    WORD32 i4_size;

    /** Pointer to Luma (Y) Buffer  */
    void *pv_y_buf;

    /** Pointer to Chroma (Cb) Buffer  */
    void *pv_u_buf;

    /** Pointer to Chroma (Cr) Buffer */
    void *pv_v_buf;

    /** Width of the Luma (Y) Buffer in pixels */
    WORD32 i4_y_wd;

    /** Height of the Luma (Y) Buffer in pixels */
    WORD32 i4_y_ht;

    /** Stride/Pitch of the Luma (Y) Buffer */
    WORD32 i4_y_strd;

    /** Width of the Chroma (Cb / Cr) Buffer in pixels */
    WORD32 i4_uv_wd;

    /** Height of the Chroma (Cb / Cr) Buffer in pixels */
    WORD32 i4_uv_ht;

    /** Stride/Pitch of the Chroma (Cb / Cr) Buffer */
    WORD32 i4_uv_strd;

} iv_yuv_buf_t;

#endif /* _ITT_VIDEO_API_H_ */
