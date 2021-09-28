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

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevc_defs.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_deblk.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_weighted_pred.h"
#include "ihevc_sao.h"
#include "ihevc_resi_trans.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_cabac_tables.h"

#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_entropy_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_global_tables.h"
#include "ihevce_dep_mngr_interface.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "hme_globals.h"
#include "hme_utils.h"
#include "hme_coarse.h"
#include "hme_refine.h"
#include "hme_err_compute.h"
#include "hme_common_utils.h"
#include "hme_search_algo.h"
#include "ihevce_profile.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

void hme_init_globals()
{
    GRID_PT_T id;
    S32 i, j;
    /*************************************************************************/
    /* Initialize the lookup table for x offset, y offset, optimized mask    */
    /* based on grid id. The design is as follows:                           */
    /*                                                                       */
    /*     a  b  c  d                                                        */
    /*    TL  T TR  e                                                        */
    /*     L  C  R  f                                                        */
    /*    BL  B BR                                                           */
    /*                                                                       */
    /*  IF a non corner pt, like T is the new minima, then we need to        */
    /*  evaluate only 3 new pts, in this case, a, b, c. So the optimal       */
    /*  grid mask would reflect this. If a corner pt like TR is the new      */
    /*  minima, then we need to evaluate 5 new pts, in this case, b, c, d,   */
    /*  e and f. So the grid mask will have 5 pts enabled.                   */
    /*************************************************************************/

    id = PT_C;
    gai4_opt_grid_mask[id] = GRID_ALL_PTS_VALID ^ (BIT_EN(PT_C));
    gai1_grid_id_to_x[id] = 0;
    gai1_grid_id_to_y[id] = 0;
    gai4_opt_grid_mask_diamond[id] = GRID_DIAMOND_ENABLE_ALL ^ (BIT_EN(PT_C));
    gai4_opt_grid_mask_conventional[id] = GRID_ALL_PTS_VALID ^ (BIT_EN(PT_C));

    id = PT_L;
    gai4_opt_grid_mask[id] = BIT_EN(PT_TL) | BIT_EN(PT_L) | BIT_EN(PT_BL);
    gai1_grid_id_to_x[id] = -1;
    gai1_grid_id_to_y[id] = 0;
    gai4_opt_grid_mask_diamond[id] = BIT_EN(PT_T) | BIT_EN(PT_L) | BIT_EN(PT_B);
    gai4_opt_grid_mask_conventional[id] = BIT_EN(PT_T) | BIT_EN(PT_L) | BIT_EN(PT_B);

    id = PT_R;
    gai4_opt_grid_mask[id] = BIT_EN(PT_TR) | BIT_EN(PT_R) | BIT_EN(PT_BR);
    gai1_grid_id_to_x[id] = 1;
    gai1_grid_id_to_y[id] = 0;
    gai4_opt_grid_mask_diamond[id] = BIT_EN(PT_T) | BIT_EN(PT_R) | BIT_EN(PT_B);
    gai4_opt_grid_mask_conventional[id] = BIT_EN(PT_T) | BIT_EN(PT_R) | BIT_EN(PT_B);

    id = PT_T;
    gai4_opt_grid_mask[id] = BIT_EN(PT_TL) | BIT_EN(PT_T) | BIT_EN(PT_TR);
    gai1_grid_id_to_x[id] = 0;
    gai1_grid_id_to_y[id] = -1;
    gai4_opt_grid_mask_diamond[id] = BIT_EN(PT_R) | BIT_EN(PT_L) | BIT_EN(PT_T);
    gai4_opt_grid_mask_conventional[id] = BIT_EN(PT_R) | BIT_EN(PT_L) | BIT_EN(PT_T);

    id = PT_B;
    gai4_opt_grid_mask[id] = BIT_EN(PT_BL) | BIT_EN(PT_B) | BIT_EN(PT_BR);
    gai1_grid_id_to_x[id] = 0;
    gai1_grid_id_to_y[id] = 1;
    gai4_opt_grid_mask_diamond[id] = BIT_EN(PT_B) | BIT_EN(PT_L) | BIT_EN(PT_R);
    gai4_opt_grid_mask_conventional[id] = BIT_EN(PT_B) | BIT_EN(PT_L) | BIT_EN(PT_R);

    id = PT_TL;
    gai4_opt_grid_mask[id] = gai4_opt_grid_mask[PT_L] | gai4_opt_grid_mask[PT_T];
    gai1_grid_id_to_x[id] = -1;
    gai1_grid_id_to_y[id] = -1;
    gai4_opt_grid_mask_conventional[id] = BIT_EN(PT_T) | BIT_EN(PT_L);

    id = PT_TR;
    gai4_opt_grid_mask[id] = gai4_opt_grid_mask[PT_R] | gai4_opt_grid_mask[PT_T];
    gai1_grid_id_to_x[id] = 1;
    gai1_grid_id_to_y[id] = -1;
    gai4_opt_grid_mask_conventional[id] = BIT_EN(PT_T) | BIT_EN(PT_R);

    id = PT_BL;
    gai4_opt_grid_mask[id] = gai4_opt_grid_mask[PT_L] | gai4_opt_grid_mask[PT_B];
    gai1_grid_id_to_x[id] = -1;
    gai1_grid_id_to_y[id] = 1;
    gai4_opt_grid_mask_conventional[id] = BIT_EN(PT_L) | BIT_EN(PT_B);

    id = PT_BR;
    gai4_opt_grid_mask[id] = gai4_opt_grid_mask[PT_R] | gai4_opt_grid_mask[PT_B];
    gai1_grid_id_to_x[id] = 1;
    gai1_grid_id_to_y[id] = 1;
    gai4_opt_grid_mask_conventional[id] = BIT_EN(PT_R) | BIT_EN(PT_B);

    ge_part_id_to_blk_size[CU_8x8][PART_ID_2Nx2N] = BLK_8x8;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_2NxN_T] = BLK_8x4;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_2NxN_B] = BLK_8x4;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_Nx2N_L] = BLK_4x8;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_Nx2N_R] = BLK_4x8;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_NxN_TL] = BLK_4x4;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_NxN_TR] = BLK_4x4;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_NxN_BL] = BLK_4x4;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_NxN_BR] = BLK_4x4;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_2NxnU_T] = BLK_INVALID;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_2NxnU_B] = BLK_INVALID;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_2NxnD_T] = BLK_INVALID;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_2NxnD_B] = BLK_INVALID;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_nLx2N_L] = BLK_INVALID;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_nLx2N_R] = BLK_INVALID;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_nRx2N_L] = BLK_INVALID;
    ge_part_id_to_blk_size[CU_8x8][PART_ID_nRx2N_R] = BLK_INVALID;

    ge_part_id_to_blk_size[CU_16x16][PART_ID_2Nx2N] = BLK_16x16;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_2NxN_T] = BLK_16x8;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_2NxN_B] = BLK_16x8;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_Nx2N_L] = BLK_8x16;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_Nx2N_R] = BLK_8x16;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_NxN_TL] = BLK_8x8;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_NxN_TR] = BLK_8x8;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_NxN_BL] = BLK_8x8;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_NxN_BR] = BLK_8x8;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_2NxnU_T] = BLK_16x4;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_2NxnU_B] = BLK_16x12;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_2NxnD_T] = BLK_16x12;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_2NxnD_B] = BLK_16x4;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_nLx2N_L] = BLK_4x16;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_nLx2N_R] = BLK_12x16;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_nRx2N_L] = BLK_12x16;
    ge_part_id_to_blk_size[CU_16x16][PART_ID_nRx2N_R] = BLK_4x16;

    ge_part_id_to_blk_size[CU_32x32][PART_ID_2Nx2N] = BLK_32x32;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_2NxN_T] = BLK_32x16;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_2NxN_B] = BLK_32x16;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_Nx2N_L] = BLK_16x32;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_Nx2N_R] = BLK_16x32;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_NxN_TL] = BLK_16x16;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_NxN_TR] = BLK_16x16;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_NxN_BL] = BLK_16x16;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_NxN_BR] = BLK_16x16;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_2NxnU_T] = BLK_32x8;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_2NxnU_B] = BLK_32x24;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_2NxnD_T] = BLK_32x24;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_2NxnD_B] = BLK_32x8;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_nLx2N_L] = BLK_8x32;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_nLx2N_R] = BLK_24x32;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_nRx2N_L] = BLK_24x32;
    ge_part_id_to_blk_size[CU_32x32][PART_ID_nRx2N_R] = BLK_8x32;

    ge_part_id_to_blk_size[CU_64x64][PART_ID_2Nx2N] = BLK_64x64;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_2NxN_T] = BLK_64x32;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_2NxN_B] = BLK_64x32;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_Nx2N_L] = BLK_32x64;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_Nx2N_R] = BLK_32x64;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_NxN_TL] = BLK_32x32;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_NxN_TR] = BLK_32x32;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_NxN_BL] = BLK_32x32;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_NxN_BR] = BLK_32x32;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_2NxnU_T] = BLK_64x16;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_2NxnU_B] = BLK_64x48;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_2NxnD_T] = BLK_64x48;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_2NxnD_B] = BLK_64x16;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_nLx2N_L] = BLK_16x64;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_nLx2N_R] = BLK_48x64;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_nRx2N_L] = BLK_48x64;
    ge_part_id_to_blk_size[CU_64x64][PART_ID_nRx2N_R] = BLK_16x64;

    gau1_num_parts_in_part_type[PRT_2Nx2N] = 1;
    gau1_num_parts_in_part_type[PRT_2NxN] = 2;
    gau1_num_parts_in_part_type[PRT_Nx2N] = 2;
    gau1_num_parts_in_part_type[PRT_NxN] = 4;
    gau1_num_parts_in_part_type[PRT_2NxnU] = 2;
    gau1_num_parts_in_part_type[PRT_2NxnD] = 2;
    gau1_num_parts_in_part_type[PRT_nLx2N] = 2;
    gau1_num_parts_in_part_type[PRT_nRx2N] = 2;

    for(i = 0; i < MAX_PART_TYPES; i++)
        for(j = 0; j < MAX_NUM_PARTS; j++)
            ge_part_type_to_part_id[i][j] = PART_ID_INVALID;

    /* 2Nx2N only one partition */
    ge_part_type_to_part_id[PRT_2Nx2N][0] = PART_ID_2Nx2N;

    /* 2NxN 2 partitions */
    ge_part_type_to_part_id[PRT_2NxN][0] = PART_ID_2NxN_T;
    ge_part_type_to_part_id[PRT_2NxN][1] = PART_ID_2NxN_B;

    /* Nx2N 2 partitions */
    ge_part_type_to_part_id[PRT_Nx2N][0] = PART_ID_Nx2N_L;
    ge_part_type_to_part_id[PRT_Nx2N][1] = PART_ID_Nx2N_R;

    /* NxN 4 partitions */
    ge_part_type_to_part_id[PRT_NxN][0] = PART_ID_NxN_TL;
    ge_part_type_to_part_id[PRT_NxN][1] = PART_ID_NxN_TR;
    ge_part_type_to_part_id[PRT_NxN][2] = PART_ID_NxN_BL;
    ge_part_type_to_part_id[PRT_NxN][3] = PART_ID_NxN_BR;

    /* AMP 2Nx (N/2 + 3N/2) 2 partitions */
    ge_part_type_to_part_id[PRT_2NxnU][0] = PART_ID_2NxnU_T;
    ge_part_type_to_part_id[PRT_2NxnU][1] = PART_ID_2NxnU_B;

    /* AMP 2Nx (3N/2 + N/2) 2 partitions */
    ge_part_type_to_part_id[PRT_2NxnD][0] = PART_ID_2NxnD_T;
    ge_part_type_to_part_id[PRT_2NxnD][1] = PART_ID_2NxnD_B;

    /* AMP (N/2 + 3N/2) x 2N 2 partitions */
    ge_part_type_to_part_id[PRT_nLx2N][0] = PART_ID_nLx2N_L;
    ge_part_type_to_part_id[PRT_nLx2N][1] = PART_ID_nLx2N_R;

    /* AMP (3N/2 + N/2) x 2N 2 partitions */
    ge_part_type_to_part_id[PRT_nRx2N][0] = PART_ID_nRx2N_L;
    ge_part_type_to_part_id[PRT_nRx2N][1] = PART_ID_nRx2N_R;

    /*************************************************************************/
    /* initialize attributes for each partition id within the cu.            */
    /*************************************************************************/
    {
        part_attr_t *ps_part_attr;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_2Nx2N];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 8;
        ps_part_attr->u1_y_count = 8;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_2NxN_T];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 8;
        ps_part_attr->u1_y_count = 4;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_2NxN_B];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 4;
        ps_part_attr->u1_x_count = 8;
        ps_part_attr->u1_y_count = 4;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_Nx2N_L];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 4;
        ps_part_attr->u1_y_count = 8;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_Nx2N_R];
        ps_part_attr->u1_x_start = 4;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 4;
        ps_part_attr->u1_y_count = 8;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_NxN_TL];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 4;
        ps_part_attr->u1_y_count = 4;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_NxN_TR];
        ps_part_attr->u1_x_start = 4;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 4;
        ps_part_attr->u1_y_count = 4;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_NxN_BL];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 4;
        ps_part_attr->u1_x_count = 4;
        ps_part_attr->u1_y_count = 4;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_NxN_BR];
        ps_part_attr->u1_x_start = 4;
        ps_part_attr->u1_y_start = 4;
        ps_part_attr->u1_x_count = 4;
        ps_part_attr->u1_y_count = 4;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_2NxnU_T];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 8;
        ps_part_attr->u1_y_count = 2;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_2NxnU_B];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 2;
        ps_part_attr->u1_x_count = 8;
        ps_part_attr->u1_y_count = 6;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_2NxnD_T];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 8;
        ps_part_attr->u1_y_count = 6;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_2NxnD_B];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 6;
        ps_part_attr->u1_x_count = 8;
        ps_part_attr->u1_y_count = 2;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_nLx2N_L];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 2;
        ps_part_attr->u1_y_count = 8;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_nLx2N_R];
        ps_part_attr->u1_x_start = 2;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 6;
        ps_part_attr->u1_y_count = 8;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_nRx2N_L];
        ps_part_attr->u1_x_start = 0;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 6;
        ps_part_attr->u1_y_count = 8;

        ps_part_attr = &gas_part_attr_in_cu[PART_ID_nRx2N_R];
        ps_part_attr->u1_x_start = 6;
        ps_part_attr->u1_y_start = 0;
        ps_part_attr->u1_x_count = 2;
        ps_part_attr->u1_y_count = 8;
    }
    for(i = 0; i < NUM_BLK_SIZES; i++)
        ge_blk_size_to_cu_size[i] = CU_INVALID;

    ge_blk_size_to_cu_size[BLK_8x8] = CU_8x8;
    ge_blk_size_to_cu_size[BLK_16x16] = CU_16x16;
    ge_blk_size_to_cu_size[BLK_32x32] = CU_32x32;
    ge_blk_size_to_cu_size[BLK_64x64] = CU_64x64;

    /* This is the reverse, given cU size, get blk size */
    ge_cu_size_to_blk_size[CU_8x8] = BLK_8x8;
    ge_cu_size_to_blk_size[CU_16x16] = BLK_16x16;
    ge_cu_size_to_blk_size[CU_32x32] = BLK_32x32;
    ge_cu_size_to_blk_size[CU_64x64] = BLK_64x64;

    gau1_is_vert_part[PRT_2Nx2N] = 0;
    gau1_is_vert_part[PRT_2NxN] = 0;
    gau1_is_vert_part[PRT_Nx2N] = 1;
    gau1_is_vert_part[PRT_NxN] = 1;
    gau1_is_vert_part[PRT_2NxnU] = 0;
    gau1_is_vert_part[PRT_2NxnD] = 0;
    gau1_is_vert_part[PRT_nLx2N] = 1;
    gau1_is_vert_part[PRT_nRx2N] = 1;

    /* Initialise the number of best results for the full pell refinement */
    gau1_num_best_results_PQ[PART_ID_2Nx2N] = 2;
    gau1_num_best_results_PQ[PART_ID_2NxN_T] = 0;
    gau1_num_best_results_PQ[PART_ID_2NxN_B] = 0;
    gau1_num_best_results_PQ[PART_ID_Nx2N_L] = 0;
    gau1_num_best_results_PQ[PART_ID_Nx2N_R] = 0;
    gau1_num_best_results_PQ[PART_ID_NxN_TL] = 1;
    gau1_num_best_results_PQ[PART_ID_NxN_TR] = 1;
    gau1_num_best_results_PQ[PART_ID_NxN_BL] = 1;
    gau1_num_best_results_PQ[PART_ID_NxN_BR] = 1;
    gau1_num_best_results_PQ[PART_ID_2NxnU_T] = 1;
    gau1_num_best_results_PQ[PART_ID_2NxnU_B] = 0;
    gau1_num_best_results_PQ[PART_ID_2NxnD_T] = 0;
    gau1_num_best_results_PQ[PART_ID_2NxnD_B] = 1;
    gau1_num_best_results_PQ[PART_ID_nLx2N_L] = 1;
    gau1_num_best_results_PQ[PART_ID_nLx2N_R] = 0;
    gau1_num_best_results_PQ[PART_ID_nRx2N_L] = 0;
    gau1_num_best_results_PQ[PART_ID_nRx2N_R] = 1;

    gau1_num_best_results_HQ[PART_ID_2Nx2N] = 2;
    gau1_num_best_results_HQ[PART_ID_2NxN_T] = 0;
    gau1_num_best_results_HQ[PART_ID_2NxN_B] = 0;
    gau1_num_best_results_HQ[PART_ID_Nx2N_L] = 0;
    gau1_num_best_results_HQ[PART_ID_Nx2N_R] = 0;
    gau1_num_best_results_HQ[PART_ID_NxN_TL] = 1;
    gau1_num_best_results_HQ[PART_ID_NxN_TR] = 1;
    gau1_num_best_results_HQ[PART_ID_NxN_BL] = 1;
    gau1_num_best_results_HQ[PART_ID_NxN_BR] = 1;
    gau1_num_best_results_HQ[PART_ID_2NxnU_T] = 1;
    gau1_num_best_results_HQ[PART_ID_2NxnU_B] = 0;
    gau1_num_best_results_HQ[PART_ID_2NxnD_T] = 0;
    gau1_num_best_results_HQ[PART_ID_2NxnD_B] = 1;
    gau1_num_best_results_HQ[PART_ID_nLx2N_L] = 1;
    gau1_num_best_results_HQ[PART_ID_nLx2N_R] = 0;
    gau1_num_best_results_HQ[PART_ID_nRx2N_L] = 0;
    gau1_num_best_results_HQ[PART_ID_nRx2N_R] = 1;

    gau1_num_best_results_MS[PART_ID_2Nx2N] = 2;
    gau1_num_best_results_MS[PART_ID_2NxN_T] = 0;
    gau1_num_best_results_MS[PART_ID_2NxN_B] = 0;
    gau1_num_best_results_MS[PART_ID_Nx2N_L] = 0;
    gau1_num_best_results_MS[PART_ID_Nx2N_R] = 0;
    gau1_num_best_results_MS[PART_ID_NxN_TL] = 1;
    gau1_num_best_results_MS[PART_ID_NxN_TR] = 1;
    gau1_num_best_results_MS[PART_ID_NxN_BL] = 1;
    gau1_num_best_results_MS[PART_ID_NxN_BR] = 1;
    gau1_num_best_results_MS[PART_ID_2NxnU_T] = 1;
    gau1_num_best_results_MS[PART_ID_2NxnU_B] = 0;
    gau1_num_best_results_MS[PART_ID_2NxnD_T] = 0;
    gau1_num_best_results_MS[PART_ID_2NxnD_B] = 1;
    gau1_num_best_results_MS[PART_ID_nLx2N_L] = 1;
    gau1_num_best_results_MS[PART_ID_nLx2N_R] = 0;
    gau1_num_best_results_MS[PART_ID_nRx2N_L] = 0;
    gau1_num_best_results_MS[PART_ID_nRx2N_R] = 1;

    gau1_num_best_results_HS[PART_ID_2Nx2N] = 2;
    gau1_num_best_results_HS[PART_ID_2NxN_T] = 0;
    gau1_num_best_results_HS[PART_ID_2NxN_B] = 0;
    gau1_num_best_results_HS[PART_ID_Nx2N_L] = 0;
    gau1_num_best_results_HS[PART_ID_Nx2N_R] = 0;
    gau1_num_best_results_HS[PART_ID_NxN_TL] = 0;
    gau1_num_best_results_HS[PART_ID_NxN_TR] = 0;
    gau1_num_best_results_HS[PART_ID_NxN_BL] = 0;
    gau1_num_best_results_HS[PART_ID_NxN_BR] = 0;
    gau1_num_best_results_HS[PART_ID_2NxnU_T] = 0;
    gau1_num_best_results_HS[PART_ID_2NxnU_B] = 0;
    gau1_num_best_results_HS[PART_ID_2NxnD_T] = 0;
    gau1_num_best_results_HS[PART_ID_2NxnD_B] = 0;
    gau1_num_best_results_HS[PART_ID_nLx2N_L] = 0;
    gau1_num_best_results_HS[PART_ID_nLx2N_R] = 0;
    gau1_num_best_results_HS[PART_ID_nRx2N_L] = 0;
    gau1_num_best_results_HS[PART_ID_nRx2N_R] = 0;

    gau1_num_best_results_XS[PART_ID_2Nx2N] = 2;
    gau1_num_best_results_XS[PART_ID_2NxN_T] = 0;
    gau1_num_best_results_XS[PART_ID_2NxN_B] = 0;
    gau1_num_best_results_XS[PART_ID_Nx2N_L] = 0;
    gau1_num_best_results_XS[PART_ID_Nx2N_R] = 0;
    gau1_num_best_results_XS[PART_ID_NxN_TL] = 0;
    gau1_num_best_results_XS[PART_ID_NxN_TR] = 0;
    gau1_num_best_results_XS[PART_ID_NxN_BL] = 0;
    gau1_num_best_results_XS[PART_ID_NxN_BR] = 0;
    gau1_num_best_results_XS[PART_ID_2NxnU_T] = 0;
    gau1_num_best_results_XS[PART_ID_2NxnU_B] = 0;
    gau1_num_best_results_XS[PART_ID_2NxnD_T] = 0;
    gau1_num_best_results_XS[PART_ID_2NxnD_B] = 0;
    gau1_num_best_results_XS[PART_ID_nLx2N_L] = 0;
    gau1_num_best_results_XS[PART_ID_nLx2N_R] = 0;
    gau1_num_best_results_XS[PART_ID_nRx2N_L] = 0;
    gau1_num_best_results_XS[PART_ID_nRx2N_R] = 0;

    gau1_num_best_results_XS25[PART_ID_2Nx2N] = MAX_NUM_CANDS_FOR_FPEL_REFINE_IN_XS25;
    gau1_num_best_results_XS25[PART_ID_2NxN_T] = 0;
    gau1_num_best_results_XS25[PART_ID_2NxN_B] = 0;
    gau1_num_best_results_XS25[PART_ID_Nx2N_L] = 0;
    gau1_num_best_results_XS25[PART_ID_Nx2N_R] = 0;
    gau1_num_best_results_XS25[PART_ID_NxN_TL] = 0;
    gau1_num_best_results_XS25[PART_ID_NxN_TR] = 0;
    gau1_num_best_results_XS25[PART_ID_NxN_BL] = 0;
    gau1_num_best_results_XS25[PART_ID_NxN_BR] = 0;
    gau1_num_best_results_XS25[PART_ID_2NxnU_T] = 0;
    gau1_num_best_results_XS25[PART_ID_2NxnU_B] = 0;
    gau1_num_best_results_XS25[PART_ID_2NxnD_T] = 0;
    gau1_num_best_results_XS25[PART_ID_2NxnD_B] = 0;
    gau1_num_best_results_XS25[PART_ID_nLx2N_L] = 0;
    gau1_num_best_results_XS25[PART_ID_nLx2N_R] = 0;
    gau1_num_best_results_XS25[PART_ID_nRx2N_L] = 0;
    gau1_num_best_results_XS25[PART_ID_nRx2N_R] = 0;

    /* Top right validity for each part id */
    gau1_partid_tr_valid[PART_ID_2Nx2N] = 1;
    gau1_partid_tr_valid[PART_ID_2NxN_T] = 1;
    gau1_partid_tr_valid[PART_ID_2NxN_B] = 0;
    gau1_partid_tr_valid[PART_ID_Nx2N_L] = 1;
    gau1_partid_tr_valid[PART_ID_Nx2N_R] = 1;
    gau1_partid_tr_valid[PART_ID_NxN_TL] = 1;
    gau1_partid_tr_valid[PART_ID_NxN_TR] = 1;
    gau1_partid_tr_valid[PART_ID_NxN_BL] = 1;
    gau1_partid_tr_valid[PART_ID_NxN_BR] = 0;
    gau1_partid_tr_valid[PART_ID_2NxnU_T] = 1;
    gau1_partid_tr_valid[PART_ID_2NxnU_B] = 0;
    gau1_partid_tr_valid[PART_ID_2NxnD_T] = 1;
    gau1_partid_tr_valid[PART_ID_2NxnD_B] = 0;
    gau1_partid_tr_valid[PART_ID_nLx2N_L] = 1;
    gau1_partid_tr_valid[PART_ID_nLx2N_R] = 1;
    gau1_partid_tr_valid[PART_ID_nRx2N_L] = 1;
    gau1_partid_tr_valid[PART_ID_nRx2N_R] = 1;

    /* Bot Left validity for each part id */
    gau1_partid_bl_valid[PART_ID_2Nx2N] = 1;
    gau1_partid_bl_valid[PART_ID_2NxN_T] = 1;
    gau1_partid_bl_valid[PART_ID_2NxN_B] = 1;
    gau1_partid_bl_valid[PART_ID_Nx2N_L] = 1;
    gau1_partid_bl_valid[PART_ID_Nx2N_R] = 0;
    gau1_partid_bl_valid[PART_ID_NxN_TL] = 1;
    gau1_partid_bl_valid[PART_ID_NxN_TR] = 0;
    gau1_partid_bl_valid[PART_ID_NxN_BL] = 1;
    gau1_partid_bl_valid[PART_ID_NxN_BR] = 0;
    gau1_partid_bl_valid[PART_ID_2NxnU_T] = 1;
    gau1_partid_bl_valid[PART_ID_2NxnU_B] = 1;
    gau1_partid_bl_valid[PART_ID_2NxnD_T] = 1;
    gau1_partid_bl_valid[PART_ID_2NxnD_B] = 1;
    gau1_partid_bl_valid[PART_ID_nLx2N_L] = 1;
    gau1_partid_bl_valid[PART_ID_nLx2N_R] = 0;
    gau1_partid_bl_valid[PART_ID_nRx2N_L] = 1;
    gau1_partid_bl_valid[PART_ID_nRx2N_R] = 0;

    /*Part id to part num of this partition id in the CU */
    gau1_part_id_to_part_num[PART_ID_2Nx2N] = 0;
    gau1_part_id_to_part_num[PART_ID_2NxN_T] = 0;
    gau1_part_id_to_part_num[PART_ID_2NxN_B] = 1;
    gau1_part_id_to_part_num[PART_ID_Nx2N_L] = 0;
    gau1_part_id_to_part_num[PART_ID_Nx2N_R] = 1;
    gau1_part_id_to_part_num[PART_ID_NxN_TL] = 0;
    gau1_part_id_to_part_num[PART_ID_NxN_TR] = 1;
    gau1_part_id_to_part_num[PART_ID_NxN_BL] = 2;
    gau1_part_id_to_part_num[PART_ID_NxN_BR] = 3;
    gau1_part_id_to_part_num[PART_ID_2NxnU_T] = 0;
    gau1_part_id_to_part_num[PART_ID_2NxnU_B] = 1;
    gau1_part_id_to_part_num[PART_ID_2NxnD_T] = 0;
    gau1_part_id_to_part_num[PART_ID_2NxnD_B] = 1;
    gau1_part_id_to_part_num[PART_ID_nLx2N_L] = 0;
    gau1_part_id_to_part_num[PART_ID_nLx2N_R] = 1;
    gau1_part_id_to_part_num[PART_ID_nRx2N_L] = 0;
    gau1_part_id_to_part_num[PART_ID_nRx2N_R] = 1;

    /*Which partition type does this partition id belong to */
    ge_part_id_to_part_type[PART_ID_2Nx2N] = PRT_2Nx2N;
    ge_part_id_to_part_type[PART_ID_2NxN_T] = PRT_2NxN;
    ge_part_id_to_part_type[PART_ID_2NxN_B] = PRT_2NxN;
    ge_part_id_to_part_type[PART_ID_Nx2N_L] = PRT_Nx2N;
    ge_part_id_to_part_type[PART_ID_Nx2N_R] = PRT_Nx2N;
    ge_part_id_to_part_type[PART_ID_NxN_TL] = PRT_NxN;
    ge_part_id_to_part_type[PART_ID_NxN_TR] = PRT_NxN;
    ge_part_id_to_part_type[PART_ID_NxN_BL] = PRT_NxN;
    ge_part_id_to_part_type[PART_ID_NxN_BR] = PRT_NxN;
    ge_part_id_to_part_type[PART_ID_2NxnU_T] = PRT_2NxnU;
    ge_part_id_to_part_type[PART_ID_2NxnU_B] = PRT_2NxnU;
    ge_part_id_to_part_type[PART_ID_2NxnD_T] = PRT_2NxnD;
    ge_part_id_to_part_type[PART_ID_2NxnD_B] = PRT_2NxnD;
    ge_part_id_to_part_type[PART_ID_nLx2N_L] = PRT_nLx2N;
    ge_part_id_to_part_type[PART_ID_nLx2N_R] = PRT_nLx2N;
    ge_part_id_to_part_type[PART_ID_nRx2N_L] = PRT_nRx2N;
    ge_part_id_to_part_type[PART_ID_nRx2N_R] = PRT_nRx2N;

    /*************************************************************************/
    /* Set up the bits to be taken up for the part type. This is equally     */
    /* divided up between the various partitions in the part-type.           */
    /* For NxN @ CU 16x16, we assume it as CU 8x8, so consider it as         */
    /* partition 2Nx2N.                                                      */
    /*************************************************************************/
    /* 1 bit for 2Nx2N partition */
    gau1_bits_for_part_id_q1[PART_ID_2Nx2N] = 2;

    /* 3 bits for symmetric part types, so 1.5 bits per partition */
    gau1_bits_for_part_id_q1[PART_ID_2NxN_T] = 3;
    gau1_bits_for_part_id_q1[PART_ID_2NxN_B] = 3;
    gau1_bits_for_part_id_q1[PART_ID_Nx2N_L] = 3;
    gau1_bits_for_part_id_q1[PART_ID_Nx2N_R] = 3;

    /* 1 bit for NxN partitions, assuming these to be 2Nx2N CUs of lower level */
    gau1_bits_for_part_id_q1[PART_ID_NxN_TL] = 2;
    gau1_bits_for_part_id_q1[PART_ID_NxN_TR] = 2;
    gau1_bits_for_part_id_q1[PART_ID_NxN_BL] = 2;
    gau1_bits_for_part_id_q1[PART_ID_NxN_BR] = 2;

    /* 4 bits for AMP so 2 bits per partition */
    gau1_bits_for_part_id_q1[PART_ID_2NxnU_T] = 4;
    gau1_bits_for_part_id_q1[PART_ID_2NxnU_B] = 4;
    gau1_bits_for_part_id_q1[PART_ID_2NxnD_T] = 4;
    gau1_bits_for_part_id_q1[PART_ID_2NxnD_B] = 4;
    gau1_bits_for_part_id_q1[PART_ID_nLx2N_L] = 4;
    gau1_bits_for_part_id_q1[PART_ID_nLx2N_R] = 4;
    gau1_bits_for_part_id_q1[PART_ID_nRx2N_L] = 4;
    gau1_bits_for_part_id_q1[PART_ID_nRx2N_R] = 4;
}

/**
********************************************************************************
*  @fn     hme_enc_num_alloc()
*
*  @brief  returns number of memtabs that is required by hme module
*
*  @return   Number of memtabs required
********************************************************************************
*/
S32 hme_enc_num_alloc(WORD32 i4_num_me_frm_pllel)
{
    if(i4_num_me_frm_pllel > 1)
    {
        return ((S32)MAX_HME_ENC_TOT_MEMTABS);
    }
    else
    {
        return ((S32)MIN_HME_ENC_TOT_MEMTABS);
    }
}

/**
********************************************************************************
*  @fn     hme_coarse_num_alloc()
*
*  @brief  returns number of memtabs that is required by hme module
*
*  @return   Number of memtabs required
********************************************************************************
*/
S32 hme_coarse_num_alloc()
{
    return ((S32)HME_COARSE_TOT_MEMTABS);
}

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_num_alloc()
*
*  @brief  returns number of memtabs that is required by Dep Mngr for hme module
*
*  @return   Number of memtabs required
********************************************************************************
*/
WORD32 hme_coarse_dep_mngr_num_alloc()
{
    return ((WORD32)((MAX_NUM_HME_LAYERS - 1) * ihevce_dmgr_get_num_mem_recs()));
}

S32 hme_validate_init_prms(hme_init_prms_t *ps_prms)
{
    S32 n_layers = ps_prms->num_simulcast_layers;

    /* The final layer has got to be a non encode coarse layer */
    if(n_layers > (MAX_NUM_LAYERS - 1))
        return (-1);

    if(n_layers < 1)
        return (-1);

    /* Width of the coarsest encode layer got to be >= 2*min_wd where min_Wd */
    /* represents the min allowed width in any layer. Ditto with ht          */
    if(ps_prms->a_wd[n_layers - 1] < 2 * (MIN_WD_COARSE))
        return (-1);
    if(ps_prms->a_ht[n_layers - 1] < 2 * (MIN_HT_COARSE))
        return (-1);
    if(ps_prms->max_num_ref > MAX_NUM_REF)
        return (-1);
    if(ps_prms->max_num_ref < 0)
        return (-1);

    return (0);
}
void hme_set_layer_res_attrs(
    layer_ctxt_t *ps_layer, S32 wd, S32 ht, S32 disp_wd, S32 disp_ht, U08 u1_enc)
{
    ps_layer->i4_wd = wd;
    ps_layer->i4_ht = ht;
    ps_layer->i4_disp_wd = disp_wd;
    ps_layer->i4_disp_ht = disp_ht;
    if(0 == u1_enc)
    {
        ps_layer->i4_inp_stride = wd + 32 + 4;
        ps_layer->i4_inp_offset = (ps_layer->i4_inp_stride * 16) + 16;
        ps_layer->i4_pad_x_inp = 16;
        ps_layer->i4_pad_y_inp = 16;
        ps_layer->pu1_inp = ps_layer->pu1_inp_base + ps_layer->i4_inp_offset;
    }
}

/**
********************************************************************************
*  @fn     hme_coarse_get_layer1_mv_bank_ref_idx_size()
*
*  @brief  returns the MV bank and ref idx size of Layer 1 (penultimate)
*
*  @return   none
********************************************************************************
*/
void hme_coarse_get_layer1_mv_bank_ref_idx_size(
    S32 n_tot_layers,
    S32 *a_wd,
    S32 *a_ht,
    S32 max_num_ref,
    S32 *pi4_mv_bank_size,
    S32 *pi4_ref_idx_size)
{
    S32 num_blks, num_mvs_per_blk, num_ref;
    S32 num_cols, num_rows, num_mvs_per_row;
    S32 is_explicit_store = 1;
    S32 wd, ht, num_layers_explicit_search;
    S32 num_results, use_4x4;
    wd = a_wd[1];
    ht = a_ht[1];

    /* Assuming abt 4 layers for 1080p, we do explicit search across all ref */
    /* frames in all but final layer In final layer, it could be 1/2 */
    //ps_hme_init_prms->num_layers_explicit_search = 3;
    num_layers_explicit_search = 3;

    if(num_layers_explicit_search <= 0)
        num_layers_explicit_search = n_tot_layers - 1;

    num_layers_explicit_search = MIN(num_layers_explicit_search, n_tot_layers - 1);

    /* Possibly implicit search for lower (finer) layers */
    if(n_tot_layers - 1 > num_layers_explicit_search)
        is_explicit_store = 0;

    /* coarsest layer alwasy uses 4x4 blks to store results */
    if(1 == (n_tot_layers - 1))
    {
        /* we store 4 results in coarsest layer per blk. 8x4L, 8x4R, 4x8T, 4x8B */
        //ps_hme_init_prms->max_num_results_coarse = 4;
        //vijay : with new algo in coarseset layer this has to be revisited
        num_results = 4;
    }
    else
    {
        /* Every refinement layer stores a max of 2 results per partition */
        //ps_hme_init_prms->max_num_results = 2;
        num_results = 2;
    }
    use_4x4 = hme_get_mv_blk_size(1, 1, n_tot_layers, 0);

    num_cols = use_4x4 ? ((wd >> 2) + 2) : ((wd >> 3) + 2);
    num_rows = use_4x4 ? ((ht >> 2) + 2) : ((ht >> 3) + 2);

    if(is_explicit_store)
        num_ref = max_num_ref;
    else
        num_ref = 2;

    num_blks = num_cols * num_rows;
    num_mvs_per_blk = num_ref * num_results;
    num_mvs_per_row = num_mvs_per_blk * num_cols;

    /* stroe the sizes */
    *pi4_mv_bank_size = num_blks * num_mvs_per_blk * sizeof(hme_mv_t);
    *pi4_ref_idx_size = num_blks * num_mvs_per_blk * sizeof(S08);

    return;
}
/**
********************************************************************************
*  @fn     hme_alloc_init_layer_mv_bank()
*
*  @brief  memory alloc and init function for MV bank
*
*  @return   Number of memtabs required
********************************************************************************
*/
S32 hme_alloc_init_layer_mv_bank(
    hme_memtab_t *ps_memtab,
    S32 max_num_results,
    S32 max_num_ref,
    S32 use_4x4,
    S32 mem_avail,
    S32 u1_enc,
    S32 wd,
    S32 ht,
    S32 is_explicit_store,
    hme_mv_t **pps_mv_base,
    S08 **pi1_ref_idx_base,
    S32 *pi4_num_mvs_per_row)
{
    S32 count = 0;
    S32 size;
    S32 num_blks, num_mvs_per_blk;
    S32 num_ref;
    S32 num_cols, num_rows, num_mvs_per_row;

    if(is_explicit_store)
        num_ref = max_num_ref;
    else
        num_ref = 2;

    /* MV Bank allocation takes into consideration following */
    /* number of results per reference x max num refrences is the amount     */
    /* bufffered up per blk. Numbero f blks in pic deps on the blk size,     */
    /* which could be either 4x4 or 8x8.                                     */
    num_cols = use_4x4 ? ((wd >> 2) + 2) : ((wd >> 3) + 2);
    num_rows = use_4x4 ? ((ht >> 2) + 2) : ((ht >> 3) + 2);

    if(u1_enc)
    {
        /* TODO: CTB64x64 is assumed. FIX according to actual CTB */
        WORD32 num_ctb_cols = ((wd + 63) >> 6);
        WORD32 num_ctb_rows = ((ht + 63) >> 6);

        num_cols = (num_ctb_cols << 3) + 2;
        num_rows = (num_ctb_rows << 3) + 2;
    }
    num_blks = num_cols * num_rows;
    num_mvs_per_blk = num_ref * max_num_results;
    num_mvs_per_row = num_mvs_per_blk * num_cols;

    size = num_blks * num_mvs_per_blk * sizeof(hme_mv_t);
    if(mem_avail)
    {
        /* store this for run time verifications */
        *pi4_num_mvs_per_row = num_mvs_per_row;
        ASSERT(ps_memtab[count].size == size);
        *pps_mv_base = (hme_mv_t *)ps_memtab[count].pu1_mem;
    }
    else
    {
        ps_memtab[count].size = size;
        ps_memtab[count].align = 4;
        ps_memtab[count].e_mem_attr = HME_PERSISTENT_MEM;
    }

    count++;
    /* Ref idx takes the same route as mvbase */

    size = num_blks * num_mvs_per_blk * sizeof(S08);
    if(mem_avail)
    {
        ASSERT(ps_memtab[count].size == size);
        *pi1_ref_idx_base = (S08 *)ps_memtab[count].pu1_mem;
    }
    else
    {
        ps_memtab[count].size = size;
        ps_memtab[count].align = 4;
        ps_memtab[count].e_mem_attr = HME_PERSISTENT_MEM;
    }
    count++;

    return (count);
}
/**
********************************************************************************
*  @fn     hme_alloc_init_layer()
*
*  @brief  memory alloc and init function
*
*  @return   Number of memtabs required
********************************************************************************
*/
S32 hme_alloc_init_layer(
    hme_memtab_t *ps_memtab,
    S32 max_num_results,
    S32 max_num_ref,
    S32 use_4x4,
    S32 mem_avail,
    S32 u1_enc,
    S32 wd,
    S32 ht,
    S32 disp_wd,
    S32 disp_ht,
    S32 segment_layer,
    S32 is_explicit_store,
    layer_ctxt_t **pps_layer)
{
    S32 count = 0;
    layer_ctxt_t *ps_layer = NULL;
    S32 size;
    S32 num_ref;

    ARG_NOT_USED(segment_layer);

    if(is_explicit_store)
        num_ref = max_num_ref;
    else
        num_ref = 2;

    /* We do not store 4x4 results for encoding layers */
    if(u1_enc)
        use_4x4 = 0;

    size = sizeof(layer_ctxt_t);
    if(mem_avail)
    {
        ASSERT(ps_memtab[count].size == size);
        ps_layer = (layer_ctxt_t *)ps_memtab[count].pu1_mem;
        *pps_layer = ps_layer;
    }
    else
    {
        ps_memtab[count].size = size;
        ps_memtab[count].align = 8;
        ps_memtab[count].e_mem_attr = HME_PERSISTENT_MEM;
    }

    count++;

    /* Input luma buffer allocated only for non encode case */
    if(0 == u1_enc)
    {
        /* Allocate input with padding of 16 pixels */
        size = (wd + 32 + 4) * (ht + 32 + 4);
        if(mem_avail)
        {
            ASSERT(ps_memtab[count].size == size);
            ps_layer->pu1_inp_base = ps_memtab[count].pu1_mem;
        }
        else
        {
            ps_memtab[count].size = size;
            ps_memtab[count].align = 16;
            ps_memtab[count].e_mem_attr = HME_PERSISTENT_MEM;
        }
        count++;
    }

    /* Allocate memory or just the layer mvbank strcture. */
    /* TODO : see if this can be removed by moving it to layer_ctxt */
    size = sizeof(layer_mv_t);

    if(mem_avail)
    {
        ASSERT(ps_memtab[count].size == size);
        ps_layer->ps_layer_mvbank = (layer_mv_t *)ps_memtab[count].pu1_mem;
    }
    else
    {
        ps_memtab[count].size = size;
        ps_memtab[count].align = 8;
        ps_memtab[count].e_mem_attr = HME_PERSISTENT_MEM;
    }

    count++;

    if(mem_avail)
    {
        hme_set_layer_res_attrs(ps_layer, wd, ht, disp_wd, disp_ht, u1_enc);
    }

    return (count);
}

S32 hme_alloc_init_search_nodes(
    search_results_t *ps_search_results,
    hme_memtab_t *ps_memtabs,
    S32 mem_avail,
    S32 max_num_ref,
    S32 max_num_results)
{
    S32 size = max_num_results * sizeof(search_node_t) * max_num_ref * TOT_NUM_PARTS;
    S32 j, k;
    search_node_t *ps_search_node;

    if(mem_avail == 0)
    {
        ps_memtabs->size = size;
        ps_memtabs->align = 4;
        ps_memtabs->e_mem_attr = HME_SCRATCH_OVLY_MEM;
        return (1);
    }

    ps_search_node = (search_node_t *)ps_memtabs->pu1_mem;
    ASSERT(ps_memtabs->size == size);
    /****************************************************************************/
    /* For each CU, we search and store N best results, per partition, per ref  */
    /* So, number of memtabs is  num_refs * num_parts                           */
    /****************************************************************************/
    for(j = 0; j < max_num_ref; j++)
    {
        for(k = 0; k < TOT_NUM_PARTS; k++)
        {
            ps_search_results->aps_part_results[j][k] = ps_search_node;
            ps_search_node += max_num_results;
        }
    }
    return (1);
}

S32 hme_derive_num_layers(S32 n_enc_layers, S32 *p_wd, S32 *p_ht, S32 *p_disp_wd, S32 *p_disp_ht)
{
    S32 i;
    /* We keep downscaling by 2 till we hit one of the conditions:           */
    /* 1. MAX_NUM_LAYERS reached.                                            */
    /* 2. Width or ht goes below min width and ht allowed at coarsest layer  */
    ASSERT(n_enc_layers < MAX_NUM_LAYERS);
    ASSERT(n_enc_layers > 0);
    ASSERT(p_wd[0] <= HME_MAX_WIDTH);
    ASSERT(p_ht[0] <= HME_MAX_HEIGHT);

    p_disp_wd[0] = p_wd[0];
    p_disp_ht[0] = p_ht[0];
    /*************************************************************************/
    /* Verify that for simulcast, lower layer to higher layer ratio is bet   */
    /* 2 (dyadic) and 1.33. Typically it should be 1.5.                      */
    /* TODO : for interlace, we may choose to have additional downscaling for*/
    /* width alone in coarsest layer to next layer.                          */
    /*************************************************************************/
    for(i = 1; i < n_enc_layers; i++)
    {
        S32 wd1, wd2, ht1, ht2;
        wd1 = FLOOR16(p_wd[i - 1] >> 1);
        wd2 = CEIL16((p_wd[i - 1] * 3) >> 2);
        ASSERT(p_wd[i] >= wd1);
        ASSERT(p_wd[i] <= wd2);
        ht1 = FLOOR16(p_ht[i - 1] >> 1);
        ht2 = CEIL16((p_ht[i - 1] * 3) >> 2);
        ASSERT(p_ht[i] >= ht1);
        ASSERT(p_ht[i] <= ht2);
    }
    ASSERT(p_wd[n_enc_layers - 1] >= 2 * MIN_WD_COARSE);
    ASSERT(p_ht[n_enc_layers - 1] >= 2 * MIN_HT_COARSE);

    for(i = n_enc_layers; i < MAX_NUM_LAYERS; i++)
    {
        if((p_wd[i - 1] < 2 * MIN_WD_COARSE) || (p_ht[i - 1] < 2 * MIN_HT_COARSE))
        {
            return (i);
        }
        /* Use CEIL16 to facilitate 16x16 searches in future, or to do       */
        /* segmentation study in future                                      */
        p_wd[i] = CEIL16(p_wd[i - 1] >> 1);
        p_ht[i] = CEIL16(p_ht[i - 1] >> 1);

        p_disp_wd[i] = p_disp_wd[i - 1] >> 1;
        p_disp_ht[i] = p_disp_ht[i - 1] >> 1;
    }
    return (i);
}

/**
********************************************************************************
*  @fn     hme_get_mv_blk_size()
*
*  @brief  returns whether blk uses 4x4 size or something else.
*
*  @param[in] enable_4x4 : input param from application to enable 4x4
*
*  @param[in] layer_id : id of current layer (0 finest)
*
*  @param[in] num_layeers : total num layers
*
*  @param[in] is_enc : Whether encoding enabled for layer
*
*  @return   1 for 4x4 blks, 0 for 8x8
********************************************************************************
*/
S32 hme_get_mv_blk_size(S32 enable_4x4, S32 layer_id, S32 num_layers, S32 is_enc)
{
    S32 use_4x4 = enable_4x4;

    if((layer_id <= 1) && (num_layers >= 4))
        use_4x4 = USE_4x4_IN_L1;
    if(layer_id == num_layers - 1)
        use_4x4 = 1;
    if(is_enc)
        use_4x4 = 0;

    return (use_4x4);
}

/**
********************************************************************************
*  @fn     hme_enc_alloc_init_mem()
*
*  @brief  Requests/ assign memory based on mem avail
*
*  @param[in] ps_memtabs : memtab array
*
*  @param[in] ps_prms : init prms
*
*  @param[in] pv_ctxt : ME ctxt
*
*  @param[in] mem_avail : request/assign flag
*
*  @return   1 for 4x4 blks, 0 for 8x8
********************************************************************************
*/
S32 hme_enc_alloc_init_mem(
    hme_memtab_t *ps_memtabs,
    hme_init_prms_t *ps_prms,
    void *pv_ctxt,
    S32 mem_avail,
    S32 i4_num_me_frm_pllel)
{
    me_master_ctxt_t *ps_master_ctxt = (me_master_ctxt_t *)pv_ctxt;
    me_ctxt_t *ps_ctxt;
    S32 count = 0, size, i, j, use_4x4;
    S32 n_tot_layers, n_enc_layers;
    S32 num_layers_explicit_search;
    S32 a_wd[MAX_NUM_LAYERS], a_ht[MAX_NUM_LAYERS];
    S32 a_disp_wd[MAX_NUM_LAYERS], a_disp_ht[MAX_NUM_LAYERS];
    S32 num_results;
    S32 num_thrds;
    S32 ctb_wd = 1 << ps_prms->log_ctb_size;

    /* MV bank changes */
    hme_mv_t *aps_mv_bank[((DEFAULT_MAX_REFERENCE_PICS << 1) * MAX_NUM_ME_PARALLEL) + 1] = { NULL };
    S32 i4_num_mvs_per_row = 0;
    S08 *api1_ref_idx[((DEFAULT_MAX_REFERENCE_PICS << 1) * MAX_NUM_ME_PARALLEL) + 1] = { NULL };

    n_enc_layers = ps_prms->num_simulcast_layers;

    /* Memtab 0: handle */
    size = sizeof(me_master_ctxt_t);
    if(mem_avail)
    {
        /* store the number of processing threads */
        ps_master_ctxt->i4_num_proc_thrds = ps_prms->i4_num_proc_thrds;
    }
    else
    {
        ps_memtabs[count].size = size;
        ps_memtabs[count].align = 8;
        ps_memtabs[count].e_mem_attr = HME_PERSISTENT_MEM;
    }

    count++;

    /* Memtab 1: ME threads ctxt */
    size = ps_prms->i4_num_proc_thrds * sizeof(me_ctxt_t);
    if(mem_avail)
    {
        me_ctxt_t *ps_me_tmp_ctxt = (me_ctxt_t *)ps_memtabs[count].pu1_mem;

        /* store the indivisual thread ctxt pointers */
        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            ps_master_ctxt->aps_me_ctxt[num_thrds] = ps_me_tmp_ctxt++;
        }
    }
    else
    {
        ps_memtabs[count].size = size;
        ps_memtabs[count].align = 8;
        ps_memtabs[count].e_mem_attr = HME_PERSISTENT_MEM;
    }

    count++;

    /* Memtab 2: ME frame ctxts */
    size = sizeof(me_frm_ctxt_t) * MAX_NUM_ME_PARALLEL * ps_prms->i4_num_proc_thrds;
    if(mem_avail)
    {
        me_frm_ctxt_t *ps_me_frm_tmp_ctxt = (me_frm_ctxt_t *)ps_memtabs[count].pu1_mem;

        for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
        {
            /* store the indivisual thread ctxt pointers */
            for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
            {
                ps_master_ctxt->aps_me_ctxt[num_thrds]->aps_me_frm_prms[i] = ps_me_frm_tmp_ctxt;

                ps_me_frm_tmp_ctxt++;
            }
        }
    }
    else
    {
        ps_memtabs[count].size = size;
        ps_memtabs[count].align = 8;
        ps_memtabs[count].e_mem_attr = HME_PERSISTENT_MEM;
    }

    count++;

    memcpy(a_wd, ps_prms->a_wd, sizeof(S32) * ps_prms->num_simulcast_layers);
    memcpy(a_ht, ps_prms->a_ht, sizeof(S32) * ps_prms->num_simulcast_layers);
    /*************************************************************************/
    /* Derive the number of HME layers, including both encoded and non encode*/
    /* This function also derives the width and ht of each layer.            */
    /*************************************************************************/
    n_tot_layers = hme_derive_num_layers(n_enc_layers, a_wd, a_ht, a_disp_wd, a_disp_ht);
    num_layers_explicit_search = ps_prms->num_layers_explicit_search;
    if(num_layers_explicit_search <= 0)
        num_layers_explicit_search = n_tot_layers - 1;

    num_layers_explicit_search = MIN(num_layers_explicit_search, n_tot_layers - 1);

    if(mem_avail)
    {
        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            me_frm_ctxt_t *ps_frm_ctxt;
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
            {
                ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[i];

                memset(ps_frm_ctxt->u1_encode, 0, n_tot_layers);
                memset(ps_frm_ctxt->u1_encode, 1, n_enc_layers);

                /* only one enocde layer is used */
                ps_frm_ctxt->num_layers = 1;

                ps_frm_ctxt->i4_wd = a_wd[0];
                ps_frm_ctxt->i4_ht = a_ht[0];
                /*
            memcpy(ps_ctxt->a_wd, a_wd, sizeof(S32)*n_tot_layers);
            memcpy(ps_ctxt->a_ht, a_ht, sizeof(S32)*n_tot_layers);
*/
                ps_frm_ctxt->num_layers_explicit_search = num_layers_explicit_search;
                ps_frm_ctxt->max_num_results = ps_prms->max_num_results;
                ps_frm_ctxt->max_num_results_coarse = ps_prms->max_num_results_coarse;
                ps_frm_ctxt->max_num_ref = ps_prms->max_num_ref;
            }
        }
    }

    /* Memtabs : Layers MV bank for encode layer */
    /* Each ref_desr in master ctxt will have seperate layer ctxt */

    for(i = 0; i < (ps_prms->max_num_ref * i4_num_me_frm_pllel) + 1; i++)
    {
        for(j = 0; j < 1; j++)
        {
            S32 is_explicit_store = 1;
            S32 wd, ht;
            U08 u1_enc = 1;
            wd = a_wd[j];
            ht = a_ht[j];

            /* Possibly implicit search for lower (finer) layers */
            if(n_tot_layers - j > num_layers_explicit_search)
                is_explicit_store = 0;

            /* Even if explicit search, we store only 2 results (L0 and L1) */
            /* in finest layer */
            if(j == 0)
            {
                is_explicit_store = 0;
            }

            /* coarsest layer alwasy uses 4x4 blks to store results */
            if(j == n_tot_layers - 1)
            {
                num_results = ps_prms->max_num_results_coarse;
            }
            else
            {
                num_results = ps_prms->max_num_results;
                if(j == 0)
                    num_results = 1;
            }
            use_4x4 = hme_get_mv_blk_size(ps_prms->use_4x4, j, n_tot_layers, u1_enc);

            count += hme_alloc_init_layer_mv_bank(
                &ps_memtabs[count],
                num_results,
                ps_prms->max_num_ref,
                use_4x4,
                mem_avail,
                u1_enc,
                wd,
                ht,
                is_explicit_store,
                &aps_mv_bank[i],
                &api1_ref_idx[i],
                &i4_num_mvs_per_row);
        }
    }

    /* Memtabs : Layers * num-ref + 1 */
    for(i = 0; i < (ps_prms->max_num_ref * i4_num_me_frm_pllel) + 1; i++)
    {
        /* layer memory allocated only for enocde layer */
        for(j = 0; j < 1; j++)
        {
            layer_ctxt_t *ps_layer;
            S32 is_explicit_store = 1;
            S32 segment_this_layer = (j == 0) ? 1 : ps_prms->segment_higher_layers;
            S32 wd, ht;
            U08 u1_enc = 1;
            wd = a_wd[j];
            ht = a_ht[j];

            /* Possibly implicit search for lower (finer) layers */
            if(n_tot_layers - j > num_layers_explicit_search)
                is_explicit_store = 0;

            /* Even if explicit search, we store only 2 results (L0 and L1) */
            /* in finest layer */
            if(j == 0)
            {
                is_explicit_store = 0;
            }

            /* coarsest layer alwasy uses 4x4 blks to store results */
            if(j == n_tot_layers - 1)
            {
                num_results = ps_prms->max_num_results_coarse;
            }
            else
            {
                num_results = ps_prms->max_num_results;
                if(j == 0)
                    num_results = 1;
            }
            use_4x4 = hme_get_mv_blk_size(ps_prms->use_4x4, j, n_tot_layers, u1_enc);

            count += hme_alloc_init_layer(
                &ps_memtabs[count],
                num_results,
                ps_prms->max_num_ref,
                use_4x4,
                mem_avail,
                u1_enc,
                wd,
                ht,
                a_disp_wd[j],
                a_disp_ht[j],
                segment_this_layer,
                is_explicit_store,
                &ps_layer);
            if(mem_avail)
            {
                /* same ps_layer memory pointer is stored in all the threads */
                for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
                {
                    ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
                    ps_ctxt->as_ref_descr[i].aps_layers[j] = ps_layer;
                }

                /* store the MV bank pointers */
                ps_layer->ps_layer_mvbank->max_num_mvs_per_row = i4_num_mvs_per_row;
                ps_layer->ps_layer_mvbank->ps_mv_base = aps_mv_bank[i];
                ps_layer->ps_layer_mvbank->pi1_ref_idx_base = api1_ref_idx[i];
            }
        }
    }

    /* Memtabs : Buf Mgr for predictor bufs and working mem */
    /* TODO : Parameterise this appropriately */
    size = MAX_WKG_MEM_SIZE_PER_THREAD * ps_prms->i4_num_proc_thrds * i4_num_me_frm_pllel;

    if(mem_avail)
    {
        U08 *pu1_mem = ps_memtabs[count].pu1_mem;

        ASSERT(ps_memtabs[count].size == size);

        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            me_frm_ctxt_t *ps_frm_ctxt;
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
            {
                ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[i];

                hme_init_wkg_mem(&ps_frm_ctxt->s_buf_mgr, pu1_mem, MAX_WKG_MEM_SIZE_PER_THREAD);

                if(i4_num_me_frm_pllel != 1)
                {
                    /* update the memory buffer pointer */
                    pu1_mem += MAX_WKG_MEM_SIZE_PER_THREAD;
                }
            }
            if(i4_num_me_frm_pllel == 1)
            {
                pu1_mem += MAX_WKG_MEM_SIZE_PER_THREAD;
            }
        }
    }
    else
    {
        ps_memtabs[count].size = size;
        ps_memtabs[count].align = 4;
        ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
    }
    count++;

    /*************************************************************************/
    /* Memtab : We need 64x64 buffer to store the entire CTB input for bidir */
    /* refinement. This memtab stores 2I - P0, I is input and P0 is L0 pred  */
    /*************************************************************************/
    size = sizeof(S16) * CTB_BLK_SIZE * CTB_BLK_SIZE * ps_prms->i4_num_proc_thrds *
           i4_num_me_frm_pllel;

    if(mem_avail)
    {
        S16 *pi2_mem = (S16 *)ps_memtabs[count].pu1_mem;

        ASSERT(ps_memtabs[count].size == size);

        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            me_frm_ctxt_t *ps_frm_ctxt;
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
            {
                ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[i];

                ps_frm_ctxt->pi2_inp_bck = pi2_mem;
                /** If no me frames running in parallel update the other aps_me_frm_prms indices with same memory **/
                if(i4_num_me_frm_pllel != 1)
                {
                    pi2_mem += (CTB_BLK_SIZE * CTB_BLK_SIZE);
                }
            }
            if(i4_num_me_frm_pllel == 1)
            {
                pi2_mem += (CTB_BLK_SIZE * CTB_BLK_SIZE);
            }
        }
    }
    else
    {
        ps_memtabs[count].size = size;
        ps_memtabs[count].align = 16;
        ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
    }

    count++;

    /* Allocate a memtab for each histogram. As many as num ref and number of threads */
    /* Loop across for each ME_FRM in PARALLEL */
    for(j = 0; j < MAX_NUM_ME_PARALLEL; j++)
    {
        for(i = 0; i < ps_prms->max_num_ref; i++)
        {
            size = ps_prms->i4_num_proc_thrds * sizeof(mv_hist_t);
            if(mem_avail)
            {
                mv_hist_t *ps_mv_hist = (mv_hist_t *)ps_memtabs[count].pu1_mem;

                ASSERT(size == ps_memtabs[count].size);

                /* divide the memory accross the threads */
                for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
                {
                    ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

                    ps_ctxt->aps_me_frm_prms[j]->aps_mv_hist[i] = ps_mv_hist;
                    ps_mv_hist++;
                }
            }
            else
            {
                ps_memtabs[count].size = size;
                ps_memtabs[count].align = 8;
                ps_memtabs[count].e_mem_attr = HME_PERSISTENT_MEM;
            }
            count++;
        }
        if((i4_num_me_frm_pllel == 1) && (j != (MAX_NUM_ME_PARALLEL - 1)))
        {
            /** If no me frames running in parallel update the other aps_me_frm_prms indices with same memory **/
            /** bring the count back to earlier value if there are no me frames in parallel. don't decrement for last loop **/
            count -= ps_prms->max_num_ref;
        }
    }

    /* Memtabs : Search nodes for 16x16 CUs, 32x32 and 64x64 CUs */
    for(j = 0; j < MAX_NUM_ME_PARALLEL; j++)
    {
        S32 count_cpy = count;
        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            if(mem_avail)
            {
                ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
            }

            for(i = 0; i < 21; i++)
            {
                search_results_t *ps_search_results = NULL;
                if(mem_avail)
                {
                    if(i < 16)
                    {
                        ps_search_results =
                            &ps_ctxt->aps_me_frm_prms[j]->as_search_results_16x16[i];
                    }
                    else if(i < 20)
                    {
                        ps_search_results =
                            &ps_ctxt->aps_me_frm_prms[j]->as_search_results_32x32[i - 16];
                        ps_search_results->ps_cu_results =
                            &ps_ctxt->aps_me_frm_prms[j]->as_cu32x32_results[i - 16];
                    }
                    else if(i == 20)
                    {
                        ps_search_results = &ps_ctxt->aps_me_frm_prms[j]->s_search_results_64x64;
                        ps_search_results->ps_cu_results =
                            &ps_ctxt->aps_me_frm_prms[j]->s_cu64x64_results;
                    }
                    else
                    {
                        /* 8x8 search results are not required in LO ME */
                        ASSERT(0);
                    }
                }
                count += hme_alloc_init_search_nodes(
                    ps_search_results, &ps_memtabs[count], mem_avail, 2, ps_prms->max_num_results);
            }
        }

        if((i4_num_me_frm_pllel == 1) && (j != (MAX_NUM_ME_PARALLEL - 1)))
        {
            count = count_cpy;
        }
    }

    /* Weighted inputs, one for each ref + one non weighted */
    for(j = 0; j < MAX_NUM_ME_PARALLEL; j++)
    {
        size = (ps_prms->max_num_ref + 1) * ctb_wd * ctb_wd * ps_prms->i4_num_proc_thrds;
        if(mem_avail)
        {
            U08 *pu1_mem;
            ASSERT(ps_memtabs[count].size == size);
            pu1_mem = ps_memtabs[count].pu1_mem;

            for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
            {
                ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

                for(i = 0; i < ps_prms->max_num_ref + 1; i++)
                {
                    ps_ctxt->aps_me_frm_prms[j]->s_wt_pred.apu1_wt_inp_buf_array[i] = pu1_mem;
                    pu1_mem += (ctb_wd * ctb_wd);
                }
            }
        }
        else
        {
            ps_memtabs[count].size = size;
            ps_memtabs[count].align = 16;
            ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
        }
        if((i4_num_me_frm_pllel != 1) || (j == (MAX_NUM_ME_PARALLEL - 1)))
        {
            count++;
        }
    }

    /* if memory is allocated the intislaise the frm prms ptr to each thrd */
    if(mem_avail)
    {
        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            me_frm_ctxt_t *ps_frm_ctxt;
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
            {
                ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[i];

                ps_frm_ctxt->ps_hme_frm_prms = &ps_master_ctxt->as_frm_prms[i];
                ps_frm_ctxt->ps_hme_ref_map = &ps_master_ctxt->as_ref_map[i];
            }
        }
    }

    /* Memory allocation for use in Clustering */
    if(ps_prms->s_me_coding_tools.e_me_quality_presets == ME_PRISTINE_QUALITY)
    {
        for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
        {
            size = 16 * sizeof(cluster_16x16_blk_t) + 4 * sizeof(cluster_32x32_blk_t) +
                   sizeof(cluster_64x64_blk_t) + sizeof(ctb_cluster_info_t);
            size *= ps_prms->i4_num_proc_thrds;

            if(mem_avail)
            {
                U08 *pu1_mem;

                ASSERT(ps_memtabs[count].size == size);
                pu1_mem = ps_memtabs[count].pu1_mem;

                for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
                {
                    ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

                    ps_ctxt->aps_me_frm_prms[i]->ps_blk_16x16 = (cluster_16x16_blk_t *)pu1_mem;
                    pu1_mem += (16 * sizeof(cluster_16x16_blk_t));

                    ps_ctxt->aps_me_frm_prms[i]->ps_blk_32x32 = (cluster_32x32_blk_t *)pu1_mem;
                    pu1_mem += (4 * sizeof(cluster_32x32_blk_t));

                    ps_ctxt->aps_me_frm_prms[i]->ps_blk_64x64 = (cluster_64x64_blk_t *)pu1_mem;
                    pu1_mem += (sizeof(cluster_64x64_blk_t));

                    ps_ctxt->aps_me_frm_prms[i]->ps_ctb_cluster_info =
                        (ctb_cluster_info_t *)pu1_mem;
                    pu1_mem += (sizeof(ctb_cluster_info_t));
                }
            }
            else
            {
                ps_memtabs[count].size = size;
                ps_memtabs[count].align = 16;
                ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
            }

            if((i4_num_me_frm_pllel != 1) || (i == (MAX_NUM_ME_PARALLEL - 1)))
            {
                count++;
            }
        }
    }
    else if(mem_avail)
    {
        for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
        {
            for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
            {
                ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

                ps_ctxt->aps_me_frm_prms[i]->ps_blk_16x16 = NULL;

                ps_ctxt->aps_me_frm_prms[i]->ps_blk_32x32 = NULL;

                ps_ctxt->aps_me_frm_prms[i]->ps_blk_64x64 = NULL;

                ps_ctxt->aps_me_frm_prms[i]->ps_ctb_cluster_info = NULL;
            }
        }
    }

    for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
    {
        size = sizeof(fullpel_refine_ctxt_t);
        size *= ps_prms->i4_num_proc_thrds;

        if(mem_avail)
        {
            U08 *pu1_mem;

            ASSERT(ps_memtabs[count].size == size);
            pu1_mem = ps_memtabs[count].pu1_mem;

            for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
            {
                ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

                ps_ctxt->aps_me_frm_prms[i]->ps_fullpel_refine_ctxt =
                    (fullpel_refine_ctxt_t *)pu1_mem;
                pu1_mem += (sizeof(fullpel_refine_ctxt_t));
            }
        }
        else
        {
            ps_memtabs[count].size = size;
            ps_memtabs[count].align = 16;
            ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
        }

        if((i4_num_me_frm_pllel != 1) || (i == (MAX_NUM_ME_PARALLEL - 1)))
        {
            count++;
        }
    }

    /* Memory for ihevce_me_optimised_function_list_t struct  */
    if(mem_avail)
    {
        ps_master_ctxt->pv_me_optimised_function_list = (void *)ps_memtabs[count++].pu1_mem;
    }
    else
    {
        ps_memtabs[count].size = sizeof(ihevce_me_optimised_function_list_t);
        ps_memtabs[count].align = 16;
        ps_memtabs[count++].e_mem_attr = HME_SCRATCH_OVLY_MEM;
    }

    ASSERT(count < hme_enc_num_alloc(i4_num_me_frm_pllel));
    return (count);
}

/**
********************************************************************************
*  @fn     hme_coarse_alloc_init_mem()
*
*  @brief  Requests/ assign memory based on mem avail
*
*  @param[in] ps_memtabs : memtab array
*
*  @param[in] ps_prms : init prms
*
*  @param[in] pv_ctxt : ME ctxt
*
*  @param[in] mem_avail : request/assign flag
*
*  @return  number of memtabs
********************************************************************************
*/
S32 hme_coarse_alloc_init_mem(
    hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms, void *pv_ctxt, S32 mem_avail)
{
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_ctxt;
    coarse_me_ctxt_t *ps_ctxt;
    S32 count = 0, size, i, j, use_4x4, wd;
    S32 n_tot_layers;
    S32 num_layers_explicit_search;
    S32 a_wd[MAX_NUM_LAYERS], a_ht[MAX_NUM_LAYERS];
    S32 a_disp_wd[MAX_NUM_LAYERS], a_disp_ht[MAX_NUM_LAYERS];
    S32 num_results;
    S32 num_thrds;
    //S32 ctb_wd = 1 << ps_prms->log_ctb_size;
    S32 sad_4x4_block_size, sad_4x4_block_stride, search_step, num_rows;
    S32 layer1_blk_width = 8;  // 8x8 search
    S32 blk_shift;

    /* MV bank changes */
    hme_mv_t *aps_mv_bank[MAX_NUM_LAYERS] = { NULL };
    S32 ai4_num_mvs_per_row[MAX_NUM_LAYERS] = { 0 };
    S08 *api1_ref_idx[MAX_NUM_LAYERS] = { NULL };

    /* Memtab 0: handle */
    size = sizeof(coarse_me_master_ctxt_t);
    if(mem_avail)
    {
        /* store the number of processing threads */
        ps_master_ctxt->i4_num_proc_thrds = ps_prms->i4_num_proc_thrds;
    }
    else
    {
        ps_memtabs[count].size = size;
        ps_memtabs[count].align = 8;
        ps_memtabs[count].e_mem_attr = HME_PERSISTENT_MEM;
    }

    count++;

    /* Memtab 1: ME threads ctxt */
    size = ps_prms->i4_num_proc_thrds * sizeof(coarse_me_ctxt_t);
    if(mem_avail)
    {
        coarse_me_ctxt_t *ps_me_tmp_ctxt = (coarse_me_ctxt_t *)ps_memtabs[count].pu1_mem;

        /* store the indivisual thread ctxt pointers */
        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            ps_master_ctxt->aps_me_ctxt[num_thrds] = ps_me_tmp_ctxt++;
        }
    }
    else
    {
        ps_memtabs[count].size = size;
        ps_memtabs[count].align = 8;
        ps_memtabs[count].e_mem_attr = HME_PERSISTENT_MEM;
    }

    count++;

    memcpy(a_wd, ps_prms->a_wd, sizeof(S32) * ps_prms->num_simulcast_layers);
    memcpy(a_ht, ps_prms->a_ht, sizeof(S32) * ps_prms->num_simulcast_layers);
    /*************************************************************************/
    /* Derive the number of HME layers, including both encoded and non encode*/
    /* This function also derives the width and ht of each layer.            */
    /*************************************************************************/
    n_tot_layers = hme_derive_num_layers(1, a_wd, a_ht, a_disp_wd, a_disp_ht);

    num_layers_explicit_search = ps_prms->num_layers_explicit_search;

    if(num_layers_explicit_search <= 0)
        num_layers_explicit_search = n_tot_layers - 1;

    num_layers_explicit_search = MIN(num_layers_explicit_search, n_tot_layers - 1);

    if(mem_avail)
    {
        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
            memset(ps_ctxt->u1_encode, 0, n_tot_layers);

            /* encode layer should be excluded during processing */
            ps_ctxt->num_layers = n_tot_layers;

            memcpy(ps_ctxt->a_wd, a_wd, sizeof(S32) * n_tot_layers);
            memcpy(ps_ctxt->a_ht, a_ht, sizeof(S32) * n_tot_layers);

            ps_ctxt->num_layers_explicit_search = num_layers_explicit_search;
            ps_ctxt->max_num_results = ps_prms->max_num_results;
            ps_ctxt->max_num_results_coarse = ps_prms->max_num_results_coarse;
            ps_ctxt->max_num_ref = ps_prms->max_num_ref;
        }
    }

    /* Memtabs : Layers MV bank for total layers - 2  */
    /* for penultimate layer MV bank will be initialsed at every frame level */
    for(j = 1; j < n_tot_layers; j++)
    {
        S32 is_explicit_store = 1;
        S32 wd, ht;
        U08 u1_enc = 0;
        wd = a_wd[j];
        ht = a_ht[j];

        /* Possibly implicit search for lower (finer) layers */
        if(n_tot_layers - j > num_layers_explicit_search)
            is_explicit_store = 0;

        /* Even if explicit search, we store only 2 results (L0 and L1) */
        /* in finest layer */
        if(j == 0)
        {
            is_explicit_store = 0;
        }

        /* coarsest layer alwasy uses 4x4 blks to store results */
        if(j == n_tot_layers - 1)
        {
            num_results = ps_prms->max_num_results_coarse;
        }
        else
        {
            num_results = ps_prms->max_num_results;
            if(j == 0)
                num_results = 1;
        }
        use_4x4 = hme_get_mv_blk_size(ps_prms->use_4x4, j, n_tot_layers, u1_enc);

        /* for penultimate compute the parameters and store */
        if(j == 1)
        {
            S32 num_blks, num_mvs_per_blk, num_ref;
            S32 num_cols, num_rows, num_mvs_per_row;

            num_cols = use_4x4 ? ((wd >> 2) + 2) : ((wd >> 3) + 2);
            num_rows = use_4x4 ? ((ht >> 2) + 2) : ((ht >> 3) + 2);

            if(is_explicit_store)
                num_ref = ps_prms->max_num_ref;
            else
                num_ref = 2;

            num_blks = num_cols * num_rows;
            num_mvs_per_blk = num_ref * num_results;
            num_mvs_per_row = num_mvs_per_blk * num_cols;

            ai4_num_mvs_per_row[j] = num_mvs_per_row;
            aps_mv_bank[j] = NULL;
            api1_ref_idx[j] = NULL;
        }
        else
        {
            count += hme_alloc_init_layer_mv_bank(
                &ps_memtabs[count],
                num_results,
                ps_prms->max_num_ref,
                use_4x4,
                mem_avail,
                u1_enc,
                wd,
                ht,
                is_explicit_store,
                &aps_mv_bank[j],
                &api1_ref_idx[j],
                &ai4_num_mvs_per_row[j]);
        }
    }

    /* Memtabs : Layers * num-ref + 1 */
    for(i = 0; i < ps_prms->max_num_ref + 1 + NUM_BUFS_DECOMP_HME; i++)
    {
        /* for all layer except encode layer */
        for(j = 1; j < n_tot_layers; j++)
        {
            layer_ctxt_t *ps_layer;
            S32 is_explicit_store = 1;
            S32 segment_this_layer = (j == 0) ? 1 : ps_prms->segment_higher_layers;
            S32 wd, ht;
            U08 u1_enc = 0;
            wd = a_wd[j];
            ht = a_ht[j];

            /* Possibly implicit search for lower (finer) layers */
            if(n_tot_layers - j > num_layers_explicit_search)
                is_explicit_store = 0;

            /* Even if explicit search, we store only 2 results (L0 and L1) */
            /* in finest layer */
            if(j == 0)
            {
                is_explicit_store = 0;
            }

            /* coarsest layer alwasy uses 4x4 blks to store results */
            if(j == n_tot_layers - 1)
            {
                num_results = ps_prms->max_num_results_coarse;
            }
            else
            {
                num_results = ps_prms->max_num_results;
                if(j == 0)
                    num_results = 1;
            }
            use_4x4 = hme_get_mv_blk_size(ps_prms->use_4x4, j, n_tot_layers, u1_enc);

            count += hme_alloc_init_layer(
                &ps_memtabs[count],
                num_results,
                ps_prms->max_num_ref,
                use_4x4,
                mem_avail,
                u1_enc,
                wd,
                ht,
                a_disp_wd[j],
                a_disp_ht[j],
                segment_this_layer,
                is_explicit_store,
                &ps_layer);
            if(mem_avail)
            {
                /* same ps_layer memory pointer is stored in all the threads */
                for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
                {
                    ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
                    ps_ctxt->as_ref_descr[i].aps_layers[j] = ps_layer;
                }

                /* store the MV bank pointers */
                ps_layer->ps_layer_mvbank->max_num_mvs_per_row = ai4_num_mvs_per_row[j];
                ps_layer->ps_layer_mvbank->ps_mv_base = aps_mv_bank[j];
                ps_layer->ps_layer_mvbank->pi1_ref_idx_base = api1_ref_idx[j];
            }
        }
    }

    /* Memtabs : Prev Row search node at coarsest layer */
    wd = a_wd[n_tot_layers - 1];

    /* Allocate a memtab for storing 4x4 SADs for n rows. As many as num ref and number of threads */
    num_rows = ps_prms->i4_num_proc_thrds + 1;
    if(ps_prms->s_me_coding_tools.e_me_quality_presets < ME_MEDIUM_SPEED)
        search_step = HME_COARSE_STEP_SIZE_HIGH_QUALITY;
    else
        search_step = HME_COARSE_STEP_SIZE_HIGH_SPEED;

    /*shift factor*/
    blk_shift = 2; /*4x4*/
    search_step >>= 1;

    sad_4x4_block_size = ((2 * MAX_MVX_SUPPORTED_IN_COARSE_LAYER) >> search_step) *
                         ((2 * MAX_MVY_SUPPORTED_IN_COARSE_LAYER) >> search_step);
    sad_4x4_block_stride = ((wd >> blk_shift) + 1) * sad_4x4_block_size;

    size = num_rows * sad_4x4_block_stride * sizeof(S16);
    for(i = 0; i < ps_prms->max_num_ref; i++)
    {
        if(mem_avail)
        {
            ASSERT(size == ps_memtabs[count].size);

            /* same row memory pointer is stored in all the threads */
            for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
            {
                ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
                ps_ctxt->api2_sads_4x4_n_rows[i] = (S16 *)ps_memtabs[count].pu1_mem;
            }
        }
        else
        {
            ps_memtabs[count].size = size;
            ps_memtabs[count].align = 4;
            ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
        }
        count++;
    }

    /* Allocate a memtab for storing best search nodes 8x4 for n rows. Row is allocated for worst case (2*min_wd_coarse/4). As many as num ref and number of threads */
    size = num_rows * ((wd >> blk_shift) + 1) * sizeof(search_node_t);
    for(i = 0; i < ps_prms->max_num_ref; i++)
    {
        if(mem_avail)
        {
            ASSERT(size == ps_memtabs[count].size);

            /* same row memory pointer is stored in all the threads */
            for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
            {
                ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
                ps_ctxt->aps_best_search_nodes_8x4_n_rows[i] =
                    (search_node_t *)ps_memtabs[count].pu1_mem;
            }
        }
        else
        {
            ps_memtabs[count].size = size;
            ps_memtabs[count].align = 4;
            ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
        }
        count++;
    }
    /* Allocate a memtab for storing best search nodes 4x8 for n rows. Row is allocated for worst case (2*min_wd_coarse/4). As many as num ref and number of threads */
    size = num_rows * ((wd >> blk_shift) + 1) * sizeof(search_node_t);
    for(i = 0; i < ps_prms->max_num_ref; i++)
    {
        if(mem_avail)
        {
            ASSERT(size == ps_memtabs[count].size);

            /* same row memory pointer is stored in all the threads */
            for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
            {
                ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
                ps_ctxt->aps_best_search_nodes_4x8_n_rows[i] =
                    (search_node_t *)ps_memtabs[count].pu1_mem;
            }
        }
        else
        {
            ps_memtabs[count].size = size;
            ps_memtabs[count].align = 4;
            ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
        }
        count++;
    }

    /* Allocate a memtab for each histogram. As many as num ref and number of threads */
    for(i = 0; i < ps_prms->max_num_ref; i++)
    {
        size = ps_prms->i4_num_proc_thrds * sizeof(mv_hist_t);
        if(mem_avail)
        {
            mv_hist_t *ps_mv_hist = (mv_hist_t *)ps_memtabs[count].pu1_mem;

            ASSERT(size == ps_memtabs[count].size);

            /* divide the memory accross the threads */
            for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
            {
                ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
                ps_ctxt->aps_mv_hist[i] = ps_mv_hist;
                ps_mv_hist++;
            }
        }
        else
        {
            ps_memtabs[count].size = size;
            ps_memtabs[count].align = 8;
            ps_memtabs[count].e_mem_attr = HME_PERSISTENT_MEM;
        }
        count++;
    }

    /* Memtabs : Search nodes for 8x8 blks */
    for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
    {
        search_results_t *ps_search_results = NULL;

        if(mem_avail)
        {
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
        }

        if(mem_avail)
        {
            ps_search_results = &ps_ctxt->s_search_results_8x8;
        }
        count += hme_alloc_init_search_nodes(
            ps_search_results,
            &ps_memtabs[count],
            mem_avail,
            ps_prms->max_num_ref,
            ps_prms->max_num_results);
    }

    /* Weighted inputs, one for each ref  */
    size = (ps_prms->max_num_ref + 1) * layer1_blk_width * layer1_blk_width *
           ps_prms->i4_num_proc_thrds;
    if(mem_avail)
    {
        U08 *pu1_mem;
        ASSERT(ps_memtabs[count].size == size);
        pu1_mem = ps_memtabs[count].pu1_mem;

        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            for(i = 0; i < ps_prms->max_num_ref + 1; i++)
            {
                ps_ctxt->s_wt_pred.apu1_wt_inp_buf_array[i] = pu1_mem;
                pu1_mem += (layer1_blk_width * layer1_blk_width);
            }
        }
    }
    else
    {
        ps_memtabs[count].size = size;
        ps_memtabs[count].align = 16;
        ps_memtabs[count].e_mem_attr = HME_SCRATCH_OVLY_MEM;
    }
    count++;

    /* if memory is allocated the intislaise the frm prms ptr to each thrd */
    if(mem_avail)
    {
        for(num_thrds = 0; num_thrds < ps_prms->i4_num_proc_thrds; num_thrds++)
        {
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            ps_ctxt->ps_hme_frm_prms = &ps_master_ctxt->s_frm_prms;
            ps_ctxt->ps_hme_ref_map = &ps_master_ctxt->s_ref_map;
        }
    }

    /* Memory for ihevce_me_optimised_function_list_t struct  */
    if(mem_avail)
    {
        ps_master_ctxt->pv_me_optimised_function_list = (void *)ps_memtabs[count++].pu1_mem;
    }
    else
    {
        ps_memtabs[count].size = sizeof(ihevce_me_optimised_function_list_t);
        ps_memtabs[count].align = 16;
        ps_memtabs[count++].e_mem_attr = HME_SCRATCH_OVLY_MEM;
    }

    //ASSERT(count < hme_enc_num_alloc());
    ASSERT(count < hme_coarse_num_alloc());
    return (count);
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_get_lyr_prms_dep_mngr \endif
*
* \brief Returns to the caller key attributes relevant for dependency manager,
*        ie, the number of vertical units in each layer
*
* \par Description:
*    This function requires the precondition that the width and ht of encode
*    layer is known.
*    The number of layers, number of vertical units in each layer, and for
*    each vertial unit in each layer, its dependency on previous layer's units
*    From ME's perspective, a vertical unit is one which is smallest min size
*    vertically (and spans the entire row horizontally). This is CTB for encode
*    layer, and 8x8 / 4x4 for non encode layers.
*
* \param[in] num_layers : Number of ME Layers
* \param[in] pai4_ht    : Array storing ht at each layer
* \param[in] pai4_wd    : Array storing wd at each layer
* \param[out] pi4_num_vert_units_in_lyr : Array of size N (num layers), each
*                     entry has num vertical units in that particular layer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_get_lyr_prms_dep_mngr(
    WORD32 num_layers, WORD32 *pai4_ht, WORD32 *pai4_wd, WORD32 *pai4_num_vert_units_in_lyr)
{
    /* Height of current and next layers */
    WORD32 ht_c, ht_n;
    /* Blk ht at a given layer and next layer*/
    WORD32 unit_ht_c, unit_ht_n, blk_ht_c, blk_ht_n;
    /* Number of vertical units in current and next layer */
    WORD32 num_vert_c, num_vert_n;

    WORD32 ctb_size = 64, num_enc_layers = 1, use_4x4 = 1, i;
    UWORD8 au1_encode[MAX_NUM_LAYERS];

    memset(au1_encode, 0, num_layers);
    memset(au1_encode, 1, num_enc_layers);

    ht_n = pai4_ht[num_layers - 2];
    ht_c = pai4_ht[num_layers - 1];

    /* compute blk ht and unit ht for c and n */
    if(au1_encode[num_layers - 1])
    {
        blk_ht_c = 16;
        unit_ht_c = ctb_size;
    }
    else
    {
        blk_ht_c = hme_get_blk_size(use_4x4, num_layers - 1, num_layers, 0);
        unit_ht_c = blk_ht_c;
    }

    num_vert_c = (ht_c + unit_ht_c - 1) / unit_ht_c;
    /* For new design in Coarsest HME layer we need */
    /* one additional row extra at the end of frame */
    /* hence num_vert_c is incremented by 1         */
    num_vert_c++;

    /*************************************************************************/
    /* Run through each layer, set the number of vertical units              */
    /*************************************************************************/
    for(i = num_layers - 1; i > 0; i--)
    {
        pai4_num_vert_units_in_lyr[i] = num_vert_c;

        /* "n" is computed for first time */
        ht_n = pai4_ht[i - 1];
        blk_ht_n = hme_get_blk_size(use_4x4, i - 1, num_layers, 0);
        unit_ht_n = blk_ht_n;
        if(au1_encode[i - 1])
            unit_ht_n = ctb_size;

        num_vert_n = (ht_n + unit_ht_n - 1) / unit_ht_n;

        /* Compute the blk size and vert unit size in each layer             */
        /* "c" denotes curr layer, and "n" denotes the layer to which result */
        /* is projected to                                                   */
        ht_c = ht_n;
        blk_ht_c = blk_ht_n;
        unit_ht_c = unit_ht_n;
        num_vert_c = num_vert_n;
    }

    /* LAYER 0 OR ENCODE LAYER UPDATE : NO OUTPUT DEPS */
    /* set the numebr of vertical units */
    pai4_num_vert_units_in_lyr[0] = num_vert_c;
}

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_alloc_mem()
*
*  @brief  Requests memory for HME Dep Mngr
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] i4_mem_space : memspace in whihc memory request should be done
*
*  @return  number of memtabs
********************************************************************************
*/
WORD32 hme_coarse_dep_mngr_alloc_mem(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_mem_space,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_resolution_id)
{
    WORD32 ai4_num_vert_units_in_lyr[MAX_NUM_HME_LAYERS];
    WORD32 a_wd[MAX_NUM_HME_LAYERS], a_ht[MAX_NUM_HME_LAYERS];
    WORD32 a_disp_wd[MAX_NUM_HME_LAYERS], a_disp_ht[MAX_NUM_HME_LAYERS];
    WORD32 n_enc_layers = 1, n_tot_layers, n_dep_tabs = 0, i;
    WORD32 min_cu_size;

    /* get the min cu size from config params */
    min_cu_size = ps_init_prms->s_config_prms.i4_min_log2_cu_size;

    min_cu_size = 1 << min_cu_size;

    /* Get the width and heights of different decomp layers */
    *a_wd = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width +
            SET_CTB_ALIGN(
                ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width, min_cu_size);

    *a_ht =
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height +
        SET_CTB_ALIGN(
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height, min_cu_size);

    n_tot_layers = hme_derive_num_layers(n_enc_layers, a_wd, a_ht, a_disp_wd, a_disp_ht);
    ASSERT(n_tot_layers >= 3);

    /* --- Get the number of vartical units in each layer for dep. mngr -- */
    ihevce_coarse_me_get_lyr_prms_dep_mngr(
        n_tot_layers, &a_ht[0], &a_wd[0], &ai4_num_vert_units_in_lyr[0]);

    /* Fill memtabs for HME layers,except for L0 layer */
    for(i = 1; i < n_tot_layers; i++)
    {
        n_dep_tabs += ihevce_dmgr_get_mem_recs(
            &ps_mem_tab[n_dep_tabs],
            DEP_MNGR_ROW_ROW_SYNC,
            ai4_num_vert_units_in_lyr[i],
            1, /* Number of Col Tiles :  Not supported in PreEnc */
            i4_num_proc_thrds,
            i4_mem_space);
    }

    ASSERT(n_dep_tabs <= hme_coarse_dep_mngr_num_alloc());

    return (n_dep_tabs);
}

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_init()
*
*  @brief  Assign memory for HME Dep Mngr
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
*  @param[in] pv_ctxt : ME ctxt
* \param[in] pv_osal_handle : Osal handle
*
*  @return  number of memtabs
********************************************************************************
*/
WORD32 hme_coarse_dep_mngr_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    void *pv_ctxt,
    void *pv_osal_handle,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_resolution_id)
{
    WORD32 ai4_num_vert_units_in_lyr[MAX_NUM_HME_LAYERS];
    WORD32 a_wd[MAX_NUM_HME_LAYERS], a_ht[MAX_NUM_HME_LAYERS];
    WORD32 a_disp_wd[MAX_NUM_HME_LAYERS], a_disp_ht[MAX_NUM_HME_LAYERS];
    WORD32 n_enc_layers = 1, n_tot_layers, n_dep_tabs = 0, i;
    WORD32 min_cu_size;

    coarse_me_master_ctxt_t *ps_me_ctxt = (coarse_me_master_ctxt_t *)pv_ctxt;

    /* get the min cu size from config params */
    min_cu_size = ps_init_prms->s_config_prms.i4_min_log2_cu_size;

    min_cu_size = 1 << min_cu_size;

    /* Get the width and heights of different decomp layers */
    *a_wd = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width +
            SET_CTB_ALIGN(
                ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width, min_cu_size);
    *a_ht =
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height +
        SET_CTB_ALIGN(
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height, min_cu_size);

    n_tot_layers = hme_derive_num_layers(n_enc_layers, a_wd, a_ht, a_disp_wd, a_disp_ht);
    ASSERT(n_tot_layers >= 3);

    /* --- Get the number of vartical units in each layer for dep. mngr -- */
    ihevce_coarse_me_get_lyr_prms_dep_mngr(
        n_tot_layers, &a_ht[0], &a_wd[0], &ai4_num_vert_units_in_lyr[0]);

    /* --- HME sync Dep Mngr Mem init --    */
    for(i = 1; i < n_tot_layers; i++)
    {
        WORD32 num_blks_in_row, num_blks_in_pic, blk_size_shift;

        if(i == (n_tot_layers - 1)) /* coarsest layer */
            blk_size_shift = 2;
        else
            blk_size_shift = 3; /* refine layers */

        GET_NUM_BLKS_IN_PIC(a_wd[i], a_ht[i], blk_size_shift, num_blks_in_row, num_blks_in_pic);

        /* Coarsest layer : 1 block extra, since the last block */
        if(i == (n_tot_layers - 1)) /*  in a row needs East block */
            num_blks_in_row += 1;

        /* Note : i-1, only for HME layers, L0 is separate */
        ps_me_ctxt->apv_dep_mngr_hme_sync[i - 1] = ihevce_dmgr_init(
            &ps_mem_tab[n_dep_tabs],
            pv_osal_handle,
            DEP_MNGR_ROW_ROW_SYNC,
            ai4_num_vert_units_in_lyr[i],
            num_blks_in_row,
            1, /* Number of Col Tiles : Not supported in PreEnc */
            i4_num_proc_thrds,
            1 /*Sem disabled*/
        );

        n_dep_tabs += ihevce_dmgr_get_num_mem_recs();
    }

    return n_dep_tabs;
}

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_reg_sem()
*
*  @brief  Assign semaphores for HME Dep Mngr
*
* \param[in] pv_me_ctxt : pointer to Coarse ME ctxt
* \param[in] ppv_sem_hdls : Arry of semaphore handles
* \param[in] i4_num_proc_thrds : Number of processing threads
*
*  @return  number of memtabs
********************************************************************************
*/
void hme_coarse_dep_mngr_reg_sem(void *pv_ctxt, void **ppv_sem_hdls, WORD32 i4_num_proc_thrds)
{
    WORD32 i;
    coarse_me_master_ctxt_t *ps_me_ctxt = (coarse_me_master_ctxt_t *)pv_ctxt;
    coarse_me_ctxt_t *ps_ctxt = ps_me_ctxt->aps_me_ctxt[0];

    /* --- HME sync Dep Mngr semaphore init --    */
    for(i = 1; i < ps_ctxt->num_layers; i++)
    {
        ihevce_dmgr_reg_sem_hdls(
            ps_me_ctxt->apv_dep_mngr_hme_sync[i - 1], ppv_sem_hdls, i4_num_proc_thrds);
    }

    return;
}

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_delete()
*
*    Destroy Coarse ME Dep Mngr module
*   Note : Only Destroys the resources allocated in the module like
*   semaphore,etc. Memory free is done Separately using memtabs
*
* \param[in] pv_me_ctxt : pointer to Coarse ME ctxt
* \param[in] ps_init_prms : Create time static parameters
*
*  @return  none
********************************************************************************
*/
void hme_coarse_dep_mngr_delete(
    void *pv_me_ctxt, ihevce_static_cfg_params_t *ps_init_prms, WORD32 i4_resolution_id)
{
    WORD32 a_wd[MAX_NUM_HME_LAYERS], a_ht[MAX_NUM_HME_LAYERS];
    WORD32 a_disp_wd[MAX_NUM_HME_LAYERS], a_disp_ht[MAX_NUM_HME_LAYERS];
    WORD32 n_enc_layers = 1, n_tot_layers, i;
    WORD32 min_cu_size;

    coarse_me_master_ctxt_t *ps_me_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;

    /* get the min cu size from config params */
    min_cu_size = ps_init_prms->s_config_prms.i4_min_log2_cu_size;

    min_cu_size = 1 << min_cu_size;

    /* Get the width and heights of different decomp layers */
    *a_wd = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width +
            SET_CTB_ALIGN(
                ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width, min_cu_size);
    *a_ht =
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height +
        SET_CTB_ALIGN(
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height, min_cu_size);
    n_tot_layers = hme_derive_num_layers(n_enc_layers, a_wd, a_ht, a_disp_wd, a_disp_ht);
    ASSERT(n_tot_layers >= 3);

    /* --- HME sync Dep Mngr Delete --    */
    for(i = 1; i < n_tot_layers; i++)
    {
        /* Note : i-1, only for HME layers, L0 is separate */
        ihevce_dmgr_del(ps_me_ctxt->apv_dep_mngr_hme_sync[i - 1]);
    }
}

/**
*******************************************************************************
*  @fn     S32 hme_enc_alloc(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms)
*
*  @brief  Fills up memtabs with memory information details required by HME
*
*  @param[out] ps_memtabs : Pointre to an array of memtabs where module fills
*              up its requirements of memory
*
*  @param[in] ps_prms : Input parameters to module crucial in calculating reqd
*                       amt of memory
*
*  @return   Number of memtabs required
*******************************************************************************
*/
S32 hme_enc_alloc(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms, WORD32 i4_num_me_frm_pllel)
{
    S32 num, tot, i;

    /* Validation of init params */
    if(-1 == hme_validate_init_prms(ps_prms))
        return (-1);

    num = hme_enc_alloc_init_mem(ps_memtabs, ps_prms, NULL, 0, i4_num_me_frm_pllel);
    tot = hme_enc_num_alloc(i4_num_me_frm_pllel);
    for(i = num; i < tot; i++)
    {
        ps_memtabs[i].size = 4;
        ps_memtabs[i].align = 4;
        ps_memtabs[i].e_mem_attr = HME_PERSISTENT_MEM;
    }
    return (tot);
}

/**
*******************************************************************************
*  @fn     S32 hme_coarse_alloc(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms)
*
*  @brief  Fills up memtabs with memory information details required by Coarse HME
*
*  @param[out] ps_memtabs : Pointre to an array of memtabs where module fills
*              up its requirements of memory
*
*  @param[in] ps_prms : Input parameters to module crucial in calculating reqd
*                       amt of memory
*
*  @return   Number of memtabs required
*******************************************************************************
*/
S32 hme_coarse_alloc(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms)
{
    S32 num, tot, i;

    /* Validation of init params */
    if(-1 == hme_validate_init_prms(ps_prms))
        return (-1);

    num = hme_coarse_alloc_init_mem(ps_memtabs, ps_prms, NULL, 0);
    tot = hme_coarse_num_alloc();
    for(i = num; i < tot; i++)
    {
        ps_memtabs[i].size = 4;
        ps_memtabs[i].align = 4;
        ps_memtabs[i].e_mem_attr = HME_PERSISTENT_MEM;
    }
    return (tot);
}

/**
*******************************************************************************
*  @fn hme_coarse_dep_mngr_alloc
*
*  @brief  Fills up memtabs with memory information details required by Coarse HME
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] i4_mem_space : memspace in whihc memory request should be done
*
*  @return   Number of memtabs required
*******************************************************************************
*/
WORD32 hme_coarse_dep_mngr_alloc(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_mem_space,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_resolution_id)
{
    S32 num, tot, i;

    num = hme_coarse_dep_mngr_alloc_mem(
        ps_mem_tab, ps_init_prms, i4_mem_space, i4_num_proc_thrds, i4_resolution_id);
    tot = hme_coarse_dep_mngr_num_alloc();
    for(i = num; i < tot; i++)
    {
        ps_mem_tab[i].i4_mem_size = 4;
        ps_mem_tab[i].i4_mem_alignment = 4;
        ps_mem_tab[i].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    }
    return (tot);
}

/**
********************************************************************************
*  @fn     hme_coarse_init_ctxt()
*
*  @brief  initialise context memory
*
*  @param[in] ps_prms : init prms
*
*  @param[in] pv_ctxt : ME ctxt
*
*  @return  number of memtabs
********************************************************************************
*/
void hme_coarse_init_ctxt(coarse_me_master_ctxt_t *ps_master_ctxt, hme_init_prms_t *ps_prms)
{
    S32 i, j, num_thrds;
    coarse_me_ctxt_t *ps_ctxt;
    S32 num_rows_coarse;

    /* initialise the parameters inot context of all threads */
    for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
    {
        ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

        /* Copy the init prms to context */
        ps_ctxt->s_init_prms = *ps_prms;

        /* Initialize some other variables in ctxt */
        ps_ctxt->i4_prev_poc = -1;

        ps_ctxt->num_b_frms = ps_prms->num_b_frms;

        ps_ctxt->apu1_ref_bits_tlu_lc[0] = &ps_ctxt->au1_ref_bits_tlu_lc[0][0];
        ps_ctxt->apu1_ref_bits_tlu_lc[1] = &ps_ctxt->au1_ref_bits_tlu_lc[1][0];

        /* Initialize num rows lookuptable */
        ps_ctxt->i4_num_row_bufs = ps_prms->i4_num_proc_thrds + 1;
        num_rows_coarse = ps_ctxt->i4_num_row_bufs;
        for(i = 0; i < ((HEVCE_MAX_HEIGHT >> 1) >> 2); i++)
        {
            ps_ctxt->ai4_row_index[i] = (i % num_rows_coarse);
        }
    }

    /* since same layer desc pointer is stored in all the threads ctxt */
    /* layer init is done only using 0th thread ctxt                   */
    ps_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    /* Initialize all layers descriptors to have -1 = poc meaning unfilled */
    for(i = 0; i < ps_ctxt->max_num_ref + 1 + NUM_BUFS_DECOMP_HME; i++)
    {
        for(j = 1; j < ps_ctxt->num_layers; j++)
        {
            layer_ctxt_t *ps_layer;
            ps_layer = ps_ctxt->as_ref_descr[i].aps_layers[j];
            ps_layer->i4_poc = -1;
            ps_layer->ppu1_list_inp = &ps_ctxt->apu1_list_inp[j][0];
            memset(
                ps_layer->s_global_mv, 0, sizeof(hme_mv_t) * ps_ctxt->max_num_ref * NUM_GMV_LOBES);
        }
    }
}

/**
********************************************************************************
*  @fn     hme_enc_init_ctxt()
*
*  @brief  initialise context memory
*
*  @param[in] ps_prms : init prms
*
*  @param[in] pv_ctxt : ME ctxt
*
*  @return  number of memtabs
********************************************************************************
*/
void hme_enc_init_ctxt(
    me_master_ctxt_t *ps_master_ctxt, hme_init_prms_t *ps_prms, rc_quant_t *ps_rc_quant_ctxt)
{
    S32 i, j, num_thrds;
    me_ctxt_t *ps_ctxt;
    me_frm_ctxt_t *ps_frm_ctxt;

    /* initialise the parameters in context of all threads */
    for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
    {
        ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
        /* Store Tile params base into ME context */
        ps_ctxt->pv_tile_params_base = ps_master_ctxt->pv_tile_params_base;

        for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
        {
            ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[i];

            /* Copy the init prms to context */
            ps_ctxt->s_init_prms = *ps_prms;

            /* Initialize some other variables in ctxt */
            ps_frm_ctxt->i4_prev_poc = INVALID_POC;

            ps_frm_ctxt->log_ctb_size = ps_prms->log_ctb_size;

            ps_frm_ctxt->num_b_frms = ps_prms->num_b_frms;

            ps_frm_ctxt->i4_is_prev_frame_reference = 0;

            ps_frm_ctxt->ps_rc_quant_ctxt = ps_rc_quant_ctxt;

            /* Initialize mv grids for L0 and L1 used in final refinement layer */
            {
                hme_init_mv_grid(&ps_frm_ctxt->as_mv_grid[0]);
                hme_init_mv_grid(&ps_frm_ctxt->as_mv_grid[1]);
                hme_init_mv_grid(&ps_frm_ctxt->as_mv_grid_fpel[0]);
                hme_init_mv_grid(&ps_frm_ctxt->as_mv_grid_fpel[1]);
                hme_init_mv_grid(&ps_frm_ctxt->as_mv_grid_qpel[0]);
                hme_init_mv_grid(&ps_frm_ctxt->as_mv_grid_qpel[1]);
            }

            ps_frm_ctxt->apu1_ref_bits_tlu_lc[0] = &ps_frm_ctxt->au1_ref_bits_tlu_lc[0][0];
            ps_frm_ctxt->apu1_ref_bits_tlu_lc[1] = &ps_frm_ctxt->au1_ref_bits_tlu_lc[1][0];
        }
    }

    /* since same layer desc pointer is stored in all the threads ctxt */
    /* layer init is done only using 0th thread ctxt                   */
    ps_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[0];

    /* Initialize all layers descriptors to have -1 = poc meaning unfilled */
    for(i = 0; i < (ps_frm_ctxt->max_num_ref * ps_master_ctxt->i4_num_me_frm_pllel) + 1; i++)
    {
        /* only enocde layer is processed */
        for(j = 0; j < 1; j++)
        {
            layer_ctxt_t *ps_layer;
            ps_layer = ps_ctxt->as_ref_descr[i].aps_layers[j];
            ps_layer->i4_poc = INVALID_POC;
            ps_layer->i4_is_free = 1;
            ps_layer->ppu1_list_inp = &ps_frm_ctxt->apu1_list_inp[j][0];
            ps_layer->ppu1_list_rec_fxfy = &ps_frm_ctxt->apu1_list_rec_fxfy[j][0];
            ps_layer->ppu1_list_rec_hxfy = &ps_frm_ctxt->apu1_list_rec_hxfy[j][0];
            ps_layer->ppu1_list_rec_fxhy = &ps_frm_ctxt->apu1_list_rec_fxhy[j][0];
            ps_layer->ppu1_list_rec_hxhy = &ps_frm_ctxt->apu1_list_rec_hxhy[j][0];
            ps_layer->ppv_dep_mngr_recon = &ps_frm_ctxt->apv_list_dep_mngr[j][0];

            memset(
                ps_layer->s_global_mv,
                0,
                sizeof(hme_mv_t) * ps_frm_ctxt->max_num_ref * NUM_GMV_LOBES);
        }
    }
}

/**
*******************************************************************************
*  @fn     S32 hme_enc_init(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms,rc_quant_t *ps_rc_quant_ctxt)
*
*  @brief  Initialises the Encode Layer HME ctxt
*
*  @param[out] ps_memtabs : Pointer to an array of memtabs where module fills
*              up its requirements of memory
*
*  @param[in] ps_prms : Input parameters to module crucial in calculating reqd
*                       amt of memory
*
*  @return   Number of memtabs required
*******************************************************************************
*/
S32 hme_enc_init(
    void *pv_ctxt,
    hme_memtab_t *ps_memtabs,
    hme_init_prms_t *ps_prms,
    rc_quant_t *ps_rc_quant_ctxt,
    WORD32 i4_num_me_frm_pllel)
{
    S32 num, tot;
    me_master_ctxt_t *ps_ctxt = (me_master_ctxt_t *)pv_ctxt;

    tot = hme_enc_num_alloc(i4_num_me_frm_pllel);
    /* Validation of init params */
    if(-1 == hme_validate_init_prms(ps_prms))
        return (-1);

    num = hme_enc_alloc_init_mem(ps_memtabs, ps_prms, pv_ctxt, 1, i4_num_me_frm_pllel);
    if(num > tot)
        return (-1);

    /* Initialize all enumerations based globals */
    //hme_init_globals(); /* done as part of coarse me */

    /* Copy the memtabs into the context for returning during free */
    memcpy(ps_ctxt->as_memtabs, ps_memtabs, sizeof(hme_memtab_t) * tot);

    /* initialize the context and related buffers */
    hme_enc_init_ctxt(ps_ctxt, ps_prms, ps_rc_quant_ctxt);
    return (0);
}

/**
*******************************************************************************
*  @fn     S32 hme_coarse_init(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms)
*
*  @brief  Initialises the Coarse HME ctxt
*
*  @param[out] ps_memtabs : Pointer to an array of memtabs where module fills
*              up its requirements of memory
*
*  @param[in] ps_prms : Input parameters to module crucial in calculating reqd
*                       amt of memory
*
*  @return   Number of memtabs required
*******************************************************************************
*/
S32 hme_coarse_init(void *pv_ctxt, hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms)
{
    S32 num, tot;
    coarse_me_master_ctxt_t *ps_ctxt = (coarse_me_master_ctxt_t *)pv_ctxt;

    tot = hme_coarse_num_alloc();
    /* Validation of init params */
    if(-1 == hme_validate_init_prms(ps_prms))
        return (-1);

    num = hme_coarse_alloc_init_mem(ps_memtabs, ps_prms, pv_ctxt, 1);
    if(num > tot)
        return (-1);

    /* Initialize all enumerations based globals */
    hme_init_globals();

    /* Copy the memtabs into the context for returning during free */
    memcpy(ps_ctxt->as_memtabs, ps_memtabs, sizeof(hme_memtab_t) * tot);

    /* initialize the context and related buffers */
    hme_coarse_init_ctxt(ps_ctxt, ps_prms);

    return (0);
}

/**
*******************************************************************************
*  @fn     S32 hme_set_resolution(void *pv_me_ctxt,
*                                   S32 n_enc_layers,
*                                   S32 *p_wd,
*                                   S32 *p_ht
*
*  @brief  Sets up the layers based on resolution information.
*
*  @param[in, out] pv_me_ctxt : ME handle, updated with the resolution info
*
*  @param[in] n_enc_layers : Number of layers encoded
*
*  @param[in] p_wd : Pointer to an array having widths for each encode layer
*
*  @param[in] p_ht : Pointer to an array having heights for each encode layer
*
*  @return   void
*******************************************************************************
*/

void hme_set_resolution(void *pv_me_ctxt, S32 n_enc_layers, S32 *p_wd, S32 *p_ht, S32 me_frm_id)
{
    S32 n_tot_layers, num_layers_explicit_search, i, j;
    me_ctxt_t *ps_thrd_ctxt;
    me_frm_ctxt_t *ps_ctxt;

    S32 a_wd[MAX_NUM_LAYERS], a_ht[MAX_NUM_LAYERS];
    S32 a_disp_wd[MAX_NUM_LAYERS], a_disp_ht[MAX_NUM_LAYERS];
    memcpy(a_wd, p_wd, n_enc_layers * sizeof(S32));
    memcpy(a_ht, p_ht, n_enc_layers * sizeof(S32));

    ps_thrd_ctxt = (me_ctxt_t *)pv_me_ctxt;

    ps_ctxt = ps_thrd_ctxt->aps_me_frm_prms[me_frm_id];

    /*************************************************************************/
    /* Derive the number of HME layers, including both encoded and non encode*/
    /* This function also derives the width and ht of each layer.            */
    /*************************************************************************/
    n_tot_layers = hme_derive_num_layers(n_enc_layers, a_wd, a_ht, a_disp_wd, a_disp_ht);
    num_layers_explicit_search = ps_thrd_ctxt->s_init_prms.num_layers_explicit_search;
    if(num_layers_explicit_search <= 0)
        num_layers_explicit_search = n_tot_layers - 1;

    num_layers_explicit_search = MIN(num_layers_explicit_search, n_tot_layers - 1);
    ps_ctxt->num_layers_explicit_search = num_layers_explicit_search;
    memset(ps_ctxt->u1_encode, 0, n_tot_layers);
    memset(ps_ctxt->u1_encode, 1, n_enc_layers);

    /* only encode layer should be processed */
    ps_ctxt->num_layers = n_tot_layers;

    ps_ctxt->i4_wd = a_wd[0];
    ps_ctxt->i4_ht = a_ht[0];

    /* Memtabs : Layers * num-ref + 1 */
    for(i = 0; i < ps_ctxt->max_num_ref + 1; i++)
    {
        for(j = 0; j < 1; j++)
        {
            S32 wd, ht;
            layer_ctxt_t *ps_layer;
            U08 u1_enc = ps_ctxt->u1_encode[j];
            wd = a_wd[j];
            ht = a_ht[j];
            ps_layer = ps_thrd_ctxt->as_ref_descr[i].aps_layers[j];
            hme_set_layer_res_attrs(ps_layer, wd, ht, a_disp_wd[j], a_disp_ht[j], u1_enc);
        }
    }
}

/**
*******************************************************************************
*  @fn     S32 hme_coarse_set_resolution(void *pv_me_ctxt,
*                                   S32 n_enc_layers,
*                                   S32 *p_wd,
*                                   S32 *p_ht
*
*  @brief  Sets up the layers based on resolution information.
*
*  @param[in, out] pv_me_ctxt : ME handle, updated with the resolution info
*
*  @param[in] n_enc_layers : Number of layers encoded
*
*  @param[in] p_wd : Pointer to an array having widths for each encode layer
*
*  @param[in] p_ht : Pointer to an array having heights for each encode layer
*
*  @return   void
*******************************************************************************
*/

void hme_coarse_set_resolution(void *pv_me_ctxt, S32 n_enc_layers, S32 *p_wd, S32 *p_ht)
{
    S32 n_tot_layers, num_layers_explicit_search, i, j;
    coarse_me_ctxt_t *ps_ctxt;
    S32 a_wd[MAX_NUM_LAYERS], a_ht[MAX_NUM_LAYERS];
    S32 a_disp_wd[MAX_NUM_LAYERS], a_disp_ht[MAX_NUM_LAYERS];
    memcpy(a_wd, p_wd, n_enc_layers * sizeof(S32));
    memcpy(a_ht, p_ht, n_enc_layers * sizeof(S32));

    ps_ctxt = (coarse_me_ctxt_t *)pv_me_ctxt;
    /*************************************************************************/
    /* Derive the number of HME layers, including both encoded and non encode*/
    /* This function also derives the width and ht of each layer.            */
    /*************************************************************************/
    n_tot_layers = hme_derive_num_layers(n_enc_layers, a_wd, a_ht, a_disp_wd, a_disp_ht);
    num_layers_explicit_search = ps_ctxt->s_init_prms.num_layers_explicit_search;
    if(num_layers_explicit_search <= 0)
        num_layers_explicit_search = n_tot_layers - 1;

    num_layers_explicit_search = MIN(num_layers_explicit_search, n_tot_layers - 1);
    ps_ctxt->num_layers_explicit_search = num_layers_explicit_search;
    memset(ps_ctxt->u1_encode, 0, n_tot_layers);
    memset(ps_ctxt->u1_encode, 1, n_enc_layers);

    /* encode layer should be excluded */
    ps_ctxt->num_layers = n_tot_layers;

    memcpy(ps_ctxt->a_wd, a_wd, sizeof(S32) * n_tot_layers);
    memcpy(ps_ctxt->a_ht, a_ht, sizeof(S32) * n_tot_layers);

    /* Memtabs : Layers * num-ref + 1 */
    for(i = 0; i < ps_ctxt->max_num_ref + 1 + NUM_BUFS_DECOMP_HME; i++)
    {
        for(j = 1; j < n_tot_layers; j++)
        {
            S32 wd, ht;
            layer_ctxt_t *ps_layer;
            U08 u1_enc = ps_ctxt->u1_encode[j];
            wd = a_wd[j];
            ht = a_ht[j];
            ps_layer = ps_ctxt->as_ref_descr[i].aps_layers[j];
            hme_set_layer_res_attrs(ps_layer, wd, ht, a_disp_wd[j], a_disp_ht[j], u1_enc);
        }
    }
}

S32 hme_find_descr_idx(me_ctxt_t *ps_ctxt, S32 i4_poc, S32 i4_idr_gop_num, S32 i4_num_me_frm_pllel)
{
    S32 i;

    for(i = 0; i < (ps_ctxt->aps_me_frm_prms[0]->max_num_ref * i4_num_me_frm_pllel) + 1; i++)
    {
        if(ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_poc == i4_poc &&
           ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_idr_gop_num == i4_idr_gop_num)
            return i;
    }
    /* Should not come here */
    ASSERT(0);
    return (-1);
}

S32 hme_coarse_find_descr_idx(coarse_me_ctxt_t *ps_ctxt, S32 i4_poc)
{
    S32 i;

    for(i = 0; i < ps_ctxt->max_num_ref + 1 + NUM_BUFS_DECOMP_HME; i++)
    {
        if(ps_ctxt->as_ref_descr[i].aps_layers[1]->i4_poc == i4_poc)
            return i;
    }
    /* Should not come here */
    ASSERT(0);
    return (-1);
}

S32 hme_find_free_descr_idx(me_ctxt_t *ps_ctxt, S32 i4_num_me_frm_pllel)
{
    S32 i;

    for(i = 0; i < (ps_ctxt->aps_me_frm_prms[0]->max_num_ref * i4_num_me_frm_pllel) + 1; i++)
    {
        if(ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_is_free == 1)
        {
            ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_is_free = 0;
            return i;
        }
    }
    /* Should not come here */
    ASSERT(0);
    return (-1);
}

S32 hme_coarse_find_free_descr_idx(void *pv_ctxt)
{
    S32 i;

    coarse_me_ctxt_t *ps_ctxt = (coarse_me_ctxt_t *)pv_ctxt;

    for(i = 0; i < ps_ctxt->max_num_ref + 1 + NUM_BUFS_DECOMP_HME; i++)
    {
        if(ps_ctxt->as_ref_descr[i].aps_layers[1]->i4_poc == -1)
            return i;
    }
    /* Should not come here */
    ASSERT(0);
    return (-1);
}

void hme_discard_frm(
    void *pv_me_ctxt, S32 *p_pocs_to_remove, S32 i4_idr_gop_num, S32 i4_num_me_frm_pllel)
{
    me_ctxt_t *ps_ctxt = (me_ctxt_t *)pv_me_ctxt;
    S32 count = 0, idx, i;
    layers_descr_t *ps_descr;

    /* Search for the id of the layer descriptor that has this poc */
    while(p_pocs_to_remove[count] != INVALID_POC)
    {
        ASSERT(count == 0);
        idx = hme_find_descr_idx(
            ps_ctxt, p_pocs_to_remove[count], i4_idr_gop_num, i4_num_me_frm_pllel);
        ps_descr = &ps_ctxt->as_ref_descr[idx];
        /*********************************************************************/
        /* Setting i4_is_free = 1 in all layers invalidates this layer ctxt        */
        /* Now this can be used for a fresh picture.                         */
        /*********************************************************************/
        for(i = 0; i < 1; i++)
        {
            ps_descr->aps_layers[i]->i4_is_free = 1;
        }
        count++;
    }
}

void hme_coarse_discard_frm(void *pv_me_ctxt, S32 *p_pocs_to_remove)
{
    coarse_me_ctxt_t *ps_ctxt = (coarse_me_ctxt_t *)pv_me_ctxt;
    S32 count = 0, idx, i;
    layers_descr_t *ps_descr;

    /* Search for the id of the layer descriptor that has this poc */
    while(p_pocs_to_remove[count] != -1)
    {
        idx = hme_coarse_find_descr_idx(ps_ctxt, p_pocs_to_remove[count]);
        ps_descr = &ps_ctxt->as_ref_descr[idx];
        /*********************************************************************/
        /* Setting poc = -1 in all layers invalidates this layer ctxt        */
        /* Now this can be used for a fresh picture.                         */
        /*********************************************************************/
        for(i = 1; i < ps_ctxt->num_layers; i++)
        {
            ps_descr->aps_layers[i]->i4_poc = -1;
        }
        count++;
    }
}

void hme_update_layer_desc(
    layers_descr_t *ps_layers_desc,
    hme_ref_desc_t *ps_ref_desc,
    S32 start_lyr_id,
    S32 num_layers,
    layers_descr_t *ps_curr_desc)
{
    layer_ctxt_t *ps_layer_ctxt, *ps_curr_layer;
    S32 i;
    for(i = start_lyr_id; i < num_layers; i++)
    {
        ps_layer_ctxt = ps_layers_desc->aps_layers[i];
        ps_curr_layer = ps_curr_desc->aps_layers[i];

        ps_layer_ctxt->i4_poc = ps_ref_desc->i4_poc;
        ps_layer_ctxt->i4_idr_gop_num = ps_ref_desc->i4_GOP_num;

        /* Copy the recon planes for the given reference pic at given layer */
        ps_layer_ctxt->pu1_rec_fxfy = ps_ref_desc->as_ref_info[i].pu1_rec_fxfy;
        ps_layer_ctxt->pu1_rec_hxfy = ps_ref_desc->as_ref_info[i].pu1_rec_hxfy;
        ps_layer_ctxt->pu1_rec_fxhy = ps_ref_desc->as_ref_info[i].pu1_rec_fxhy;
        ps_layer_ctxt->pu1_rec_hxhy = ps_ref_desc->as_ref_info[i].pu1_rec_hxhy;

        /*********************************************************************/
        /* reconstruction strides, offsets and padding info are copied for   */
        /* this reference pic. It is assumed that these will be same across  */
        /* pics, so even the current pic has this info updated, though the   */
        /* current pic still does not have valid recon pointers.             */
        /*********************************************************************/
        ps_layer_ctxt->i4_rec_stride = ps_ref_desc->as_ref_info[i].luma_stride;
        ps_layer_ctxt->i4_rec_offset = ps_ref_desc->as_ref_info[i].luma_offset;
        ps_layer_ctxt->i4_pad_x_rec = ps_ref_desc->as_ref_info[i].u1_pad_x;
        ps_layer_ctxt->i4_pad_y_rec = ps_ref_desc->as_ref_info[i].u1_pad_y;

        ps_curr_layer->i4_rec_stride = ps_ref_desc->as_ref_info[i].luma_stride;
        ps_curr_layer->i4_pad_x_rec = ps_ref_desc->as_ref_info[i].u1_pad_x;
        ps_curr_layer->i4_pad_y_rec = ps_ref_desc->as_ref_info[i].u1_pad_y;
    }
}

void hme_add_inp(void *pv_me_ctxt, hme_inp_desc_t *ps_inp_desc, S32 me_frm_id, S32 i4_thrd_id)
{
    layers_descr_t *ps_desc;
    layer_ctxt_t *ps_layer_ctxt;
    me_master_ctxt_t *ps_master_ctxt = (me_master_ctxt_t *)pv_me_ctxt;
    me_ctxt_t *ps_thrd_ctxt;
    me_frm_ctxt_t *ps_ctxt;

    hme_inp_buf_attr_t *ps_attr;
    S32 i4_poc, idx, i, i4_prev_poc;
    S32 num_thrds, prev_me_frm_id;
    S32 i4_idr_gop_num, i4_is_reference;

    /* since same layer desc pointer is stored in all thread ctxt */
    /* a free idx is obtained using 0th thread ctxt pointer */

    ps_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[i4_thrd_id];

    ps_ctxt = ps_thrd_ctxt->aps_me_frm_prms[me_frm_id];

    /* Deriving the previous poc from previous frames context */
    if(me_frm_id == 0)
        prev_me_frm_id = (MAX_NUM_ME_PARALLEL - 1);
    else
        prev_me_frm_id = me_frm_id - 1;

    i4_prev_poc = ps_thrd_ctxt->aps_me_frm_prms[prev_me_frm_id]->i4_curr_poc;

    /* Obtain an empty layer descriptor */
    idx = hme_find_free_descr_idx(ps_thrd_ctxt, ps_master_ctxt->i4_num_me_frm_pllel);
    ps_desc = &ps_thrd_ctxt->as_ref_descr[idx];

    /* initialise the parameters for all the threads */
    for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
    {
        me_frm_ctxt_t *ps_tmp_frm_ctxt;

        ps_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
        ps_tmp_frm_ctxt = ps_thrd_ctxt->aps_me_frm_prms[me_frm_id];

        ps_tmp_frm_ctxt->ps_curr_descr = &ps_thrd_ctxt->as_ref_descr[idx];

        /* Do the initialization for the first thread alone */
        i4_poc = ps_inp_desc->i4_poc;
        i4_idr_gop_num = ps_inp_desc->i4_idr_gop_num;
        i4_is_reference = ps_inp_desc->i4_is_reference;
        /*Update poc id of previously encoded frm and curr frm */
        ps_tmp_frm_ctxt->i4_prev_poc = i4_prev_poc;
        ps_tmp_frm_ctxt->i4_curr_poc = i4_poc;
    }

    /* since same layer desc pointer is stored in all thread ctxt */
    /* following processing is done using 0th thread ctxt pointer */
    ps_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    /* only encode layer */
    for(i = 0; i < 1; i++)
    {
        ps_layer_ctxt = ps_desc->aps_layers[i];
        ps_attr = &ps_inp_desc->s_layer_desc[i];

        ps_layer_ctxt->i4_poc = i4_poc;
        ps_layer_ctxt->i4_idr_gop_num = i4_idr_gop_num;
        ps_layer_ctxt->i4_is_reference = i4_is_reference;
        ps_layer_ctxt->i4_non_ref_free = 0;

        /* If this layer is encoded, copy input attributes */
        if(ps_ctxt->u1_encode[i])
        {
            ps_layer_ctxt->pu1_inp = ps_attr->pu1_y;
            ps_layer_ctxt->i4_inp_stride = ps_attr->luma_stride;
            ps_layer_ctxt->i4_pad_x_inp = 0;
            ps_layer_ctxt->i4_pad_y_inp = 0;
        }
        else
        {
            /* If not encoded, then ME owns the buffer.*/
            S32 wd, dst_stride;

            ASSERT(i != 0);

            wd = ps_ctxt->i4_wd;

            /* destination has padding on either side of 16 */
            dst_stride = CEIL16((wd >> 1)) + 32 + 4;
            ps_layer_ctxt->i4_inp_stride = dst_stride;
        }
    }

    return;
}

void hme_coarse_add_inp(void *pv_me_ctxt, hme_inp_desc_t *ps_inp_desc, WORD32 i4_curr_idx)
{
    layers_descr_t *ps_desc;
    layer_ctxt_t *ps_layer_ctxt;
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    coarse_me_ctxt_t *ps_ctxt;
    hme_inp_buf_attr_t *ps_attr;
    S32 i4_poc, i;
    S32 num_thrds;

    /* since same layer desc pointer is stored in all thread ctxt */
    /* a free idx is obtained using 0th thread ctxt pointer */
    ps_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    ps_desc = &ps_ctxt->as_ref_descr[i4_curr_idx];

    /* initialise the parameters for all the threads */
    for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
    {
        ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
        ps_ctxt->ps_curr_descr = &ps_ctxt->as_ref_descr[i4_curr_idx];
        i4_poc = ps_inp_desc->i4_poc;

        /*Update poc id of previously encoded frm and curr frm */
        ps_ctxt->i4_prev_poc = ps_ctxt->i4_curr_poc;
        ps_ctxt->i4_curr_poc = i4_poc;
    }

    /* since same layer desc pointer is stored in all thread ctxt */
    /* following processing is done using 0th thread ctxt pointer */
    ps_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    /* only non encode layer */
    for(i = 1; i < ps_ctxt->num_layers; i++)
    {
        ps_layer_ctxt = ps_desc->aps_layers[i];
        ps_attr = &ps_inp_desc->s_layer_desc[i];

        ps_layer_ctxt->i4_poc = i4_poc;
        /* If this layer is encoded, copy input attributes */
        if(ps_ctxt->u1_encode[i])
        {
            ps_layer_ctxt->pu1_inp = ps_attr->pu1_y;
            ps_layer_ctxt->i4_inp_stride = ps_attr->luma_stride;
            ps_layer_ctxt->i4_pad_x_inp = 0;
            ps_layer_ctxt->i4_pad_y_inp = 0;
        }
        else
        {
            /* If not encoded, then ME owns the buffer.           */
            /* decomp of lower layers happens on a seperate pass  */
            /* Coarse Me should export the pointers to the caller */
            S32 wd, dst_stride;

            ASSERT(i != 0);

            wd = ps_ctxt->a_wd[i - 1];

            /* destination has padding on either side of 16 */
            dst_stride = CEIL16((wd >> 1)) + 32 + 4;
            ps_layer_ctxt->i4_inp_stride = dst_stride;
        }
    }
}

static __inline U08 hme_determine_num_results_per_part(
    U08 u1_layer_id, U08 u1_num_layers, ME_QUALITY_PRESETS_T e_quality_preset)
{
    U08 u1_num_results_per_part = MAX_RESULTS_PER_PART;

    if((u1_layer_id == 0) && !!RESTRICT_NUM_PARTITION_LEVEL_L0ME_RESULTS_TO_1)
    {
        switch(e_quality_preset)
        {
        case ME_XTREME_SPEED_25:
        case ME_XTREME_SPEED:
        case ME_HIGH_SPEED:
        case ME_MEDIUM_SPEED:
        case ME_HIGH_QUALITY:
        case ME_PRISTINE_QUALITY:
        {
            u1_num_results_per_part = 1;

            break;
        }
        default:
        {
            u1_num_results_per_part = MAX_RESULTS_PER_PART;

            break;
        }
        }
    }
    else if((u1_layer_id == 1) && !!RESTRICT_NUM_PARTITION_LEVEL_L1ME_RESULTS_TO_1)
    {
        switch(e_quality_preset)
        {
        case ME_XTREME_SPEED_25:
        case ME_HIGH_QUALITY:
        case ME_PRISTINE_QUALITY:
        {
            u1_num_results_per_part = 1;

            break;
        }
        default:
        {
            u1_num_results_per_part = MAX_RESULTS_PER_PART;

            break;
        }
        }
    }
    else if((u1_layer_id == 2) && (u1_num_layers > 3) && !!RESTRICT_NUM_PARTITION_LEVEL_L2ME_RESULTS_TO_1)
    {
        switch(e_quality_preset)
        {
        case ME_XTREME_SPEED_25:
        case ME_XTREME_SPEED:
        case ME_HIGH_SPEED:
        case ME_MEDIUM_SPEED:
        {
            u1_num_results_per_part = 1;

            break;
        }
        default:
        {
            u1_num_results_per_part = MAX_RESULTS_PER_PART;

            break;
        }
        }
    }

    return u1_num_results_per_part;
}

static __inline void hme_max_search_cands_per_search_cand_loc_populator(
    hme_frm_prms_t *ps_frm_prms,
    U08 *pu1_num_fpel_search_cands,
    U08 u1_layer_id,
    ME_QUALITY_PRESETS_T e_quality_preset)
{
    if(0 == u1_layer_id)
    {
        S32 i;

        for(i = 0; i < NUM_SEARCH_CAND_LOCATIONS; i++)
        {
            switch(e_quality_preset)
            {
#if RESTRICT_NUM_SEARCH_CANDS_PER_SEARCH_CAND_LOC
            case ME_XTREME_SPEED_25:
            case ME_XTREME_SPEED:
            case ME_HIGH_SPEED:
            case ME_MEDIUM_SPEED:
            {
                pu1_num_fpel_search_cands[i] = 1;

                break;
            }
#endif
            default:
            {
                pu1_num_fpel_search_cands[i] =
                    MAX(2,
                        MAX(ps_frm_prms->u1_num_active_ref_l0, ps_frm_prms->u1_num_active_ref_l1) *
                            ((COLOCATED == (SEARCH_CAND_LOCATIONS_T)i) + 1));

                break;
            }
            }
        }
    }
}

static __inline U08
    hme_determine_max_2nx2n_tu_recur_cands(U08 u1_layer_id, ME_QUALITY_PRESETS_T e_quality_preset)
{
    U08 u1_num_cands = 2;

    if((u1_layer_id == 0) && !!RESTRICT_NUM_2NX2N_TU_RECUR_CANDS)
    {
        switch(e_quality_preset)
        {
        case ME_XTREME_SPEED_25:
        case ME_XTREME_SPEED:
        case ME_HIGH_SPEED:
        case ME_MEDIUM_SPEED:
        {
            u1_num_cands = 1;

            break;
        }
        default:
        {
            u1_num_cands = 2;

            break;
        }
        }
    }

    return u1_num_cands;
}

static __inline U08
    hme_determine_max_num_fpel_refine_centers(U08 u1_layer_id, ME_QUALITY_PRESETS_T e_quality_preset)
{
    U08 i;

    U08 u1_num_centers = 0;

    if(0 == u1_layer_id)
    {
        switch(e_quality_preset)
        {
        case ME_XTREME_SPEED_25:
        {
            for(i = 0; i < TOT_NUM_PARTS; i++)
            {
                u1_num_centers += gau1_num_best_results_XS25[i];
            }

            break;
        }
        case ME_XTREME_SPEED:
        {
            for(i = 0; i < TOT_NUM_PARTS; i++)
            {
                u1_num_centers += gau1_num_best_results_XS[i];
            }

            break;
        }
        case ME_HIGH_SPEED:
        {
            for(i = 0; i < TOT_NUM_PARTS; i++)
            {
                u1_num_centers += gau1_num_best_results_HS[i];
            }

            break;
        }
        case ME_MEDIUM_SPEED:
        {
            for(i = 0; i < TOT_NUM_PARTS; i++)
            {
                u1_num_centers += gau1_num_best_results_MS[i];
            }

            break;
        }
        case ME_HIGH_QUALITY:
        {
            for(i = 0; i < TOT_NUM_PARTS; i++)
            {
                u1_num_centers += gau1_num_best_results_HQ[i];
            }

            break;
        }
        case ME_PRISTINE_QUALITY:
        {
            for(i = 0; i < TOT_NUM_PARTS; i++)
            {
                u1_num_centers += gau1_num_best_results_PQ[i];
            }

            break;
        }
        }
    }

    return u1_num_centers;
}

static __inline U08 hme_determine_max_num_subpel_refine_centers(
    U08 u1_layer_id, U08 u1_max_2Nx2N_subpel_cands, U08 u1_max_NxN_subpel_cands)
{
    U08 u1_num_centers = 0;

    if(0 == u1_layer_id)
    {
        u1_num_centers += u1_max_2Nx2N_subpel_cands + 4 * u1_max_NxN_subpel_cands;
    }

    return u1_num_centers;
}

void hme_set_refine_prms(
    void *pv_refine_prms,
    U08 u1_encode,
    S32 num_ref,
    S32 layer_id,
    S32 num_layers,
    S32 num_layers_explicit_search,
    S32 use_4x4,
    hme_frm_prms_t *ps_frm_prms,
    double **ppd_intra_costs,
    me_coding_params_t *ps_me_coding_tools)
{
    refine_prms_t *ps_refine_prms = (refine_prms_t *)pv_refine_prms;

    ps_refine_prms->i4_encode = u1_encode;
    ps_refine_prms->bidir_enabled = ps_frm_prms->bidir_enabled;
    ps_refine_prms->i4_layer_id = layer_id;
    /*************************************************************************/
    /* Refinement layers have two lambdas, one for closed loop, another for  */
    /* open loop. Non encode layers use only open loop lambda.               */
    /*************************************************************************/
    ps_refine_prms->lambda_inp = ps_frm_prms->i4_ol_sad_lambda_qf;
    ps_refine_prms->lambda_recon = ps_frm_prms->i4_cl_sad_lambda_qf;
    ps_refine_prms->lambda_q_shift = ps_frm_prms->lambda_q_shift;
    ps_refine_prms->lambda_inp =
        ((float)ps_refine_prms->lambda_inp) * (100.0f - ME_LAMBDA_DISCOUNT) / 100.0f;
    ps_refine_prms->lambda_recon =
        ((float)ps_refine_prms->lambda_recon) * (100.0f - ME_LAMBDA_DISCOUNT) / 100.0f;

    if((u1_encode) && (NULL != ppd_intra_costs))
    {
        ps_refine_prms->pd_intra_costs = ppd_intra_costs[layer_id];
    }

    /* Explicit or implicit depends on number of layers having eplicit search */
    if((layer_id == 0) || (num_layers - layer_id > num_layers_explicit_search))
    {
        ps_refine_prms->explicit_ref = 0;
        ps_refine_prms->i4_num_ref_fpel = MIN(2, num_ref);
    }
    else
    {
        ps_refine_prms->explicit_ref = 1;
        ps_refine_prms->i4_num_ref_fpel = num_ref;
    }

    ps_refine_prms->e_search_complexity = SEARCH_CX_HIGH;

    ps_refine_prms->i4_num_steps_hpel_refine = ps_me_coding_tools->i4_num_steps_hpel_refine;
    ps_refine_prms->i4_num_steps_qpel_refine = ps_me_coding_tools->i4_num_steps_qpel_refine;

    if(u1_encode)
    {
        ps_refine_prms->i4_num_mvbank_results = 1;
        ps_refine_prms->i4_use_rec_in_fpel = 1;
        ps_refine_prms->i4_num_steps_fpel_refine = 1;

        if(ps_me_coding_tools->e_me_quality_presets == ME_PRISTINE_QUALITY)
        {
            ps_refine_prms->i4_num_fpel_results = 4;
            ps_refine_prms->i4_num_32x32_merge_results = 4;
            ps_refine_prms->i4_num_64x64_merge_results = 4;
            ps_refine_prms->i4_num_steps_post_refine_fpel = 3;
            ps_refine_prms->i4_use_satd_subpel = 1;
            ps_refine_prms->u1_max_subpel_candts_2Nx2N = 2;
            ps_refine_prms->u1_max_subpel_candts_NxN = 1;
            ps_refine_prms->u1_subpel_candt_threshold = 1;
            ps_refine_prms->e_search_complexity = SEARCH_CX_MED;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_PQ;
            ps_refine_prms->limit_active_partitions = 0;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_HIGH_QUALITY)
        {
            ps_refine_prms->i4_num_fpel_results = 4;
            ps_refine_prms->i4_num_32x32_merge_results = 4;
            ps_refine_prms->i4_num_64x64_merge_results = 4;
            ps_refine_prms->i4_num_steps_post_refine_fpel = 3;
            ps_refine_prms->i4_use_satd_subpel = 1;
            ps_refine_prms->u1_max_subpel_candts_2Nx2N = 2;
            ps_refine_prms->u1_max_subpel_candts_NxN = 1;
            ps_refine_prms->u1_subpel_candt_threshold = 2;
            ps_refine_prms->e_search_complexity = SEARCH_CX_MED;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_HQ;
            ps_refine_prms->limit_active_partitions = 0;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_MEDIUM_SPEED)
        {
            ps_refine_prms->i4_num_fpel_results = 1;
            ps_refine_prms->i4_num_32x32_merge_results = 2;
            ps_refine_prms->i4_num_64x64_merge_results = 2;
            ps_refine_prms->i4_num_steps_post_refine_fpel = 0;
            ps_refine_prms->i4_use_satd_subpel = 1;
            ps_refine_prms->u1_max_subpel_candts_2Nx2N = 2;
            ps_refine_prms->u1_max_subpel_candts_NxN = 1;
            ps_refine_prms->u1_subpel_candt_threshold = 3;
            ps_refine_prms->e_search_complexity = SEARCH_CX_MED;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_MS;
            ps_refine_prms->limit_active_partitions = 1;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_HIGH_SPEED)
        {
            ps_refine_prms->i4_num_fpel_results = 1;
            ps_refine_prms->i4_num_32x32_merge_results = 2;
            ps_refine_prms->i4_num_64x64_merge_results = 2;
            ps_refine_prms->i4_num_steps_post_refine_fpel = 0;
            ps_refine_prms->u1_max_subpel_candts_2Nx2N = 1;
            ps_refine_prms->u1_max_subpel_candts_NxN = 1;
            ps_refine_prms->i4_use_satd_subpel = 0;
            ps_refine_prms->u1_subpel_candt_threshold = 0;
            ps_refine_prms->e_search_complexity = SEARCH_CX_MED;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_HS;
            ps_refine_prms->limit_active_partitions = 1;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_XTREME_SPEED)
        {
            ps_refine_prms->i4_num_fpel_results = 1;
            ps_refine_prms->i4_num_32x32_merge_results = 2;
            ps_refine_prms->i4_num_64x64_merge_results = 2;
            ps_refine_prms->i4_num_steps_post_refine_fpel = 0;
            ps_refine_prms->i4_use_satd_subpel = 0;
            ps_refine_prms->u1_max_subpel_candts_2Nx2N = 1;
            ps_refine_prms->u1_max_subpel_candts_NxN = 0;
            ps_refine_prms->u1_subpel_candt_threshold = 0;
            ps_refine_prms->e_search_complexity = SEARCH_CX_MED;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_XS;
            ps_refine_prms->limit_active_partitions = 1;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_XTREME_SPEED_25)
        {
            ps_refine_prms->i4_num_fpel_results = 1;
            ps_refine_prms->i4_num_32x32_merge_results = 2;
            ps_refine_prms->i4_num_64x64_merge_results = 2;
            ps_refine_prms->i4_num_steps_post_refine_fpel = 0;
            ps_refine_prms->i4_use_satd_subpel = 0;
            ps_refine_prms->u1_max_subpel_candts_2Nx2N = 1;
            ps_refine_prms->u1_max_subpel_candts_NxN = 0;
            ps_refine_prms->u1_subpel_candt_threshold = 0;
            ps_refine_prms->e_search_complexity = SEARCH_CX_LOW;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_XS25;
            ps_refine_prms->limit_active_partitions = 1;
        }
    }
    else
    {
        ps_refine_prms->i4_num_fpel_results = 2;
        ps_refine_prms->i4_use_rec_in_fpel = 0;
        ps_refine_prms->i4_num_steps_fpel_refine = 1;
        ps_refine_prms->i4_num_steps_hpel_refine = 0;
        ps_refine_prms->i4_num_steps_qpel_refine = 0;

        if(ps_me_coding_tools->e_me_quality_presets == ME_HIGH_SPEED)
        {
            ps_refine_prms->i4_num_steps_post_refine_fpel = 0;
            ps_refine_prms->i4_use_satd_subpel = 1;
            ps_refine_prms->e_search_complexity = SEARCH_CX_LOW;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_HS;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_XTREME_SPEED)
        {
            ps_refine_prms->i4_num_steps_post_refine_fpel = 0;
            ps_refine_prms->i4_use_satd_subpel = 0;
            ps_refine_prms->e_search_complexity = SEARCH_CX_LOW;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_XS;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_XTREME_SPEED_25)
        {
            ps_refine_prms->i4_num_steps_post_refine_fpel = 0;
            ps_refine_prms->i4_use_satd_subpel = 0;
            ps_refine_prms->e_search_complexity = SEARCH_CX_LOW;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_XS25;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_PRISTINE_QUALITY)
        {
            ps_refine_prms->i4_num_steps_post_refine_fpel = 2;
            ps_refine_prms->i4_use_satd_subpel = 1;
            ps_refine_prms->e_search_complexity = SEARCH_CX_MED;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_PQ;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_HIGH_QUALITY)
        {
            ps_refine_prms->i4_num_steps_post_refine_fpel = 2;
            ps_refine_prms->i4_use_satd_subpel = 1;
            ps_refine_prms->e_search_complexity = SEARCH_CX_MED;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_HQ;
        }
        else if(ps_me_coding_tools->e_me_quality_presets == ME_MEDIUM_SPEED)
        {
            ps_refine_prms->i4_num_steps_post_refine_fpel = 0;
            ps_refine_prms->i4_use_satd_subpel = 1;
            ps_refine_prms->e_search_complexity = SEARCH_CX_LOW;
            ps_refine_prms->pu1_num_best_results = gau1_num_best_results_MS;
        }

        /* Following fields unused in the non-encode layers */
        /* But setting the same to default values           */
        ps_refine_prms->i4_num_32x32_merge_results = 4;
        ps_refine_prms->i4_num_64x64_merge_results = 4;

        if(!ps_frm_prms->bidir_enabled)
        {
            ps_refine_prms->limit_active_partitions = 0;
        }
        else
        {
            ps_refine_prms->limit_active_partitions = 1;
        }
    }

    ps_refine_prms->i4_enable_4x4_part =
        hme_get_mv_blk_size(use_4x4, layer_id, num_layers, u1_encode);

    if(!ps_me_coding_tools->u1_l0_me_controlled_via_cmd_line)
    {
        ps_refine_prms->i4_num_results_per_part = hme_determine_num_results_per_part(
            layer_id, num_layers, ps_me_coding_tools->e_me_quality_presets);

        hme_max_search_cands_per_search_cand_loc_populator(
            ps_frm_prms,
            ps_refine_prms->au1_num_fpel_search_cands,
            layer_id,
            ps_me_coding_tools->e_me_quality_presets);

        ps_refine_prms->u1_max_2nx2n_tu_recur_cands = hme_determine_max_2nx2n_tu_recur_cands(
            layer_id, ps_me_coding_tools->e_me_quality_presets);

        ps_refine_prms->u1_max_num_fpel_refine_centers = hme_determine_max_num_fpel_refine_centers(
            layer_id, ps_me_coding_tools->e_me_quality_presets);

        ps_refine_prms->u1_max_num_subpel_refine_centers =
            hme_determine_max_num_subpel_refine_centers(
                layer_id,
                ps_refine_prms->u1_max_subpel_candts_2Nx2N,
                ps_refine_prms->u1_max_subpel_candts_NxN);
    }
    else
    {
        if(0 == layer_id)
        {
            ps_refine_prms->i4_num_results_per_part =
                ps_me_coding_tools->u1_num_results_per_part_in_l0me;
        }
        else if(1 == layer_id)
        {
            ps_refine_prms->i4_num_results_per_part =
                ps_me_coding_tools->u1_num_results_per_part_in_l1me;
        }
        else if((2 == layer_id) && (num_layers > 3))
        {
            ps_refine_prms->i4_num_results_per_part =
                ps_me_coding_tools->u1_num_results_per_part_in_l2me;
        }
        else
        {
            ps_refine_prms->i4_num_results_per_part = hme_determine_num_results_per_part(
                layer_id, num_layers, ps_me_coding_tools->e_me_quality_presets);
        }

        memset(
            ps_refine_prms->au1_num_fpel_search_cands,
            ps_me_coding_tools->u1_max_num_coloc_cands,
            sizeof(ps_refine_prms->au1_num_fpel_search_cands));

        ps_refine_prms->u1_max_2nx2n_tu_recur_cands =
            ps_me_coding_tools->u1_max_2nx2n_tu_recur_cands;

        ps_refine_prms->u1_max_num_fpel_refine_centers =
            ps_me_coding_tools->u1_max_num_fpel_refine_centers;

        ps_refine_prms->u1_max_num_subpel_refine_centers =
            ps_me_coding_tools->u1_max_num_subpel_refine_centers;
    }

    if(layer_id != 0)
    {
        ps_refine_prms->i4_num_mvbank_results = ps_refine_prms->i4_num_results_per_part;
    }

    /* 4 * lambda */
    ps_refine_prms->sdi_threshold =
        (ps_refine_prms->lambda_recon + (1 << (ps_frm_prms->lambda_q_shift - 1))) >>
        (ps_frm_prms->lambda_q_shift - 2);

    ps_refine_prms->u1_use_lambda_derived_from_min_8x8_act_in_ctb =
        MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON && ps_frm_prms->u1_is_cu_qp_delta_enabled;
}

void hme_set_ctb_boundary_attrs(ctb_boundary_attrs_t *ps_attrs, S32 num_8x8_horz, S32 num_8x8_vert)
{
    S32 cu_16x16_valid_flag = 0, merge_pattern_x, merge_pattern_y;
    S32 blk, blk_x, blk_y;
    S32 num_16x16_horz, num_16x16_vert;
    blk_ctb_attrs_t *ps_blk_attrs = &ps_attrs->as_blk_attrs[0];

    num_16x16_horz = (num_8x8_horz + 1) >> 1;
    num_16x16_vert = (num_8x8_vert + 1) >> 1;
    ps_attrs->u1_num_blks_in_ctb = (U08)(num_16x16_horz * num_16x16_vert);

    /*************************************************************************/
    /* Run through each blk assuming all 16x16 CUs valid. The order would be */
    /* 0   1   4   5                                                         */
    /* 2   3   6   7                                                         */
    /* 8   9   12  13                                                        */
    /* 10  11  14  15                                                        */
    /* Out of these some may not be valid. For example, if num_16x16_horz is */
    /* 2 and num_16x16_vert is 4, then right 2 columns not valid. In this    */
    /* case, blks 8-11 get encoding number of 4-7. Further, the variable     */
    /* cu_16x16_valid_flag will be 1111 0000 1111 0000. Also, the variable   */
    /* u1_merge_to_32x32_flag will be 1010, and u1_merge_to_64x64_flag 0     */
    /*************************************************************************/
    for(blk = 0; blk < 16; blk++)
    {
        U08 u1_blk_8x8_mask = 0xF;
        blk_x = gau1_encode_to_raster_x[blk];
        blk_y = gau1_encode_to_raster_y[blk];
        if((blk_x >= num_16x16_horz) || (blk_y >= num_16x16_vert))
        {
            continue;
        }

        /* The CU at encode location blk is valid */
        cu_16x16_valid_flag |= (1 << blk);
        ps_blk_attrs->u1_blk_id_in_full_ctb = blk;
        ps_blk_attrs->u1_blk_x = blk_x;
        ps_blk_attrs->u1_blk_y = blk_y;

        /* Disable blks 1 and 3 if the 16x16 blk overshoots on rt border */
        if(((blk_x << 1) + 2) > num_8x8_horz)
            u1_blk_8x8_mask &= 0x5;
        /* Disable blks 2 and 3 if the 16x16 blk overshoots on bot border */
        if(((blk_y << 1) + 2) > num_8x8_vert)
            u1_blk_8x8_mask &= 0x3;
        ps_blk_attrs->u1_blk_8x8_mask = u1_blk_8x8_mask;
        ps_blk_attrs++;
    }

    ps_attrs->cu_16x16_valid_flag = cu_16x16_valid_flag;

    /* 32x32 merge is logical combination of what merge is possible          */
    /* horizontally as well as vertically.                                   */
    if(num_8x8_horz < 4)
        merge_pattern_x = 0x0;
    else if(num_8x8_horz < 8)
        merge_pattern_x = 0x5;
    else
        merge_pattern_x = 0xF;

    if(num_8x8_vert < 4)
        merge_pattern_y = 0x0;
    else if(num_8x8_vert < 8)
        merge_pattern_y = 0x3;
    else
        merge_pattern_y = 0xF;

    ps_attrs->u1_merge_to_32x32_flag = (U08)(merge_pattern_x & merge_pattern_y);

    /* Do not attempt 64x64 merge if any blk invalid */
    if(ps_attrs->u1_merge_to_32x32_flag != 0xF)
        ps_attrs->u1_merge_to_64x64_flag = 0;
    else
        ps_attrs->u1_merge_to_64x64_flag = 1;
}

void hme_set_ctb_attrs(ctb_boundary_attrs_t *ps_attrs, S32 wd, S32 ht)
{
    S32 is_cropped_rt, is_cropped_bot;

    is_cropped_rt = ((wd & 63) != 0) ? 1 : 0;
    is_cropped_bot = ((ht & 63) != 0) ? 1 : 0;

    if(is_cropped_rt)
    {
        hme_set_ctb_boundary_attrs(&ps_attrs[CTB_RT_PIC_BOUNDARY], (wd & 63) >> 3, 8);
    }
    if(is_cropped_bot)
    {
        hme_set_ctb_boundary_attrs(&ps_attrs[CTB_BOT_PIC_BOUNDARY], 8, (ht & 63) >> 3);
    }
    if(is_cropped_rt & is_cropped_bot)
    {
        hme_set_ctb_boundary_attrs(
            &ps_attrs[CTB_BOT_RT_PIC_BOUNDARY], (wd & 63) >> 3, (ht & 63) >> 3);
    }
    hme_set_ctb_boundary_attrs(&ps_attrs[CTB_CENTRE], 8, 8);
}

/**
********************************************************************************
*  @fn     hme_scale_for_ref_idx(S32 curr_poc, S32 poc_from, S32 poc_to)
*
*  @brief  When we have an mv with ref id "poc_to" for which predictor to be
*          computed, and predictor is ref id "poc_from", this funciton returns
*          scale factor in Q8 for such a purpose
*
*  @param[in] curr_poc : input picture poc
*
*  @param[in] poc_from : POC of the pic, pointed to by ref id to be scaled
*
*  @param[in] poc_to : POC of hte pic, pointed to by ref id to be scaled to
*
*  @return Scale factor in Q8 format
********************************************************************************
*/
S16 hme_scale_for_ref_idx(S32 curr_poc, S32 poc_from, S32 poc_to)
{
    S32 td, tx, tb;
    S16 i2_scf;
    /*************************************************************************/
    /* Approximate scale factor: 256 * num / denom                           */
    /* num = curr_poc - poc_to, denom = curr_poc - poc_from                  */
    /* Exact implementation as per standard.                                 */
    /*************************************************************************/

    tb = HME_CLIP((curr_poc - poc_to), -128, 127);
    td = HME_CLIP((curr_poc - poc_from), -128, 127);

    tx = (16384 + (ABS(td) >> 1)) / td;
    //i2_scf = HME_CLIP((((tb*tx)+32)>>6), -128, 127);
    i2_scf = HME_CLIP((((tb * tx) + 32) >> 6), -4096, 4095);

    return (i2_scf);
}

/**
********************************************************************************
*  @fn     hme_process_frm_init
*
*  @brief  HME frame level initialsation processing function
*
*  @param[in] pv_me_ctxt : ME ctxt pointer
*
*  @param[in] ps_ref_map : Reference map prms pointer
*
*  @param[in] ps_frm_prms :Pointer to frame params
*
*  called only for encode layer
*
*  @return Scale factor in Q8 format
********************************************************************************
*/
void hme_process_frm_init(
    void *pv_me_ctxt,
    hme_ref_map_t *ps_ref_map,
    hme_frm_prms_t *ps_frm_prms,
    WORD32 i4_me_frm_id,
    WORD32 i4_num_me_frm_pllel)
{
    me_ctxt_t *ps_thrd_ctxt = (me_ctxt_t *)pv_me_ctxt;
    me_frm_ctxt_t *ps_ctxt = (me_frm_ctxt_t *)ps_thrd_ctxt->aps_me_frm_prms[i4_me_frm_id];

    S32 i, j, desc_idx;
    S16 i2_max_x = 0, i2_max_y = 0;

    /* Set the Qp of current frm passed by caller. Required for intra cost */
    ps_ctxt->frm_qstep = ps_frm_prms->qstep;
    ps_ctxt->qstep_ls8 = ps_frm_prms->qstep_ls8;

    /* Bidir enabled or not */
    ps_ctxt->s_frm_prms = *ps_frm_prms;

    /*************************************************************************/
    /* Set up the ref pic parameters across all layers. For this, we do the  */
    /* following: the application has given us a ref pic list, we go index   */
    /* by index and pick up the picture. A picture can be uniquely be mapped */
    /* to a POC. So we search all layer descriptor array to find the POC     */
    /* Once found, we update all attributes in this descriptor.              */
    /* During this updation process we also create an index of descriptor id */
    /* to ref id mapping. It is important to find the same POC in the layers */
    /* descr strcture since it holds the pyramid inputs for non encode layers*/
    /* Apart from this, e also update array containing the index of the descr*/
    /* During processing for ease of access, each layer has a pointer to aray*/
    /* of pointers containing fxfy, fxhy, hxfy, hxhy and inputs for each ref */
    /* we update this too.                                                   */
    /*************************************************************************/
    ps_ctxt->num_ref_past = 0;
    ps_ctxt->num_ref_future = 0;
    for(i = 0; i < ps_ref_map->i4_num_ref; i++)
    {
        S32 ref_id_lc, idx;
        hme_ref_desc_t *ps_ref_desc;

        ps_ref_desc = &ps_ref_map->as_ref_desc[i];
        ref_id_lc = ps_ref_desc->i1_ref_id_lc;
        /* Obtain the id of descriptor that contains this POC */
        idx = hme_find_descr_idx(
            ps_thrd_ctxt, ps_ref_desc->i4_poc, ps_ref_desc->i4_GOP_num, i4_num_me_frm_pllel);

        /* Update all layers in this descr with the reference attributes */
        hme_update_layer_desc(
            &ps_thrd_ctxt->as_ref_descr[idx],
            ps_ref_desc,
            0,
            1,  //ps_ctxt->num_layers,
            ps_ctxt->ps_curr_descr);

        /* Update the pointer holder for the recon planes */
        ps_ctxt->ps_curr_descr->aps_layers[0]->ppu1_list_inp = &ps_ctxt->apu1_list_inp[0][0];
        ps_ctxt->ps_curr_descr->aps_layers[0]->ppu1_list_rec_fxfy =
            &ps_ctxt->apu1_list_rec_fxfy[0][0];
        ps_ctxt->ps_curr_descr->aps_layers[0]->ppu1_list_rec_hxfy =
            &ps_ctxt->apu1_list_rec_hxfy[0][0];
        ps_ctxt->ps_curr_descr->aps_layers[0]->ppu1_list_rec_fxhy =
            &ps_ctxt->apu1_list_rec_fxhy[0][0];
        ps_ctxt->ps_curr_descr->aps_layers[0]->ppu1_list_rec_hxhy =
            &ps_ctxt->apu1_list_rec_hxhy[0][0];
        ps_ctxt->ps_curr_descr->aps_layers[0]->ppv_dep_mngr_recon =
            &ps_ctxt->apv_list_dep_mngr[0][0];

        /* Update the array having ref id lc to descr id mapping */
        ps_ctxt->a_ref_to_descr_id[ps_ref_desc->i1_ref_id_lc] = idx;

        /* From ref id lc we need to work out the POC, So update this array */
        ps_ctxt->ai4_ref_idx_to_poc_lc[ref_id_lc] = ps_ref_desc->i4_poc;

        /* When computing costs in L0 and L1 directions, we need the */
        /* respective ref id L0 and L1, so update this mapping */
        ps_ctxt->a_ref_idx_lc_to_l0[ref_id_lc] = ps_ref_desc->i1_ref_id_l0;
        ps_ctxt->a_ref_idx_lc_to_l1[ref_id_lc] = ps_ref_desc->i1_ref_id_l1;
        if((ps_ctxt->i4_curr_poc > ps_ref_desc->i4_poc) || ps_ctxt->i4_curr_poc == 0)
        {
            ps_ctxt->au1_is_past[ref_id_lc] = 1;
            ps_ctxt->ai1_past_list[ps_ctxt->num_ref_past] = ref_id_lc;
            ps_ctxt->num_ref_past++;
        }
        else
        {
            ps_ctxt->au1_is_past[ref_id_lc] = 0;
            ps_ctxt->ai1_future_list[ps_ctxt->num_ref_future] = ref_id_lc;
            ps_ctxt->num_ref_future++;
        }

        if(1 == ps_ctxt->i4_wt_pred_enable_flag)
        {
            /* copy the weight and offsets from current ref desc */
            ps_ctxt->s_wt_pred.a_wpred_wt[ref_id_lc] = ps_ref_desc->i2_weight;

            /* inv weight is stored in Q15 format */
            ps_ctxt->s_wt_pred.a_inv_wpred_wt[ref_id_lc] =
                ((1 << 15) + (ps_ref_desc->i2_weight >> 1)) / ps_ref_desc->i2_weight;
            ps_ctxt->s_wt_pred.a_wpred_off[ref_id_lc] = ps_ref_desc->i2_offset;
        }
        else
        {
            /* store default wt and offset*/
            ps_ctxt->s_wt_pred.a_wpred_wt[ref_id_lc] = WGHT_DEFAULT;

            /* inv weight is stored in Q15 format */
            ps_ctxt->s_wt_pred.a_inv_wpred_wt[ref_id_lc] =
                ((1 << 15) + (WGHT_DEFAULT >> 1)) / WGHT_DEFAULT;

            ps_ctxt->s_wt_pred.a_wpred_off[ref_id_lc] = 0;
        }
    }

    ps_ctxt->ai1_future_list[ps_ctxt->num_ref_future] = -1;
    ps_ctxt->ai1_past_list[ps_ctxt->num_ref_past] = -1;

    /*************************************************************************/
    /* Preparation of the TLU for bits for reference indices.                */
    /* Special case is that of numref = 2. (TEV)                             */
    /* Other cases uses UEV                                                  */
    /*************************************************************************/
    for(i = 0; i < MAX_NUM_REF; i++)
    {
        ps_ctxt->au1_ref_bits_tlu_lc[0][i] = 0;
        ps_ctxt->au1_ref_bits_tlu_lc[1][i] = 0;
    }

    if(ps_ref_map->i4_num_ref == 2)
    {
        ps_ctxt->au1_ref_bits_tlu_lc[0][0] = 1;
        ps_ctxt->au1_ref_bits_tlu_lc[1][0] = 1;
        ps_ctxt->au1_ref_bits_tlu_lc[0][1] = 1;
        ps_ctxt->au1_ref_bits_tlu_lc[1][1] = 1;
    }
    else if(ps_ref_map->i4_num_ref > 2)
    {
        for(i = 0; i < ps_ref_map->i4_num_ref; i++)
        {
            S32 l0, l1;
            l0 = ps_ctxt->a_ref_idx_lc_to_l0[i];
            l1 = ps_ctxt->a_ref_idx_lc_to_l1[i];
            ps_ctxt->au1_ref_bits_tlu_lc[0][i] = gau1_ref_bits[l0];
            ps_ctxt->au1_ref_bits_tlu_lc[1][i] = gau1_ref_bits[l1];
        }
    }

    /*************************************************************************/
    /* Preparation of the scaling factors for reference indices. The scale   */
    /* factor depends on distance of the two ref indices from current input  */
    /* in terms of poc delta.                                                */
    /*************************************************************************/
    for(i = 0; i < ps_ref_map->i4_num_ref; i++)
    {
        for(j = 0; j < ps_ref_map->i4_num_ref; j++)
        {
            S16 i2_scf_q8;
            S32 poc_from, poc_to;

            poc_from = ps_ctxt->ai4_ref_idx_to_poc_lc[j];
            poc_to = ps_ctxt->ai4_ref_idx_to_poc_lc[i];

            i2_scf_q8 = hme_scale_for_ref_idx(ps_ctxt->i4_curr_poc, poc_from, poc_to);
            ps_ctxt->ai2_ref_scf[j + i * MAX_NUM_REF] = i2_scf_q8;
        }
    }

    /*************************************************************************/
    /* We store simplified look ups for 4 hpel planes and inp y plane for    */
    /* every layer and for every ref id in the layer. So update these lookups*/
    /*************************************************************************/
    for(i = 0; i < 1; i++)
    {
        U08 **ppu1_rec_fxfy, **ppu1_rec_hxfy, **ppu1_rec_fxhy, **ppu1_rec_hxhy;
        U08 **ppu1_inp;
        void **ppvlist_dep_mngr;
        layer_ctxt_t *ps_layer_ctxt = ps_ctxt->ps_curr_descr->aps_layers[i];

        ppvlist_dep_mngr = &ps_ctxt->apv_list_dep_mngr[i][0];
        ppu1_rec_fxfy = &ps_ctxt->apu1_list_rec_fxfy[i][0];
        ppu1_rec_hxfy = &ps_ctxt->apu1_list_rec_hxfy[i][0];
        ppu1_rec_fxhy = &ps_ctxt->apu1_list_rec_fxhy[i][0];
        ppu1_rec_hxhy = &ps_ctxt->apu1_list_rec_hxhy[i][0];
        ppu1_inp = &ps_ctxt->apu1_list_inp[i][0];
        for(j = 0; j < ps_ref_map->i4_num_ref; j++)
        {
            hme_ref_desc_t *ps_ref_desc;
            hme_ref_buf_info_t *ps_buf_info;
            layer_ctxt_t *ps_layer;
            S32 ref_id_lc;

            ps_ref_desc = &ps_ref_map->as_ref_desc[j];
            ps_buf_info = &ps_ref_desc->as_ref_info[i];
            ref_id_lc = ps_ref_desc->i1_ref_id_lc;

            desc_idx = ps_ctxt->a_ref_to_descr_id[ref_id_lc];
            ps_layer = ps_thrd_ctxt->as_ref_descr[desc_idx].aps_layers[i];

            ppu1_inp[j] = ps_buf_info->pu1_ref_src;
            ppu1_rec_fxfy[j] = ps_buf_info->pu1_rec_fxfy;
            ppu1_rec_hxfy[j] = ps_buf_info->pu1_rec_hxfy;
            ppu1_rec_fxhy[j] = ps_buf_info->pu1_rec_fxhy;
            ppu1_rec_hxhy[j] = ps_buf_info->pu1_rec_hxhy;
            ppvlist_dep_mngr[j] = ps_buf_info->pv_dep_mngr;

            /* Update the curr descriptors reference pointers here */
            ps_layer_ctxt->ppu1_list_inp[j] = ps_buf_info->pu1_ref_src;
            ps_layer_ctxt->ppu1_list_rec_fxfy[j] = ps_buf_info->pu1_rec_fxfy;
            ps_layer_ctxt->ppu1_list_rec_hxfy[j] = ps_buf_info->pu1_rec_hxfy;
            ps_layer_ctxt->ppu1_list_rec_fxhy[j] = ps_buf_info->pu1_rec_fxhy;
            ps_layer_ctxt->ppu1_list_rec_hxhy[j] = ps_buf_info->pu1_rec_hxhy;
        }
    }
    /*************************************************************************/
    /* The mv range for each layer is computed. For dyadic layers it will    */
    /* keep shrinking by 2, for non dyadic it will shrink by ratio of wd and */
    /* ht. In general formula used is scale by ratio of wd for x and ht for y*/
    /*************************************************************************/
    for(i = 0; i < 1; i++)
    {
        layer_ctxt_t *ps_layer_ctxt;
        if(i == 0)
        {
            i2_max_x = ps_frm_prms->i2_mv_range_x;
            i2_max_y = ps_frm_prms->i2_mv_range_y;
        }
        else
        {
            i2_max_x = (S16)FLOOR8(((i2_max_x * ps_ctxt->i4_wd) / ps_ctxt->i4_wd));
            i2_max_y = (S16)FLOOR8(((i2_max_y * ps_ctxt->i4_ht) / ps_ctxt->i4_ht));
        }
        ps_layer_ctxt = ps_ctxt->ps_curr_descr->aps_layers[i];
        ps_layer_ctxt->i2_max_mv_x = i2_max_x;
        ps_layer_ctxt->i2_max_mv_y = i2_max_y;

        /*********************************************************************/
        /* Every layer maintains a reference id lc to POC mapping. This is   */
        /* because the mapping is unique for every frm. Also, in next frm,   */
        /* we require colocated mvs which means scaling according to temporal*/
        /*distance. Hence this mapping needs to be maintained in every       */
        /* layer ctxt                                                        */
        /*********************************************************************/
        memset(ps_layer_ctxt->ai4_ref_id_to_poc_lc, -1, sizeof(S32) * ps_ctxt->max_num_ref);
        if(ps_ref_map->i4_num_ref)
        {
            memcpy(
                ps_layer_ctxt->ai4_ref_id_to_poc_lc,
                ps_ctxt->ai4_ref_idx_to_poc_lc,
                ps_ref_map->i4_num_ref * sizeof(S32));
        }
    }

    return;
}

/**
********************************************************************************
*  @fn     hme_coarse_process_frm_init
*
*  @brief  HME frame level initialsation processing function
*
*  @param[in] pv_me_ctxt : ME ctxt pointer
*
*  @param[in] ps_ref_map : Reference map prms pointer
*
*  @param[in] ps_frm_prms :Pointer to frame params
*
*  @return Scale factor in Q8 format
********************************************************************************
*/
void hme_coarse_process_frm_init(
    void *pv_me_ctxt, hme_ref_map_t *ps_ref_map, hme_frm_prms_t *ps_frm_prms)
{
    coarse_me_ctxt_t *ps_ctxt = (coarse_me_ctxt_t *)pv_me_ctxt;
    S32 i, j, desc_idx;
    S16 i2_max_x = 0, i2_max_y = 0;

    /* Set the Qp of current frm passed by caller. Required for intra cost */
    ps_ctxt->frm_qstep = ps_frm_prms->qstep;

    /* Bidir enabled or not */
    ps_ctxt->s_frm_prms = *ps_frm_prms;

    /*************************************************************************/
    /* Set up the ref pic parameters across all layers. For this, we do the  */
    /* following: the application has given us a ref pic list, we go index   */
    /* by index and pick up the picture. A picture can be uniquely be mapped */
    /* to a POC. So we search all layer descriptor array to find the POC     */
    /* Once found, we update all attributes in this descriptor.              */
    /* During this updation process we also create an index of descriptor id */
    /* to ref id mapping. It is important to find the same POC in the layers */
    /* descr strcture since it holds the pyramid inputs for non encode layers*/
    /* Apart from this, e also update array containing the index of the descr*/
    /* During processing for ease of access, each layer has a pointer to aray*/
    /* of pointers containing fxfy, fxhy, hxfy, hxhy and inputs for each ref */
    /* we update this too.                                                   */
    /*************************************************************************/
    ps_ctxt->num_ref_past = 0;
    ps_ctxt->num_ref_future = 0;
    for(i = 0; i < ps_ref_map->i4_num_ref; i++)
    {
        S32 ref_id_lc, idx;
        hme_ref_desc_t *ps_ref_desc;

        ps_ref_desc = &ps_ref_map->as_ref_desc[i];
        ref_id_lc = ps_ref_desc->i1_ref_id_lc;
        /* Obtain the id of descriptor that contains this POC */
        idx = hme_coarse_find_descr_idx(ps_ctxt, ps_ref_desc->i4_poc);

        /* Update all layers in this descr with the reference attributes */
        hme_update_layer_desc(
            &ps_ctxt->as_ref_descr[idx],
            ps_ref_desc,
            1,
            ps_ctxt->num_layers - 1,
            ps_ctxt->ps_curr_descr);

        /* Update the array having ref id lc to descr id mapping */
        ps_ctxt->a_ref_to_descr_id[ps_ref_desc->i1_ref_id_lc] = idx;

        /* From ref id lc we need to work out the POC, So update this array */
        ps_ctxt->ai4_ref_idx_to_poc_lc[ref_id_lc] = ps_ref_desc->i4_poc;

        /* From ref id lc we need to work out the display num, So update this array */
        ps_ctxt->ai4_ref_idx_to_disp_num[ref_id_lc] = ps_ref_desc->i4_display_num;

        /* When computing costs in L0 and L1 directions, we need the */
        /* respective ref id L0 and L1, so update this mapping */
        ps_ctxt->a_ref_idx_lc_to_l0[ref_id_lc] = ps_ref_desc->i1_ref_id_l0;
        ps_ctxt->a_ref_idx_lc_to_l1[ref_id_lc] = ps_ref_desc->i1_ref_id_l1;
        if((ps_ctxt->i4_curr_poc > ps_ref_desc->i4_poc) || ps_ctxt->i4_curr_poc == 0)
        {
            ps_ctxt->au1_is_past[ref_id_lc] = 1;
            ps_ctxt->ai1_past_list[ps_ctxt->num_ref_past] = ref_id_lc;
            ps_ctxt->num_ref_past++;
        }
        else
        {
            ps_ctxt->au1_is_past[ref_id_lc] = 0;
            ps_ctxt->ai1_future_list[ps_ctxt->num_ref_future] = ref_id_lc;
            ps_ctxt->num_ref_future++;
        }
        if(1 == ps_ctxt->i4_wt_pred_enable_flag)
        {
            /* copy the weight and offsets from current ref desc */
            ps_ctxt->s_wt_pred.a_wpred_wt[ref_id_lc] = ps_ref_desc->i2_weight;

            /* inv weight is stored in Q15 format */
            ps_ctxt->s_wt_pred.a_inv_wpred_wt[ref_id_lc] =
                ((1 << 15) + (ps_ref_desc->i2_weight >> 1)) / ps_ref_desc->i2_weight;

            ps_ctxt->s_wt_pred.a_wpred_off[ref_id_lc] = ps_ref_desc->i2_offset;
        }
        else
        {
            /* store default wt and offset*/
            ps_ctxt->s_wt_pred.a_wpred_wt[ref_id_lc] = WGHT_DEFAULT;

            /* inv weight is stored in Q15 format */
            ps_ctxt->s_wt_pred.a_inv_wpred_wt[ref_id_lc] =
                ((1 << 15) + (WGHT_DEFAULT >> 1)) / WGHT_DEFAULT;

            ps_ctxt->s_wt_pred.a_wpred_off[ref_id_lc] = 0;
        }
    }

    ps_ctxt->ai1_future_list[ps_ctxt->num_ref_future] = -1;
    ps_ctxt->ai1_past_list[ps_ctxt->num_ref_past] = -1;

    /*************************************************************************/
    /* Preparation of the TLU for bits for reference indices.                */
    /* Special case is that of numref = 2. (TEV)                             */
    /* Other cases uses UEV                                                  */
    /*************************************************************************/
    for(i = 0; i < MAX_NUM_REF; i++)
    {
        ps_ctxt->au1_ref_bits_tlu_lc[0][i] = 0;
        ps_ctxt->au1_ref_bits_tlu_lc[1][i] = 0;
    }

    if(ps_ref_map->i4_num_ref == 2)
    {
        ps_ctxt->au1_ref_bits_tlu_lc[0][0] = 1;
        ps_ctxt->au1_ref_bits_tlu_lc[1][0] = 1;
        ps_ctxt->au1_ref_bits_tlu_lc[0][1] = 1;
        ps_ctxt->au1_ref_bits_tlu_lc[1][1] = 1;
    }
    else if(ps_ref_map->i4_num_ref > 2)
    {
        for(i = 0; i < ps_ref_map->i4_num_ref; i++)
        {
            S32 l0, l1;
            l0 = ps_ctxt->a_ref_idx_lc_to_l0[i];
            l1 = ps_ctxt->a_ref_idx_lc_to_l1[i];
            ps_ctxt->au1_ref_bits_tlu_lc[0][i] = gau1_ref_bits[l0];
            ps_ctxt->au1_ref_bits_tlu_lc[1][i] = gau1_ref_bits[l1];
        }
    }

    /*************************************************************************/
    /* Preparation of the scaling factors for reference indices. The scale   */
    /* factor depends on distance of the two ref indices from current input  */
    /* in terms of poc delta.                                                */
    /*************************************************************************/
    for(i = 0; i < ps_ref_map->i4_num_ref; i++)
    {
        for(j = 0; j < ps_ref_map->i4_num_ref; j++)
        {
            S16 i2_scf_q8;
            S32 poc_from, poc_to;

            poc_from = ps_ctxt->ai4_ref_idx_to_poc_lc[j];
            poc_to = ps_ctxt->ai4_ref_idx_to_poc_lc[i];

            i2_scf_q8 = hme_scale_for_ref_idx(ps_ctxt->i4_curr_poc, poc_from, poc_to);
            ps_ctxt->ai2_ref_scf[j + i * MAX_NUM_REF] = i2_scf_q8;
        }
    }

    /*************************************************************************/
    /* We store simplified look ups for inp y plane for                      */
    /* every layer and for every ref id in the layer.                        */
    /*************************************************************************/
    for(i = 1; i < ps_ctxt->num_layers; i++)
    {
        U08 **ppu1_inp;

        ppu1_inp = &ps_ctxt->apu1_list_inp[i][0];
        for(j = 0; j < ps_ref_map->i4_num_ref; j++)
        {
            hme_ref_desc_t *ps_ref_desc;
            hme_ref_buf_info_t *ps_buf_info;
            layer_ctxt_t *ps_layer;
            S32 ref_id_lc;

            ps_ref_desc = &ps_ref_map->as_ref_desc[j];
            ps_buf_info = &ps_ref_desc->as_ref_info[i];
            ref_id_lc = ps_ref_desc->i1_ref_id_lc;

            desc_idx = ps_ctxt->a_ref_to_descr_id[ref_id_lc];
            ps_layer = ps_ctxt->as_ref_descr[desc_idx].aps_layers[i];

            ppu1_inp[j] = ps_layer->pu1_inp;
        }
    }
    /*************************************************************************/
    /* The mv range for each layer is computed. For dyadic layers it will    */
    /* keep shrinking by 2, for non dyadic it will shrink by ratio of wd and */
    /* ht. In general formula used is scale by ratio of wd for x and ht for y*/
    /*************************************************************************/

    /* set to layer 0 search range params */
    i2_max_x = ps_frm_prms->i2_mv_range_x;
    i2_max_y = ps_frm_prms->i2_mv_range_y;

    for(i = 1; i < ps_ctxt->num_layers; i++)
    {
        layer_ctxt_t *ps_layer_ctxt;

        {
            i2_max_x = (S16)FLOOR8(((i2_max_x * ps_ctxt->a_wd[i]) / ps_ctxt->a_wd[i - 1]));
            i2_max_y = (S16)FLOOR8(((i2_max_y * ps_ctxt->a_ht[i]) / ps_ctxt->a_ht[i - 1]));
        }
        ps_layer_ctxt = ps_ctxt->ps_curr_descr->aps_layers[i];
        ps_layer_ctxt->i2_max_mv_x = i2_max_x;
        ps_layer_ctxt->i2_max_mv_y = i2_max_y;

        /*********************************************************************/
        /* Every layer maintains a reference id lc to POC mapping. This is   */
        /* because the mapping is unique for every frm. Also, in next frm,   */
        /* we require colocated mvs which means scaling according to temporal*/
        /*distance. Hence this mapping needs to be maintained in every       */
        /* layer ctxt                                                        */
        /*********************************************************************/
        memset(ps_layer_ctxt->ai4_ref_id_to_poc_lc, -1, sizeof(S32) * ps_ctxt->max_num_ref);
        if(ps_ref_map->i4_num_ref)
        {
            memcpy(
                ps_layer_ctxt->ai4_ref_id_to_poc_lc,
                ps_ctxt->ai4_ref_idx_to_poc_lc,
                ps_ref_map->i4_num_ref * sizeof(S32));
            memcpy(
                ps_layer_ctxt->ai4_ref_id_to_disp_num,
                ps_ctxt->ai4_ref_idx_to_disp_num,
                ps_ref_map->i4_num_ref * sizeof(S32));
        }
    }

    return;
}

/**
********************************************************************************
*  @fn     hme_process_frm
*
*  @brief  HME frame level processing function
*
*  @param[in] pv_me_ctxt : ME ctxt pointer
*
*  @param[in] ps_ref_map : Reference map prms pointer
*
*  @param[in] ppd_intra_costs : pointer to array of intra cost cost buffers for each layer
*
*  @param[in] ps_frm_prms : pointer to Frame level parameters of HME
*
*  @param[in] pf_ext_update_fxn : function pointer to update CTb results
*
*  @param[in] pf_get_intra_cu_and_cost :function pointer to get intra cu size and cost
*
*  @param[in] ps_multi_thrd_ctxt :function pointer to get intra cu size and cost
*
*  @return Scale factor in Q8 format
********************************************************************************
*/

void hme_process_frm(
    void *pv_me_ctxt,
    pre_enc_L0_ipe_encloop_ctxt_t *ps_l0_ipe_input,
    hme_ref_map_t *ps_ref_map,
    double **ppd_intra_costs,
    hme_frm_prms_t *ps_frm_prms,
    PF_EXT_UPDATE_FXN_T pf_ext_update_fxn,
    void *pv_coarse_layer,
    void *pv_multi_thrd_ctxt,
    S32 i4_frame_parallelism_level,
    S32 thrd_id,
    S32 i4_me_frm_id)
{
    refine_prms_t s_refine_prms;
    me_ctxt_t *ps_thrd_ctxt = (me_ctxt_t *)pv_me_ctxt;
    me_frm_ctxt_t *ps_ctxt = ps_thrd_ctxt->aps_me_frm_prms[i4_me_frm_id];

    S32 lyr_job_type;
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt;
    layer_ctxt_t *ps_coarse_layer = (layer_ctxt_t *)pv_coarse_layer;

    ps_multi_thrd_ctxt = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;

    lyr_job_type = ME_JOB_ENC_LYR;
    /*************************************************************************/
    /* Final L0 layer ME call                                                */
    /*************************************************************************/
    {
        /* Set the CTB attributes dependin on corner/rt edge/bot edge/center*/
        hme_set_ctb_attrs(ps_ctxt->as_ctb_bound_attrs, ps_ctxt->i4_wd, ps_ctxt->i4_ht);

        hme_set_refine_prms(
            &s_refine_prms,
            ps_ctxt->u1_encode[0],
            ps_ref_map->i4_num_ref,
            0,
            ps_ctxt->num_layers,
            ps_ctxt->num_layers_explicit_search,
            ps_thrd_ctxt->s_init_prms.use_4x4,
            ps_frm_prms,
            ppd_intra_costs,
            &ps_thrd_ctxt->s_init_prms.s_me_coding_tools);

        hme_refine(
            ps_thrd_ctxt,
            &s_refine_prms,
            pf_ext_update_fxn,
            ps_coarse_layer,
            ps_multi_thrd_ctxt,
            lyr_job_type,
            thrd_id,
            i4_me_frm_id,
            ps_l0_ipe_input);

        /* Set current ref pic status which will used as perv frame ref pic */
        if(i4_frame_parallelism_level)
        {
            ps_ctxt->i4_is_prev_frame_reference = 0;
        }
        else
        {
            ps_ctxt->i4_is_prev_frame_reference =
                ps_multi_thrd_ctxt->aps_cur_inp_me_prms[i4_me_frm_id]
                    ->ps_curr_inp->s_lap_out.i4_is_ref_pic;
        }
    }

    return;
}

/**
********************************************************************************
*  @fn     hme_coarse_process_frm
*
*  @brief  HME frame level processing function (coarse + refine)
*
*  @param[in] pv_me_ctxt : ME ctxt pointer
*
*  @param[in] ps_ref_map : Reference map prms pointer
*
*  @param[in] ps_frm_prms : pointer to Frame level parameters of HME
*
*  @param[in] ps_multi_thrd_ctxt :Multi thread related ctxt
*
*  @return Scale factor in Q8 format
********************************************************************************
*/

void hme_coarse_process_frm(
    void *pv_me_ctxt,
    hme_ref_map_t *ps_ref_map,
    hme_frm_prms_t *ps_frm_prms,
    void *pv_multi_thrd_ctxt,
    WORD32 i4_ping_pong,
    void **ppv_dep_mngr_hme_sync)
{
    S16 i2_max;
    S32 layer_id;
    coarse_prms_t s_coarse_prms;
    refine_prms_t s_refine_prms;
    coarse_me_ctxt_t *ps_ctxt = (coarse_me_ctxt_t *)pv_me_ctxt;
    S32 lyr_job_type;
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt;

    ps_multi_thrd_ctxt = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;
    /*************************************************************************/
    /* Fire processing of all layers, starting with coarsest layer.          */
    /*************************************************************************/
    layer_id = ps_ctxt->num_layers - 1;
    i2_max = ps_ctxt->ps_curr_descr->aps_layers[layer_id]->i2_max_mv_x;
    i2_max = MAX(i2_max, ps_ctxt->ps_curr_descr->aps_layers[layer_id]->i2_max_mv_y);
    s_coarse_prms.i4_layer_id = layer_id;
    {
        S32 log_start_step;
        /* Based on Preset, set the starting step size for Refinement */
        if(ME_MEDIUM_SPEED > ps_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets)
        {
            log_start_step = 0;
        }
        else
        {
            log_start_step = 1;
        }

        s_coarse_prms.i4_max_iters = i2_max >> log_start_step;
        s_coarse_prms.i4_start_step = 1 << log_start_step;
    }
    s_coarse_prms.i4_num_ref = ps_ref_map->i4_num_ref;
    s_coarse_prms.do_full_search = 1;
    if(s_coarse_prms.do_full_search)
    {
        /* Set to 2 or 4 */
        if(ps_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets < ME_MEDIUM_SPEED)
            s_coarse_prms.full_search_step = HME_COARSE_STEP_SIZE_HIGH_QUALITY;
        else if(ps_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets >= ME_MEDIUM_SPEED)
            s_coarse_prms.full_search_step = HME_COARSE_STEP_SIZE_HIGH_SPEED;
    }
    s_coarse_prms.num_results = ps_ctxt->max_num_results_coarse;

    /* Coarse layer uses only 1 lambda, i.e. the one for open loop ME */
    s_coarse_prms.lambda = ps_frm_prms->i4_ol_sad_lambda_qf;
    s_coarse_prms.lambda_q_shift = ps_frm_prms->lambda_q_shift;
    s_coarse_prms.lambda = ((float)s_coarse_prms.lambda * (100.0 - ME_LAMBDA_DISCOUNT) / 100.0);

    hme_coarsest(ps_ctxt, &s_coarse_prms, ps_multi_thrd_ctxt, i4_ping_pong, ppv_dep_mngr_hme_sync);

    /* all refinement layer processed in the loop below */
    layer_id--;
    lyr_job_type = ps_multi_thrd_ctxt->i4_me_coarsest_lyr_type + 1;

    /*************************************************************************/
    /* This loop will run for all refine layers (non- encode layers)          */
    /*************************************************************************/
    while(layer_id > 0)
    {
        hme_set_refine_prms(
            &s_refine_prms,
            ps_ctxt->u1_encode[layer_id],
            ps_ref_map->i4_num_ref,
            layer_id,
            ps_ctxt->num_layers,
            ps_ctxt->num_layers_explicit_search,
            ps_ctxt->s_init_prms.use_4x4,
            ps_frm_prms,
            NULL,
            &ps_ctxt->s_init_prms.s_me_coding_tools);

        hme_refine_no_encode(
            ps_ctxt,
            &s_refine_prms,
            ps_multi_thrd_ctxt,
            lyr_job_type,
            i4_ping_pong,
            ppv_dep_mngr_hme_sync);

        layer_id--;
        lyr_job_type++;
    }
}
/**
********************************************************************************
*  @fn     hme_fill_neighbour_mvs
*
*  @brief  HME neighbour MV population function
*
*  @param[in] pps_mv_grid : MV grid array pointer
*
*  @param[in] i4_ctb_x : CTB pos X

*  @param[in] i4_ctb_y : CTB pos Y
*
*  @remarks :  Needs to be populated for proper implementation of cost fxn
*
*  @return Scale factor in Q8 format
********************************************************************************
*/
void hme_fill_neighbour_mvs(
    mv_grid_t **pps_mv_grid, S32 i4_ctb_x, S32 i4_ctb_y, S32 i4_num_ref, void *pv_ctxt)
{
    /* TODO : Needs to be populated for proper implementation of cost fxn */
    ARG_NOT_USED(pps_mv_grid);
    ARG_NOT_USED(i4_ctb_x);
    ARG_NOT_USED(i4_ctb_y);
    ARG_NOT_USED(i4_num_ref);
    ARG_NOT_USED(pv_ctxt);
}

/**
*******************************************************************************
*  @fn     void hme_get_active_pocs_list(void *pv_me_ctxt,
*                                       S32 *p_pocs_buffered_in_me)
*
*  @brief  Returns the list of active POCs in ME ctxt
*
*  @param[in] pv_me_ctxt : handle to ME context
*
*  @param[out] p_pocs_buffered_in_me : pointer to an array which this fxn
*                                      populates with pocs active
*
*  @return   void
*******************************************************************************
*/
WORD32 hme_get_active_pocs_list(void *pv_me_ctxt, S32 i4_num_me_frm_pllel)
{
    me_ctxt_t *ps_ctxt = (me_ctxt_t *)pv_me_ctxt;
    S32 i, count = 0;

    for(i = 0; i < (ps_ctxt->aps_me_frm_prms[0]->max_num_ref * i4_num_me_frm_pllel) + 1; i++)
    {
        S32 poc = ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_poc;
        S32 i4_is_free = ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_is_free;

        if((i4_is_free == 0) && (poc != INVALID_POC))
        {
            count++;
        }
    }
    if(count == (ps_ctxt->aps_me_frm_prms[0]->max_num_ref * i4_num_me_frm_pllel) + 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
*******************************************************************************
*  @fn     void hme_coarse_get_active_pocs_list(void *pv_me_ctxt,
*                                       S32 *p_pocs_buffered_in_me)
*
*  @brief  Returns the list of active POCs in ME ctxt
*
*  @param[in] pv_me_ctxt : handle to ME context
*
*  @param[out] p_pocs_buffered_in_me : pointer to an array which this fxn
*                                      populates with pocs active
*
*  @return   void
*******************************************************************************
*/
void hme_coarse_get_active_pocs_list(void *pv_me_ctxt, S32 *p_pocs_buffered_in_me)
{
    coarse_me_ctxt_t *ps_ctxt = (coarse_me_ctxt_t *)pv_me_ctxt;
    S32 i, count = 0;

    for(i = 0; i < ps_ctxt->max_num_ref + 1 + NUM_BUFS_DECOMP_HME; i++)
    {
        S32 poc = ps_ctxt->as_ref_descr[i].aps_layers[1]->i4_poc;

        if(poc != -1)
        {
            p_pocs_buffered_in_me[count] = poc;
            count++;
        }
    }
    p_pocs_buffered_in_me[count] = -1;
}

S32 hme_get_blk_size(S32 use_4x4, S32 layer_id, S32 n_layers, S32 encode)
{
    /* coarsest layer uses 4x4 blks, lowermost layer/encode layer uses 16x16 */
    if(layer_id == n_layers - 1)
        return 4;
    else if((layer_id == 0) || (encode))
        return 16;

    /* Intermediate non encode layers use 8 */
    return 8;
}
