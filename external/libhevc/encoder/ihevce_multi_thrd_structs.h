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
* \file ihevce_multi_thrd_structs.h
*
* \brief
*    This file contains structure definations  of multi thread based processing
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_MULTI_THRD_STRUCTS_H_
#define _IHEVCE_MULTI_THRD_STRUCTS_H_

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/** Maximum number of modules on whose outputs any module's inputs are dependent */
#define MAX_IN_DEP 80

/** Maximum number of modules whose inputs are dependent on any module's outputs */
#define MAX_OUT_DEP 80

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

typedef enum
{
    ME_JOB_ENC_LYR = 0,
    ENC_LOOP_JOB,
    ENC_LOOP_JOB1,
    ENC_LOOP_JOB2,
    ENC_LOOP_JOB3,
    ENC_LOOP_JOB4,  //MBR: enc_loop job instance created for each bit-rate.
    //change instances based on IHEVCE_MAX_NUM_BITRATES

    NUM_ENC_JOBS_QUES,

} HEVCE_ENC_JOB_TYPES_T;

typedef enum
{
    DECOMP_JOB_LYR0 = 0,
    DECOMP_JOB_LYR1,
    DECOMP_JOB_LYR2,
    DECOMP_JOB_LYR3,
    ME_JOB_LYR4,
    ME_JOB_LYR3,
    ME_JOB_LYR2,
    ME_JOB_LYR1,
    IPE_JOB_LYR0,

    NUM_PRE_ENC_JOBS_QUES,

} HEVCE_PRE_ENC_JOB_TYPES_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief     IPE Job parameters structure
******************************************************************************
 */
typedef struct
{
    /*!< Index of the CTB Row */
    WORD32 i4_ctb_row_no;

} ipe_job_node_t;

/**
******************************************************************************
 *  @brief     ME Job parameters structure
******************************************************************************
 */
typedef struct
{
    /** Index of the Vertical unit Row */
    WORD32 i4_vert_unit_row_no;
    WORD32 i4_tile_col_idx;

} me_job_node_t;

/**
******************************************************************************
 *  @brief     Encode Loop Job parameters structure
******************************************************************************
 */
typedef struct
{
    /** Index of the CTB Row */
    WORD32 i4_ctb_row_no;
    WORD32 i4_tile_col_idx;
    WORD32 i4_bitrate_instance_no;

} enc_loop_job_node_t;

/**
******************************************************************************
 *  @brief     Decomposition Job parameters structure
******************************************************************************
 */
typedef struct
{
    /** Index of the Vertical unit Row */
    WORD32 i4_vert_unit_row_no;

} decomp_job_node_t;

/**
******************************************************************************
 *  @brief     Union Job parameters structure
******************************************************************************
 */
typedef union /* Make sure that the size is a multiple of 4 */
{
    ipe_job_node_t s_ipe_job_info;

    me_job_node_t s_me_job_info;

    enc_loop_job_node_t s_enc_loop_job_info;

    decomp_job_node_t s_decomp_job_info;

} job_info_t;

/**
******************************************************************************
 *  @brief     Job Queue Element parameters structure
******************************************************************************
 */
typedef struct
{
    /** Array of flags indicating the input dependencies of the module.
      *      Flag set to 0 indicates that the input dependency is resolved.
      *     Processing can start only after all the flags are 0.
      *
            *    This has to be the first element of the array, MAX_IN_DEP has to be multiple of 4
            */
    UWORD8 au1_in_dep[MAX_IN_DEP];

    /** Pointer to the next link in the job queue */
    void *pv_next;

    /** Job information ctxt of the module */
    job_info_t s_job_info;

    /** Array of offsets for the output dependencies' pointers.
             *   Indicates the location where  the dependency flag needs to
             *   be set after the processing of the current NMB/row/slice
             */
    UWORD32 au4_out_ofsts[MAX_OUT_DEP];

    /** Number of input dependencies to be checked before starting current task */
    WORD32 i4_num_input_dep;

    /** Number of output dependencies to be updated after finishing current task */
    WORD32 i4_num_output_dep;

    /** indicates what type of task is to be    executed
     * [ME_JOB for layer 0,ENC_LOOP_JOB] are valid
     * -1 will be set if this hob task type is irrelevant
     */
    HEVCE_ENC_JOB_TYPES_T i4_task_type;

    /** indicates what type of task is to be    executed
     * [ME_JOB for coarse and refine layers, DECOMP Jobs  and IPE JOB] are valid
     * -1 will be set if this hob task type is irrelevant
     */
    HEVCE_PRE_ENC_JOB_TYPES_T i4_pre_enc_task_type;

} job_queue_t;

/**
******************************************************************************
 *  @brief     Job Queue Handle structure
******************************************************************************
 */
typedef struct
{
    /** Pointer to the next link in the job queue  */
    void *pv_next;

} job_queue_handle_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

#endif /* _IHEVCE_MULTI_THRD_STRUCTS_H_ */
