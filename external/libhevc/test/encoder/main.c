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
* \file main.c
*
* \brief
*    This file contains sample application for HEVC Encoder
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_api.h"
#include "ihevce_plugin.h"
#include "ihevce_profile.h"
#include "app.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define DYN_BITRATE_TEST 0
#define FORCE_IDR_TEST 0

/*****************************************************************************/
/* Global definitions                                                        */
/*****************************************************************************/

/*!
*******************************************************************************
* \brief
*    list of supported arguments
*
*****************************************************************************
*/
// clang-format off
static const argument_t argument_mapping[] =
{
    {"-h",                  "--help",                       HELP,                     "Print help \n"},
    {"-c",                  "--config",                     CONFIG,                   "Input Config file \n" },
    {"-v",                  "--version",                    VERSION,                  "Encoder version \n"},
    {"",                    "",                             GRPINFO, "\n " " File I/O Parameters  \n" " ---------------------\n"},
    {"-i",                  "--input",                      INPUT_YUV,                "Input yuv file {mandatory} \n"},
    {"-o",                  "--output",                     OUTPUT,                   "Output bitstream file {mandatory}\n"},
    {"-frames",             "--num_frames_to_encode",       NUM_FRAMES_TO_ENCODE,     "Number of frames to encode \n"},
    {"-log",                "--log_dump_level",             LOG_DUMP_LEVEL,           "0- [No log/prints] 1- [BitsGenerated, POC, Qp, Pic-type]\n"
    "                                                 2- [1 + PSNR + Seq Summary] 3- [2 + SSIM + Frame Summary] {0}\n"},
    {"",                    "",                             GRPINFO,                  "\n " " Source Parameters    \n" " ---------------------\n"},
    {"-sw",                 "--src_width",                  SRC_WIDTH,                "Input Source Width {mandatory}[240:4096]\n"},
    {"-sh",                 "--src_height",                 SRC_HEIGHT,               "Input Source Height {mandatory}[128:2176] [ \n"},
    {"-fNum",               "--src_frame_rate_num",         SRC_FRAME_RATE_NUM,       "Frame rate numerator {30000}[7500:120000]\n"},
    {"-fDen",               "--src_frame_rate_denom",       SRC_FRAME_RATE_DENOM,     "Frame rate denominator {1000}[1000,1001]\n"},
    {"-pixfmt",             "--input_chroma_format",        INPUT_CHROMA_FORMAT,      "11- YUV_420P; 13- YUV_422P {11}\n"},
    {"",                    "",                             GRPINFO,                  "\n " " Target Parameters  (for all the layers of multi-resolution encoding)    \n" " ------------------------------------------------------------------------\n"},
    {"-level",              "--codec_level",                CODEC_LEVEL,              "Coded Level multiplied by 30 {153}[0:153]\n"},
    {"-b",                  "--tgt_bitrate",                TGT_BITRATE,              "Target bitrates in bps{5000000}."
    "                                                 For MRESxMBR comma seperated BR1,BR2,BR3...\n"},
    {"-qp",                 "--frame_qp",                   FRAME_QP,                 "Initial QP values.Dependes on bit depth {38},"
    "                                                 For MRESxMBR comma seperated QP1,QP2,QP3...\n"},
    {"-obd",                "--output_bit_depth",           OUTPUT_BIT_DEPTH,         "Output bit depth common for all Res.{-ibd}[8,10,12] \n"},
    {"",                    "",                             GRPINFO,                  "\n " " GOP structure Parameters    \n" " ----------------------------\n"},
    {"-maxCgop",            "--max_closed_gop_period",      MAX_CLOSED_GOP_PERIOD,    "Max IDR Pic distance- Closed GOP {0}[0:300] \n"},
    {"-minCgop",            "--min_closed_gop_period",      MIN_CLOSED_GOP_PERIOD,    "Min IDR Pic distance- Closed GOP {0}[0:300]\n"},
    {"-craOgop",            "--max_cra_open_gop_period",    MAX_CRA_OPEN_GOP_PERIOD,  "Max CRA Pic distance- Open GOP {60}[0:300]\n"},
    {"-maxIgop",            "--max_i_open_gop_period",      MAX_I_OPEN_GOP_PERIOD,    "Max I (non CRA, non IDR) Pic distance {0}[0:300]\n"},
    {"-bpicTL",             "--max_temporal_layers",        MAX_TEMPORAL_LAYERS,      "B pyramid layers {3}[0:3] \n"},
    {"",                    "",                             GRPINFO,                  "\n " " Coding tools Parameters    \n" " ---------------------------\n"},
    {"-preset",             "--quality_preset",             QUALITY_PRESET,           "0- PQ, 2- HQ, 3- MS, 4- HS, 5- ES {3}\n"},
    {"-lfd",                "--deblocking_type",            DEBLOCKING_TYPE,          "Debocking 0- enabled, 1- disabled {0}\n"},
    {"-scm",                "--use_default_sc_mtx",         USE_DEFAULT_SC_MTX,       "0- disabled, 1- enabled {0}\n"},
    {"-wpp",                "--enable_entropy_sync",        ENABLE_ENTROPY_SYNC,      "Entropy sync 1- enabled, 0- disabled {0}\n"},
    {"-intraTD",            "--max_tr_tree_depth_I",        MAX_TR_TREE_DEPTH_I,      "Max transform tree depth for intra {3}[1,2,3]\n"},
    {"-interTD",            "--max_tr_tree_depth_nI",       MAX_TR_TREE_DEPTH_NI,     "Max transform tree depth for inter {3}[2,3,4]\n"},
    {"-hrange",             "--max_search_range_horz",      MAX_SEARCH_RANGE_HORZ,    "Horizontal search range {512}[64:512]\n"},
    {"-vrange",             "--max_search_range_vert",      MAX_SEARCH_RANGE_VERT,    "Vertical search range {256}[32:256]\n"},
    {"-arch",                 "--archType",                 ARCH_TYPE,                 "0 => Automatic, 4 => ARM(No neon)\n"},

    {"",                    "",                             GRPINFO,                  "\n " " Multi Core parameters    \n" " -------------------------\n"},
    {"-core",               "--num_cores",                  NUM_CORES,                "#Logical cores (Include hyperthreads){auto}[1:80] \n"},
    {"",                    "",                             GRPINFO,                  "\n " " Rate Control parameters  \n" " -------------------------\n"},
    {"-rc",                 "--rate_control_mode",          RATE_CONTROL_MODE,        "1 -Capped VBR,2- VBR ,3- CQP, 5- CBR {5} \n"},
    {"-aq",                 "--cu_level_rc",                CU_LEVEL_RC,              "CU Qp Modulation 0- Disable 1-Spatial QP modulation \n"},
    {"-maxqp",              "--max_frame_qp",               MAX_FRAME_QP,             "Max frame Qp for I frame {51}[51] \n"},
    {"-minqp",              "--min_frame_qp",               MIN_FRAME_QP,             "Min frame Qp for I frame. Depends on Bit depth {1}[1/-12/-24] \n"},
    {"",                    "",                             GRPINFO,                  "\n " " Look Ahead Processing Parameters  \n" " ----------------------------------\n"},

    {"-lapwindow",          "--rc_look_ahead_pics",         RC_LOOK_AHEAD_PICS,       "RC look ahead window {60}[0:120] \n"},
    {"",                    "",                             GRPINFO,                  "\n " " Output stream Parameters          \n" " ----------------------------------\n"},
    {"-codec",              "--codec_type",                 CODEC_TYPE,               "0- HEVC {0}\n"},
    {"-profile",            "--codec_profile",              CODEC_PROFILE,            "1- Main 2- Main10 4- RExt {1} \n"},
    {"-tier",               "--codec_tier",                 CODEC_TIER,               "0- Main 1- High {1} \n"},
    {"-sps",                "--sps_at_cdr_enable",          SPS_AT_CDR_ENABLE,        "1- enable, 0- disable {1}\n"},
    {"",                    "",                             GRPINFO,                  "\n " " Tile  Parameters          \n" " --------------------------\n"},
    {"-tiles",              "--tiles_enabled_flag",         TILES_ENABLED_FLAG,       "Tile encoding 0- disable 1-enable {0} \n"},
    {"",                    "",                             GRPINFO,                  "\n " " Slice  Parameters         \n" " --------------------------\n"},
    {"-slicemode",          "--slice_segment_mode",         SLICE_SEGMENT_MODE,       "Flag to control dependent slice generation {0}[0,1,2]\n"
    "                                                  0- Disable slices\n"
    "                                                  1- CTB/Slice\n"
    "                                                  2- Bytes/Slice  \n"},
    {"",                    "",                            GRPINFO, "\n " " SEI  parameters            \n" " ---------------------------\n"},
    {"-sei",                "--sei_enable_flags",           SEI_ENABLE_FLAGS,         "1- enable, 0- disable {0}\n"},
    {"-seipayload",         "--sei_payload_enable_flags",   SEI_PAYLOAD_ENABLE_FLAGS, "1- enable, 0- disable {0}\n"},
    {"-seipayloadpath",     "--sei_payload_path",            SEI_PAYLOAD_PATH,          "Input SEI Payload Path (optional)" },
    {"-seibuf",             "--sei_buffer_period_flags",    SEI_BUFFER_PERIOD_FLAGS,  "1- enable, 0- disable {0}\n"},
    {"-seipictime",         "--sei_pic_timing_flags",       SEI_PIC_TIMING_FLAGS,     "1- enable, 0- disable {0}\n"},
    {"-seirecpt",           "--sei_recovery_point_flags",   SEI_RECOVERY_POINT_FLAGS, "1- enable, 0- disable {0}\n"},
    {"-seihash",            "--sei_hash_flags",             SEI_HASH_FLAGS,           "3- Checksum, 2- CRC, 1- MD5, 0- disable {0}\n"},
    {"-seidispcol",         "--sei_mastering_disp_colour_vol_flags",    SEI_MASTERING_DISP_COLOUR_VOL_FLAGS,           "1: enable, 0: disable {0}\n"},
    {"-seiprimx",           "--display_primaries_x",        DISPLAY_PRIMARIES_X,      "X-Primaries: comma separated R,G,B values {}[0:50000] \n"},
    {"-seiprimy",           "--display_primaries_y",        DISPLAY_PRIMARIES_Y,      "Y-Primaries: comma separated R,G,B values {}[0:50000]  \n"},
    {"-seiwhiteptx",        "--white_point_x",              WHITE_POINT_X,            "X White point value {}[0:50000] \n"},
    {"-seiwhitepty",        "--white_point_y",              WHITE_POINT_Y,            "Y White point value {}[0:50000] \n"},
    {"-seidisplummax",      "--max_display_mastering_luminance",  MAX_DISPLAY_MASTERING_LUMINANCE, "Max mastering Luminance. In units  of  0.0001  Candelas/sqmtr {} \n"},
    {"-seidisplummin",      "--min_display_mastering_luminance",  MIN_DISPLAY_MASTERING_LUMINANCE, "Min mastering Luminance. In units  of  0.0001  Candelas/sqmtr {}\n"},
    {"-seicllinfo",         "--sei_content_light_level_info",      SEI_CLL_INFO_ENABLE, "1- enable, 0- disable {0}\n"},
    {"-seimaxcll",          "--max_content_light_level",          SEI_MAX_CLL,        "16bit unsigned number indicating max pixel intensity\n"},
    {"-seiavgcll",          "--max_frame_average_light_level",      SEI_AVG_CLL,      "16bit unsigned number indicating max avg pixel intensity\n"},
    {"",                    "",                             GRPINFO,                  "\n " " VUI  Parameters         \n" " ------------------------\n"},
    {"-vui",                "--vui_enable",                 VUI_ENABLE,               "1- enable, 0- disable {0}\n"},
    {"-arFlag",             "--aspect_ratio_info_present_flag",   ASPECT_RATIO_INFO_PRESENT_FLAG,  "Aspect Ratio 1-enable 0-diable {0} \n"},
    {"-arIdc",              "--aspect_ratio_idc",           ASPECT_RATIO_IDC,        "Aspect Ration IDC {255}[0:255]\n"},
    {"-sarw",               "--sar_width",                  SAR_WIDTH,               "SAR Width {4}[0:65535]\n"},
    {"-sarh",               "--sar_height",                 SAR_HEIGHT,              "SAR Height {3}[0:65535] \n"},
    {"-overscan",           "--overscan_info_present_flag", OVERSCAN_INFO_PRESENT_FLAG,  "Overscan Info. 1-enable 0-disable {0}\n"},
    {"-overscanValid",      "--overscan_appropriate_flag",  OVERSCAN_APPROPRIATE_FLAG,   "Overscan Appropriate 1-enable 0-disable {0}\n"},
    {"-vidsigp",            "--video_signal_type_present_flag",      VIDEO_SIGNAL_TYPE_PRESENT_FLAG,    "Video Signal Type Present. 1-enable 0-diable {1} \n"},
    {"-vidfmt",             "--video_format",               VIDEO_FORMAT,            "Video Format {5}[0:5]\n"},
    {"-fullrange",          "--video_full_range_flag",      VIDEO_FULL_RANGE_FLAG,   "Video Full Range. 1-enable 0-diable {1}\n"},
    {"-colorDesc",          "--colour_description_present_flag",     COLOUR_DESCRIPTION_PRESENT_FLAG,  "Colour description.1-enable 0-diable {0}\n"},
    {"-colorPrim",          "--colour_primaries",           COLOUR_PRIMARIES,        "Colour Primaries {2}[0:255] \n"},
    {"-xferCh",             "--transfer_characteristics",   TRANSFER_CHARACTERISTICS,                  "Transfer Characteristic {2}[0:255]\n"},
    {"-mxcoeff",            "--matrix_coefficients",        MATRIX_COEFFICIENTS,                       "Matrix Coefficients {2}[0:255]\n"},
    {"-chloc",              "--chroma_loc_info_present_flag",        CHROMA_LOC_INFO_PRESENT_FLAG,     "Presence of chroma_sample_loc_type_top_field and "
    "chroma_sample_loc_type_bottom_field.1-enable 0-diable {0}\n"},
    {"-chtf",               "--chroma_sample_loc_type_top_field",    CHROMA_SAMPLE_LOC_TYPE_TOP_FIELD, "Location of Chroma samples for Top field.{0}[0,1] \n"},
    {"-chbf",               "--chroma_sample_loc_type_bottom_field", CHROMA_SAMPLE_LOC_TYPE_BOTTOM_FIELD,    "Location of Chroma samples for Bottom field..{0}[0,1] \n"},
    {"-timinginfo",         "--timing_info_present_flag",            TIMING_INFO_PRESENT_FLAG,        "Timing info.1-enable 0-diable {0}\n"},
    {"-vuihrdparam",        "--vui_hrd_parameters_present_flag",     VUI_HRD_PARAMETERS_PRESENT_FLAG,  "HRD parameters.1-enable 0-diable {0} \n"},
    {"-nalhrdparam",        "--nal_hrd_parameters_present_flag",     NAL_HRD_PARAMETERS_PRESENT_FLAG,  "NAL HRD parameters.1-enable 0-diable {0}\n"}
};
// clang-format on

/*!
******************************************************************************
* \if Function name : print_usage \endif
*
* \brief
*    prints application usage
*
*****************************************************************************
*/
void print_usage(void)
{
    WORD32 i = 0;
    WORD32 num_entries = sizeof(argument_mapping) / sizeof(argument_t);

    printf("\nUsage:\n");
    while(i < num_entries)
    {
        printf("%-32s\t %s", argument_mapping[i].argument_name, argument_mapping[i].description);
        i++;
    }
}

/*!
******************************************************************************
* \if Function name : get_argument \endif
*
* \brief
*    Maps input string to a argument. If the input string is not recognized,
*    returns INVALID
*
*****************************************************************************
*/
ARGUMENT_T get_argument(CHAR *name)
{
    WORD32 i;
    WORD32 num_entries = sizeof(argument_mapping) / sizeof(argument_t);

    for(i = 0; i < num_entries; i++)
    {
        if((0 == strcmp(argument_mapping[i].argument_name, name)) ||
           ((0 == strcmp(argument_mapping[i].argument_shortname, name)) &&
            (0 != strcmp(argument_mapping[i].argument_shortname, "--"))))
        {
            return argument_mapping[i].argument;
        }
    }
    return INVALID;
}

/*!
******************************************************************************
* \if Function name : codec_exit \endif
*
* \brief
*    handles unrecoverable errors. Prints error message to console and exits
*
*****************************************************************************
*/
void codec_exit(CHAR *pc_err_message)
{
    printf("%s\n", pc_err_message);
    exit(-1);
}

/*!
******************************************************************************
* \if Function name : parse_argument \endif
*
* \brief
*    Parse input argument
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T parse_argument(appl_ctxt_t *ps_ctxt, CHAR *argument, CHAR *value)
{
    ihevce_static_cfg_params_t *ps_static_prms = &ps_ctxt->s_static_cfg_prms;
    ARGUMENT_T arg = get_argument(argument);
    WORD32 i4_value = 0;
    UWORD8 au1_keywd_str[STR_LEN];
    UWORD8 *pu1_keywd_str = au1_keywd_str;

    switch(arg)
    {
    case HELP:
        print_usage();
        return IHEVCE_EFAIL;

    case VERSION:
        break;

    case INPUT_YUV:
        sscanf(value, "%s", ps_ctxt->au1_in_file);
        assert(strlen((char *)ps_ctxt->au1_in_file) < STR_LEN);
        break;

    case OUTPUT:
        sscanf(value, "%s", ps_ctxt->au1_out_file[0][0]);
        assert(strlen((char *)ps_ctxt->au1_out_file[0][0]) < STR_LEN);
        break;
    case NUM_FRAMES_TO_ENCODE:
        sscanf(value, "%d", &i4_value);
        if(i4_value < 0)
            ps_static_prms->s_config_prms.i4_num_frms_to_encode = INT32_MAX - 1;
        else
            ps_static_prms->s_config_prms.i4_num_frms_to_encode = i4_value;
        break;

    case LOG_DUMP_LEVEL:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->i4_log_dump_level = i4_value;
        break;
    case SRC_WIDTH:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_src_prms.i4_width = i4_value;
        break;

    case SRC_HEIGHT:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_src_prms.i4_height = i4_value;
        break;

    case SRC_FRAME_RATE_NUM:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_src_prms.i4_frm_rate_num = i4_value;
        break;

    case SRC_FRAME_RATE_DENOM:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_src_prms.i4_frm_rate_denom = i4_value;
        break;
    case INPUT_CHROMA_FORMAT:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_src_prms.inp_chr_format = (IV_COLOR_FORMAT_T)i4_value;
        break;
    case CODEC_LEVEL:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_tgt_lyr_prms.as_tgt_params[0].i4_codec_level = i4_value;
        break;
    case TGT_BITRATE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_tgt_lyr_prms.as_tgt_params[0].ai4_tgt_bitrate[0] = i4_value;
        break;

    case FRAME_QP:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_tgt_lyr_prms.as_tgt_params[0].ai4_frame_qp[0] = i4_value;
        break;
    case MAX_CLOSED_GOP_PERIOD:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_coding_tools_prms.i4_max_closed_gop_period = i4_value;
        break;

    case MIN_CLOSED_GOP_PERIOD:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_coding_tools_prms.i4_min_closed_gop_period = i4_value;
        break;

    case MAX_CRA_OPEN_GOP_PERIOD:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_coding_tools_prms.i4_max_cra_open_gop_period = i4_value;
        break;

    case MAX_I_OPEN_GOP_PERIOD:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_coding_tools_prms.i4_max_i_open_gop_period = i4_value;
        break;

    case MAX_TEMPORAL_LAYERS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_coding_tools_prms.i4_max_temporal_layers = i4_value;
        break;

    case QUALITY_PRESET:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_tgt_lyr_prms.as_tgt_params[0].i4_quality_preset =
            (IHEVCE_QUALITY_CONFIG_T)i4_value;
        break;

    case DEBLOCKING_TYPE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_coding_tools_prms.i4_deblocking_type = i4_value;
        break;

    case USE_DEFAULT_SC_MTX:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_coding_tools_prms.i4_use_default_sc_mtx = i4_value;
        break;

    case ENABLE_ENTROPY_SYNC:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_coding_tools_prms.i4_enable_entropy_sync = i4_value;
        break;

    case MAX_TR_TREE_DEPTH_I:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_config_prms.i4_max_tr_tree_depth_I = i4_value;
        break;

    case MAX_TR_TREE_DEPTH_NI:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_config_prms.i4_max_tr_tree_depth_nI = i4_value;
        break;

    case MAX_SEARCH_RANGE_HORZ:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_config_prms.i4_max_search_range_horz = i4_value;
        break;

    case MAX_SEARCH_RANGE_VERT:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_config_prms.i4_max_search_range_vert = i4_value;
        break;
    case ARCH_TYPE:
        sscanf(value, "%d", &i4_value);
        switch(i4_value)
        {
        case 0:
            ps_static_prms->e_arch_type = ARCH_NA;
            break;
        case 4:
            ps_static_prms->e_arch_type = ARCH_ARM_NONEON;
            break;
        default:
            ps_static_prms->e_arch_type = ARCH_ARM_NONEON;
            break;
        }
        break;

    case NUM_CORES:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_multi_thrd_prms.i4_max_num_cores = i4_value;
        if((i4_value > MAX_NUM_CORES) || (i4_value < 1))
        {
            printf("APLN ERROR >> Number of cores per CPU configured is "
                   "unsupported \n");
            return IHEVCE_EFAIL;
        }
        break;
    case RATE_CONTROL_MODE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_config_prms.i4_rate_control_mode = i4_value;
        break;
    case CU_LEVEL_RC:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_config_prms.i4_cu_level_rc = i4_value;
        break;
    case MAX_FRAME_QP:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_config_prms.i4_max_frame_qp = i4_value;
        break;

    case MIN_FRAME_QP:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_config_prms.i4_min_frame_qp = i4_value;
        break;

    case RC_LOOK_AHEAD_PICS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_lap_prms.i4_rc_look_ahead_pics = i4_value;
        break;

    case CODEC_TYPE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_codec_type = i4_value;
        break;

    case CODEC_PROFILE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_codec_profile = i4_value;
        break;

    case CODEC_TIER:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_codec_tier = i4_value;
        break;

    case SPS_AT_CDR_ENABLE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_sps_at_cdr_enable = i4_value;
        break;

    case VUI_ENABLE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_vui_enable = i4_value;
        break;

    case SEI_ENABLE_FLAGS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_sei_enable_flag = i4_value;
        break;

    case SEI_PAYLOAD_ENABLE_FLAGS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_sei_payload_enable_flag = i4_value;
        break;

    case SEI_PAYLOAD_PATH:
        sscanf(value, "%s", ps_ctxt->ai1_sei_payload_path);
        assert(strlen((char *)ps_ctxt->ai1_sei_payload_path) < STR_LEN);
        break;

    case SEI_BUFFER_PERIOD_FLAGS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_sei_buffer_period_flags = i4_value;
        break;

    case SEI_PIC_TIMING_FLAGS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_sei_pic_timing_flags = i4_value;
        break;

    case SEI_RECOVERY_POINT_FLAGS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_sei_recovery_point_flags = i4_value;
        break;

    case SEI_HASH_FLAGS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag = i4_value;
        break;

    case SEI_MASTERING_DISP_COLOUR_VOL_FLAGS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags = i4_value;
        break;

    case DISPLAY_PRIMARIES_X:
    {
        char *token;
        char *str;
        const char s[2] = ",";
        WORD32 i;

        if(0 == ps_static_prms->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags)
        {
            break;
        }
        sscanf(value, "%s", pu1_keywd_str);

        str = (char *)pu1_keywd_str;
        token = strtok(str, s);

        for(i = 0; i < 3; i++)
        {
            if(token != NULL)
            {
                sscanf(token, "%d", &i4_value);
                ps_static_prms->s_out_strm_prms.au2_display_primaries_x[i] = i4_value;
                token = strtok(NULL, s);
            }
            else if((token == NULL) && (i != 2))
            {
                printf("APLN ERROR >> Insufficient number of display_primary_x "
                       "values entered \n");
                return IHEVCE_EFAIL;
            }
        }
    }
    break;

    case DISPLAY_PRIMARIES_Y:
    {
        char *token;
        char *str;
        const char s[2] = ",";
        WORD32 i;

        if(0 == ps_static_prms->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags)
        {
            break;
        }
        sscanf(value, "%s", pu1_keywd_str);

        str = (char *)pu1_keywd_str;
        token = strtok(str, s);

        for(i = 0; i < 3; i++)
        {
            if(token != NULL)
            {
                sscanf(token, "%d", &i4_value);
                ps_static_prms->s_out_strm_prms.au2_display_primaries_y[i] = i4_value;
                token = strtok(NULL, s);
            }
            else if((token == NULL) && (i != 2))
            {
                printf("APLN ERROR >> Insufficient number of display_primary_x "
                       "values entered \n");
                return IHEVCE_EFAIL;
            }
        }
    }
    break;

    case WHITE_POINT_X:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.u2_white_point_x = i4_value;
        break;

    case WHITE_POINT_Y:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.u2_white_point_y = i4_value;
        break;

    case MAX_DISPLAY_MASTERING_LUMINANCE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.u4_max_display_mastering_luminance = i4_value;
        break;

    case MIN_DISPLAY_MASTERING_LUMINANCE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.u4_min_display_mastering_luminance = i4_value;
        break;

    case SEI_CLL_INFO_ENABLE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.i4_sei_cll_enable = i4_value;
        break;

    case SEI_MAX_CLL:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.u2_sei_max_cll = i4_value;
        break;

    case SEI_AVG_CLL:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_out_strm_prms.u2_sei_avg_cll = i4_value;
        break;

    case TILES_ENABLED_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_app_tile_params.i4_tiles_enabled_flag = i4_value;
        break;
    case SLICE_SEGMENT_MODE:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_slice_params.i4_slice_segment_mode = i4_value;
        break;
    case ASPECT_RATIO_INFO_PRESENT_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_aspect_ratio_info_present_flag = i4_value;
        break;

    case ASPECT_RATIO_IDC:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.au1_aspect_ratio_idc[0] = i4_value;
        break;

    case SAR_WIDTH:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.au2_sar_width[0] = i4_value;
        break;

    case SAR_HEIGHT:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.au2_sar_height[0] = i4_value;
        break;

    case OVERSCAN_INFO_PRESENT_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_overscan_info_present_flag = i4_value;
        break;

    case OVERSCAN_APPROPRIATE_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_overscan_appropriate_flag = i4_value;
        break;

    case VIDEO_SIGNAL_TYPE_PRESENT_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_video_signal_type_present_flag = i4_value;
        break;

    case VIDEO_FORMAT:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_video_format = i4_value;
        break;

    case VIDEO_FULL_RANGE_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_video_full_range_flag = i4_value;
        break;

    case COLOUR_DESCRIPTION_PRESENT_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_colour_description_present_flag = i4_value;
        break;

    case COLOUR_PRIMARIES:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_colour_primaries = i4_value;
        break;

    case TRANSFER_CHARACTERISTICS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_transfer_characteristics = i4_value;
        break;

    case MATRIX_COEFFICIENTS:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_matrix_coefficients = i4_value;
        break;

    case CHROMA_LOC_INFO_PRESENT_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_chroma_loc_info_present_flag = i4_value;
        break;

    case CHROMA_SAMPLE_LOC_TYPE_TOP_FIELD:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_chroma_sample_loc_type_top_field = i4_value;
        break;

    case CHROMA_SAMPLE_LOC_TYPE_BOTTOM_FIELD:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_chroma_sample_loc_type_bottom_field = i4_value;
        break;

    case TIMING_INFO_PRESENT_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_timing_info_present_flag = i4_value;
        break;

    case VUI_HRD_PARAMETERS_PRESENT_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_vui_hrd_parameters_present_flag = i4_value;
        break;

    case NAL_HRD_PARAMETERS_PRESENT_FLAG:
        sscanf(value, "%d", &i4_value);
        ps_static_prms->s_vui_sei_prms.u1_nal_hrd_parameters_present_flag = i4_value;
        break;

    case INVALID:
    default:
        printf("APLN ERROR >> Argument %s is invalid, ignoring \n", argument);
        break;
    }

    return IHEVCE_EOK;
}

/*!
******************************************************************************
* \if Function name : read_cfg_file \endif
*
* \brief
*    Parse config file
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T read_cfg_file(appl_ctxt_t *ps_ctxt, FILE *fp_cfg)
{
    while(1)
    {
        CHAR line[STR_LEN] = { '\0' };
        CHAR argument[STR_LEN] = { '\0' };
        CHAR value[STR_LEN];
        CHAR description[STR_LEN];
        IHEVCE_PLUGIN_STATUS_T status;

        if(NULL == fgets(line, STR_LEN, fp_cfg))
            return IHEVCE_EOK;

        /* split string on whitespace */
        sscanf(line, "%s %s %s", argument, value, description);
        if(argument[0] == '\0' || argument[0] == '#')
            continue;

        status = parse_argument(ps_ctxt, argument, value);
        if(status != IHEVCE_EOK)
        {
            return status;
        }
    }
}

/*!
******************************************************************************
* \if Function name : libihevce_encode_init \endif
*
* \brief
*    Allocates the memory and calls encoder init
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T libihevce_encode_init(appl_ctxt_t *ps_ctxt)
{
    ihevce_static_cfg_params_t *params = &ps_ctxt->s_static_cfg_prms;
    CHAR ac_error[STR_LEN];

    /* call the function to initialise encoder*/
    if(IHEVCE_EFAIL == ihevce_init(params, (void *)&ps_ctxt->ihevceHdl))
    {
        sprintf(ac_error, "Unable to initialise libihevce encoder\n");
        return IHEVCE_EFAIL;
    }

    return IHEVCE_EOK;
}

/*!
******************************************************************************
* \if Function name : allocate_input \endif
*
* \brief
*    allocate input buffers
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T allocate_input(appl_ctxt_t *ps_ctxt, ihevce_inp_buf_t *inp_pic)
{
    ihevce_static_cfg_params_t *params = &ps_ctxt->s_static_cfg_prms;
    WORD32 y_sz = params->s_src_prms.i4_width * params->s_src_prms.i4_height;
    WORD32 uv_sz = y_sz >> 1;
    WORD32 pic_size = y_sz + uv_sz;
    UWORD8 *pu1_buf;

#ifdef X86_MINGW
    pu1_buf = (UWORD8 *)_aligned_malloc(pic_size, 64);
#else
    posix_memalign((void **)&pu1_buf, 64, pic_size);
#endif
    if(NULL == pu1_buf)
    {
        return (IHEVCE_EFAIL);
    }
    if(IV_YUV_420P == params->s_src_prms.inp_chr_format)
    {
        inp_pic->apv_inp_planes[0] = pu1_buf;
        inp_pic->apv_inp_planes[1] = pu1_buf + y_sz;
        inp_pic->apv_inp_planes[2] = pu1_buf + y_sz + (uv_sz >> 1);

        inp_pic->ai4_inp_strd[0] = params->s_src_prms.i4_width;
        inp_pic->ai4_inp_strd[1] = params->s_src_prms.i4_width >> 1;
        inp_pic->ai4_inp_strd[2] = params->s_src_prms.i4_width >> 1;

        inp_pic->ai4_inp_size[0] = y_sz;
        inp_pic->ai4_inp_size[1] = (uv_sz >> 1);
        inp_pic->ai4_inp_size[2] = (uv_sz >> 1);
    }
    else if(IV_YUV_420SP_UV == params->s_src_prms.inp_chr_format)
    {
        inp_pic->apv_inp_planes[0] = pu1_buf;
        inp_pic->apv_inp_planes[1] = pu1_buf + y_sz;
        inp_pic->apv_inp_planes[2] = NULL;

        inp_pic->ai4_inp_strd[0] = params->s_src_prms.i4_width;
        inp_pic->ai4_inp_strd[1] = params->s_src_prms.i4_width;
        inp_pic->ai4_inp_strd[2] = 0;

        inp_pic->ai4_inp_size[0] = y_sz;
        inp_pic->ai4_inp_size[1] = uv_sz;
        inp_pic->ai4_inp_size[2] = 0;
    }

    inp_pic->i4_curr_bitrate = params->s_tgt_lyr_prms.as_tgt_params[0].ai4_tgt_bitrate[0];
    inp_pic->i4_curr_peak_bitrate = params->s_tgt_lyr_prms.as_tgt_params[0].ai4_peak_bitrate[0];
    inp_pic->u8_pts = 0;
    inp_pic->i4_force_idr_flag = 0;

    return IHEVCE_EOK;
}

/*!
******************************************************************************
* \if Function name : read_input \endif
*
* \brief
*    read input from a file
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T read_input(appl_ctxt_t *ps_ctxt, FILE *fp, ihevce_inp_buf_t *inp_pic)
{
    ihevce_static_cfg_params_t *params = &ps_ctxt->s_static_cfg_prms;
    WORD32 au4_wd[3] = { 0 };
    WORD32 au4_ht[3] = { 0 };
    WORD32 num_comp = 3;
    WORD32 comp_idx;
    WORD32 i;

    if(IV_YUV_420P == params->s_src_prms.inp_chr_format)
    {
        au4_wd[0] = params->s_src_prms.i4_width;
        au4_wd[1] = au4_wd[2] = params->s_src_prms.i4_width >> 1;
        au4_ht[0] = params->s_src_prms.i4_height;
        au4_ht[1] = au4_ht[2] = params->s_src_prms.i4_height >> 1;

        num_comp = 3;
    }
    else if(IV_YUV_420SP_UV == params->s_src_prms.inp_chr_format)
    {
        au4_wd[0] = params->s_src_prms.i4_width;
        au4_wd[1] = params->s_src_prms.i4_width;
        au4_ht[0] = params->s_src_prms.i4_height;
        au4_ht[1] = params->s_src_prms.i4_height >> 1;

        num_comp = 2;
    }

    for(comp_idx = 0; comp_idx < num_comp; comp_idx++)
    {
        WORD32 wd = au4_wd[comp_idx];
        WORD32 ht = au4_ht[comp_idx];
        WORD32 strd = inp_pic->ai4_inp_strd[comp_idx];
        UWORD8 *pu1_buf = inp_pic->apv_inp_planes[comp_idx];

        for(i = 0; i < ht; i++)
        {
            WORD32 bytes = fread(pu1_buf, sizeof(UWORD8), wd, fp);
            if(bytes != wd)
            {
                return (IHEVCE_EFAIL);
            }
            pu1_buf += strd;
        }
    }

    return IHEVCE_EOK;
}

/*!
******************************************************************************
* \if Function name : write_output \endif
*
* \brief
*    Write bitstream buffers to a file
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T write_output(FILE *fp, ihevce_out_buf_t *out_pic)
{
    WORD32 bytes;

    bytes = fwrite(out_pic->pu1_output_buf, sizeof(UWORD8), out_pic->i4_bytes_generated, fp);
    if(bytes != out_pic->i4_bytes_generated)
        return IHEVCE_EFAIL;

    return IHEVCE_EOK;
}

/*!
******************************************************************************
* \if Function name : free_input \endif
*
* \brief
*    free input buffers
*
*****************************************************************************
*/
void free_input(ihevce_inp_buf_t *inp_pic)
{
    if(inp_pic->apv_inp_planes[0])
    {
#ifdef X86_MINGW
        _aligned_free(inp_pic->apv_inp_planes[0]);
#else
        free(inp_pic->apv_inp_planes[0]);
#endif
    }
}

/*!
******************************************************************************
* \if Function name : libihevce_encode_close \endif
*
* \brief
*    Frees all the allocated resources and call encoder free
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T libihevce_encode_close(appl_ctxt_t *ps_ctxt)
{
    /* encoder close */
    if(ps_ctxt->ihevceHdl)
        ihevce_close(ps_ctxt->ihevceHdl);

    return IHEVCE_EOK;
}

/*!
******************************************************************************
* \if Function name : libihevce_encode_frame \endif
*
* \brief
*    Calls encoder process and copied the output to pckt buffer
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T libihevce_encode_frame(appl_ctxt_t *ps_ctxt, FILE *pf_inp_yuv, FILE *pf_out)
{
    ihevce_static_cfg_params_t *params = &ps_ctxt->s_static_cfg_prms;
    IHEVCE_PLUGIN_STATUS_T status = IHEVCE_EOK;
    WORD32 i4_num_frames = 0;
    ihevce_inp_buf_t inp_pic;
    ihevce_out_buf_t out_pic;
    CHAR ac_error[STR_LEN];
    profile_database_t s_profile_data;
#if HEADER_MODE
    ihevce_out_buf_t out_pic_hdr;
#endif

    (void)s_profile_data;
    memset(&inp_pic, 0, sizeof(inp_pic));
    memset(&out_pic, 0, sizeof(out_pic));
#if HEADER_MODE
    memset(&out_pic_hdr, 0, sizeof(out_pic_hdr));
#endif

    status = allocate_input(ps_ctxt, &inp_pic);
    if(status != IHEVCE_EOK)
    {
        sprintf(ac_error, "Unable to allocate input");
        return IHEVCE_EFAIL;
    }

#if HEADER_MODE
    status = ihevce_encode_header(ps_ctxt->ihevceHdl, &out_pic_hdr);
    if(status != IHEVCE_EOK)
    {
        sprintf(ac_error, "encode header call failed");
        return IHEVCE_EFAIL;
    }
    if(out_pic_hdr.i4_bytes_generated)
    {
        status = write_output(pf_out, &out_pic_hdr);
        if(status != IHEVCE_EOK)
        {
            sprintf(ac_error, "Unable to write output");
            return IHEVCE_EFAIL;
        }
    }
#endif

    PROFILE_INIT(&s_profile_data);

    while(1)
    {
        ihevce_inp_buf_t *ps_inp_pic = &inp_pic;

        ps_inp_pic->i4_force_idr_flag = 0;

        if(i4_num_frames < params->s_config_prms.i4_num_frms_to_encode)
        {
            status = read_input(ps_ctxt, pf_inp_yuv, &inp_pic);
            if(status != IHEVCE_EOK)
            {
                ps_inp_pic = NULL;
            }
        }
        else
        {
            ps_inp_pic = NULL;
        }
#if DYN_BITRATE_TEST
        if((i4_num_frames == 200) && (ps_inp_pic != NULL))
        {
            ps_inp_pic->i4_curr_bitrate = ps_inp_pic->i4_curr_bitrate << 1;
        }
#endif
#if FORCE_IDR_TEST
        if((i4_num_frames == 70) && (ps_inp_pic != NULL))
        {
            ps_inp_pic->i4_force_idr_flag = 1;
        }
#endif
        /* call encoder process frame */
        PROFILE_START(&s_profile_data);
        status = ihevce_encode(ps_ctxt->ihevceHdl, ps_inp_pic, &out_pic);
        PROFILE_STOP(&s_profile_data, NULL);
        if(status != IHEVCE_EOK)
        {
            sprintf(ac_error, "Unable to process encode");
            return IHEVCE_EFAIL;
        }

        if(out_pic.i4_bytes_generated)
        {
            status = write_output(pf_out, &out_pic);
            if(status != IHEVCE_EOK)
            {
                sprintf(ac_error, "Unable to write output");
                return IHEVCE_EFAIL;
            }
        }

        if(out_pic.i4_end_flag)
            break;

        i4_num_frames++;
        inp_pic.u8_pts +=
            (1000000 * params->s_src_prms.i4_frm_rate_denom) / params->s_src_prms.i4_frm_rate_num;
    }

    PROFILE_END(&s_profile_data, "encode API call");

    free_input(&inp_pic);

    return IHEVCE_EOK;
}

/*!
******************************************************************************
* \if Function name : main \endif
*
* \brief
*    Application to demonstrate codec API. Shows how to use create,
*    process, control and delete
*
*****************************************************************************
*/
int main(int argc, char *argv[])
{
    /* Main context */
    main_ctxt_t s_main_ctxt;

    /* app ctxt */
    appl_ctxt_t *ps_ctxt = &s_main_ctxt.s_app_ctxt;

    /* cfg params */
    ihevce_static_cfg_params_t *params = &ps_ctxt->s_static_cfg_prms;

    /* error string */
    CHAR ac_error[STR_LEN];

    /* config file name */
    CHAR ac_cfg_fname[STR_LEN];

    WORD32 i;
    FILE *fp_cfg = NULL;
    FILE *pf_inp_yuv = NULL;
    FILE *pf_out = NULL;

    /* error status */
    IHEVCE_PLUGIN_STATUS_T status = IHEVCE_EOK;

    /* call the function to set default params */
    if(IHEVCE_EFAIL == ihevce_set_def_params(params))
    {
        sprintf(ac_error, "Unable to set default parameters\n");
        codec_exit(ac_error);
    }

    /* Usage */
    if(argc < 2)
    {
        printf("Using enc.cfg as configuration file \n");
        strcpy(ac_cfg_fname, "enc.cfg");
    }
    else if(argc == 2)
    {
        if(!strcmp(argv[1], "--help"))
        {
            print_usage();
            exit(-1);
        }
        strcpy(ac_cfg_fname, argv[1]);
    }

    /*************************************************************************/
    /* Parse arguments                                                       */
    /*************************************************************************/
    /* Read command line arguments */
    if(argc > 2)
    {
        for(i = 1; i + 1 < argc; i += 2)
        {
            if(CONFIG == get_argument(argv[i]))
            {
                strcpy(ac_cfg_fname, argv[i + 1]);
                if((fp_cfg = fopen(ac_cfg_fname, "r")) == NULL)
                {
                    sprintf(ac_error, "Could not open Configuration file %s", ac_cfg_fname);
                    codec_exit(ac_error);
                }
                status = read_cfg_file(ps_ctxt, fp_cfg);
                if(status != IHEVCE_EOK)
                {
                    sprintf(ac_error, "Encountered error in cfg file");
                    codec_exit(ac_error);
                }
                fclose(fp_cfg);
            }
            else
            {
                status = parse_argument(ps_ctxt, argv[i], argv[i + 1]);
                if(status != IHEVCE_EOK)
                {
                    sprintf(ac_error, "Encountered error in cfg file");
                    codec_exit(ac_error);
                }
            }
        }
    }
    else
    {
        if((fp_cfg = fopen(ac_cfg_fname, "r")) == NULL)
        {
            sprintf(ac_error, "Could not open Configuration file %s", ac_cfg_fname);
            codec_exit(ac_error);
        }
        status = read_cfg_file(ps_ctxt, fp_cfg);
        if(status != IHEVCE_EOK)
        {
            sprintf(ac_error, "Unable to set Configuration parameter");
            codec_exit(ac_error);
        }
        fclose(fp_cfg);
    }

    pf_inp_yuv = fopen(ps_ctxt->au1_in_file, "rb");
    printf("Input file %s \n", ps_ctxt->au1_in_file);
    if(NULL == pf_inp_yuv)
    {
        sprintf(ac_error, "Could not open input file");
        codec_exit(ac_error);
    }

    pf_out = fopen(ps_ctxt->au1_out_file[0][0], "wb");
    printf("Output file %s \n", ps_ctxt->au1_out_file[0][0]);
    if(NULL == pf_out)
    {
        sprintf(ac_error, "Could not open output file");
        codec_exit(ac_error);
    }

    status = libihevce_encode_init(ps_ctxt);
    if(status != IHEVCE_EOK)
    {
        sprintf(ac_error, "Unable to init encoder");
        codec_exit(ac_error);
    }

    status = libihevce_encode_frame(ps_ctxt, pf_inp_yuv, pf_out);
    if(status != IHEVCE_EOK)
    {
        sprintf(ac_error, "Unable to encode frame");
        codec_exit(ac_error);
    }

    status = libihevce_encode_close(ps_ctxt);
    if(status != IHEVCE_EOK)
    {
        sprintf(ac_error, "Unable to close encoder");
        return IHEVCE_EFAIL;
    }

    if(NULL != pf_inp_yuv)
        fclose(pf_inp_yuv);

    if(NULL != pf_out)
        fclose(pf_out);

    return 0;
}
