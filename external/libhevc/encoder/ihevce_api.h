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
* \file ihevce_api.h
*
* \brief
*    This file contains definitions and structures which are shared between
*    application and HEVC Encoder Processing interface layer
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_API_H_
#define _IHEVCE_API_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define IHEVCE_MAX_IO_BUFFERS 3

#define IHEVCE_EXTENDED_SAR 255

#define IHEVCE_MBR_CORE_WEIGHTAGE 0.25f

/** Maximum number of resolutions encoder can run */
#define IHEVCE_MAX_NUM_RESOLUTIONS 1  //10

/** Maximum number of bit-rate instances encoder can run */
#define IHEVCE_MAX_NUM_BITRATES 1  //5

#define MAX_NUM_CORES 8  // Supports upto 160 logical cores.

/* Max length of filenames */
#define MAX_LEN_FILENAME 200

/* max number of tiles per row/cols */
//Main/Main10 profile (=4096/256) //Don't change this
#define MAX_TILE_COLUMNS 16
//Main/Main10 profile (=2160/64)  //Don't change this
#define MAX_TILE_ROWS 34

#define IHEVCE_ASYNCH_ERROR_START 0x0000E600
#define IHEVCE_SYNCH_ERROR_START 0x0000E700

#define MAX_NUM_DYN_BITRATE_CMDS (IHEVCE_MAX_NUM_RESOLUTIONS * IHEVCE_MAX_NUM_BITRATES)

/* NAL units related definations */
#define MAX_NUM_PREFIX_NALS_PER_AU 20
#define MAX_NUM_SUFFIX_NALS_PER_AU 20
#define MAX_NUM_VCL_NALS_PER_AU 200 /* as per level 5.1 from spec */

/* Maximum number of processor groups supported */
#define MAX_NUMBER_PROC_GRPS 4

/** @brief maximum length of CC User Data in a single frame */
#define MAX_SEI_PAYLOAD_PER_TLV (0x200)

#define MAX_NUMBER_OF_SEI_PAYLOAD (10)

#define IHEVCE_COMMANDS_TAG_MASK (0x0000FFFF)

// Upper 16 bits are used to communicate payload type
#define IHEVCE_PAYLOAD_TYPE_MASK (0xFFFF0000)

#define IHEVCE_PAYLOAD_TYPE_SHIFT (16)

#define MAX_FRAME_RATE  120.0
#define MIN_FRAME_RATE  1.0

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/**
 *  @brief      Enumerations for Quality config.
 */
typedef enum
{
    IHEVCE_QUALITY_DUMMY = 0xFFFFFFFF,
    IHEVCE_QUALITY_P0 = 0,
    IHEVCE_QUALITY_P2 = 2,
    IHEVCE_QUALITY_P3,
    IHEVCE_QUALITY_P4,
    IHEVCE_QUALITY_P5,
    IHEVCE_QUALITY_P6,
    IHEVCE_QUALITY_P7,
    IHEVCE_NUM_QUALITY_PRESET
} IHEVCE_QUALITY_CONFIG_T;

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/**
 *  @brief      Enumerations for Quality config for auxilary bitrate in case of MBR.
 */
typedef enum
{
    IHEVCE_MBR_DUMMY = -1,
    IHEVCE_MBR_HIGH_QUALITY = 0,
    IHEVCE_MBR_MEDIUM_SPEED,
    IHEVCE_MBR_HIGH_SPEED,
    IHEVCE_MBR_EXTREME_SPEED
} IHEVCE_QUALITY_CONFIG_MBR_T;

/**
 *  @brief      Enumerations for Rate Control config.
 */
typedef enum
{
    IHEVCE_RC_DUMMY = 0xFFFFFFFF,
    IHEVCE_RC_LOW_DELAY = 1,
    IHEVCE_RC_STORAGE = 2,
    IHEVCE_RC_TWOPASS = 3,
    IHEVCE_RC_NONE = 4,
    IHEVCE_RC_USER_DEFINED = 5,
    IHEVCE_RC_RATECONTROLPRESET_DEFAULT = IHEVCE_RC_LOW_DELAY
} IHEVCE_RATE_CONTROL_CONFIG_T;

/**
 *  @brief      Enumerations for Intra Refresh config.
 */
typedef enum
{
    IHEVCE_REFRESH_DUMMY = 0,
    IHEVCE_I_SILICE = 1,
    IHEVCE_COLUMN_BASED = 2,
    IHEVCE_DBR = 3,
    IHEVCE_GDR = 4
} IHEVCE_REFRESH_CONFIG_T;

/**
 *  @brief      Enumerations for ASYNCH Control Commands Tags.
 */
typedef enum
{
    IHEVCE_ASYNCH_API_END_TAG = 0xFFFF,
    IHEVCE_ASYNCH_API_SETBITRATE_TAG = 0x01,
    IHEVCE_ASYNCH_API_SET_RF_TAG = 0x02,
    IHEVCE_ASYNCH_API_FORCE_CLOSE_TAG = 0x03
} IHEVCE_ASYNCH_API_COMMAND_TAG_T;

typedef enum
{
    IHEVCE_ASYNCH_ERR_NO_END_TAG = IHEVCE_ASYNCH_ERROR_START + 0x01,
    IHEVCE_ASYNCH_ERR_TLV_ERROR = IHEVCE_ASYNCH_ERROR_START + 0x02,
    IHEVCE_ASYNCH_ERR_LENGTH_NOT_ZERO = IHEVCE_ASYNCH_ERROR_START + 0x03,
    IHEVCE_ASYNCH_ERR_BR_NOT_BYTE = IHEVCE_ASYNCH_ERROR_START + 0x04,
    IHEVCE_ASYNCH_FORCE_CLOSE_NOT_SUPPORTED = IHEVCE_ASYNCH_ERROR_START + 0x05
} IHEVCE_ASYNCH_ERROR_TAG_T;

/**
 *  @brief      Enumerations for SYNCH Control Commands Tags.
 */
typedef enum
{
    IHEVCE_SYNCH_API_END_TAG = 0xFFFF,
    IHEVCE_SYNCH_API_FLUSH_TAG = 0x21,
    IHEVCE_SYNCH_API_FORCE_IDR_TAG = 0x22,
    IHEVCE_SYNCH_API_REG_KEYFRAME_SEI_TAG = 0x23,
    IHEVCE_SYNCH_API_REG_ALLFRAME_SEI_TAG = 0x24,
    IHEVCE_SYNCH_API_SET_RES_TAG = 0x25
} IHEVCE_SYNCH_API_COMMAND_TAG_T;

typedef enum
{
    IHEVCE_SYNCH_ERR_NO_END_TAG = IHEVCE_SYNCH_ERROR_START + 0x11,
    IHEVCE_SYNCH_ERR_TLV_ERROR = IHEVCE_SYNCH_ERROR_START + 0x12,
    IHEVCE_SYNCH_ERR_LENGTH_NOT_ZERO = IHEVCE_SYNCH_ERROR_START + 0x13,
    IHEVCE_SYNCH_ERR_NO_PADDING = IHEVCE_SYNCH_ERROR_START + 0x14,
    IHEVCE_SYNCH_ERR_WRONG_LENGTH = IHEVCE_SYNCH_ERROR_START + 0x15,
    IHEVCE_SYNCH_ERR_FREQ_FORCE_IDR_RECEIVED = IHEVCE_SYNCH_ERROR_START + 0x16,
    IHEVCE_SYNCH_ERR_TOO_MANY_SEI_MSG = IHEVCE_SYNCH_ERROR_START + 0x17,
    IHEVCE_SYNCH_ERR_SET_RES_NOT_SUPPORTED = IHEVCE_SYNCH_ERROR_START + 0x18
} IHEVCE_SYNCH_ERROR_TAG_T;

/**
 *  @brief      Enumerations for output status identifier
 */
typedef enum
{
    IHEVCE_PROCESS = 0,
    IHEVCE_CONTROL_STS,
    IHEVCE_CREATE_STS,
} IHEVCE_OUT_STS_ID_T;

/**
  * Scenetype enums
  */
typedef enum
{
    IHEVCE_SCENE_TYPE_NORMAL = 0,
    IHEVCE_SCENE_TYPE_SCENE_CUT,
    IHEVCE_SCENE_TYPE_FLASH,
    IHEVCE_SCENE_TYPE_FADE_IN,
    IHEVCE_SCENE_TYPE_FADE_OUT,
    IHEVCE_SCENE_TYPE_DISSOLVE,
    IHEVCE_MAX_NUM_SCENE_TYPES
} IHEVCE_SCENE_TYPE;

/**
  * Type of data. Used for scanning the config file
  */
typedef enum
{
    IHEVCE_STRING = 0,
    IHEVCE_INT,
    IHEVCE_FLOAT
} IHEVCE_DATA_TYPE;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/**
 *  @brief      Structure to describe the properties of Source of encoder.
 */
typedef struct
{
    /** Used for checking version compatibility  */
    WORD32 i4_size;

    /**  Input chroma format
     * @sa : IV_COLOR_FORMAT_T
     */
    WORD32 inp_chr_format;

    /**  Internal chroma format
     * @sa : IV_COLOR_FORMAT_T
     */
    WORD32 i4_chr_format;

    /** Width of input luma */
    WORD32 i4_width;

    /** Height of input luma */
    WORD32 i4_height;

    /** Configured Width of input luma */
    WORD32 i4_orig_width;

    /** Configured Height of input luma */
    WORD32 i4_orig_height;

    /** Width of each pixel in bits */
    WORD32 i4_input_bit_depth;

    /** Input Content Type
     * @sa : IV_CONTENT_TYPE_T
     */
    WORD32 i4_field_pic;

    /** Frame/Field rate numerator
     * (final fps = frame_rate_num/frame_rate_denom)
     */
    WORD32 i4_frm_rate_num;

    /** Can be 1000 or 1001 to allow proper representation
     *  of fractional frame-rates
     */
    WORD32 i4_frm_rate_denom;

    /**
     *  Whether Top field is encoded first or bottom
     */
    WORD32 i4_topfield_first;

} ihevce_src_params_t;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
/**
 *  @brief      Structure to describe attributes of a layer.
 */
typedef struct
{
    /** Used for checking version compatibility  */
    WORD32 i4_size;

    /** Width of input luma */
    WORD32 i4_width;

    /** Height of input luma */
    WORD32 i4_height;

    /** Frame/Field rate
     * (final fps = src frame_rate_num/src frame_rate_denom/i4_frm_rate_scale_factor)
     */
    WORD32 i4_frm_rate_scale_factor;

    /** Quality vs. complexity
     * @sa : IHEVCE_QUALITY_CONFIG_T
     */
    IHEVCE_QUALITY_CONFIG_T i4_quality_preset;

    /** 0 : Level 4, any level above this not supported */
    WORD32 i4_codec_level;

    /** Number of bit-rate instances for the current layer
    */
    WORD32 i4_num_bitrate_instances;

    /** Target Bit-rate in bits for Constant bitrate cases  */
    WORD32 ai4_tgt_bitrate[IHEVCE_MAX_NUM_BITRATES];

    /** Peak Bit-rate in bits for each bitrate */
    WORD32 ai4_peak_bitrate[IHEVCE_MAX_NUM_BITRATES];

    /** Maximum VBV buffer size in bits for each and each bitrate */
    WORD32 ai4_max_vbv_buffer_size[IHEVCE_MAX_NUM_BITRATES];

    /** Frame level Qp for Constant Qp mode */
    WORD32 ai4_frame_qp[IHEVCE_MAX_NUM_BITRATES];

} ihevce_tgt_params_t;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
/**
 *  @brief      Structure to describe the properties of target
                resolution of encoder.
 */
typedef struct
{
    /** Used for checking version compatibility  */
    WORD32 i4_size;

    /**  Number of resolution layers
     */
    WORD32 i4_num_res_layers;

    /*  Applicable only for multi res cases.
     Output of only one resolution to be dumped */

    WORD32 i4_mres_single_out;

    /* Specify starting resolution id for mres single out case.
     This is only valid in mres_single out mode */

    WORD32 i4_start_res_id;

    /**  To enable reuse across layers
     */
    WORD32 i4_multi_res_layer_reuse;

    /** Quality vs. complexity for auxilary bitrates
     * @sa : IHEVCE_QUALITY_CONFIG_MBR_T
     */
    IHEVCE_QUALITY_CONFIG_MBR_T i4_mbr_quality_setting;

    /**
     *Bit depth used by encoder
     */
    WORD32 i4_internal_bit_depth;

    /**
    *Temporal scalability enable Flag
    */
    WORD32 i4_enable_temporal_scalability;

    /** Resolution and frame rate scaling factors for
     *  each layer
     */
    ihevce_tgt_params_t as_tgt_params[IHEVCE_MAX_NUM_RESOLUTIONS];

    /*Scaler handle */
    void *pv_scaler_handle;

    /*Function pointer for scaling luma data*/
    void (*pf_scale_luma)(
        void *pv_scaler_handle,
        UWORD8 *pu1_in_buf,
        WORD32 i4_inp_width,
        WORD32 i4_inp_height,
        WORD32 i4_inp_stride,
        UWORD8 *pu1_out_buf,
        WORD32 i4_out_width,
        WORD32 i4_out_height,
        WORD32 i4_out_stride);

    /*Function pointer for scaling chroma data*/
    void (*pf_scale_chroma)(
        void *pv_scaler_handle,
        UWORD8 *pu1_in_buf,
        WORD32 i4_inp_width,
        WORD32 i4_inp_height,
        WORD32 i4_inp_stride,
        UWORD8 *pu1_out_buf,
        WORD32 i4_out_width,
        WORD32 i4_out_height,
        WORD32 i4_out_stride);

} ihevce_tgt_layer_params_t;

/**
 *  @brief    Structure to describe the stream level
 *            properties encoder should adhere to
 */
typedef struct
{
    /** Used for checking version compatibility */
    WORD32 i4_size;

    /**  0 - HEVC , no other value supported */
    WORD32 i4_codec_type;

    /**1 : Main Profile ,2: Main 10 Profile. no other value supported */
    WORD32 i4_codec_profile;

    /** 0: Main Tier ,1: High Tier. no other value supported */
    WORD32 i4_codec_tier;

    /** Enable VUI output  1: enable 0 : disable */
    WORD32 i4_vui_enable;

    /** Enable specific SEI messages in the stream
     *  1: enable 0 : disable
     */
    WORD32 i4_sei_enable_flag;

    /** Enable specific SEI payload (other than pic timing and buffering period) messages in the stream
     *  1: enable 0 : disable
     */
    WORD32 i4_sei_payload_enable_flag;

    /** Enable specific SEI buffering period messages in the stream
     *  1: enable 0 : disable
     */
    WORD32 i4_sei_buffer_period_flags;

    /** Enable specific SEI Picture timing messages in the stream
     *  1: enable 0 : disable
     */
    WORD32 i4_sei_pic_timing_flags;

    /** Enable specific SEI recovery point messages in the stream
     *  1: enable 0 : disable
     */
    WORD32 i4_sei_recovery_point_flags;

    /** Enable specific SEI mastering display colour volume in the stream
     *  1: enable 0 : disable
     */
    WORD32 i4_sei_mastering_disp_colour_vol_flags;

    /**
     * Array to store the display_primaries_x values
     */
    UWORD16 au2_display_primaries_x[3];

    /**
     * Array to store the display_primaries_y values
     */
    UWORD16 au2_display_primaries_y[3];

    /**
     * Variable to store the white point x value
     */
    UWORD16 u2_white_point_x;

    /**
     * Variable to store the white point y value
     */
    UWORD16 u2_white_point_y;

    /**
     * Variable to store the max display mastering luminance value
     */
    UWORD32 u4_max_display_mastering_luminance;

    /**
     * Variable to store the min display mastering luminance value
     */
    UWORD32 u4_min_display_mastering_luminance;

    /**
     * Enable Content Level Light Info
     */
    WORD32 i4_sei_cll_enable;

    /**
     * 16bit unsigned number which indicates the maximum pixel intensity of all samples in bit-stream in units of 1 candela per square metre
     */
    UWORD16 u2_sei_max_cll;

    /**
     * 16bit unsigned number which indicates the average pixel intensity of all samples in bit-stream in units of 1 candela per square metre
     */
    UWORD16 u2_sei_avg_cll;

    /** Enable/Disable SEI Hash on the Decoded picture & Hash type
     *  3 : Checksum, 2 : CRC, 1 : MD5, 0 : disable
     */
    WORD32 i4_decoded_pic_hash_sei_flag;

    /** Enable specific AUD messages in the stream
     *  1: enable 0 : disable
     */
    WORD32 i4_aud_enable_flags;

    /** Enable EOS messages in the stream
     *  1: enable 0 : disable
     */
    WORD32 i4_eos_enable_flags;

    /** Enable automatic insertion of SPS at each CDR
     *  1: enable 0 : disable
     */
    WORD32 i4_sps_at_cdr_enable;

    WORD32 i4_interop_flags;

} ihevce_out_strm_params_t;

/**
 *  @brief   Structure to describe the Encoding Coding tools
 *           to be used by the Encoder
 */
typedef struct
{
    /** Used for checking version compatibility*/
    WORD32 i4_size;

    /** Max spacing between IDR frames -
      *  0 indicates only at the beginning
      */
    WORD32 i4_max_closed_gop_period;

    /** Min spacing between IDR frames  -
     *  Max = Min provides fixed segment length
     */
    WORD32 i4_min_closed_gop_period;

    /** Max spacing between CRA frames -
      *
      */
    WORD32 i4_max_cra_open_gop_period;

    /** Max spacing between I frames  -
     *
     */
    WORD32 i4_max_i_open_gop_period;

    /** Maximum number of dyadic temporal layers */
    WORD32 i4_max_temporal_layers;

    /** Maximum number of reference frames */
    WORD32 i4_max_reference_frames;

    /** Enable weighted prediction
     * 0 - disabled (default); 1 -enabled
     */
    WORD32 i4_weighted_pred_enable;

    /**  Deblocking type 0 - no deblocking;
     *  1 - default; 2 - disable across slices
     */
    WORD32 i4_deblocking_type;

    /** Use default scaling matrices
     * 0 - disabled; 1 - enabled (default)
     */
    WORD32 i4_use_default_sc_mtx;

    /** Cropping mode for cases  where frame dimensions
     *  are not multiple of MIN CU size
     *  1 - enable padding to min_cu multiple and generate cropping flags;
     *  0 - report error
     */
    WORD32 i4_cropping_mode;

    /** 0 - no slices; 1 - packet based; 2 - CU based */
    WORD32 i4_slice_type;

    /** Use default scaling matrices
     * 0 - disabled; 1 - enabled (default)
     */
    WORD32 i4_enable_entropy_sync;

    /** VQET control parameter */
    WORD32 i4_vqet;

} ihevce_coding_params_t;

/**
 *  @brief  Structure to describe the Configurable parameters of Encoder
 */
typedef struct
{
    /** Used for checking version compatibility */
    WORD32 i4_size;

    /* ---------- Tiles related parameters ------------ */

    /* ----------- CU related parameters -------------- */

    /** 4 - 16x16; 5 - 32x32 (default); 6 - 64x64 */
    WORD32 i4_max_log2_cu_size;

    /** 3 - 8x8; 4 - 16x16 (default); 5 - 32x32 ; 6 - 64x64 */
    WORD32 i4_min_log2_cu_size;

    /** 2 - 4x4 (default) ; 3 - 8x8; 4 - 16x16; 5 - 32x32 */
    WORD32 i4_min_log2_tu_size;

    /** 2 - 4x4; 3 - 8x8 (default); 4 - 16x16; 5 - 32x32 */
    WORD32 i4_max_log2_tu_size;

    /** Max transform tree depth for intra */
    WORD32 i4_max_tr_tree_depth_I;

    /** Max transform tree depth for inter */
    WORD32 i4_max_tr_tree_depth_nI;

    /* ---------- Rate Control related parameters ------ */

    /** Rate control mode  0 - constant qp (default); 1- CBR  */
    WORD32 i4_rate_control_mode;

    /** CU level Qp modulation
        0 - No Qp modulation at CU level;
        1 - QP modulation level 1
        2 - QP modulation level 2
        3 - QP modulation level 3*/
    WORD32 i4_cu_level_rc;

    /* Unused variable retained for backward compatibility*/
    WORD32 i4_rate_factor;

    /** Enable stuffing 0 - disabled (default); 1 -enabled */
    WORD32 i4_stuffing_enable;

    /*The max deivaiton allowed from file size (used only in VBR, in CBR vbv buffer size dictates the deviaiton allowed)*/
    WORD32 i4_vbr_max_peak_rate_dur;

    /*Number of frames to encode. required to control allowed bit deviation at any point of time*/
    WORD32 i4_num_frms_to_encode;

    /** Initial buffer fullness when decoding starts */
    WORD32 i4_init_vbv_fullness;

    /** Frame level I frame max qp in rate control mode */
    WORD32 i4_max_frame_qp;

    /** Frame level I frame min qp in rate control mode */
    WORD32 i4_min_frame_qp;
    /* --------- ME related parameters ---------------- */

    /** Maximum search range in full pel units. horizontal direction */
    WORD32 i4_max_search_range_horz;

    /** Maximum search range in full pel units.  vertical direction */
    WORD32 i4_max_search_range_vert;
} ihevce_config_prms_t;

/**
 *  @brief    Structure to describe Dynamic configuralbe
 *            parameters of encoder
 *
 *            these new params can be passed as async commands
 *            to the enocder by sending a IHEVCE_CMD_CTL_SETPARAMS command
 */
typedef struct
{
    /** Used for checking version compatibility */
    WORD32 i4_size;

    /** Resolution ID  of the stream for which bitrate change needs to be applied */
    WORD32 i4_tgt_res_id;

    /** Bitrate ID in the Resolution ID  of the stream for which bitrate change needs to be applied */
    WORD32 i4_tgt_br_id;

    /** New Target Bit-rate for on the fly change */
    WORD32 i4_new_tgt_bitrate;

    /** New Peak Bit-rate for on the fly change */
    WORD32 i4_new_peak_bitrate;
} ihevce_dyn_config_prms_t;

/**
 *  @brief    Structure to describe Dynamic configuralbe
 *            parameters of encoder for dynamic resolution change
 *
 *            these new params can be passed as synchromous commands
 *            to the enocder by sending a IHEVCE_SYNCH_API_SET_RES_TAG command
 */
typedef struct
{
    /** Resolution ID  of the stream for which bitrate change needs to be applied */
    WORD32 i4_new_res_id;

    /** New Target Bit-rate for on the fly change */
    WORD32 i4_new_tgt_bitrate;

} ihevce_dyn_res_prms_t;

/**
 *  @brief    Structure to describe the Look Ahead
 *            Processing Parameters of Encoder
 */
typedef struct
{
    /** Used for checking version compatibility */
    WORD32 i4_size;

    /** Number of frames to look-ahead for RC and adaptive quant -
     * counts each fields as one frame for interlaced
     */
    WORD32 i4_rc_look_ahead_pics;

    /** Enable computation of weights & offsets for weighted prediction */
    WORD32 i4_enable_wts_ofsts;

    /* Enables denoiser as a part of video preprocessing. */
    WORD32 i4_denoise_enable;

    /* Enable this flag if input is interlaced and output is progressive */
    WORD32 i4_deinterlacer_enable;

} ihevce_lap_params_t;

/**
 *  @brief    Structure to describe the parameters
 *            related to multi-bitrate encoding
 */
typedef struct
{
    /** Number of bit-rate instances */
    WORD32 i4_num_bitrate_instances;

    /* Number of intra modes to be evaluated for derived instance */
    WORD32 i4_num_modes_intra;

    /* Number of inter modes to be evaluated for derived instance */
    WORD32 i4_num_modes_inter;

} ihevce_mbr_params_t;

/**
 *  @brief  Vui/Sei parameters of Encoder
 */
typedef struct
{
    /**
    *  indicates the presence of aspect_ratio
    */
    UWORD8 u1_aspect_ratio_info_present_flag;

    /**
    *  specifies the aspect ratio of the luma samples
    */
    UWORD8 au1_aspect_ratio_idc[IHEVCE_MAX_NUM_RESOLUTIONS];

    /**
    *  width of the luma samples. user dependent
    */
    UWORD16 au2_sar_width[IHEVCE_MAX_NUM_RESOLUTIONS];

    /**
    *  height of the luma samples. user dependent
    */
    UWORD16 au2_sar_height[IHEVCE_MAX_NUM_RESOLUTIONS];

    /**
    * if 1, specifies that the overscan_appropriate_flag is present
    * if 0, the preferred display method for the video signal is unspecified
    */
    UWORD8 u1_overscan_info_present_flag;

    /**
    * if 1,indicates that the cropped decoded pictures output
    * are suitable for display using overscan
    */
    UWORD8 u1_overscan_appropriate_flag;

    /**
    * if 1 specifies that video_format, video_full_range_flag and
    * colour_description_present_flag are present
    */
    UWORD8 u1_video_signal_type_present_flag;

    /**
    *
    */
    UWORD8 u1_video_format;

    /**
    * indicates the black level and range of the luma and chroma signals
    */
    UWORD8 u1_video_full_range_flag;

    /**
    * if 1,to 1 specifies that colour_primaries, transfer_characteristics
    * and matrix_coefficients are present
    */
    UWORD8 u1_colour_description_present_flag;

    /**
    * indicates the chromaticity coordinates of the source primaries
    */
    UWORD8 u1_colour_primaries;

    /**
    * indicates the opto-electronic transfer characteristic of the source picture
    */
    UWORD8 u1_transfer_characteristics;

    /**
    * the matrix coefficients used in deriving luma and chroma signals
    * from the green, blue, and red primaries
    */
    UWORD8 u1_matrix_coefficients;

    /**
    * if 1, specifies that chroma_sample_loc_type_top_field and
    * chroma_sample_loc_type_bottom_field are present
    */
    UWORD8 u1_chroma_loc_info_present_flag;

    /**
    * location of chroma samples
    */
    UWORD8 u1_chroma_sample_loc_type_top_field;

    UWORD8 u1_chroma_sample_loc_type_bottom_field;

    /**
    *  to 1 specifies that the syntax structure hrd_parameters is present in the vui_parameters syntax structue
    */
    UWORD8 u1_vui_hrd_parameters_present_flag;

    /**
    *  VUI level HRD parameters
    */
    //hrd_params_t s_vui_hrd_parameters;

    /**
    *   HRD parameter Indicates the presence of the
    *   num_units_in_ticks, time_scale flag
    */
    UWORD8 u1_timing_info_present_flag;

    /**
    * Nal- hrd parameters flag
    */
    UWORD8 u1_nal_hrd_parameters_present_flag;

} ihevce_vui_sei_params_t;

/**
 *  @brief  Multi thread related parameters passed to the encoder during create
 */

typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Total number of logical cores, which are assigned to be used by the encoder
     */
    WORD32 i4_max_num_cores;

    /** Total number of groups in the machine on which encoder is run.
     */
    WORD32 i4_num_proc_groups;

    /** Total number of logical cores present per processor group of the machine.
     */
    WORD32 ai4_num_cores_per_grp[MAX_NUMBER_PROC_GRPS];

    /** Flag to enableUse thread affintiy feature
     * 0: Thread affinity disabled
     * 1: Thread affinity enabled
     */
    WORD32 i4_use_thrd_affinity;

    /**
     * Memory allocation control flag: Reserved (to be used later)
     */
    WORD32 i4_memory_alloc_ctrl_flag;

    /**
     * Array of thread affinity masks for frame processing threads
     * PRE Enc Group
     */
    ULWORD64 au8_core_aff_mask[MAX_NUM_CORES];

} ihevce_static_multi_thread_params_t;

/**
 *  @brief  File IO APIs
 */
typedef struct
{
    FILE *(*ihevce_fopen)(void *pv_cb_handle, const char *pi1_filename, const char *pi1_mode);

    int (*ihevce_fclose)(void *pv_cb_handle, FILE *pf_stream);

    int (*ihevce_fflush)(void *pv_cb_handle, FILE *pf_stream);

    int (*ihevce_fseek)(void *pv_cb_handle, FILE *pf_stream, long i4_offset, int i4_origin);

    size_t (*ihevce_fread)(
        void *pv_cb_handle, void *pv_ptr, size_t u4_size, size_t u4_count, FILE *pf_stream);

    int (*ihevce_fscanf)(
        void *pv_cb_handle,
        IHEVCE_DATA_TYPE e_data_type,
        FILE *file_ptr,
        const char *format,
        void *pv_dst);

    int (*ihevce_fprintf)(void *pv_cb_handle, FILE *pf_stream, const char *pi1_format, ...);

    size_t (*ihevce_fwrite)(
        void *pv_cb_handle, const void *pv_ptr, size_t i4_size, size_t i4_count, FILE *pf_stream);

    char *(*ihevce_fgets)(void *pv_cb_handle, char *pi1_str, int i4_size, FILE *pf_stream);

} ihevce_file_io_api_t;

/**
 *  @brief  System APIs to implement call back functions in encoder
 */
typedef struct
{
    /*Call back handle for all system api*/
    void *pv_cb_handle;

    /* Console APIs */
    int (*ihevce_printf)(void *pv_cb_handle, const char *i1_str, ...);

    //int   (*ihevce_scanf)    (void *pv_handle, const char *i1_str, ...);

    int (*ihevce_sscanf)(void *pv_cb_handle, const char *pv_src, const char *format, int *p_dst_int);

    int (*ihevce_sprintf)(void *pv_cb_handle, char *pi1_str, const char *format, ...);

    int (*ihevce_sprintf_s)(
        void *pv_cb_handle, char *pi1_str, size_t i4_size, const char *format, ...);

    /* File I/O APIs */
    ihevce_file_io_api_t s_file_io_api;

} ihevce_sys_api_t;

/**
 *  @brief    Structure to describe multipass related params
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /* 0:Normal mode 1: only dumps stat 2: 2nd pass reads from stat file and rewrites the same file*/
    WORD32 i4_pass;

    /* Flag to specify the algorithm used for bit-distribution
    in second pass */
    WORD32 i4_multi_pass_algo_mode;

    /* Stat file to read or write data of frame statistics */
    WORD8 *pi1_frame_stats_filename;

    /* stat file to read or write data of gop level statstics*/
    WORD8 *pi1_gop_stats_filename;

    /* Stat file to read or write CTB level data*/
    WORD8 *pi1_sub_frames_stats_filename;

} ihevce_pass_prms_t;

/**
 *  @brief    Structure to describe tile params
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /* flag to indicate tile encoding enabled/disabled */
    WORD32 i4_tiles_enabled_flag;

    /* flag to indicate unifrom spacing of tiles */
    WORD32 i4_uniform_spacing_flag;

    /* num syntactical tiles in a frame */
    WORD32 i4_num_tile_cols;
    WORD32 i4_num_tile_rows;

    /* Column width array to store width of each tile column */
    WORD32 ai4_column_width[MAX_TILE_COLUMNS];

    /* Row height array to store height of each tile row */
    WORD32 ai4_row_height[MAX_TILE_ROWS];

} ihevce_app_tile_params_t;

/**
 *  @brief    Structure to describe slice params
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Flag to control dependent slices.
    0: Disable all slice segment limits
    1: Enforce max number of CTBs
    2: Enforce max number of bytes **/
    WORD32 i4_slice_segment_mode;

    /** Depending on i4_slice_segment_mode being:
    1: max number of CTBs per slice segment
    2: max number of bytes per slice segment **/
    WORD32 i4_slice_segment_argument;

} ihevce_slice_params_t;

/**
 *  @brief  Static configuration parameters of Encoder
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Structure describing the input parameters - Applciatiopn should populate
     * maximum values in this structure . Run time values
     * should always be lessthan create time values
     */
    ihevce_src_params_t s_src_prms;

    /** Parmeters for target use-case */
    ihevce_tgt_layer_params_t s_tgt_lyr_prms;

    /** Output stream parameters */
    ihevce_out_strm_params_t s_out_strm_prms;

    /** Coding parameters for the encoder */
    ihevce_coding_params_t s_coding_tools_prms;

    /** Configurable parameters for Encoder */
    ihevce_config_prms_t s_config_prms;

    /** VUI SEI app parameters*/
    ihevce_vui_sei_params_t s_vui_sei_prms;

    /** Multi threads specific pamrameters */
    ihevce_static_multi_thread_params_t s_multi_thrd_prms;

    /** Look-ahead processor related parameters */
    ihevce_lap_params_t s_lap_prms;

    /** Save Recon flag */
    WORD32 i4_save_recon;

    /** Compute PSNR Flag */
    /*  0: No logs
        1: (Frame level:Bits generation + POC) + (summary level: BitRate)
        2: (Frame level:Bits generation + POC + Qp + Pic-type) + (summary level: BitRate + PSNR)
    */
    WORD32 i4_log_dump_level;

    WORD32 i4_enable_csv_dump;

    FILE *apF_csv_file[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];

    /** Enable Logo for Eval versions */
    WORD32 i4_enable_logo;

    /* API structure for exporting console and file I/O operation */
    ihevce_sys_api_t s_sys_api;

    /* Structure to describe multipass related params */
    ihevce_pass_prms_t s_pass_prms;

    /* Structure to describe tile params */
    ihevce_app_tile_params_t s_app_tile_params;

    /** Structure to describe slice segment params */
    ihevce_slice_params_t s_slice_params;

    /** Resolution ID of the current encoder context **/
    WORD32 i4_res_id;

    /** Bitrate ID of the current encoder context **/
    WORD32 i4_br_id;

    /* Architecture type */
    IV_ARCH_T e_arch_type;

    /* Control to free the entropy output buffers   */
    /* 1  for non_blocking mode */
    /* and 0 for blocking mode */
    WORD32 i4_outbuf_buf_free_control;

} ihevce_static_cfg_params_t;

/**
 *  @brief  Input structure in which input data and
 *          other parameters are sent to Encoder
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Buffer id for the current buffer */
    WORD32 i4_buf_id;

    /** is bottom field  0 = top field,  1 = bottom field  */
    WORD32 i4_bottom_field;

    /** top field first input in case of interlaced case */
    WORD32 i4_topfield_first;

    /** input time stamp in terms of ticks: lower 32  */
    WORD32 i4_inp_timestamp_low;

    /** input time stamp in terms of ticks: higher 32 */
    WORD32 i4_inp_timestamp_high;

    /**  colour format of input,
     * should be same as create time value
     */
    WORD32 u1_colour_format;

    /**
     * Input frame buffer valid flag
     * 1 : valid data is present in the s_input_buf
     * 0 : Only command buffer is valid input buffer is a non valid input (dumy input)
     */
    WORD32 i4_inp_frm_data_valid_flag;

    /** Synchronous control commands buffer
     * this will an Tag Length Value (TLV) buffer.
     * All commands must be terminated with a tag
     * Tag should be set to IHEVCE_SYNCH_API_END_TAG
     */
    void *pv_synch_ctrl_bufs;

    /**
     * Synchronous control commands buffer
     * size in number of bytes
     */
    WORD32 i4_cmd_buf_size;

    /** for system use if run time buffer allocation is used*/
    void *pv_metadata;

    /** for system to pass frame context from Input to Output
       Same pointer will be returned on the output buffer of this frame */
    void *pv_app_frm_ctxt;

    /** Input YUV buffers pointers and related parameters
     *   are set in this structure
     */
    iv_yuv_buf_t s_input_buf;

} iv_input_data_ctrl_buffs_t;

/**
 *  @brief  Input structure in which input async control
 *          commands are sent to Encoder
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Buffer id for the current buffer */
    WORD32 i4_buf_id;

    /** Asynchronous control commands buffer
     * this will an Tag Length Value (TLV) buffer.
     * The buffer must be ended with a IHEVCE_ASYNCH_API_END_TAG
     */
    void *pv_asynch_ctrl_bufs;

    /**
    * Asynchronous control commands buffer
    * size in number of bytes
    */
    WORD32 i4_cmd_buf_size;

} iv_input_ctrl_buffs_t;

/**
 *  @brief  Ouput structure in which ouput data
 *          and related parameters are sent from Encoder
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Buffer id for the current buffer */
    WORD32 i4_buf_id;

    /** processing status of the current output returned */
    WORD32 i4_process_ret_sts;

    /** if error encountered the  error code */
    WORD32 i4_process_error_code;

    /** picture type of the current encoded output */
    IV_PICTURE_CODING_TYPE_T i4_encoded_frame_type;

    /** output time stamp of curr encoded buffer : lower 32  */
    WORD32 i4_out_timestamp_low;

    /** output time stamp of curr encoded buffer : higher 32  */
    WORD32 i4_out_timestamp_high;

    /** skip status of the current encoded output */
    WORD32 i4_frame_skipped;

    /** bytes generated in the output buffer */
    WORD32 i4_bytes_generated;

    /** End flag to communicate this is last frame output from encoder */
    WORD32 i4_end_flag;

    /** End flag to communicate encoder that this is the last buffer from application
        1 - Last buf, 0 - Not last buffer. No other values are supported.
        Application has to set the appropriate value before queing in encoder queue */
    WORD32 i4_is_last_buf;

    /** DBF level after the dynamic bitrate change
        -1 - Value not set by codec
        Encoder sets to positive value when bitrate change control call is done*/
    LWORD64 i8_cur_vbv_level;

    /** Output buffer pointer */
    void *pv_bitstream_bufs;

    /** Output buffer size */
    WORD32 i4_bitstream_buf_size;

    /** Can be used for tracking purpose if run time buffer allocation is used*/
    void *pv_metadata;

    /** for system to retrive frame context from Input to Output */
    void *pv_app_frm_ctxt;

    /** Can be used for tracking the buffer that is sent back during callback */
    WORD32 i4_cb_buf_id;

    /** Number of Prefix Non-VCL NAL units in the output buffer */
    WORD32 i4_num_non_vcl_prefix_nals;

    /** Number of Suffix Non-VCL NAL units in the output buffer */
    WORD32 i4_num_non_vcl_suffix_nals;

    /** Number of VCL NAL units in the output buffer */
    WORD32 i4_num_vcl_nals;

    /************************************************************************/
    /* Size of each NAL based on type: Non-VCL Prefix/ VCL / Non-VCL Suffix */
    /*                                                                      */
    /* Ordering of NALS in output buffer is as follows:                     */
    /* Non-VCL Prefix NALs ->  VCL NALs -> Non-VCL Suffix NALs              */
    /*                                                                      */
    /* As there are no holes between adjacent NALs, these sizes can be used */
    /* to compute the offsets w.r.t start of the output buffer              */
    /************************************************************************/

    /** Array to the store the size in bytes of Prefix Non-VCL NAL units */
    WORD32 ai4_size_non_vcl_prefix_nals[MAX_NUM_PREFIX_NALS_PER_AU];

    /* Array to the store the size in bytes of Suffix Non-VCL NAL units */
    WORD32 ai4_size_non_vcl_suffix_nals[MAX_NUM_SUFFIX_NALS_PER_AU];

    /** Array to the store the size in bytes of VCL NAL units */
    WORD32 ai4_size_vcl_nals[MAX_NUM_VCL_NALS_PER_AU];

} iv_output_data_buffs_t;

/**
 *  @brief  Output structure in which output async control
 *          acknowledgement are sent from Encoder
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Buffer id for the current buffer */
    WORD32 i4_buf_id;

    /** Asynchronous control commands ack buffer
     * this will an Tag Length Value (TLV) buffer.
     */
    void *pv_status_bufs;

} iv_output_status_buffs_t;

/**
 *  @brief  structure in which recon data
 *          and related parameters are sent from Encoder
 */
typedef struct
{
    /** Kept for maintaining backwards compatibility in future */
    WORD32 i4_size;

    /** Buffer id for the current buffer */
    WORD32 i4_buf_id;

    /** POC of the current buffer */
    WORD32 i4_poc;

    /** End flag to communicate this is last frame output from encoder */
    WORD32 i4_end_flag;

    /** End flag to communicate encoder that this is the last buffer from application
        1 - Last buf, 0 - Not last buffer. No other values are supported.
        Application has to set the appropriate value before queing in encoder queue */
    WORD32 i4_is_last_buf;

    /** Recon luma buffer pointer */
    void *pv_y_buf;

    /** Recon cb buffer pointer */
    void *pv_cb_buf;

    /** Recon cr buffer pointer */
    void *pv_cr_buf;

    /** Luma size **/
    WORD32 i4_y_pixels;

    /** Chroma size **/
    WORD32 i4_uv_pixels;

} iv_recon_data_buffs_t;

/* @brief iv_res_layer_output_bufs_req_t: This structure contains the parameters
 * related to output (data and control) buffer requirements of the codec for all
 * target resolution layers
 * Application can call the memory query API to get these requirements
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /*Memory requirements for each of target resolutions*/
    iv_output_bufs_req_t s_output_buf_req[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];

} iv_res_layer_output_bufs_req_t;

/* @brief iv_res_layer_recon_bufs_req_t: This structure contains the parameters
 * related to recon buffer requirements of the codec for all target resolution layers
 * Application can call the memory query API to get these requirements
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /*Memory requirements for each of target resolutions*/
    iv_recon_bufs_req_t s_recon_buf_req[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES];
} iv_res_layer_recon_bufs_req_t;

/* @brief iv_res_layer_output_data_buffs_desc_t: This structure contains
 * the parameters related to output data buffers for all target resolution layers
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /*Output buffer requirements of each taregt resolution layer*/
    iv_output_data_buffs_desc_t s_output_data_buffs[IHEVCE_MAX_NUM_RESOLUTIONS]
                                                   [IHEVCE_MAX_NUM_BITRATES];

} iv_res_layer_output_data_buffs_desc_t;

/* @brief iv_res_layer_output_status_buffs_desc_t: This structure contains
 * the parameters related to recon data buffers for all target resolution layers
 */

typedef struct
{
    /** i4_size of the structure : used for verison tracking */
    WORD32 i4_size;

    /*Output buffer requirements of each taregt resolution layer*/
    iv_recon_data_buffs_desc_t s_recon_data_buffs[IHEVCE_MAX_NUM_RESOLUTIONS]
                                                 [IHEVCE_MAX_NUM_BITRATES];

} iv_res_layer_recon_data_buffs_desc_t;

#endif  // _IHEVCE_API_H_
