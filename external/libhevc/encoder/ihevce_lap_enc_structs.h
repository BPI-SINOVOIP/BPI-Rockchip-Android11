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
* \file ihevce_lap_enc_structs.h
*
* \brief
*    This file contains structure definations shared between Encoder and LAP
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_LAP_ENC_STRUCTS_H_
#define _IHEVCE_LAP_ENC_STRUCTS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define MAX_NUM_BUFS_LAP_ENC 15
#define MAX_REF_PICS 16
#define MAX_PICS_FOR_SGI 16 /*max pics to be hold for Sub-Gop Interleave*/
#define MAX_DUPLICATE_ENTRIES_IN_REF_LIST 2
#define MAX_LAP_WINDOW_SIZE 60
#define MAX_SUB_GOP_SIZE 16
#define MAX_SCENE_NUM 30
#define INIT_HEVCE_QP_RC (-300)
#define MAX_TEMPORAL_LAYERS 3
#define NUM_LAP2_LOOK_AHEAD 25

#define INFINITE_GOP_CDR_TIME_S 3
#define FRAME_PARALLEL_LVL 0
#define NUM_SG_INTERLEAVED (1 + FRAME_PARALLEL_LVL)

#define MAX_NUM_ENC_LOOP_PARALLEL 1
#define MAX_NUM_ME_PARALLEL 1
#define DIST_MODE_3_NON_REF_B 0  // disabled for normal cases

#define DENOM_DEFAULT 7
#define WGHT_DEFAULT (1 << DENOM_DEFAULT)

#define MAX_NON_REF_B_PICS_IN_QUEUE_SGI MAX_PICS_FOR_SGI  //ELP_RC

/*minimum stagger in non sequential operation*/
#define MIN_L1_L0_STAGGER_NON_SEQ 1

/* Enable or disable Psedo presets*/
#undef PSEUDO_PRESETS

/**
*******************************************************************************
@brief Ivalid POC value since negative POCs are also valid as per syntax
*******************************************************************************
 */
#define INVALID_POC -16384
/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/* Scenetype enums */
typedef enum SCENE_TYPE_E
{
    SCENE_TYPE_NORMAL = 0,
    SCENE_TYPE_SCENE_CUT,
    SCENE_TYPE_FLASH,
    SCENE_TYPE_FADE_IN,
    SCENE_TYPE_FADE_OUT,
    SCENE_TYPE_DISSOLVE,
    SCENE_TYPE_PAUSE_TO_RESUME,
    MAX_NUM_SCENE_TYPES
} SCENE_TYPE_E;
/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief   Logo structure
******************************************************************************
 */

typedef struct
{
    /** i4_is_logo_on  : Specifies if logo is on or off */
    WORD32 i4_is_logo_on;

    /** logo_width  : Width of the logo in pixels */
    WORD32 logo_width;

    /** logo_height  : Width of the logo in pixels */
    WORD32 logo_height;

    /** logo_x_offset  : horizontal offset for logo from the right end of pic */
    WORD32 logo_x_offset;

    /** logo_y_offset  : vertical offset for logo from the bottom end of pic */
    WORD32 logo_y_offset;

} ihevce_logo_attrs_t;

typedef struct
{
    /**
    *  Input YUV buffers pointers and related parameters
    */
    ihevce_lap_params_t s_lap_params;

    /** Width of input luma */
    WORD32 i4_width;

    /** Height of input luma */
    WORD32 i4_height;

    /** Max closed gop period  : Max spacing between IDR frames  */
    WORD32 i4_max_closed_gop_period;

    /** Min closed gop period  : Min spacing between IDR frames  */
    WORD32 i4_min_closed_gop_period;

    /** Max CRA open gop period: Max spacing between CRA frames  */
    WORD32 i4_max_cra_open_gop_period;

    /** Max i open gop period: Max spacing between I frames  */
    WORD32 i4_max_i_open_gop_period;

    /** limits Max gopsize = 2 ^ i4_max_temporal_layers - 1 */
    WORD32 i4_max_temporal_layers;

    /** Minimum temporal ID from which B-pictures are coded; Tid=1 (default) 0 (no B) */
    WORD32 i4_min_temporal_id_for_b;

    /** Maximum number of reference frames */
    WORD32 i4_max_reference_frames;

    /** Interlace field */
    WORD32 i4_src_interlace_field;

    /* Frame rate*/
    WORD32 i4_frame_rate;

    /** Enable Logo flag */
    WORD32 i4_enable_logo;

    /** Bit Depth */
    WORD32 i4_internal_bit_depth;

    WORD32 i4_input_bit_depth;

    /* 0 - 400; 1 - 420; 2 - 422; 3 - 444 */
    UWORD8 u1_chroma_array_type;

    WORD32 ai4_quality_preset[IHEVCE_MAX_NUM_RESOLUTIONS];

    WORD32 i4_rc_pass_num;

    /* If enable, enables blu ray compatibility of op*/
    WORD32 i4_blu_ray_spec;

    IV_ARCH_T e_arch_type;

    UWORD8 u1_is_popcnt_available;

    WORD32 i4_mres_single_out;

    WORD32 i4_luma_size_copy_src_logo;

} ihevce_lap_static_params_t;

/**
  *  @biref luma and chroma weight and offset container structure
  */
typedef struct
{
    /**
    *  flag to control the weighted pred for luma component of
    *  this reference frame
    *  Range [0 : 1]
    */
    UWORD8 u1_luma_weight_enable_flag;

    /**
    *  flag to control the weighted pred for chroma component of
    *  this reference frame
    *  Range [0 : 1]
    */
    UWORD8 u1_chroma_weight_enable_flag;

    /**
    *  luma weight factor for a reference frame,
    *  Range [0 : 128]
    *  Default = 1 << as_wght_offst
    */
    WORD16 i2_luma_weight;

    /**
    *  luma offset to be added after weighing for reference frame
    *  Range [-128 : 127]
    *  Default = 0
    */
    WORD16 i2_luma_offset;

    /**
    *  chroma weight factor for a reference frame, Default = 1
    */
    WORD16 i2_cb_weight;

    /**
    *  chroma offset to be added after weighing for reference frame, Default = 0
    */
    WORD16 i2_cb_offset;

    /**
    *  chroma weight factor for a reference frame, Default = 1
    */
    WORD16 i2_cr_weight;

    /**
    *  chroma offset to be added after weighing for reference frame, Default = 0
    */
    WORD16 i2_cr_offset;

} ihevce_wght_offst_t;

/**
  *  @biref defines the attributes of a reference picture
  */
typedef struct
{
    /**
    *  weighted prediction attribute for each duplicate entry of a ref pic
    *  Note : Duplicate entries help in using same reference with different
    *         weights and offsets. Example being partial flashes in scence
    */
    ihevce_wght_offst_t as_wght_off[MAX_DUPLICATE_ENTRIES_IN_REF_LIST];

    /**
    * delta POC of reference frame w.r.t current Picture POC,
    */
    WORD32 i4_ref_pic_delta_poc;

    /**
    * flag indicating if this reference frame is to be used as
    * reference by current picture
    * shall be 0 or 1
    */
    WORD32 i4_used_by_cur_pic_flag;

    /**
    * Indicates the number of duplicate entries of a reference picture
    * in the reference picture list. A reference picture may see multiple
    * entries in the reference picture list, since that allows the LAP to
    * assign multiple weighting related parameters to a single reference picture.
    * Range [1, MAX_DUPLICATE_ENTRIES_IN_REF_LIST]
    *
    * Used only when weighted prediction is enabled
    *
    */
    WORD32 i4_num_duplicate_entries_in_ref_list;

} ihevce_ref_pic_attrs_t;

/* @brief IV_YUV_BUF_T: This structure defines attributes
 *        for the input yuv used in enc and lap buffer
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

    /** Luma Process start offset : x dir. */
    WORD32 i4_start_offset_x;

    /** Luma Process start offset : y dir. */
    WORD32 i4_start_offset_y;

    /** Width of the Chroma (Cb / Cr) Buffer in pixels */
    WORD32 i4_uv_wd;

    /** Height of the Chroma (Cb / Cr) Buffer in pixels */
    WORD32 i4_uv_ht;

    /** Stride/Pitch of the Chroma (Cb / Cr) Buffer */
    WORD32 i4_uv_strd;

} iv_enc_yuv_buf_t;

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

} iv_enc_yuv_buf_src_t;

typedef struct
{
    /*********** common params for both lap_out and rc_lap_out ****************/

    /* hevc pic types : IDR/CDR/I/P/B etc */
    WORD32 i4_pic_type;
    /* picture order count */
    WORD32 i4_poc;
    /* temporal layer of the current picture */
    WORD32 i4_temporal_lyr_id;
    /**
     * indicates if the current frame is reference pic
     * 0 : not ref pic
     * 1 : ref pic at lower layers (w.r.t to highest layer id)
     * 2 : ref pic at highest temporal layer id layer
     */
    WORD32 i4_is_ref_pic;
    /**
      * Scene type such as Scene Cut, fade in/ out, dissolve, flash etc
      * enum used is IHEVCE_SCENE_TYPE
    */
    WORD32 i4_scene_type;
    /**
      * Scene number helps to identify the reference frames
      *  for the current frame of same scene and
      * also it can be used to reset the RC model
      *  for each layer whenever scene cut happens
    */
    UWORD32 u4_scene_num;
    /*display order num*/
    WORD32 i4_display_num;

    WORD32 i4_quality_preset;

    /*********** parameters specific to lap_out structure **************/
    /* cra pic type flag */
    WORD32 i4_is_cra_pic;
    /** IDR GOP number */
    WORD32 i4_idr_gop_num;
    /** weighted prediction enable flag     */
    WORD8 i1_weighted_pred_flag;
    /** weighted bipred enable flag         */
    WORD8 i1_weighted_bipred_flag;
    /* number of references for current pic */
    WORD32 i4_num_ref_pics;
    /**
     * common denominator used for luma weights across all ref pics
     * Default = 0, Shall be in the range [0:7]
    */
    WORD32 i4_log2_luma_wght_denom;
    /**
     * common denominator used for chroma weights across all ref pics
     * Default = 0, Shall be in the range [0:7]
    */
    WORD32 i4_log2_chroma_wght_denom;
    /* ref pics to str current Picture POC */
    ihevce_ref_pic_attrs_t as_ref_pics[MAX_REF_PICS];
    /* Structure for the ITTIAM logo */
    ihevce_logo_attrs_t s_logo_ctxt;
    /* first field flag */
    WORD32 i4_first_field;
    /* associated IRAP poc */
    WORD32 i4_assoc_IRAP_poc;
    WORD32 i4_is_prev_pic_in_Tid0_same_scene;

    WORD32 i4_is_I_in_any_field;
    WORD32 i4_used;

    WORD32 i4_end_flag;
    WORD32 i4_force_idr_flag;
    WORD32 i4_out_flush_flag;
    WORD32 i4_first_frm_new_res;

    /***** Spatial QP offset related *****/
    float f_strength;

    long double ld_curr_frame_8x8_log_avg[2];
    long double ld_curr_frame_16x16_log_avg[3];
    long double ld_curr_frame_32x32_log_avg[3];

    LWORD64 i8_curr_frame_8x8_avg_act[2];
    LWORD64 i8_curr_frame_16x16_avg_act[3];
    LWORD64 i8_curr_frame_32x32_avg_act[3];

    WORD32 i4_i_pic_lamda_offset;

    double f_i_pic_lamda_modifier;

    WORD32 i4_curr_frm_qp;

    iv_enc_yuv_buf_t s_input_buf;

    /** Frame - level L0 satd accum*/
    LWORD64 i8_frame_l0_acc_satd;

    /* Frame - level L1 Activity factor */
    LWORD64 i8_frame_level_activity_fact;
    /*bits esimated for frame calulated for sub pic rc bit control */
    WORD32 ai4_frame_bits_estimated[IHEVCE_MAX_NUM_BITRATES];
    float f_pred_factor;

} ihevce_lap_output_params_t;

/**
******************************************************************************
 *  @brief   Encoder and LAP I/O structutre
 *  s_input_buf : input buffer will be populated by applciation
 *  when LAP gets this buffer only input will be populated
 *  During the time of seeting the encode order for current buffer
 *  LAP should populate the s_lap_out structure.
******************************************************************************
 */
typedef struct
{
    /**
    *  Input YUV buffers pointers and related parameters
    */
    iv_input_data_ctrl_buffs_t s_input_buf;

    /**
    * Following parameters are output of LAP
    * for the current buffer to be encoded
    */
    ihevce_lap_output_params_t s_lap_out;
    /**
    * Following parameters are output of LAP
    * for the current buffer to be encoded,
    * which are RC specific parameters
    */
    rc_lap_out_params_t s_rc_lap_out;

    /**
    * Following parameters are context of LAP QUEUE
    */
    frame_info_t s_frame_info;
} ihevce_lap_enc_buf_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

#endif /* _IHEVCE_LAP_ENC_STRUCTS_H_ */
