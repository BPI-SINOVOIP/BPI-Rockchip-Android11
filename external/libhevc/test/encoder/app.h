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

/**
*******************************************************************************
* @file
*  app.h
*
* @brief
*  This file contains all the necessary structure and enumeration definitions
*  needed for the Application
*
* @author
*  ittiam
*
* @remarks
*  none
*
*******************************************************************************
*/

#ifndef _APP_H_
#define _APP_H_

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)

#define STR_LEN 512

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    INVALID,
    HELP,
    VERSION,
    INPUT_YUV,
    OUTPUT,
    STAT_FILE,
    STAT_FILE_BLK,
    SAVE_RECON,
    RECON_YUV,
    NUM_FRAMES_TO_ENCODE,
    START_FRM_OFFSET,
    LOG_DUMP_LEVEL,
    PERF_MODE,
    ENABLE_CSV_DUMP,
    CSV_FILE_PATH,
    ENABLE_LOOPBACK,
    ENABLE_LOGO,
    RES_CHNG_INTRVL,
    SRC_WIDTH,
    SRC_HEIGHT,
    SRC_FRAME_RATE_NUM,
    SRC_FRAME_RATE_DENOM,
    SRC_INTERLACED,
    INPUT_CHROMA_FORMAT,
    INPUT_BIT_DEPTH,
    TOPFIELD_FIRST,
    NUM_RESOLUTIONS,
    MRES_SINGLE_OUT,
    START_RES_ID,
    MBR_QUALITY_SETTING,
    TGT_WIDTH,
    TGT_HEIGHT,
    CODEC_LEVEL,
    NUM_BITRATES,
    TGT_BITRATE,
    FRAME_QP,
    OUTPUT_BIT_DEPTH,
    ENABLE_TEMPORAL_SCALABILITY,
    MAX_CLOSED_GOP_PERIOD,
    MIN_CLOSED_GOP_PERIOD,
    MAX_CRA_OPEN_GOP_PERIOD,
    MAX_I_OPEN_GOP_PERIOD,
    MAX_TEMPORAL_LAYERS,
    QUALITY_PRESET,
    DEBLOCKING_TYPE,
    USE_DEFAULT_SC_MTX,
    ENABLE_ENTROPY_SYNC,
    MAX_TR_TREE_DEPTH_I,
    MAX_TR_TREE_DEPTH_NI,
    MAX_SEARCH_RANGE_HORZ,
    MAX_SEARCH_RANGE_VERT,
    VISUAL_QUALITY_ENHANCEMENTS_TOGGLER,
    ARCH_TYPE,
    NUM_CORES,
    ENABLE_THREAD_AFFINITY,
    RATE_CONTROL_MODE,
    CU_LEVEL_RC,
    PASS,
    MAX_VBV_BUFFER_SIZE,
    PEAK_BITRATE,
    RATE_FACTOR,
    VBR_MAX_PEAK_RATE_DUR,
    MAX_FRAME_QP,
    MIN_FRAME_QP,
    ENABLE_LOOK_AHEAD,
    RC_LOOK_AHEAD_PICS,
    ENABLE_WEIGHTED_PREDICTION,
    CODEC_TYPE,
    CODEC_PROFILE,
    CODEC_TIER,
    AUD_ENABLE_FLAGS,
    INTEROP_FLAGS,
    SPS_AT_CDR_ENABLE,
    SEI_VUI_INFO_CFG,
    VUI_ENABLE,
    SEI_ENABLE_FLAGS,
    SEI_PAYLOAD_ENABLE_FLAGS,
    SEI_PAYLOAD_PATH,
    FORCE_IDR_LOCS_ENABLE,
    FORCE_IDR_LOCS_FILENAME,
    SEI_BUFFER_PERIOD_FLAGS,
    SEI_PIC_TIMING_FLAGS,
    SEI_RECOVERY_POINT_FLAGS,
    SEI_HASH_FLAGS,
    SEI_MASTERING_DISP_COLOUR_VOL_FLAGS,
    DISPLAY_PRIMARIES_X,
    DISPLAY_PRIMARIES_Y,
    WHITE_POINT_X,
    WHITE_POINT_Y,
    MAX_DISPLAY_MASTERING_LUMINANCE,
    MIN_DISPLAY_MASTERING_LUMINANCE,
    SEI_CLL_INFO_ENABLE,
    SEI_MAX_CLL,
    SEI_AVG_CLL,
    TILES_ENABLED_FLAG,
    UNIFORM_SPACING_FLAG,
    NUM_TILE_COLS,
    NUM_TILE_ROWS,
    COLUMN_WIDTH_ARRAY,
    ROW_HEIGHT_ARRAY,
    SLICE_SEGMENT_MODE,
    SLICE_SEGMENT_ARGUMENT,
    ASPECT_RATIO_INFO_PRESENT_FLAG,
    ASPECT_RATIO_IDC,
    SAR_WIDTH,
    SAR_HEIGHT,
    OVERSCAN_INFO_PRESENT_FLAG,
    OVERSCAN_APPROPRIATE_FLAG,
    VIDEO_SIGNAL_TYPE_PRESENT_FLAG,
    VIDEO_FORMAT,
    VIDEO_FULL_RANGE_FLAG,
    COLOUR_DESCRIPTION_PRESENT_FLAG,
    COLOUR_PRIMARIES,
    TRANSFER_CHARACTERISTICS,
    MATRIX_COEFFICIENTS,
    CHROMA_LOC_INFO_PRESENT_FLAG,
    CHROMA_SAMPLE_LOC_TYPE_TOP_FIELD,
    CHROMA_SAMPLE_LOC_TYPE_BOTTOM_FIELD,
    TIMING_INFO_PRESENT_FLAG,
    VUI_HRD_PARAMETERS_PRESENT_FLAG,
    NAL_HRD_PARAMETERS_PRESENT_FLAG,
    CONFIG,
    GRPINFO
} ARGUMENT_T;

/*****************************************************************************/
/*  Structure definitions                                                    */
/*****************************************************************************/

typedef struct
{
    void *ihevceHdl;

    char au1_in_file[STR_LEN]; /*!< input yuv file name
                                    */
    char au1_out_file[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES]
                     [STR_LEN]; /*!< output bitstream filename
                                    */
    char au1_recon_file[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES]
                       [STR_LEN]; /*!< Recon yuv filename
                                    */
    char au1_stat_file[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES]
                      [STR_LEN]; /*!< stat filename from pass1
                                    */
    char au1_stat_blk_file[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES]
                          [STR_LEN]; /*!< stat filename from pass1
                                    */
    char au1_csv_file[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES][STR_LEN];

    ihevce_static_cfg_params_t s_static_cfg_prms;

    char ai1_sei_payload_path[STR_LEN];

} appl_ctxt_t;

typedef struct
{
    /** App context pointer */
    appl_ctxt_t s_app_ctxt;
} main_ctxt_t;

typedef struct
{
    char argument_shortname[25];
    char argument_name[128];
    ARGUMENT_T argument;
    char description[512];
} argument_t;

/*****************************************************************************/
/*  Function Declarations                                                    */
/*****************************************************************************/
void codec_exit(CHAR *pc_err_message);

#endif /* _APP_H_ */
